/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wifi.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QSet>
#include <algorithm>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView NMService{"org.freedesktop.NetworkManager"};
constexpr QLatin1StringView NMPath{"/org/freedesktop/NetworkManager"};
// The Settings interface lives on its own object, not on the manager root -
// asking the root for it fails with UnknownMethod and the join silently dies.
constexpr QLatin1StringView SettingsPath{"/org/freedesktop/NetworkManager/Settings"};
constexpr QLatin1StringView NMInterface{"org.freedesktop.NetworkManager"};
constexpr QLatin1StringView PropertiesInterface{"org.freedesktop.DBus.Properties"};
constexpr QLatin1StringView DeviceInterface{"org.freedesktop.NetworkManager.Device"};
constexpr QLatin1StringView WirelessInterface{"org.freedesktop.NetworkManager.Device.Wireless"};
constexpr QLatin1StringView ApInterface{"org.freedesktop.NetworkManager.AccessPoint"};
constexpr QLatin1StringView ActiveInterface{"org.freedesktop.NetworkManager.Connection.Active"};
constexpr QLatin1StringView SettingsInterface{"org.freedesktop.NetworkManager.Settings"};
constexpr QLatin1StringView ConnectionInterface{"org.freedesktop.NetworkManager.Settings.Connection"};

// NM_DEVICE_TYPE_WIFI; the P2P placeholder device is type 30 and never matches.
constexpr uint DeviceTypeWifi = 2;
// NM_802_11_AP_FLAGS_PRIVACY: the network wants a key.
constexpr uint ApFlagPrivacy = 0x1;
// NM_802_11_AP_SEC_KEY_MGMT_* bits in the Wpa/Rsn flags: which key
// management the network speaks, so a fresh profile can say the same.
constexpr uint ApSecKeyMgmtPsk = 0x100;
constexpr uint ApSecKeyMgmtSae = 0x400;
constexpr uint ApSecKeyMgmtOwe = 0x800;
// NM_ACTIVE_CONNECTION_STATE values; anything at or past DEACTIVATING is over.
constexpr uint ActiveStateDeactivating = 3;

QString variantPath(const QVariant &value)
{
    if (value.canConvert<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value.toString();
}

QString ssidString(const QVariant &value)
{
    return QString::fromUtf8(value.toByteArray());
}

/**
 * Ask one object for every property of one interface. The one call covers
 * what several per-property reads would, and the reply is a plain map.
 */
void fetchProps(const QString &path, const QString &interface,
                std::function<void(bool ok, const QVariantMap &props)> handler)
{
    auto message = QDBusMessage::createMethodCall(QString(NMService), path,
                                                  QString(PropertiesInterface), u"GetAll"_s);
    message << interface;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [handler, watcher, path, interface](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *self;
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0 && reply.isError()) {
            qWarning("atoll: wifi props %s on %s failed: %s",
                     qUtf8Printable(interface), qUtf8Printable(path),
                     qUtf8Printable(reply.error().message()));
        }
        handler(!reply.isError(), reply.value());
    });
}

/** Everything a connect attempt needs to pick a profile once the settings are in. */
struct ActivateSearch {
    int remaining = 0;
    QString ssid;
    QString apPath;
    QString keyMgmt;
    QString bestPath;
};

/**
 * NetworkManager's GetSettings answers a{sa{sv}} - a map of section name to
 * section dict. A plain QVariantMap can only carry a{sv}, so Qt's typed reply
 * rejects the signature outright; this map-of-maps is what lets the reply
 * through, which is what makes the saved-profile match possible at all.
 */
using SettingsMap = QMap<QString, QVariantMap>;

QDBusArgument &operator<<(QDBusArgument &arg, const SettingsMap &map)
{
    arg.beginMap(qMetaTypeId<QString>(), qMetaTypeId<QVariantMap>());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        arg.beginMapEntry();
        arg << it.key() << it.value();
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, SettingsMap &map)
{
    arg.beginMap();
    map.clear();
    while (!arg.atEnd()) {
        QString key;
        QVariantMap value;
        arg.beginMapEntry();
        arg >> key >> value;
        arg.endMapEntry();
        map.insert(key, value);
    }
    arg.endMap();
    return arg;
}
} // namespace

