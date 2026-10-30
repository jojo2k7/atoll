/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QStringList>

class Config;

/**
 * One thing the assistant can be pointed at: a class, a widget, a provider.
 */
struct ProjectSymbol {
    QString name;
    /** class | mixin | enum | extension | typedef | function | constant */
    QString kind;
    /** How the file is written when it is shown: "lib/x.dart", "package:flutter/src/…". */
    QString path;
    int line = 0;
    /** The declaration itself, trimmed to one line. */
    QString signature;
    /** The first line of the doc comment above it, when there is one. */
    QString doc;
};

/**
 * The codebase the assistant is for, held in memory so that answering does not
 * start with a search.
 *
 * Atoll's assistant used to begin every question about the app the same way:
 * list a directory, grep for a name, read the wrong file, try again. Four
 * round trips before the first useful sentence, and each one a chance to go
 * somewhere else. This class removes the search from the critical path. Every
 * declaration in the project - and in the packages its questions are really
 * about, Flutter itself among them - is scanned once, cached on disk, and then
 * answered from a hash table in microseconds.
 *
 * Two things follow from that, and they are the whole point:
 *
 * - The prompt can carry a *map* of the project rather than an invitation to
 *   go looking. What the app is, how lib/ is laid out, which packages matter,
 *   how big each part is. It costs a couple of thousand tokens once, it is
 *   cached by the provider, and it turns most questions into one tool call.
 * - The tools that remain are exact. `lookup` knows where AdaptiveButton is
 *   declared; it does not guess, and it cannot answer about a class that is
 *   not there. Being told "no such symbol" in 3 ms is worth more than a
 *   plausible paragraph.
 *
 * The knowledge graph is the third leg: where the index answers "where is X",
 * graphify answers "what does X have to do with Y", and that is left to it
 * rather than reinvented here.
 */
class ProjectIndex : public QObject
{
    Q_OBJECT

public:
    explicit ProjectIndex(Config *config, QObject *parent = nullptr);
    ~ProjectIndex() override;

    /** Whether project mode is switched on and the directory is really there. */
    bool available() const;
    /** Absolute path of the project, without a trailing slash. */
    QString root() const
    {
        return m_root;
    }
    /** What to call it in a sentence: "Corely". */
    QString label() const;
    /** The Dart package name, as pubspec.yaml has it: "corely". */
    QString packageName() const
    {
        return m_package;
    }

    /** Whether a graphify graph exists for this project, and where the tool is. */
    bool graphAvailable() const;
    QString graphProgram() const
    {
        return m_graphify;
    }

    /**
     * The block that goes into the system prompt: what the project is, how it
     * is laid out, what is indexed. A few thousand characters, unchanged
     * between questions, and therefore cached by the provider rather than
     * re-read on every turn.
     */
    QString digest();

    // ---- what the tools call ---------------------------------------------

    /** Everywhere a name is declared, nearest match first. */
    QString lookup(const QString &name, const QString &kind, int limit);
    /** Regular-expression search over the sources, as file:line + the line. */
    QString search(const QString &pattern, const QString &scope, const QString &glob, int limit);
    /** A file, or a window into one. */
    QString read(const QString &path, int from, int count);
    /** Every declaration in one file, with its line numbers. */
    QString outline(const QString &path);

    /**
     * Turn whatever the model wrote into an absolute path: a project-relative
     * path, an absolute one inside a known root, or a `package:foo/bar.dart`
     * import URI. Empty when it points outside everything indexed, which is
     * what keeps these tools read-only *and* narrow.
     */
    QString resolve(const QString &pathOrUri) const;
    /** The other direction, for showing a path back to the user. */
    QString displayPath(const QString &absolute) const;

private:
    /** One directory tree that is indexed as a unit. */
    struct Root {
        /** "project", or the package name. */
        QString key;
        /** Absolute directory. */
        QString dir;
        /** What paths inside it are called: "" for the project, "package:flutter/" else. */
        QString prefix;
        bool isProject = false;
        int files = 0;
    };

    void configure();
    /** Start the scan on a pool thread; queries wait for it if they get there first. */
    void beginBuild();
    /** Block until the index is usable. Cheap after the first time. */
    void ensureReady() const;

    void buildNow();
    QList<ProjectSymbol> scanRoot(const Root &root, int *fileCount) const;
    static QList<ProjectSymbol> scanFile(const QString &absolutePath, const QString &shownPath);
    QStringList dartFiles(const QString &dir) const;
    QString fingerprintOf(const Root &root) const;

    bool loadCache();
    void saveCache() const;
    QString cachePath() const;

    QString buildDigest() const;
    /** pubspec.yaml, reduced to the lines that say something. */
    QString pubspecSummary() const;
    /** The shape of lib/, two levels deep, with file counts. */
    QString layoutSummary() const;
    /** The project's own README, reduced to its headings. */
    QString readmeSummary() const;
    /** Which packages are resolved where, from .dart_tool/package_config.json. */
    QHash<QString, QString> packageRoots() const;

    Config *m_config = nullptr;

    QString m_root;
    QString m_label;
    QString m_package;
    QString m_graphify;
    bool m_enabled = false;

    QList<Root> m_roots;
    /** Lower-cased name → every declaration of it. */
    QHash<QString, QList<ProjectSymbol>> m_symbols;
    /** Fingerprint per root, so an unchanged tree is never scanned twice. */
    QHash<QString, QString> m_fingerprints;
    QString m_digest;

    /** Released once when the build finishes; acquired by every reader. */
    mutable QSemaphore m_ready;
    bool m_building = false;
};
