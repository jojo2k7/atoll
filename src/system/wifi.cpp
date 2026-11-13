/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "wifi.h"

#include "config/config.h"
#include "system/wifimodel.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QMap>
#include <QProcess>
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
// The access-point flags and key-management bits live in wifimodel.h, next to
// the classification that reads them.
// NM_ACTIVE_CONNECTION_STATE values; anything at or past DEACTIVATING is over.
constexpr uint ActiveStateDeactivating = 3;

// A scan takes the card off-channel for a moment. Opening the panel twice in
// a row should not do that twice.
constexpr qint64 MinScanIntervalMs = 10'000;
// A join that neither lands nor errors is given up on after this; the timer
// runs on its own, so cancelling a password dialog does not leave a spinner
// behind until the next poll comes round.
constexpr int JoinPatienceMs = 30'000;

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

/**
 * NetworkManager's GetSettings answers a{sa{sv}} - a map of section name to
 * section dict - and its connection profiles go out the same way. A plain
 * QVariantMap can only carry a{sv}, so Qt's typed reply rejects the signature
 * outright and a profile written as one is refused with InvalidArgs; this
 * map-of-maps is what makes both directions work, which is what makes the
 * saved-profile match possible at all.
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

WifiService::WifiService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    qDBusRegisterMetaType<SettingsMap>();

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &WifiService::doRefresh);

    // Signals carry the news. This poll only re-reads what NetworkManager
    // already knows, so it costs a few property Get calls and never takes the
    // radio off-channel.
    m_refreshTimer.setInterval(20'000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &WifiService::doRefresh);

    m_pendingTimer.setSingleShot(true);
    m_pendingTimer.setInterval(JoinPatienceMs);
    // The join deadline runs on its own clock. Checking it from a poll would
    // leave "Connecting..." up for the gap between the deadline and the next
    // refresh - which is the bug this timer exists to avoid - so it drops the
    // pending row itself.
    connect(&m_pendingTimer, &QTimer::timeout, this, [this] {
        if (m_pendingPaths.isEmpty()) {
            return;
        }
        fail(m_pendingConnect ? tr("Could not join %1").arg(m_pendingLabel)
                              : tr("Could not disconnect"));
        clearPending();
        scheduleRefresh();
    });

    connect(m_config, &Config::changed, this, &WifiService::configure);
    configure();
}

void WifiService::configure()
{
    const bool wanted = m_config->value(u"modules.wifi"_s, true).toBool();
    const int rows = m_config->value(u"wifi.maxRows"_s, 12).toInt();
    const bool mergeBands = m_config->value(u"wifi.mergeBands"_s, true).toBool();
    const bool listChanged = rows != m_maxRows || mergeBands != m_mergeBands;
    m_maxRows = rows > 0 ? rows : 12;
    m_mergeBands = mergeBands;

    if (wanted == m_enabled) {
        // Grouping the list differently is a setting, not a restart: re-read
        // straight away so the panel regroups as the switch moves.
        if (m_enabled && listChanged) {
            scheduleRefresh();
        }
        return;
    }
    m_enabled = wanted;

    if (m_enabled) {
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi service enabled");
        }
        // One read, so the panel knows whether a radio exists and is on
        // before it is first opened. Everything else - the bus subscription
        // and the poll - waits for something to actually show the list, which
        // the settings process never does.
        doRefresh();
    } else {
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi service disabled");
        }
        m_refreshTimer.stop();
        m_debounce.stop();
        m_pendingTimer.stop();
        unsubscribe();
        clearState();
    }
    Q_EMIT changed();
}

void WifiService::subscribe()
{
    if (m_subscribed) {
        return;
    }
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
    m_subscribed = true;
}

void WifiService::unsubscribe()
{
    if (!m_subscribed) {
        return;
    }
    auto bus = QDBusConnection::systemBus();
    bus.disconnect(QString(NMService), QString(NMPath), QString(NMInterface),
                   u"DeviceAdded"_s, this, SLOT(onDeviceAdded(QDBusObjectPath)));
    bus.disconnect(QString(NMService), QString(NMPath), QString(NMInterface),
                   u"DeviceRemoved"_s, this, SLOT(onDeviceRemoved(QDBusObjectPath)));
    bus.disconnect(QString(NMService), QString(), QString(PropertiesInterface),
                   u"PropertiesChanged"_s, this,
                   SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    m_subscribed = false;
}

void WifiService::clearState()
{
    m_generation++;
    m_devicePath.clear();
    m_activeConnPath.clear();
    m_activeApPath.clear();
    m_pendingAp.clear();
    m_pendingPaths.clear();
    m_pendingLabel.clear();
    m_pendingConnect = false;
    m_pendingConnPath.clear();
    m_attemptProgressed = false;
    m_waitingForSecret = false;
    m_pendingPaused = false;
    m_lastError.clear();
    m_present = false;
    m_powered = false;
    m_connected = false;
    m_activeSsid.clear();
    m_networks.clear();
}

void WifiService::refresh()
{
    if (!m_enabled) {
        return;
    }
    beginPolling();
    doRefresh();
}

void WifiService::scan()
{
    if (!m_enabled) {
        return;
    }
    beginPolling();
    if (!m_lastScan.isValid() || m_lastScan.elapsed() > MinScanIntervalMs) {
        m_lastScan.restart();
        requestScan();
        // A scan answers asynchronously; give it a moment before reading, so
        // the list that comes back is the one the scan just found.
        m_debounce.start(900);
        return;
    }
    doRefresh();
}

void WifiService::doRefresh()
{
    if (!m_enabled) {
        return;
    }

    auto refresh = std::make_shared<Refresh>();
    refresh->generation = ++m_generation;
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: wifi refresh gen=%llu", static_cast<unsigned long long>(refresh->generation));
    }

    // The manager object itself: is the radio on at all?
    ++refresh->pending;
    fetchProps(QString(NMPath), QString(NMInterface), [this, refresh](bool ok, const QVariantMap &props) {
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
    fetchProps(path, QString(DeviceInterface), [this, refresh, path](bool ok, const QVariantMap &props) {
        if (ok && refresh->devicePath.isEmpty()
            && props.value(u"DeviceType"_s).toUInt() == DeviceTypeWifi) {
            refresh->devicePath = path;
            refresh->deviceState = props.value(u"State"_s).toUInt();
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
    fetchProps(path, QString(WirelessInterface), [this, refresh](bool ok, const QVariantMap &props) {
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
    fetchProps(path, QString(ActiveInterface), [this, refresh](bool ok, const QVariantMap &props) {
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
            const QList<QDBusObjectPath> aps = reply.value();
            for (const QDBusObjectPath &ap : aps) {
                fetchAccessPoint(refresh, ap.path());
            }
        }
        settle(refresh);
    });
}

void WifiService::fetchAccessPoint(const std::shared_ptr<Refresh> &refresh, const QString &path)
{
    ++refresh->pending;
    fetchProps(path, QString(ApInterface), [this, refresh, path](bool ok, const QVariantMap &props) {
        if (ok) {
            const QString ssid = ssidString(props.value(u"Ssid"_s));
            if (!ssid.isEmpty()) {
                const WifiApKind kind =
                    classifyAccessPoint(props.value(u"Flags"_s).toUInt(),
                                        props.value(u"WpaFlags"_s).toUInt(),
                                        props.value(u"RsnFlags"_s).toUInt());
                const bool supported = wifiKindSupported(kind);

                refresh->aps.append(QVariantMap{
                    {u"path"_s, path},
                    {u"ssid"_s, ssid},
                    // Where the network really is. The band the panel shows is
                    // derived from this, never from the name, and the BSSID is
                    // what the panel treats as one radio.
                    {u"freq"_s, props.value(u"Frequency"_s).toUInt()},
                    {u"bssid"_s, props.value(u"HwAddress"_s).toString()},
                    {u"strength"_s, props.value(u"Strength"_s).toUInt()},
                    {u"secure"_s, kind != WifiApKind::Open},
                    // Nothing Atoll can write into a profile would make an
                    // enterprise or WEP network work, so no key management is
                    // offered for them rather than a wrong one.
                    {u"keyMgmt"_s, supported ? wifiKeyMgmt(kind) : QString()},
                    {u"kind"_s, wifiKindName(kind)},
                    {u"supported"_s, supported},
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
    const bool wasWaiting = m_waitingForSecret;
    const QString wasSsid = m_activeSsid;
    const QVariantList wasNetworks = m_networks;

    const bool present = !refresh->devicePath.isEmpty();
    const bool associated = present && !refresh->activeApPath.isEmpty()
                            && refresh->activeApPath != u"/"_s;
    // Associated is not the same as connected. NetworkManager spends the whole
    // WPA handshake at ACTIVATING - two or three seconds, and as long again on
    // a wrong password before it gives up - so the row only says connected once
    // the active connection reaches ACTIVATED.
    const bool connected = associated && wifiIsAssociated(refresh->activeState);
    const bool powered = present && refresh->wirelessEnabled;

    // One row per network, one row per join: the collapsing and the ordering
    // live in wifimodel.h so they can be tested without a radio. The path is
    // only handed over as "connected" once the connection reached ACTIVATED -
    // passing the associated one is what made a pending join look finished.
    const QVariantList aps = collapseNetworks(refresh->aps,
                                              wifiConnectedPath(refresh->activeState,
                                                                refresh->activeApPath),
                                              m_pendingAp, m_maxRows, m_mergeBands);

    // The label of the row in use, which is what the panel shows - the raw
    // SSID may carry a band suffix the row itself no longer displays.
    QString activeSsid;
    for (const QVariant &entry : aps) {
        const QVariantMap ap = entry.toMap();
        if (ap.value(u"connected"_s).toBool()) {
            activeSsid = ap.value(u"label"_s).toString();
            break;
        }
    }

    // A connect or disconnect in flight is worth more to the user than a
    // moment of truth, so the attempt waits for the network itself, a
    // refusal, or the patience timer - whichever comes first.
    if (!m_pendingPaths.isEmpty()) {
        // Any sighting of the row counts: a dual-band join may associate with
        // either band, and roaming between them is not a failure.
        const bool onPending = connected && m_pendingPaths.contains(refresh->activeApPath);

        // A password dialog is not a stalled join. While the device sits in
        // NEED_AUTH, NetworkManager is waiting on the desktop's secret agent and
        // nothing will answer until somebody types - so the deadline is
        // suspended rather than expiring behind the dialog.
        if (m_pendingConnect && wifiDeviceProgressed(refresh->deviceState)) {
            m_attemptProgressed = true;
        }
        m_waitingForSecret = m_pendingConnect && wifiAwaitingSecret(refresh->deviceState);
        if (m_waitingForSecret) {
            if (m_pendingTimer.isActive()) {
                m_pendingTimer.stop();
                m_pendingPaused = true;
            }
        } else if (m_pendingPaused) {
            m_pendingPaused = false;
            m_pendingTimer.start();
        }

        bool done = m_pendingConnect ? onPending : !onPending;
        if (!done && refresh->activeState >= ActiveStateDeactivating) {
            done = true;
        }
        // Cancelling the password dialog, or a handshake that simply lost the
        // argument, ends the attempt: without this the row stayed busy - and
        // every other click stayed blocked - until the patience timer ran out.
        if (!done && m_pendingConnect && m_attemptProgressed
            && wifiDeviceGaveUp(refresh->deviceState)) {
            done = true;
        }
        if (done) {
            if (m_pendingConnect && !onPending) {
                // The handshake ended without us - a wrong key, or a refusal
                // from NetworkManager. Say so instead of quietly going back to
                // "Secured".
                fail(tr("Could not join %1").arg(m_pendingLabel));
            } else if (!m_pendingConnect && !onPending) {
                m_lastError.clear();
            }
            clearPending();
        }
    }

    m_devicePath = refresh->devicePath;
    m_activeConnPath = refresh->activeConnPath;
    m_activeApPath = refresh->activeApPath;
    m_present = present;
    m_powered = powered;
    m_connected = connected;
    m_activeSsid = activeSsid;
    m_networks = aps;

    if (wasPresent != m_present || wasPowered != m_powered || wasConnected != m_connected
        || wasSsid != m_activeSsid || wasNetworks != aps || wasWaiting != m_waitingForSecret) {
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi present=%d powered=%d connected=%d aps=%lld networks=%d (%s)",
                     m_present, m_powered, m_connected,
                     static_cast<long long>(refresh->aps.size()), m_networks.size(),
                     qUtf8Printable(m_activeSsid));
            // One line naming every row and the bands its radios reported, so
            // the grouping can be checked against `nmcli` without a screenshot.
            QStringList rows;
            for (const QVariant &entry : m_networks) {
                const QVariantMap row = entry.toMap();
                rows.append(QStringLiteral("%1[%2]%3")
                                .arg(row.value(u"label"_s).toString(),
                                     row.value(u"bands"_s).toStringList().join(u','),
                                     row.value(u"connected"_s).toBool() ? u"*"_s : QString()));
            }
            qWarning("atoll: wifi rows: %s", qUtf8Printable(rows.join(u"; "_s)));
        }
        Q_EMIT changed();
    }
}

/**
 * Start the slow re-read, but only once something is actually showing the
 * list. The island's dashboard asks on open; the settings process never asks,
 * so it never polls and never costs the desktop a thing.
 */