WifiService::WifiService(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<SettingsMap>();

    auto bus = QDBusConnection::systemBus();
    bus.connect(QString(NMService), QString(NMPath), QString(NMInterface),
                u"DeviceAdded"_s, this, SLOT(onDeviceAdded(QDBusObjectPath)));
    bus.connect(QString(NMService), QString(NMPath), QString(NMInterface),
                u"DeviceRemoved"_s, this, SLOT(onDeviceRemoved(QDBusObjectPath)));
    // Devices, access points and active connections each announce their own
    // changes, so the match has to be left open on the path.
    bus.connect(QString(NMService), QString(), QString(PropertiesInterface),
                u"PropertiesChanged"_s, this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &WifiService::doRefresh);

    // Signals carry the news, but a slow poll keeps the strength columns
    // honest without anyone noticing it is there.
    m_refreshTimer.setInterval(20'000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &WifiService::refresh);
    m_refreshTimer.start();

    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: wifi service starting");
    }
    refresh();
}

void WifiService::refresh()
{
    requestScan();
    doRefresh();
}

void WifiService::doRefresh()
{
    auto refresh = std::make_shared<Refresh>();
    refresh->generation = ++m_generation;
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: wifi refresh gen=%llu", static_cast<unsigned long long>(refresh->generation));
    }

    // The manager object itself: is the radio on at all?
    ++refresh->pending;
    fetchProps(NMPath, NMInterface, [this, refresh](bool ok, const QVariantMap &props) {
        refresh->wirelessEnabled = ok && props.value(u"WirelessEnabled"_s).toBool();
        settle(refresh);
    });

    // The devices, then the first wireless one in depth.
    ++refresh->pending;
    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(NMPath),
                                                  QString(NMInterface), u"GetDevices"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message));
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, refresh](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QList<QDBusObjectPath>> reply = *self;
        if (!reply.isError()) {
            for (const QDBusObjectPath &device : reply.value()) {
                inspectDevice(refresh, device.path());
            }
        }
        settle(refresh);
    });
}

void WifiService::inspectDevice(const std::shared_ptr<Refresh> &refresh, const QString &path)
{
    ++refresh->pending;
    fetchProps(path, DeviceInterface, [this, refresh, path](bool ok, const QVariantMap &props) {
        if (ok && refresh->devicePath.isEmpty()
            && props.value(u"DeviceType"_s).toUInt() == DeviceTypeWifi) {
            refresh->devicePath = path;
            refresh->activeConnPath = variantPath(props.value(u"ActiveConnection"_s));
            inspectWireless(refresh, path);
            fetchAccessPoints(refresh, path);
            if (!refresh->activeConnPath.isEmpty() && refresh->activeConnPath != u"/"_s) {
                inspectActiveConnection(refresh, refresh->activeConnPath);
            }
        }
        settle(refresh);
    });
}

void WifiService::inspectWireless(const std::shared_ptr<Refresh> &refresh, const QString &path)
{
    ++refresh->pending;
    fetchProps(path, WirelessInterface, [this, refresh](bool ok, const QVariantMap &props) {
        if (ok) {
            refresh->activeApPath = variantPath(props.value(u"ActiveAccessPoint"_s));
            if (!refresh->activeApPath.isEmpty() && refresh->activeApPath != u"/"_s) {
                // The active network is worth its own read: a hidden SSID
                // never shows up in the scan list.
                fetchAccessPoint(refresh, refresh->activeApPath);
            }
        }
        settle(refresh);
    });
}

void WifiService::inspectActiveConnection(const std::shared_ptr<Refresh> &refresh, const QString &path)
{
    ++refresh->pending;
    fetchProps(path, ActiveInterface, [this, refresh](bool ok, const QVariantMap &props) {
        if (ok) {
            refresh->activeState = props.value(u"State"_s).toUInt();
        }
        settle(refresh);
    });
}

void WifiService::fetchAccessPoints(const std::shared_ptr<Refresh> &refresh, const QString &devicePath)
{
    ++refresh->pending;
    auto message = QDBusMessage::createMethodCall(QString(NMService), devicePath,
                                                  QString(WirelessInterface), u"GetAccessPoints"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message));
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, refresh](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QList<QDBusObjectPath>> reply = *self;
        if (!reply.isError()) {
            for (const QDBusObjectPath &ap : reply.value()) {
                fetchAccessPoint(refresh, ap.path());
            }
        }
        settle(refresh);
    });
}

