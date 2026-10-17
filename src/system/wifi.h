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

/**
 * NetworkManager, as far as managing connections cares: whether a wireless
 * device exists, whether the radio is on, which network the machine is talking
 * to, and what else is within reach.
 *
 * Discovery walks the manager's devices one object at a time rather than
 * through a single tree dump, so it works against every NetworkManager
 * build - including ones that do not offer the bulk object-manager call.
 *
 * Connecting to a network the machine already has a profile for needs nothing
 * from the user. For anything else the request goes out with just the SSID,
 * which is the point at which the desktop's own secret agent - plasma-nm on
 * KDE - asks for the password; without one, the attempt simply fails and the
 * list tells the truth again.
 */
class WifiService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.wifi")

    /** A wireless device exists at all; everything else hides until it does. */
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    /** Whether the device is associated with a network right now. */
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    /** The SSID in use, empty when not connected. */
    Q_PROPERTY(QString activeSsid READ activeSsid NOTIFY changed)
    /**
     * Visible access points, connected first and then by signal strength.
     * Each entry carries path, ssid, strength, secure, connected, busy and
     * busyConnect fields.
     */
    Q_PROPERTY(QVariantList networks READ networks NOTIFY changed)

public:
    explicit WifiService(QObject *parent = nullptr);

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

    /** Re-read every device and access point from NetworkManager, and scan. */
    Q_INVOKABLE void refresh();
    /** Turn the radio on or off. */
    Q_INVOKABLE void setPowered(bool on);
    /** Join the network behind this access point. */
    Q_INVOKABLE void connectNetwork(const QString &apPath);
    /** Leave the network currently in use. */
    Q_INVOKABLE void disconnectNetwork();

Q_SIGNALS:
    void changed();

private Q_SLOTS:
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
        QString devicePath;
        QString activeConnPath;
        QString activeApPath;
        QVariantList aps;
    };

    void inspectDevice(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void doRefresh();
    void inspectWireless(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void inspectActiveConnection(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void fetchAccessPoints(const std::shared_ptr<Refresh> &refresh, const QString &devicePath);
    void fetchAccessPoint(const std::shared_ptr<Refresh> &refresh, const QString &path);
    void settle(const std::shared_ptr<Refresh> &refresh);
    void assemble(const std::shared_ptr<Refresh> &refresh);

    void scheduleRefresh();
    void requestScan();
    void setPending(const QString &apPath, bool connecting);
    void clearPending();
    void activateConnection(const QString &ssid, const QString &apPath, const QString &keyMgmt);
    void activateSaved(const QString &connPath, const QString &apPath);
    void activateSavedAttempt(const QString &connPath, const QString &apPath, bool legacyShape);
    void addAndActivate(const QString &ssid, const QString &apPath, const QString &keyMgmt);
    void addAndActivateAttempt(const QString &ssid, const QString &apPath, const QString &keyMgmt,
                               bool legacyShape);

    QTimer m_refreshTimer;
    QTimer m_debounce;
    QElapsedTimer m_pendingAge;
    quint64 m_generation = 0;
    QString m_devicePath;
    QString m_activeConnPath;
    QString m_pendingAp;
    bool m_pendingConnect = false;
    bool m_present = false;
    bool m_powered = false;
    bool m_connected = false;
    QString m_activeSsid;
    QVariantList m_networks;
};