void WifiService::beginPolling()
{
    if (!m_enabled) {
        return;
    }
    // Subscribing is also a cost that only makes sense once somebody is
    // looking: unwatched, the island draws nothing Wi-Fi-shaped, and the
    // settings process never asks at all.
    if (!m_subscribed) {
        subscribe();
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi watching the bus");
        }
    }
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start();
        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi poll started");
        }
    }
}

void WifiService::scheduleRefresh()
{
    if (!m_enabled) {
        return;
    }
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
    if (!m_enabled || !m_present || m_powered == on) {
        return;
    }
    // Optimistic, because the property change echoes back within moments; a
    // refusal shows up as a refresh that puts things back.
    m_powered = on;
    if (!on) {
        m_lastError.clear();
    }
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
            fail(tr("Could not change the radio"));
            scheduleRefresh();
        }
    });
}

void WifiService::connectNetwork(const QString &apPath)
{
    if (!m_enabled || !m_present || !m_powered || apPath.isEmpty() || m_devicePath.isEmpty()) {
        return;
    }

    for (const QVariant &entry : std::as_const(m_networks)) {
        const QVariantMap ap = entry.toMap();
        if (ap.value(u"path"_s).toString() != apPath) {
            continue;
        }
        if (!ap.value(u"supported"_s).toBool()) {
            // Nothing Atoll could put in a profile would make these work, so
            // the click goes where they can be set up properly.
            openNetworkSettings();
            return;
        }
        if (ap.value(u"connected"_s).toBool()) {
            disconnectNetwork();
            return;
        }
        // A merged row stands for every sighting of the network, so the join
        // gets all of them and picks whichever one already has a profile.
        QVariantList members = ap.value(u"members"_s).toList();
        if (members.isEmpty()) {
            members.append(ap);
        }
        QStringList paths;
        for (const QVariant &member : std::as_const(members)) {
            paths.append(member.toMap().value(u"path"_s).toString());
        }

        // Clicking a network while another attempt is in flight means "that
        // one instead", not "nothing at all": the attempt is called off and
        // this one starts immediately. Clicking the same row again is a
        // no-op rather than a restart.
        if (!m_pendingPaths.isEmpty()) {
            if (m_pendingPaths.contains(apPath)) {
                return;
            }
            abortPending();
        }

        if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
            qWarning("atoll: wifi connect to %s (%s, %lld sightings)",
                     qUtf8Printable(ap.value(u"label"_s).toString()),
                     qUtf8Printable(ap.value(u"kind"_s).toString()),
                     static_cast<long long>(paths.size()));
        }
        m_lastError.clear();
        setPending(paths, ap.value(u"label"_s).toString(), true);
        activateRow(members, ap.value(u"keyMgmt"_s).toString());
        return;
    }
}