void WifiService::fetchAccessPoint(const std::shared_ptr<Refresh> &refresh, const QString &path)
{
    ++refresh->pending;
    fetchProps(path, ApInterface, [this, refresh, path](bool ok, const QVariantMap &props) {
        if (ok) {
            const QString ssid = ssidString(props.value(u"Ssid"_s));
            if (!ssid.isEmpty()) {
                const uint wpa = props.value(u"WpaFlags"_s).toUInt();
                const uint rsn = props.value(u"RsnFlags"_s).toUInt();
                const bool secure = (props.value(u"Flags"_s).toUInt() & ApFlagPrivacy) != 0
                                    || wpa != 0 || rsn != 0;
                // Which key management the network speaks; a fresh profile
                // has to say the same or the join is refused outright.
                // Transition networks advertise both - PSK is the compatible
                // choice, so it is asked for first.
                QString keyMgmt;
                if ((wpa | rsn) & ApSecKeyMgmtPsk) {
                    keyMgmt = u"wpa-psk"_s;
                } else if ((wpa | rsn) & ApSecKeyMgmtSae) {
                    keyMgmt = u"sae"_s;
                } else if ((wpa | rsn) & ApSecKeyMgmtOwe) {
                    keyMgmt = u"owe"_s;
                } else if (secure) {
                    keyMgmt = u"wpa-psk"_s;
                }
                refresh->aps.append(QVariantMap{
                    {u"path"_s, path},
                    {u"ssid"_s, ssid},
                    {u"strength"_s, props.value(u"Strength"_s).toUInt()},
                    {u"secure"_s, secure},
                    {u"keyMgmt"_s, keyMgmt},
                });
            }
        }
        settle(refresh);
    });
}

void WifiService::settle(const std::shared_ptr<Refresh> &refresh)
{
    if (refresh->generation != m_generation) {
        // A newer pass is already running; its findings will speak instead.
        return;
    }
    if (--refresh->pending == 0) {
        assemble(refresh);
    }
}

void WifiService::assemble(const std::shared_ptr<Refresh> &refresh)
{
    const bool wasPresent = m_present;
    const bool wasPowered = m_powered;
    const bool wasConnected = m_connected;
    const QString wasSsid = m_activeSsid;
    const QVariantList wasNetworks = m_networks;

    const bool present = !refresh->devicePath.isEmpty();
    const bool connected = present && !refresh->activeApPath.isEmpty()
                           && refresh->activeApPath != u"/"_s;
    const bool powered = present && refresh->wirelessEnabled;

    QString activeSsid;
    QVariantList aps;
    QSet<QString> seen;
    for (const QVariant &entry : std::as_const(refresh->aps)) {
        const QVariantMap ap = entry.toMap();
        const QString path = ap.value(u"path"_s).toString();
        if (seen.contains(path)) {
            continue;
        }
        seen.insert(path);
        QVariantMap out = ap;
        out.insert(u"connected"_s, path == refresh->activeApPath);
        out.insert(u"busy"_s, false);
        out.insert(u"busyConnect"_s, false);
        if (out.value(u"connected"_s).toBool()) {
            activeSsid = out.value(u"ssid"_s).toString();
        }
        aps.append(out);
    }

    // A connect or disconnect in flight is worth more to the user than a
    // moment of truth, so the attempt waits for the network itself, a
    // refusal, or a patience of thirty seconds - whichever comes first.
    if (!m_pendingAp.isEmpty()) {
        bool done = false;
        for (const QVariant &entry : std::as_const(aps)) {
            const QVariantMap ap = entry.toMap();
            if (ap.value(u"path"_s).toString() != m_pendingAp) {
                continue;
            }
            const bool nowConnected = ap.value(u"connected"_s).toBool();
            done = m_pendingConnect ? nowConnected : !nowConnected;
            break;
        }
        if (!done && refresh->activeState >= ActiveStateDeactivating) {
            done = true;
        }
        if (!done && m_pendingAge.hasExpired(30'000)) {
            done = true;
        }
        if (done) {
            m_pendingAp.clear();
            m_pendingConnect = false;
        }
    }

    for (QVariant &entry : aps) {
        QVariantMap ap = entry.toMap();
        if (ap.value(u"path"_s).toString() == m_pendingAp) {
            ap.insert(u"busy"_s, true);
            ap.insert(u"busyConnect"_s, m_pendingConnect);
            entry = ap;
        }
    }

    std::sort(aps.begin(), aps.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap am = a.toMap();
        const QVariantMap bm = b.toMap();
        const bool ac = am.value(u"connected"_s).toBool();
        const bool bc = bm.value(u"connected"_s).toBool();
        if (ac != bc) {
            return ac;
        }
        const uint as = am.value(u"strength"_s).toUInt();
        const uint bs = bm.value(u"strength"_s).toUInt();
        if (as != bs) {
            return as > bs;
        }
        return am.value(u"ssid"_s).toString().localeAwareCompare(bm.value(u"ssid"_s).toString()) < 0;
    });

    m_devicePath = refresh->devicePath;
    m_activeConnPath = refresh->activeConnPath;
    m_present = present;
    m_powered = powered;
    m_connected = connected;
    m_activeSsid = activeSsid;
    m_networks = aps;

    if (wasPresent != m_present || wasPowered != m_powered || wasConnected != m_connected
        || wasSsid != m_activeSsid || wasNetworks != aps) {
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi present=%d powered=%d connected=%d networks=%d (%s)",
                     m_present, m_powered, m_connected, m_networks.size(),
                     qUtf8Printable(m_activeSsid));
        }
        Q_EMIT changed();
    }
}

