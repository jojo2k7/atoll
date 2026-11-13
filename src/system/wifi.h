/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class Config;

/**
 * NetworkManager, as far as managing connections cares: whether a wireless
 * device exists, whether the radio is on, which network the machine is talking
 * to, and what else is within reach.
 *
 * Nothing is scanned until somebody asks: the periodic poll only re-reads the
 * properties NetworkManager already holds, and a scan goes out when the
 * dashboard's panel opens. A scan takes the card off-channel for a moment,
 * which is a cost worth paying while somebody is looking at the list and a
 * waste when nobody is - the old service paid it every twenty seconds for the
 * lifetime of the process, settings window included.
 *
 * The service is inert unless modules.wifi is on: no timers, no bus
 * subscriptions, and no state worth showing.
 *
 * Connecting to a network the machine already has a profile for needs nothing
 * from the user. For anything else the request goes out with just the SSID and
 * the network's own key management, which is the point at which the desktop's
 * own secret agent - plasma-nm on KDE - asks for the password; without one the
 * attempt simply fails and the list tells the truth again.
 *
 * Networks Atoll cannot join honestly - enterprise 802.1X and WEP - are listed
 * as unsupported rather than attempted with the wrong key management, and a
 * click on one hands off to Plasma's own network settings.
 */
class WifiService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.wifi")

    /** modules.wifi: when off, the service holds nothing and watches nothing. */
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    /** A wireless device exists at all; everything else hides until it does. */
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    /** Whether the device is associated with a network right now. */
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    /** The SSID in use, empty when not connected. */
    Q_PROPERTY(QString activeSsid READ activeSsid NOTIFY changed)
    /**
     * Visible networks, the one in use first and then by signal strength. One
     * entry per SSID, whatever the number of access points behind it. Each
     * entry carries path, ssid, strength, secure, keyMgmt, kind, supported,
     * connected, busy and busyConnect fields.
     */
    Q_PROPERTY(QVariantList networks READ networks NOTIFY changed)
    /** The last failed join, for the panel to show; empty once one works. */
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
    /**
     * NetworkManager is waiting on the desktop's password dialog. The row keeps
     * saying so instead of timing out while somebody is still typing.
     */
    Q_PROPERTY(bool waitingForSecret READ waitingForSecret NOTIFY changed)

public:
    explicit WifiService(Config *config, QObject *parent = nullptr);

    bool enabled() const
    {
        return m_enabled;
    }
    bool present() const
    {
        return m_present;
    }
    bool powered() const
    {
        return m_powered;
    }
    bool connected() const
    {
        return m_connected;
    }
    QString activeSsid() const
    {
        return m_activeSsid;
    }
    QVariantList networks() const
    {
        return m_networks;
    }
    QString lastError() const
    {
        return m_lastError;
    }
    bool waitingForSecret() const
    {
        return m_waitingForSecret;
    }

    /**
     * Re-read every device and access point from NetworkManager. This never
     * scans, so it is safe to run on a timer; see scan() for the other half.
     */
    Q_INVOKABLE void refresh();
    /** Ask for a fresh scan, then re-read. Called when the panel opens. */
    Q_INVOKABLE void scan();
    /** Turn the radio on or off. */
    Q_INVOKABLE void setPowered(bool on);
    /** Join the network behind this access point, or leave it if in use. */
    Q_INVOKABLE void connectNetwork(const QString &apPath);
    /** Leave the network currently in use. */
    Q_INVOKABLE void disconnectNetwork();
    /** Hand an unsupported network off to Plasma's own settings window. */
    Q_INVOKABLE void openNetworkSettings();

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void configure();
    void onDeviceAdded(const QDBusObjectPath &path);
    void onDeviceRemoved(const QDBusObjectPath &path);
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    /**
     * One discovery pass. Replies carry the generation they were started in,
     * so a pass that a newer one superseded simply drops its findings.
     */
    struct Refresh {
        quint64 generation = 0;
        int pending = 0;
        bool wirelessEnabled = false;
        uint activeState = 0;
        uint deviceState = 0;
        QString devicePath;
        QString activeConnPath;
        QString activeApPath;
        QVariantList aps;
    };

    void subscribe();
    void unsubscribe();

    void doRefresh();
    void inspectDevice(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void inspectWireless(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void inspectActiveConnection(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void fetchAccessPoints(const std::shared_ptr<Refresh> &refresh, const QString &devicePath);
    void fetchAccessPoint(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void settle(const std::shared_ptr<Refresh> &refresh);
    void assemble(const std::shared_ptr<Refresh> &refresh);

    void beginPolling();
    void scheduleRefresh();
    void requestScan();
    void clearState();

    void setPending(const QStringList &paths, const QString &label, bool connecting);
    /** Call off the attempt in flight and clear its state. */
    void abortPending();
    void clearPending();
    void fail(const QString &why);
    void activateRow(const QVariantList &members, const QString &keyMgmt);
    void activateSaved(const QString &connPath, const QString &apPath);
    void addAndActivate(const QString &ssid, const QString &apPath, const QString &keyMgmt);

    Config *m_config = nullptr;

    QTimer m_refreshTimer;
    QTimer m_debounce;
    QTimer m_pendingTimer;
    QElapsedTimer m_lastScan;

    quint64 m_generation = 0;
    int m_maxRows = 12;
    QString m_devicePath;
    QString m_activeConnPath;
    QString m_activeApPath;
    QString m_pendingAp;
    QStringList m_pendingPaths;
    QString m_pendingLabel;
    QString m_lastError;
    bool m_enabled = false;
    bool m_subscribed = false;
    bool m_mergeBands = true;
    bool m_pendingConnect = false;
    bool m_waitingForSecret = false;
    // The active-connection object this attempt created, once NetworkManager
    // has answered with one. Deactivating it is how a superseded click is
    // called off instead of being waited out.
    QString m_pendingConnPath;
    // Bumped whenever an attempt starts or is called off, so replies from an
    // abandoned attempt cannot start a second join behind the user's back.
    quint64 m_attemptId = 0;
    // Whether the device picked the attempt up at all.
    bool m_attemptProgressed = false;
    // The patience timer is paused while the password dialog is open, and
    // resumes where it left off once it closes.
    bool m_pendingPaused = false;
    bool m_present = false;
    bool m_powered = false;
    bool m_connected = false;
    QString m_activeSsid;
    QVariantList m_networks;
};