/**
 * Join a row. It may stand for several SSIDs - the same router on 2.4 and
 * 5 GHz - and NetworkManager keeps one profile per SSID, so a profile that
 * exists for the sibling band is used instead of prompting for a password the
 * user has already given.
 */
void WifiService::activateRow(const QVariantList &members, const QString &keyMgmt)
{
    const QVariantMap first = members.first().toMap();
    const QString firstSsid = first.value(u"ssid"_s).toString();
    const QString firstPath = first.value(u"path"_s).toString();
    const quint64 attempt = m_attemptId;

    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(SettingsPath),
                                                  QString(SettingsInterface), u"ListConnections"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, members, keyMgmt, firstSsid, firstPath,
             attempt](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (attempt != m_attemptId) {
            // Somebody picked another network while this search was running.
            return;
        }
        const QDBusPendingReply<QList<QDBusObjectPath>> reply = *self;
        if (reply.isError()) {
            fail(tr("NetworkManager is not answering"));
            clearPending();
            scheduleRefresh();
            return;
        }

        const QList<QDBusObjectPath> connections = reply.value();
        if (connections.isEmpty()) {
            // Nothing saved at all - join with a fresh profile and let the
            // desktop's secret agent collect whatever key is needed.
            addAndActivate(firstSsid, firstPath, keyMgmt);
            return;
        }

        // Saved profiles keyed by the SSID they carry, so the row can be joined
        // through whichever band already has one.
        auto saved = std::make_shared<QMap<QString, QString>>();
        auto remaining = std::make_shared<int>(connections.size());

        for (const QDBusObjectPath &connection : connections) {
            const QString connPath = connection.path();
            auto settingsCall = QDBusMessage::createMethodCall(QString(NMService), connPath,
                                                               QString(ConnectionInterface), u"GetSettings"_s);
            auto *settingsWatcher =
                new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(settingsCall), this);
            connect(settingsWatcher, &QDBusPendingCallWatcher::finished, this,
                    [this, saved, remaining, connPath, members, keyMgmt, firstSsid,
                     firstPath, attempt](QDBusPendingCallWatcher *settingsSelf) {
                settingsSelf->deleteLater();
                if (attempt != m_attemptId) {
                    return;
                }
                // GetSettings answers a{sa{sv}}; SettingsMap is the only shape
                // Qt will accept for it.
                const QDBusPendingReply<SettingsMap> settingsReply = *settingsSelf;
                if (!settingsReply.isError()) {
                    const SettingsMap profile = settingsReply.value();
                    const QString type = profile.value(u"connection"_s).value(u"type"_s).toString();
                    const QString profileSsid =
                        ssidString(profile.value(u"802-11-wireless"_s).value(u"ssid"_s));
                    if (type == u"802-11-wireless"_s && !profileSsid.isEmpty()
                        && !saved->contains(profileSsid)) {
                        saved->insert(profileSsid, connPath);
                    }
                } else if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                    qWarning("atoll: wifi GetSettings %s failed: %s",
                             qUtf8Printable(connPath), qUtf8Printable(settingsReply.error().message()));
                }

                if (--(*remaining) > 0) {
                    return;
                }

                // Members arrive best sighting first; the first one that
                // already has a profile wins, so a dual-band row reconnects to
                // the band the user set up instead of asking again.
                const int index = wifiSavedMember(members, saved->keys());
                if (index >= 0) {
                    const QVariantMap member = members.at(index).toMap();
                    const QString ssid = member.value(u"ssid"_s).toString();
                    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                        qWarning("atoll: wifi saved profile for %s -> %s",
                                 qUtf8Printable(ssid), qUtf8Printable(saved->value(ssid)));
                    }
                    activateSaved(saved->value(ssid), member.value(u"path"_s).toString());
                    return;
                }

                if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                    qWarning("atoll: wifi no saved profile - fresh join, agent may ask for a key");
                }
                addAndActivate(firstSsid, firstPath, keyMgmt);
            });
        }
    });
}

