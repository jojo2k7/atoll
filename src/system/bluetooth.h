/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

/**
 * BlueZ, as far as managing connections cares: whether an adapter is there,
 * whether it is powered, and the paired or connected devices around it.
 *
 * Devices the machine has never been introduced to - left-overs of somebody
 * else's scan sitting in BlueZ's cache - are kept out of the list. Everything
 * the island shows can be reconnected to with one click.
 */
class BluetoothService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.bluetooth")

    /** An adapter exists at all; everything else hides until it does. */
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(bool powered READ powered NOTIFY changed)
    /**
     * Paired, trusted or connected devices, connected ones first and then
     * alphabetically. Each entry carries path, name, address, icon, paired,
     * trusted, connected and busy flags.
     */
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY changed)

public:
    explicit BluetoothService(QObject *parent = nullptr);

    bool present() const
    {
        return m_present;
    }
    bool powered() const
    {
        return m_powered;
    }
    QVariantList devices() const
    {
        return m_devices;
    }
    int connectedCount() const;

    /** Re-read every adapter and device from BlueZ. */
    Q_INVOKABLE void refresh();
    /** Power the first adapter on or off. */
    Q_INVOKABLE void setPowered(bool on);
    Q_INVOKABLE void connectDevice(const QString &path);
    Q_INVOKABLE void disconnectDevice(const QString &path);

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    /**
     * BlueZ's GetManagedObjects: object paths to the interfaces they carry,
     * each interface named against its own property map.
     */
    using InterfaceMap = QMap<QString, QVariantMap>;
    using ObjectMap = QMap<QDBusObjectPath, InterfaceMap>;

    void applyObjects(const ObjectMap &objects);
    void scheduleRefresh();
    void callDevice(const QString &path, const QString &method, bool connecting);
    void setDeviceBusy(const QString &path, bool busy, bool connecting);

    QTimer m_refreshTimer;
    QTimer m_debounce;
    QString m_adapterPath;
    bool m_present = false;
    bool m_powered = false;
    QVariantList m_devices;
};