void WifiService::scheduleRefresh()
{
    m_debounce.start();
}

void WifiService::requestScan()
{
    if (m_devicePath.isEmpty() || !m_powered) {
        return;
    }
    auto message = QDBusMessage::createMethodCall(QString(NMService), m_devicePath,
                                                  QString(WirelessInterface), u"RequestScan"_s);
    message << QVariantMap{};
    QDBusConnection::systemBus().asyncCall(message);
}

void WifiService::setPowered(bool on)
{
    if (!m_present || m_powered == on) {
        return;
    }
    // Optimistic, because the property change echoes back within moments; a
    // refusal shows up as a refresh that puts things back.
    m_powered = on;
    Q_EMIT changed();

    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(NMPath),
                                                  QString(PropertiesInterface), u"Set"_s);
    message << QString(NMInterface) << u"WirelessEnabled"_s
            << QVariant::fromValue(QDBusVariant(on));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<> reply = *self;
        if (reply.isError()) {
            scheduleRefresh();
        }
    });
}

void WifiService::connectNetwork(const QString &apPath)
{
    if (!m_present || !m_powered || apPath.isEmpty() || m_devicePath.isEmpty() || !m_pendingAp.isEmpty()) {
        return;
    }

    for (const QVariant &entry : std::as_const(m_networks)) {
        const QVariantMap ap = entry.toMap();
        if (ap.value(u"path"_s).toString() != apPath) {
            continue;
        }
        if (ap.value(u"connected"_s).toBool()) {
            disconnectNetwork();
            return;
        }
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi connect to %s (%s)", qUtf8Printable(ap.value(u"ssid"_s).toString()),
                     ap.value(u"secure"_s).toBool() ? qUtf8Printable(ap.value(u"keyMgmt"_s).toString()) : "open");
        }
        setPending(apPath, true);
        activateConnection(ap.value(u"ssid"_s).toString(), apPath, ap.value(u"keyMgmt"_s).toString());
        return;
    }
}

