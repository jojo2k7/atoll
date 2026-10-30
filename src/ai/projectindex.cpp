/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "projectindex.h"

#include "config/config.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

#include <functional>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(ATOLL_PROJECT, "atoll.project")

namespace
{
/** The declarations worth knowing the address of, in the order they are tried. */
struct Pattern {
    QString kind;
    QRegularExpression expression;
};

const QList<Pattern> &patterns()
{
    // Anchored at the start of the line on purpose: a member of a class is
    // indented, and indexing every method by name would fill the table with
    // twenty `build`s and no way to choose between them. What is wanted here
    // is the thing you can import - the rest is what search is for.
    static const QList<Pattern> list = {
        {u"class"_s,
         QRegularExpression(uR"(^(?:abstract |base |final |interface |sealed |mixin )*class\s+([A-Za-z_$][\w$]*))"_s)},
        {u"mixin"_s, QRegularExpression(uR"(^(?:base )?mixin\s+([A-Za-z_$][\w$]*))"_s)},
        {u"enum"_s, QRegularExpression(uR"(^enum\s+([A-Za-z_$][\w$]*))"_s)},
        {u"extension"_s, QRegularExpression(uR"(^extension(?:\s+type)?\s+([A-Za-z_$][\w$]*))"_s)},
        {u"typedef"_s, QRegularExpression(uR"(^typedef\s+([A-Za-z_$][\w$]*))"_s)},
        {u"function"_s,
         QRegularExpression(uR"(^(?:Future|Stream|void|bool|int|double|num|String|List|Map|Set|Widget|[A-Z][\w$]*)(?:<[^>]*>)?\??\s+([a-z_$][\w$]*)\s*\()"_s)},
        {u"constant"_s,
         QRegularExpression(uR"(^(?:const|final)\s+(?:[\w$<>,\s\?\[\]\.]+\s+)?([A-Za-z_$][\w$]*)\s*=)"_s)},
    };
    return list;
}

QString trimmedLine(const QString &line)
{
    QString text = line.trimmed();
    if (text.size() > 200) {
        text = text.left(197) + u"…"_s;
    }
    return text;
}

/** A runnable that is only ever a thin wrapper around one call. */
class Task : public QRunnable
{
public:
    explicit Task(std::function<void()> work)
        : m_work(std::move(work))
    {
        setAutoDelete(true);
    }
    void run() override
    {
        m_work();
    }

private:
    std::function<void()> m_work;
};
}

ProjectIndex::ProjectIndex(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    configure();
    if (config) {
        // The project can be pointed somewhere else while Atoll is running,
        // and the next question should be about the new place.
        connect(config, &Config::changed, this, [this] {
            const QString before = m_root;
            const bool wasEnabled = m_enabled;
            configure();
            if (m_root != before || m_enabled != wasEnabled) {
                m_symbols.clear();
                m_fingerprint.clear();
                m_digest.clear();
                beginBuild();
            }
        });
    }
    beginBuild();
}

ProjectIndex::~ProjectIndex() = default;

void ProjectIndex::configure()
{
    if (!m_config) {
        m_enabled = false;
        return;
    }

    m_enabled = m_config->value(u"ai.project.enabled"_s, false).toBool();

    QString root = m_config->value(u"ai.project.root"_s, QString()).toString().trimmed();
    if (root.startsWith(u'~')) {
        root = QDir::homePath() + root.mid(1);
    }
    m_root = root.isEmpty() ? QString() : QDir::cleanPath(QDir(root).absolutePath());

    m_label = m_config->value(u"ai.project.name"_s, QString()).toString().trimmed();

    m_package.clear();
    QFile pubspec(QDir(m_root).filePath(u"pubspec.yaml"_s));
    if (pubspec.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(pubspec.read(4000)).split(u'\n');
        for (const QString &line : lines) {
            if (line.startsWith(u"name:"_s)) {
                m_package = line.mid(5).trimmed();
                break;
            }
        }
    }

    // graphify is usually a user-level install, and a desktop session does not
    // always carry ~/.local/bin in its PATH, so look there as well as asking.
    m_graphify = QStandardPaths::findExecutable(u"graphify"_s);
    if (m_graphify.isEmpty()) {
        const QString local = QDir(QDir::homePath()).filePath(u".local/bin/graphify"_s);
        if (QFileInfo(local).isExecutable()) {
            m_graphify = local;
        }
    }
}

