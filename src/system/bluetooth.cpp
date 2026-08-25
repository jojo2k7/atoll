/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "bluetooth.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView BlueZService{"org.bluez"};
constexpr QLatin1StringView ObjectManager{"org.freedesktop.DBus.ObjectManager"};
constexpr QLatin1StringView PropertiesInterface{"org.freedesktop.DBus.Properties"};
constexpr QLatin1StringView AdapterInterface{"org.bluez.Adapter1"};
constexpr QLatin1StringView DeviceInterface{"org.bluez.Device1"};
} // namespace

BluetoothService::BluetoothService(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ObjectMap>();

    auto bus = QDBusConnection::systemBus();
    bus.connect(QString(BlueZService),
                u"/"_s,
                QString(ObjectManager),
                u"InterfacesAdded"_s,
                this,
                SLOT(onInterfacesAdded(QDBusObjectPath, QVariantMap)));
    bus.connect(QString(BlueZService),
                u"/"_s,
                QString(ObjectManager),
                u"InterfacesRemoved"_s,
                this,
                SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
    // Device and adapter objects each announce their own changes, so the match
    // has to be left open on the path.
    bus.connect(QString(BlueZService),
                QString(),
                QString(PropertiesInterface),
                u"PropertiesChanged"_s,
                this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &BluetoothService::refresh);

    // Signals carry the news, but BlueZ occasionally forgets to send it; a
    // slow poll keeps the list honest without anyone noticing it is there.
    m_refreshTimer.setInterval(20'000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &BluetoothService::refresh);
    m_refreshTimer.start();

    refresh();
}

int BluetoothService::connectedCount() const
{
    int count = 0;
    for (const QVariant &entry : m_devices) {
        if (entry.toMap().value(u"connected"_s).toBool()) {
            ++count;
        }
    }
    return count;
}

void BluetoothService::refresh()
{
    auto message = QDBusMessage::createMethodCall(QString(BlueZService),
                                                  u"/"_s,
                                                  QString(ObjectManager),
                                                  u"GetManagedObjects"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<ObjectMap> reply = *self;
        if (reply.isError()) {
            // No BlueZ on the system bus: the island leaves every slot out.
            applyObjects({});
            return;
        }
        applyObjects(reply.value());
    });
}

void BluetoothService::applyObjects(const ObjectMap &objects)
{
    const bool wasPresent = m_present;
    const bool wasPowered = m_powered;
    const QVariantList wasDevices = m_devices;

    QVariantList devices;
    QString adapterPath;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QString path = it.key().path();

        if (it.value().contains(AdapterInterface)) {
            if (adapterPath.isEmpty()) {
                adapterPath = path;
            }
            continue;
        }

        const auto device = it.value().constFind(DeviceInterface);
        if (device == it.value().cend()) {
            continue;
        }
        const QVariantMap props = device.value();

        const bool paired = props.value(u"Paired"_s).toBool();
        const bool trusted = props.value(u"Trusted"_s).toBool();
        const bool connected = props.value(u"Connected"_s).toBool();
        // Devices discovered by somebody else's scan are nobody's business here.
        if (!paired && !trusted && !connected) {
            continue;
        }

        QString name = props.value(u"Alias"_s).toString();
        if (name.isEmpty()) {
            name = props.value(u"Name"_s).toString();
        }
        if (name.isEmpty()) {
            name = props.value(u"Address"_s).toString();
        }

        devices.append(QVariantMap{
            {u"path"_s, path},
            {u"name"_s, name},
            {u"address"_s, props.value(u"Address"_s).toString()},
            {u"icon"_s, props.value(u"Icon"_s).toString()},
            {u"paired"_s, paired},
            {u"trusted"_s, trusted},
            {u"connected"_s, connected},
            {u"busy"_s, false},
            {u"busyConnect"_s, false},
        });
    }

    std::sort(devices.begin(), devices.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap am = a.toMap();
        const QVariantMap bm = b.toMap();
        const bool ac = am.value(u"connected"_s).toBool();
        const bool bc = bm.value(u"connected"_s).toBool();
        if (ac != bc) {
            return ac;
        }
        return am.value(u"name"_s).toString().localeAwareCompare(bm.value(u"name"_s).toString()) < 0;
    });

    m_adapterPath = adapterPath;
    m_present = !adapterPath.isEmpty();

    if (!adapterPath.isEmpty()) {
        const QVariantMap adapterProps =
            objects.value(QDBusObjectPath(adapterPath)).value(AdapterInterface);
        m_powered = adapterProps.value(u"Powered"_s).toBool();
    } else {
        m_powered = false;
    }

    // A connect or disconnect in flight is worth more to the user than a
    // moment of truth, so its flag survives the re-read.
    QHash<QString, bool> pending;
    pending.reserve(m_devices.size());
    for (const QVariant &entry : std::as_const(m_devices)) {
        const QVariantMap old = entry.toMap();
        if (old.value(u"busy"_s).toBool()) {
            pending.insert(old.value(u"path"_s).toString(), old.value(u"busyConnect"_s).toBool());
        }
    }
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        for (QVariant &entry : devices) {
            QVariantMap device = entry.toMap();
            if (device.value(u"path"_s) == it.key()) {
                device.insert(u"busy"_s, true);
                device.insert(u"busyConnect"_s, it.value());
                entry = device;
                break;
            }
        }
    }

    m_devices = devices;
    if (wasPresent != m_present || wasPowered != m_powered || wasDevices != devices) {
        Q_EMIT changed();
    }
}