void WifiService::activateConnection(const QString &ssid, const QString &apPath, const QString &keyMgmt)
{
    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(SettingsPath),
                                                  QString(SettingsInterface), u"ListConnections"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, ssid, apPath, keyMgmt](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QList<QDBusObjectPath>> reply = *self;
        if (reply.isError()) {
            clearPending();
            scheduleRefresh();
            return;
        }

        const QList<QDBusObjectPath> connections = reply.value();
        if (connections.isEmpty()) {
            // Nothing saved at all - join with a fresh profile and let the
            // desktop's secret agent collect whatever key is needed.
            addAndActivate(ssid, apPath, keyMgmt);
            return;
        }

        auto search = std::make_shared<ActivateSearch>();
        search->remaining = connections.size();
        search->ssid = ssid;
        search->apPath = apPath;
        search->keyMgmt = keyMgmt;

        for (const QDBusObjectPath &connection : connections) {
            const QString connPath = connection.path();
            auto settingsCall = QDBusMessage::createMethodCall(QString(NMService), connPath,
                                                               QString(ConnectionInterface), u"GetSettings"_s);
            auto *settingsWatcher =
                new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(settingsCall), this);
            connect(settingsWatcher, &QDBusPendingCallWatcher::finished, this,
                    [this, search, connPath](QDBusPendingCallWatcher *settingsSelf) {
                settingsSelf->deleteLater();
                // GetSettings answers a{sa{sv}}. QDBusPendingReply<QVariantMap>
                // expects a{sv} and always errors on that signature, so the
                // saved-profile match silently never fired: every join fell
                // through to a fresh profile, prompting for passwords even on
                // saved networks and duplicating profiles behind the user's
                // back. SettingsMap carries the map-of-maps instead.
                const QDBusPendingReply<SettingsMap> settingsReply = *settingsSelf;
                if (!settingsReply.isError()) {
                    const QVariantMap wireless = settingsReply.value().value(u"802-11-wireless"_s);
                    if (search->bestPath.isEmpty() && ssidString(wireless.value(u"ssid"_s)) == search->ssid) {
                        search->bestPath = connPath;
                    }
                } else if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                    qWarning("atoll: wifi GetSettings %s failed: %s",
                             qUtf8Printable(connPath), qUtf8Printable(settingsReply.error().message()));
                }
                if (--search->remaining == 0) {
                    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                        qWarning("atoll: wifi saved profile match: %s",
                                 search->bestPath.isEmpty()
                                     ? "none - fresh join, agent may ask for a key"
                                     : qUtf8Printable(search->bestPath));
                    }
                    if (search->bestPath.isEmpty()) {
                        addAndActivate(search->ssid, search->apPath, search->keyMgmt);
                    } else {
                        activateSaved(search->bestPath, search->apPath);
                    }
                }
            });
        }
    });
}

void WifiService::activateSaved(const QString &connPath, const QString &apPath)
{
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: wifi activating saved profile %s", qUtf8Printable(connPath));
    }
    activateSavedAttempt(connPath, apPath, false);
}

void WifiService::activateSavedAttempt(const QString &connPath, const QString &apPath, bool legacyShape)
{
    // Newer NetworkManager wants (connection, device, specific_object); older
    // ones also accept a trailing unused options dictionary. Try the modern
    // shape first and fall back on a type mismatch.
    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(NMPath),
                                                  QString(NMInterface), u"ActivateConnection"_s);
    message << QDBusObjectPath(connPath) << QDBusObjectPath(m_devicePath) << QDBusObjectPath(apPath);
    if (legacyShape) {
        message << QVariantMap{};
    }
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, connPath, apPath, legacyShape](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<> reply = *self;
        if (reply.isError()) {
            if (!legacyShape && reply.error().type() == QDBusError::InvalidArgs) {
                activateSavedAttempt(connPath, apPath, true);
                return;
            }
            if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                qWarning("atoll: wifi activate saved failed: %s", qUtf8Printable(reply.error().message()));
            }
            clearPending();
        }
        scheduleRefresh();
    });
}

void WifiService::addAndActivate(const QString &ssid, const QString &apPath, const QString &keyMgmt)
{
    addAndActivateAttempt(ssid, apPath, keyMgmt, false);
}

