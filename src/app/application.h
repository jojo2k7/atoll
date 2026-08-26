/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Every backend below is exposed as a Q_PROPERTY, and the meta type system
// needs the complete types, not forward declarations.
#include "ai/aiservice.h"
#include "app/shellwindow.h"
#include "calendar/calendarservice.h"
#include "dbus/dbusmonitor.h"
#include "config/config.h"
#include "dbus/mprismanager.h"
#include "dbus/notificationmodel.h"
#include "dbus/osdmonitor.h"
#include "ipc/ipcservice.h"
#include "media/lyricsservice.h"
#include "share/shareservice.h"
#include "system/appactivator.h"
#include "system/battery.h"
#include "system/bluetooth.h"
#include "system/clock.h"
#include "system/lockmonitor.h"
#include "system/privacymonitor.h"
#include "system/visualizer.h"

class DBusMonitor;
class NotificationMonitor;
class QQmlEngine;
class QJSEngine;

/**
 * The one object QML talks to. It owns every backend, wires the session-bus
 * tap to the pieces that care about it, and exposes the handful of side
 * effects the UI is allowed to cause.
 */
class Application : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(Config *config READ config CONSTANT)
    Q_PROPERTY(ShellWindow *shell READ shell CONSTANT)
    Q_PROPERTY(OsdMonitor *osd READ osd CONSTANT)
    Q_PROPERTY(NotificationModel *notifications READ notifications CONSTANT)
    Q_PROPERTY(MprisManager *media READ media CONSTANT)
    Q_PROPERTY(Battery *battery READ battery CONSTANT)
    Q_PROPERTY(BluetoothService *bluetooth READ bluetooth CONSTANT)
    Q_PROPERTY(Clock *clock READ clock CONSTANT)
    Q_PROPERTY(Visualizer *visualizer READ visualizer CONSTANT)
    Q_PROPERTY(LyricsService *lyrics READ lyrics CONSTANT)
    Q_PROPERTY(LockMonitor *lock READ lock CONSTANT)
    Q_PROPERTY(ShareService *share READ share CONSTANT)
    Q_PROPERTY(AiService *ai READ ai CONSTANT)
    Q_PROPERTY(IpcService *ipc READ ipc CONSTANT)
    Q_PROPERTY(CalendarService *calendar READ calendar CONSTANT)
    Q_PROPERTY(PrivacyMonitor *privacy READ privacy CONSTANT)

    Q_PROPERTY(QString version READ version CONSTANT)
    /** ATOLL_DEBUG_SURFACE=1 paints the whole layer surface, to see its bounds. */
    Q_PROPERTY(bool debugSurface READ debugSurface CONSTANT)
    /** ATOLL_DEBUG_STATE=1 logs every state change the island goes through. */
    Q_PROPERTY(bool debugState READ debugState CONSTANT)
    /** False when the bus tap could not be established (see busError). */
    Q_PROPERTY(bool busTapActive READ busTapActive NOTIFY busTapChanged)
    Q_PROPERTY(QString busError READ busError NOTIFY busTapChanged)
    /** Which settings page to open on, when the island asked for a specific one. */
    Q_PROPERTY(QString settingsPage READ settingsPage CONSTANT)

public:
    static Application *instance();

    /**
     * QML singleton factory. Note that Application deliberately has no default
     * constructor: with one, the QML engine happily builds a *second*
     * Application of its own instead of calling this, and the UI then talks to
     * a backend that was never started - no bus tap, no D-Bus control, and no
     * obvious symptom beyond features quietly doing nothing.
     */
    static Application *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    explicit Application(QObject *parent);
    ~Application() override;

    void start();

    Config *config() const
    {
        return m_config;
    }
    ShellWindow *shell() const
    {
        return m_shell;
    }
    OsdMonitor *osd() const
    {
        return m_osd;
    }
    NotificationModel *notifications() const
    {
        return m_notifications;
    }
    MprisManager *media() const
    {
        return m_media;
    }
    Battery *battery() const
    {
        return m_battery;
    }
    BluetoothService *bluetooth() const
    {
        return m_bluetooth;
    }
    Clock *clock() const
    {
        return m_clock;
    }
    Visualizer *visualizer() const
    {
        return m_visualizer;
    }
    LyricsService *lyrics() const
    {
        return m_lyrics;
    }
    LockMonitor *lock() const
    {
        return m_lock;
    }
    IpcService *ipc() const
    {
        return m_ipc;
    }
    ShareService *share() const
    {
        return m_share;
    }
    AiService *ai() const
    {
        return m_ai;
    }
    CalendarService *calendar() const
    {
        return m_calendar;
    }
    PrivacyMonitor *privacy() const
    {
        return m_privacy;
    }

    QString version() const;
    QString settingsPage() const;
    bool debugSurface() const;
    bool debugState() const;
    bool busTapActive() const
    {
        return m_busTapActive;
    }
    QString busError() const
    {
        return m_busError;
    }

    /** Nudge the default sink and echo the result through the island's OSD. */
    Q_INVOKABLE void adjustVolume(int deltaPercent);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void openUrl(const QString &url);
    /** Put text on the clipboard, for the settings window's copy buttons. */
    Q_INVOKABLE void copyText(const QString &text);
    /**
     * Open the settings window, or raise the one that is already up. `page` is
     * a page name to land on, e.g. "ai" when the island offers to set the
     * assistant up.
     */
    Q_INVOKABLE void openSettings(const QString &page = {});
    /** Whether an island process holds the bus name (asked by the settings window). */
    Q_INVOKABLE bool islandRunning() const;
    /** Start an island, for when the settings window is the only thing running. */
    Q_INVOKABLE void startIsland();

Q_SIGNALS:
    void busTapChanged();

private:
    void onBusMessage(const DBusMessageInfo &message);
    void reportVolume();

    Config *m_config = nullptr;
    ShellWindow *m_shell = nullptr;
    DBusMonitor *m_monitor = nullptr;
    OsdMonitor *m_osd = nullptr;
    NotificationMonitor *m_notificationMonitor = nullptr;
    NotificationModel *m_notifications = nullptr;
    MprisManager *m_media = nullptr;
    Battery *m_battery = nullptr;
    BluetoothService *m_bluetooth = nullptr;
    Clock *m_clock = nullptr;
    Visualizer *m_visualizer = nullptr;
    LyricsService *m_lyrics = nullptr;
    LockMonitor *m_lock = nullptr;
    IpcService *m_ipc = nullptr;
    ShareService *m_share = nullptr;
    AiService *m_ai = nullptr;
    CalendarService *m_calendar = nullptr;
    AppActivator *m_activator = nullptr;
    PrivacyMonitor *m_privacy = nullptr;

    bool m_busTapActive = false;
    QString m_busError;
};