bool ProjectIndex::available() const
{
    return m_enabled && !m_root.isEmpty() && QFileInfo(m_root).isDir();
}

QString ProjectIndex::label() const
{
    if (!m_label.isEmpty()) {
        return m_label;
    }
    if (!m_package.isEmpty()) {
        return m_package;
    }
    return QFileInfo(m_root).fileName();
}

bool ProjectIndex::graphAvailable() const
{
    return !m_graphify.isEmpty()
        && QFileInfo::exists(QDir(m_root).filePath(u"graphify-out/graph.json"_s));
}

// ---- building ------------------------------------------------------------

void ProjectIndex::beginBuild()
{
    if (!available() || m_building) {
        return;
    }
    m_building = true;
    // Off the GUI thread: a first scan of a large project plus the Flutter SDK
    // is about a second of work, and a second is a visibly stuck island.
    // Anyone who asks a question before it lands waits on the semaphore.
    QThreadPool::globalInstance()->start(new Task([this] {
        buildNow();
        m_building = false;
        m_ready.release();
    }));
}

void ProjectIndex::ensureReady() const
{
    if (!available()) {
        return;
    }
    // Acquire-and-give-back: the first reader through blocks until the build
    // releases its permit, and every reader after that passes straight on.
    m_ready.acquire();
    m_ready.release();
}

QStringList ProjectIndex::dartFiles(const QString &dir) const
{
    QStringList files;
    if (!QFileInfo(dir).isDir()) {
        return files;
    }
    QDirIterator it(dir, {u"*.dart"_s}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        // Generated output is noise in an index: it is nobody's source of
        // truth, and it is where the duplicate class names come from.
        if (path.contains(u"/.dart_tool/"_s) || path.contains(u"/build/"_s)
            || path.endsWith(u".g.dart"_s) || path.endsWith(u".freezed.dart"_s)
            || path.endsWith(u".mocks.dart"_s)) {
            continue;
        }
        files.append(path);
    }
    files.sort();
    return files;
}