void WifiService::addAndActivateAttempt(const QString &ssid, const QString &apPath,
                                        const QString &keyMgmt, bool legacyShape)
{
    // A fresh profile carrying the SSID and the network's own key management:
    // NetworkManager asks its registered secret agents for the rest, so KDE's
    // own password dialog appears when the network needs a key. Without the
    // security section the profile reads as an open network and the join is
    // refused before any agent ever hears about it.
    QVariantMap connection;
    connection.insert(u"id"_s, ssid);
    connection.insert(u"type"_s, u"802-11-wireless"_s);
    QVariantMap wireless;
    wireless.insert(u"ssid"_s, ssid.toUtf8());
    wireless.insert(u"mode"_s, u"infrastructure"_s);
    QVariantMap security;
    security.insert(u"key-mgmt"_s, keyMgmt);

    // The profile must marshal as a{sa{sv}} - a plain QVariantMap goes out as
    // a{sv} and NetworkManager rejects it with InvalidArgs before looking at
    // anything else, which is why joins used to die silently.
    QDBusArgument profile;
    profile.beginMap(qMetaTypeId<QString>(), qMetaTypeId<QVariantMap>());
    profile.beginMapEntry();
    profile << QString(u"connection"_s) << connection;
    profile.endMapEntry();
    profile.beginMapEntry();
    profile << QString(u"802-11-wireless"_s) << wireless;
    profile.endMapEntry();
    if (!keyMgmt.isEmpty()) {
        profile.beginMapEntry();
        profile << QString(u"802-11-wireless-security"_s) << security;
        profile.endMapEntry();
    }
    profile.endMap();

    // Newer NetworkManager wants (connection, device, specific_object); older
    // ones also accept a trailing options dictionary. Try the modern shape
    // first and fall back on a type mismatch.
    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(NMPath),
                                                  QString(NMInterface), u"AddAndActivateConnection"_s);
    message << QVariant::fromValue(profile) << QDBusObjectPath(m_devicePath) << QDBusObjectPath(apPath);
    if (legacyShape) {
        message << QVariantMap{};
    }
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, ssid, apPath, keyMgmt, legacyShape](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<> reply = *self;
        if (reply.isError()) {
            if (!legacyShape && reply.error().type() == QDBusError::InvalidArgs) {
                addAndActivateAttempt(ssid, apPath, keyMgmt, true);
                return;
            }
            if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                qWarning("atoll: wifi join %s failed: %s", qUtf8Printable(ssid),
                         qUtf8Printable(reply.error().message()));
            }
            clearPending();
        }
        scheduleRefresh();
    });
}

void WifiService::disconnectNetwork()
{
    if (m_devicePath.isEmpty() || !m_connected || !m_pendingAp.isEmpty()) {
        return;
    }
    for (const QVariant &entry : std::as_const(m_networks)) {
        const QVariantMap ap = entry.toMap();
        if (ap.value(u"connected"_s).toBool()) {
            setPending(ap.value(u"path"_s).toString(), false);
            break;
        }
    }

    // Device.Disconnect rather than ActiveConnection.DeactivateConnection:
    // deactivating leaves autoconnect free to bring the same network straight
    // back, which made "disconnect" appear to work and then instantly
    // reconnect on its own. Disconnect() blocks autoconnect until the user
    // explicitly reconnects - the behavior every other wifi menu has.
    auto message = QDBusMessage::createMethodCall(QString(NMService), m_devicePath,
                                                  QString(DeviceInterface), u"Disconnect"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<> reply = *self;
        if (reply.isError()) {
            if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                qWarning("atoll: wifi disconnect failed: %s", qUtf8Printable(reply.error().message()));
            }
            clearPending();
        }
        scheduleRefresh();
    });
}

void WifiService::setPending(const QString &apPath, bool connecting)
{
    m_pendingAp = apPath;
    m_pendingConnect = connecting;
    m_pendingAge.restart();

    for (QVariant &entry : m_networks) {
        QVariantMap ap = entry.toMap();
        if (ap.value(u"path"_s).toString() == apPath) {
            ap.insert(u"busy"_s, true);
            ap.insert(u"busyConnect"_s, connecting);
            entry = ap;
        }
    }
    Q_EMIT changed();
}

void WifiService::clearPending()
{
    if (m_pendingAp.isEmpty()) {
        return;
    }
    m_pendingAp.clear();
    m_pendingConnect = false;

    for (QVariant &entry : m_networks) {
        QVariantMap ap = entry.toMap();
        if (ap.value(u"busy"_s).toBool()) {
            ap.insert(u"busy"_s, false);
            ap.insert(u"busyConnect"_s, false);
            entry = ap;
        }
    }
    Q_EMIT changed();
}

void WifiService::onDeviceAdded(const QDBusObjectPath &path)
{
    Q_UNUSED(path)
    scheduleRefresh();
}

void WifiService::onDeviceRemoved(const QDBusObjectPath &path)
{
    Q_UNUSED(path)
    scheduleRefresh();
}

void WifiService::onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                      const QStringList &invalidated)
{
    Q_UNUSED(changed)
    Q_UNUSED(invalidated)
    if (interface == NMInterface || interface == DeviceInterface || interface == WirelessInterface
        || interface == ApInterface || interface == ActiveInterface) {
        scheduleRefresh();
    }
}
