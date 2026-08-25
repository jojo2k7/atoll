/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "credentialstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace
{
constexpr int SecretTimeoutMs = 4000;

QString secretTool()
{
    static const QString path = QStandardPaths::findExecutable(u"secret-tool"_s);
    return path;
}
}

CredentialStore::CredentialStore(QObject *parent)
    : QObject(parent)
{
}

bool CredentialStore::walletAvailable()
{
    return !secretTool().isEmpty();
}

QString CredentialStore::environmentVariable(const QString &provider)
{
    if (provider == u"gemini"_s) {
        // Google's own tooling reads either of these, so Atoll does too.
        const QString gemini = qEnvironmentVariable("GEMINI_API_KEY");
        return gemini.isEmpty() ? qEnvironmentVariable("GOOGLE_API_KEY") : gemini;
    }
    if (provider == u"openrouter"_s) {
        return qEnvironmentVariable("OPENROUTER_API_KEY");
    }
    return qEnvironmentVariable("ANTHROPIC_API_KEY");
}

QString CredentialStore::filePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(u"atoll/credentials.json"_s);
}

QString CredentialStore::fromWallet(const QString &provider)
{
    if (secretTool().isEmpty()) {
        return {};
    }
    QProcess process;
    process.start(secretTool(), {u"lookup"_s, u"service"_s, u"atoll"_s, u"account"_s, provider});
    if (!process.waitForFinished(SecretTimeoutMs) || process.exitCode() != 0) {
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

bool CredentialStore::toWallet(const QString &provider, const QString &value)
{
    if (secretTool().isEmpty()) {
        return false;
    }
    if (value.isEmpty()) {
        QProcess clear;
        clear.start(secretTool(), {u"clear"_s, u"service"_s, u"atoll"_s, u"account"_s, provider});
        clear.waitForFinished(SecretTimeoutMs);
        return true;
    }

    QProcess process;
    process.start(secretTool(),
                  {u"store"_s,
                   u"--label"_s,
                   u"Atoll assistant (%1)"_s.arg(provider),
                   u"service"_s,
                   u"atoll"_s,
                   u"account"_s,
                   provider});
    if (!process.waitForStarted(SecretTimeoutMs)) {
        return false;
    }
    process.write(value.toUtf8());
    process.closeWriteChannel();
    return process.waitForFinished(SecretTimeoutMs) && process.exitCode() == 0;
}

QString CredentialStore::fromFile(const QString &provider)
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    return doc.object().value(provider).toString();
}

void CredentialStore::toFile(const QString &provider, const QString &value)
{
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject object;
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        object = QJsonDocument::fromJson(existing.readAll()).object();
        existing.close();
    }
    if (value.isEmpty()) {
        object.remove(provider);
    } else {
        object.insert(provider, value);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("atoll: cannot write %s", qUtf8Printable(path));
        return;
    }
    // Set before the contents land, so the key is never briefly world readable.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning("atoll: cannot commit %s", qUtf8Printable(path));
        return;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString CredentialStore::key(const QString &provider) const
{
    const QString fromEnvironment = environmentVariable(provider);
    if (!fromEnvironment.isEmpty()) {
        return fromEnvironment;
    }
    const QString wallet = fromWallet(provider);
    if (!wallet.isEmpty()) {
        return wallet;
    }
    return fromFile(provider);
}

bool CredentialStore::hasKey(const QString &provider) const
{
    return !key(provider).isEmpty();
}

void CredentialStore::setKey(const QString &provider, const QString &value)
{
    if (!toWallet(provider, value)) {
        toFile(provider, value);
        return;
    }
    // A key that moved into the wallet has no business staying on disk too.
    if (!fromFile(provider).isEmpty()) {
        toFile(provider, {});
    }
}

QString CredentialStore::backendFor(const QString &provider) const
{
    if (!environmentVariable(provider).isEmpty()) {
        return u"environment"_s;
    }
    if (!fromWallet(provider).isEmpty()) {
        return u"wallet"_s;
    }
    if (!fromFile(provider).isEmpty()) {
        return u"file"_s;
    }
    return u"none"_s;
}
