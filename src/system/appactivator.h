/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

/**
 * Turns "a notification from this app" into "that app is in front".
 *
 * The freedesktop way - the daemon re-broadcasting ActionInvoked to the
 * sender - only reaches senders that do not filter on its bus name, and most
 * of them do. This is the other half: resolve which application sent a
 * notification, then either ask the running instance to raise itself through
 * org.freedesktop.Application.Activate, or start it, handing it an
 * xdg-activation token so the compositor lets the new window take focus. A
 * fresh start of an already running chat client is deliberate: its
 * single-instance handler is what brings the window forward.
 *
 * Resolution prefers the notification's own `desktop-entry` hint; failing
 * that, the app name is matched case-insensitively against the desktop files
 * on the system (file name, Name, StartupWMClass, Icon and Exec all count).
 */
class AppActivator : public QObject
{
    Q_OBJECT

public:
    explicit AppActivator(QObject *parent = nullptr);

    /** Raise or launch whatever published this notification. */
    void activate(const QString &desktopEntry, const QString &appName);

private:
    struct DesktopApp {
        QString path;
        QString id;
        QString exec;
        QString wmClass;
        /** Everything the fuzzy matcher may hit on: id, Name, Icon, Exec head. */
        QStringList matchKeys;
        bool dbusActivatable = false;
    };

    std::optional<DesktopApp> resolve(const QString &desktopEntry, const QString &appName) const;
    void bringUp(const DesktopApp &app);
    void requestToken(const std::function<void(const QString &)> &done);
    static DesktopApp parseDesktopFile(const QString &path);
    static QString expandFieldCodes(QString exec);

    QStringList searchPaths() const;
    /** Lower-cased lookup key -> desktop file path, built once and kept. */
    const QHash<QString, QString> &nameIndex() const;

    mutable QHash<QString, QString> m_index;
    mutable bool m_indexBuilt = false;
};