void WifiService::activateSaved(const QString &connPath, const QString &apPath)
{
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: wifi activating saved profile %s", qUtf8Printable(connPath));
    }
    // ActivateConnection(ooo) - connection, device, specific object. The extra
    // options argument some older code carried belongs to
    // AddAndActivateConnection2 and is not accepted here.
    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(NMPath),
                                                  QString(NMInterface), u"ActivateConnection"_s);
    message << QDBusObjectPath(connPath) << QDBusObjectPath(m_devicePath) << QDBusObjectPath(apPath);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    const quint64 attempt = m_attemptId;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, attempt](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (attempt != m_attemptId) {
            return;
        }
        const QDBusPendingReply<QDBusObjectPath> reply = *self;
        if (!reply.isError()) {
            m_pendingConnPath = reply.value().path();
        }
        if (reply.isError()) {
            if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                qWarning("atoll: wifi activate saved failed: %s", qUtf8Printable(reply.error().message()));
            }
            fail(tr("Could not join the saved network"));
            clearPending();
        }
        scheduleRefresh();
    });
}

void WifiService::addAndActivate(const QString &ssid, const QString &apPath, const QString &keyMgmt)
{
    // A fresh profile carrying the SSID and the network's own key management:
    // NetworkManager asks its registered secret agents for the rest, so KDE's
    // own password dialog appears when the network needs a key. Without the
    // security section the profile reads as an open network and the join is
    // refused before any agent ever hears about it.
    SettingsMap profile;

    QVariantMap connection;
    connection.insert(u"id"_s, ssid);
    connection.insert(u"type"_s, u"802-11-wireless"_s);
    profile.insert(u"connection"_s, connection);

    QVariantMap wireless;
    wireless.insert(u"ssid"_s, ssid.toUtf8());
    wireless.insert(u"mode"_s, u"infrastructure"_s);
    profile.insert(u"802-11-wireless"_s, wireless);

    if (!keyMgmt.isEmpty()) {
        QVariantMap security;
        security.insert(u"key-mgmt"_s, keyMgmt);
        profile.insert(u"802-11-wireless-security"_s, security);
    }

    // AddAndActivateConnection(a{sa{sv}}oo): the profile has to marshal as the
    // map-of-maps NetworkManager expects, or it is refused with InvalidArgs
    // before anything else is looked at.
    auto message = QDBusMessage::createMethodCall(QString(NMService), QString(NMPath),
                                                  QString(NMInterface), u"AddAndActivateConnection"_s);
    message << QVariant::fromValue(profile) << QDBusObjectPath(m_devicePath) << QDBusObjectPath(apPath);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    const quint64 attempt = m_attemptId;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, ssid, attempt](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (attempt != m_attemptId) {
            return;
        }
        const QDBusPendingReply<QDBusObjectPath, QDBusObjectPath> reply = *self;
        if (!reply.isError()) {
            m_pendingConnPath = reply.argumentAt<0>().path();
        }
        if (reply.isError()) {
            if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
                qWarning("atoll: wifi join %s failed: %s", qUtf8Printable(ssid),
                         qUtf8Printable(reply.error().message()));
            }
            fail(tr("Could not join %1").arg(ssid));
            clearPending();
        } else {
            m_lastError.clear();
        }
        scheduleRefresh();
    });
}

