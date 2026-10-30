/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * Runtime configuration, backed by ~/.config/atoll/atoll.json.
 *
 * The user file is deep-merged on top of the built-in defaults, so a config
 * only has to name the keys it wants to override. The file is watched and
 * re-read on change, which makes tweaking the island a live operation - and
 * that is also what carries edits made in the settings window, which runs in
 * its own process, back to the running island.
 */
class Config : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.config")

    /** The fully merged configuration tree, as nested maps. */
    Q_PROPERTY(QVariantMap data READ data NOTIFY changed)
    Q_PROPERTY(QString path READ path CONSTANT)

public:
    explicit Config(QObject *parent = nullptr);

    QVariantMap data() const
    {
        return m_data;
    }
    QString path() const
    {
        return m_path;
    }

    /** Look up a dotted path, e.g. value("island.collapsedWidth", 172). */
    Q_INVOKABLE QVariant value(const QString &dottedKey, const QVariant &fallback = {}) const;

    /** The built-in value for a dotted path, ignoring the user file. */
    Q_INVOKABLE QVariant defaultValue(const QString &dottedKey) const;

    /**
     * Write a dotted path and persist it. The change is published immediately
     * and the file is written a moment later, so dragging a slider does not
     * mean one disk write per frame.
     */
    Q_INVOKABLE void setValue(const QString &dottedKey, const QVariant &value);

    /** Put one key back to its built-in value. */
    Q_INVOKABLE void resetValue(const QString &dottedKey);

    /** Put every key back to its built-in value. */
    Q_INVOKABLE void resetAll();

    /** Re-read the file from disk. */
    Q_INVOKABLE void reload();

    /** Write the built-in defaults to disk if no config file exists yet. */
    void ensureUserFile();

    static QVariantMap defaults();

    /**
     * Read the keys of configs older than the Controls page into "controls",
     * in place, where the user file does not already set the new key. Applied
     * to the user's own tree before the defaults go underneath it; the file on
     * disk is left alone. See docs/controls.md.
     */
    static void migrate(QVariantMap &user);

Q_SIGNALS:
    void changed();

private:
    void applyWatch();
    void save();
    static void deepMerge(QVariantMap &target, const QVariantMap &overlay);
    static bool insertAt(QVariantMap &target, const QStringList &parts, const QVariant &value);

    QString m_path;
    QVariantMap m_data;
    QFileSystemWatcher m_watcher;
    QTimer m_saveTimer;
};