void BluetoothService::scheduleRefresh()
{
    m_debounce.start();
}

void BluetoothService::setPowered(bool on)
{
    if (m_adapterPath.isEmpty() || m_powered == on) {
        return;
    }
    // Optimistic, because the property change echoes back within moments; a
    // refusal shows up as a refresh that puts things back.
    m_powered = on;
    Q_EMIT changed();

    auto message = QDBusMessage::createMethodCall(QString(BlueZService),
                                                  m_adapterPath,
                                                  QString(PropertiesInterface),
                                                  u"Set"_s);
    message << QString(AdapterInterface) << u"Powered"_s << QVariant::fromValue(QDBusVariant(on));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<> reply = *self;
        if (reply.isError()) {
            scheduleRefresh();
        }
    });
}

void BluetoothService::connectDevice(const QString &path)
{
    callDevice(path, u"Connect"_s, true);
}

void BluetoothService::disconnectDevice(const QString &path)
{
    callDevice(path, u"Disconnect"_s, false);
}

void BluetoothService::callDevice(const QString &path, const QString &method, bool connecting)
{
    if (path.isEmpty()) {
        return;
    }
    setDeviceBusy(path, true, connecting);

    auto message = QDBusMessage::createMethodCall(QString(BlueZService), path,
                                                  QString(DeviceInterface), method);
    // Connecting can take a headset a good while to think about.
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(message, 25'000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        setDeviceBusy(path, false, false);
        scheduleRefresh();
    });
}

void BluetoothService::setDeviceBusy(const QString &path, bool busy, bool connecting)
{
    for (QVariant &entry : m_devices) {
        QVariantMap device = entry.toMap();
        if (device.value(u"path"_s) == path && device.value(u"busy"_s).toBool() != busy) {
            device.insert(u"busy"_s, busy);
            device.insert(u"busyConnect"_s, connecting);
            entry = device;
            Q_EMIT changed();
            return;
        }
    }
}

void BluetoothService::onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces)
{
    Q_UNUSED(path)
    if (interfaces.contains(AdapterInterface) || interfaces.contains(DeviceInterface)) {
        scheduleRefresh();
    }
}

void BluetoothService::onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces)
{
    Q_UNUSED(path)
    if (interfaces.contains(AdapterInterface) || interfaces.contains(DeviceInterface)) {
        scheduleRefresh();
    }
}

void BluetoothService::onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                           const QStringList &invalidated)
{
    Q_UNUSED(changed)
    Q_UNUSED(invalidated)
    if (interface == AdapterInterface || interface == DeviceInterface) {
        scheduleRefresh();
    }
}