void WifiService::disconnectNetwork()
{
    if (m_devicePath.isEmpty() || !m_connected) {
        return;
    }
    // Asking to leave while a join is in flight means abandoning that join.
    if (!m_pendingPaths.isEmpty() && !m_pendingConnect) {
        return; // already on its way out
    }
    if (!m_pendingPaths.isEmpty()) {
        abortPending();
    }
    for (const QVariant &entry : std::as_const(m_networks)) {
        const QVariantMap ap = entry.toMap();
        if (ap.value(u"connected"_s).toBool()) {
            setPending(ap.value(u"paths"_s).toStringList(),
                       ap.value(u"label"_s).toString(), false);
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
            fail(tr("Could not disconnect"));
            clearPending();
        }
        scheduleRefresh();
    });
}

void WifiService::openNetworkSettings()
{
    // Enterprise and WEP networks need more than an SSID and a password, and
    // Plasma already has the page for that.
    if (!QProcess::startDetached(u"systemsettings"_s, QStringList{u"kcm_networkmanagement"_s})) {
        QProcess::startDetached(u"kcmshell6"_s, QStringList{u"kcm_networkmanagement"_s});
    }
}

void WifiService::setPending(const QStringList &paths, const QString &label, bool connecting)
{
    m_pendingPaths = paths;
    m_pendingAp = paths.value(0);
    m_pendingLabel = label;
    m_pendingConnect = connecting;
    m_pendingConnPath.clear();
    m_attemptProgressed = false;
    ++m_attemptId;
    m_pendingTimer.start();

    for (QVariant &entry : m_networks) {
        QVariantMap ap = entry.toMap();
        const QStringList rowPaths = ap.value(u"paths"_s).toStringList();
        const bool hit = std::any_of(rowPaths.cbegin(), rowPaths.cend(),
                                     [&paths](const QString &path) { return paths.contains(path); });
        if (hit) {
            ap.insert(u"busy"_s, true);
            ap.insert(u"busyConnect"_s, connecting);
            entry = ap;
        }
    }
    Q_EMIT changed();
}