QHash<QString, QString> ProjectIndex::packageRoots() const
{
    QHash<QString, QString> roots;
    QFile file(QDir(m_root).filePath(u".dart_tool/package_config.json"_s));
    if (!file.open(QIODevice::ReadOnly)) {
        return roots;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray packages = object.value(u"packages"_s).toArray();
    for (const QJsonValue &value : packages) {
        const QJsonObject package = value.toObject();
        const QString name = package.value(u"name"_s).toString();
        const QString rootUri = package.value(u"rootUri"_s).toString();
        if (name.isEmpty() || rootUri.isEmpty()) {
            continue;
        }
        QString base = rootUri.startsWith(u"file:"_s) ? QUrl(rootUri).toLocalFile() : rootUri;
        if (!base.startsWith(u'/')) {
            // Relative URIs are relative to .dart_tool/, which is where the
            // file that holds them lives.
            base = QDir(QDir(m_root).filePath(u".dart_tool"_s)).absoluteFilePath(base);
        }
        const QString libDir = package.value(u"packageUri"_s).toString(u"lib/"_s);
        roots.insert(name, QDir::cleanPath(QDir(base).filePath(libDir)));
    }
    return roots;
}

QList<ProjectIndex::Root> ProjectIndex::rootsToScan() const
{
    QList<Root> roots;

    Root project;
    project.key = u"project"_s;
    project.dir = m_root;
    project.isProject = true;
    roots.append(project);

    // Then whichever packages the questions are really about. Flutter is in
    // that list because "how does a ListView decide when to build a child" is
    // a question about this app as much as any other, and reading the
    // framework beats remembering it.
    const QHash<QString, QString> resolved = packageRoots();
    const QVariant configured = m_config ? m_config->value(u"ai.project.packages"_s) : QVariant();
    for (const QVariant &entry : configured.toList()) {
        const QString name = entry.toString().trimmed();
        const auto it = resolved.constFind(name);
        if (name.isEmpty() || it == resolved.constEnd() || !QFileInfo(*it).isDir()) {
            continue;
        }
        Root root;
        root.key = name;
        root.dir = *it;
        root.prefix = u"package:%1/"_s.arg(name);
        roots.append(root);
    }
    return roots;
}

void ProjectIndex::buildNow()
{
    QElapsedTimer timer;
    timer.start();

    m_roots = rootsToScan();

    // One fingerprint over everything, rather than one per root. Scanning is
    // cheap and rare; keeping two half-caches in step is neither.
    QCryptographicHash hash(QCryptographicHash::Sha1);
    QList<QStringList> fileLists;
    for (const Root &root : std::as_const(m_roots)) {
        const QStringList files = dartFiles(root.dir);
        fileLists.append(files);
        hash.addData(root.dir.toUtf8());
        for (const QString &file : files) {
            const QFileInfo info(file);
            hash.addData(file.toUtf8());
            hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
            hash.addData(QByteArray::number(info.size()));
        }
    }
    const QString fingerprint = QString::fromLatin1(hash.result().toHex());

    if (loadCache(fingerprint)) {
        qCDebug(ATOLL_PROJECT, "index restored from cache in %lld ms", timer.elapsed());
        return;
    }

    m_symbols.clear();
    for (qsizetype index = 0; index < m_roots.size(); ++index) {
        Root &root = m_roots[index];
        const QStringList &files = fileLists.at(index);
        root.files = int(files.size());
        for (const QString &file : files) {
            const QString shown = root.isProject
                ? QDir(m_root).relativeFilePath(file)
                : root.prefix + QDir(root.dir).relativeFilePath(file);
            const QList<ProjectSymbol> found = scanFile(file, shown);
            for (const ProjectSymbol &symbol : found) {
                m_symbols[symbol.name.toLower()].append(symbol);
            }
        }
    }

    m_fingerprint = fingerprint;
    m_digest = buildDigest();
    saveCache();

    qCDebug(ATOLL_PROJECT, "indexed %d symbols from %lld roots in %lld ms",
            symbolCount(), static_cast<long long>(m_roots.size()), timer.elapsed());
}

int ProjectIndex::symbolCount() const
{
    int total = 0;
    for (auto it = m_symbols.constBegin(); it != m_symbols.constEnd(); ++it) {
        total += int(it.value().size());
    }
    return total;
}

QList<ProjectSymbol> ProjectIndex::scanFile(const QString &absolutePath, const QString &shownPath)
{
    QList<ProjectSymbol> symbols;
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return symbols;
    }

    const QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n');
    QString doc;
    for (qsizetype index = 0; index < lines.size(); ++index) {
        const QString &line = lines.at(index);

        if (line.startsWith(u"///"_s)) {
            // Only the first line of a doc comment is kept: it is the sentence
            // that says what the thing is, and the rest is for whoever opens
            // the file.
            if (doc.isEmpty()) {
                doc = line.mid(3).trimmed();
            }
            continue;
        }

        bool matched = false;
        for (const Pattern &pattern : patterns()) {
            const auto match = pattern.expression.match(line);
            if (!match.hasMatch()) {
                continue;
            }
            ProjectSymbol symbol;
            symbol.name = match.captured(1);
            symbol.kind = pattern.kind;
            symbol.path = shownPath;
            symbol.line = int(index) + 1;
            symbol.signature = trimmedLine(line);
            symbol.doc = doc;
            symbols.append(symbol);
            matched = true;
            break;
        }

        if (matched || !line.trimmed().isEmpty()) {
            doc.clear();
        }
    }
    return symbols;
}