/**
 * Call off the attempt in flight.
 *
 * Deactivating the active connection it created is what actually stops
 * NetworkManager trying; without it the old request keeps the device busy and
 * the next click queues behind it. When the attempt never got that far - still
 * waiting on a password dialog - disconnecting the device is the equivalent.
 */
void WifiService::abortPending()
{
    if (m_pendingPaths.isEmpty()) {
        return;
    }
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: wifi aborting the attempt in flight");
    }

    if (!m_pendingConnPath.isEmpty()) {
        auto message = QDBusMessage::createMethodCall(QString(NMService), m_pendingConnPath,
                                                      QString(ActiveInterface), u"Deactivate"_s);
        QDBusConnection::systemBus().asyncCall(message);
    } else if (m_pendingConnect && !m_devicePath.isEmpty()) {
        auto message = QDBusMessage::createMethodCall(QString(NMService), m_devicePath,
                                                      QString(DeviceInterface), u"Disconnect"_s);
        QDBusConnection::systemBus().asyncCall(message);
    }

    clearPending();
    scheduleRefresh();
}

void WifiService::clearPending()
{
    if (m_pendingPaths.isEmpty() && m_pendingAp.isEmpty()) {
        return;
    }
    m_pendingTimer.stop();
    ++m_attemptId;
    m_pendingAp.clear();
    m_pendingPaths.clear();
    m_pendingLabel.clear();
    m_pendingConnect = false;
    m_pendingConnPath.clear();
    m_attemptProgressed = false;
    m_waitingForSecret = false;
    m_pendingPaused = false;

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

void WifiService::fail(const QString &why)
{
    m_lastError = why;
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
