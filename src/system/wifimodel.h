/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

/**
 * The two decisions the Wi-Fi panel makes that are worth testing on their own,
 * away from NetworkManager: what kind of network an access point is, and which
 * access points deserve a row.
 *
 * Both are pure functions over the values the bus hands back, so they can be
 * exercised without a radio, a bus or a running desktop.
 */

// NM_802_11_AP_FLAGS_PRIVACY.
inline constexpr uint WifiApFlagPrivacy = 0x1;
// NM_802_11_AP_SEC_KEY_MGMT_* bits, from WpaFlags | RsnFlags.
inline constexpr uint WifiKeyMgmt8021x = 0x8;
inline constexpr uint WifiKeyMgmtPsk = 0x100;
inline constexpr uint WifiKeyMgmtSae = 0x400;
inline constexpr uint WifiKeyMgmtOwe = 0x800;
// NM_ACTIVE_CONNECTION_STATE_ACTIVATED. Everything below it is still trying.
inline constexpr uint WifiActiveStateActivated = 2;
// NM_DEVICE_STATE_NEED_AUTH: NetworkManager has asked the desktop for a
// secret and is waiting on the dialog. Nothing is wrong; nobody has typed yet.
inline constexpr uint WifiDeviceStateNeedAuth = 60;
// NM_DEVICE_STATE_DISCONNECTED / FAILED: nothing is in flight any more.
inline constexpr uint WifiDeviceStateDisconnected = 30;
inline constexpr uint WifiDeviceStateFailed = 120;

/** How a network has to be joined, as far as a profile cares. */
enum class WifiApKind {
    Open,
    WpaPsk,
    Sae,
    Owe,
    Enterprise, // 802.1X: identity, certificate, domain - not an SSID and a key
    Wep,        // privacy with no WPA/RSN key management at all
};

/**
 * Which key management the network speaks. A fresh profile has to say the same
 * or the join is refused outright; transition networks advertise PSK and SAE
 * together, and PSK is the compatible choice, so it is asked for first.
 *
 * Privacy set with no key management at all is WEP, and 802.1X without
 * PSK/SAE/OWE is enterprise. Neither can be joined from an SSID and a
 * password, which is why they are not guessed at.
 */
inline WifiApKind classifyAccessPoint(uint flags, uint wpa, uint rsn)
{
    const uint bits = wpa | rsn;
    const bool secure = (flags & WifiApFlagPrivacy) != 0 || bits != 0;
    if (!secure) {
        return WifiApKind::Open;
    }
    if (bits & WifiKeyMgmtPsk) {
        return WifiApKind::WpaPsk;
    }
    if (bits & WifiKeyMgmtSae) {
        return WifiApKind::Sae;
    }
    if (bits & WifiKeyMgmtOwe) {
        return WifiApKind::Owe;
    }
    if (bits & WifiKeyMgmt8021x) {
        return WifiApKind::Enterprise;
    }
    return WifiApKind::Wep;
}

/** The `key-mgmt` a fresh profile should carry; empty when there is none. */
inline QString wifiKeyMgmt(WifiApKind kind)
{
    switch (kind) {
    case WifiApKind::WpaPsk:
        return QStringLiteral("wpa-psk");
    case WifiApKind::Sae:
        return QStringLiteral("sae");
    case WifiApKind::Owe:
        return QStringLiteral("owe");
    case WifiApKind::Open:
    case WifiApKind::Enterprise:
    case WifiApKind::Wep:
        break;
    }
    return QString();
}

/** The name the panel shows for a network Atoll will not try to join. */
inline QString wifiKindName(WifiApKind kind)
{
    switch (kind) {
    case WifiApKind::Open:
        return QStringLiteral("open");
    case WifiApKind::WpaPsk:
        return QStringLiteral("wpa-psk");
    case WifiApKind::Sae:
        return QStringLiteral("sae");
    case WifiApKind::Owe:
        return QStringLiteral("owe");
    case WifiApKind::Enterprise:
        return QStringLiteral("enterprise");
    case WifiApKind::Wep:
        return QStringLiteral("wep");
    }
    return QString();
}

/** Whether an SSID and a password are enough to join this kind. */
inline bool wifiKindSupported(WifiApKind kind)
{
    return kind != WifiApKind::Enterprise && kind != WifiApKind::Wep;
}

/**
 * The band an access point is really on, from its centre frequency in MHz.
 *
 * This is the only honest source: NetworkManager reports `Frequency` per access
 * point, and it cannot be faked by a name. An SSID may say "5G" and be a 2.4 GHz
 * radio; only the frequency knows.
 *
 * 2.4 GHz spans channels 1-14 (2412-2484), 5 GHz runs 4900-5895, and 6 GHz -
 * Wi-Fi 6E - starts at 5925 and ends around 7125. 60 GHz is left named as such
 * because it is rare and nothing else would fit.
 */
inline QString wifiBandFromFrequency(uint mhz)
{
    if (mhz >= 5925 && mhz <= 7125) {
        return QStringLiteral("6G");
    }
    if (mhz >= 4900 && mhz <= 5895) {
        return QStringLiteral("5G");
    }
    if (mhz >= 2400 && mhz <= 2500) {
        return QStringLiteral("2.4G");
    }
    if (mhz >= 57000) {
        return QStringLiteral("60G");
    }
    return QString();
}

/** Band order for the badge: low frequency first. */
inline int wifiBandRank(const QString &band)
{
    if (band == QStringLiteral("2.4G")) {
        return 0;
    }
    if (band == QStringLiteral("5G")) {
        return 1;
    }
    if (band == QStringLiteral("6G")) {
        return 2;
    }
    return 3;
}

/**
 * Which sighting of a network a join should use, given the SSIDs that already
 * have a saved profile.
 *
 * A dual-band router keeps one profile per SSID, so reconnecting is only free
 * of a password prompt if the band the user set up is the one chosen. Members
 * arrive best-first; this returns the index of the first one with a profile,
 * or -1 when none has one and a fresh profile is the only option.
 */
inline int wifiSavedMember(const QVariantList &members, const QStringList &savedSsids)
{
    for (int i = 0; i < members.size(); ++i) {
        const QString ssid = members.at(i).toMap().value(QStringLiteral("ssid")).toString();
        if (savedSsids.contains(ssid)) {
            return i;
        }
    }
    return -1;
}

/**
 * A connection is only up once NetworkManager says ACTIVATED.
 *
 * Seeing an access point marked active is not enough: during a handshake the
 * active connection sits at ACTIVATING, and a wrong password spends two or
 * three seconds there before failing. Treating that as connected is what made
 * the panel claim a network was up and then take it back.
 */
inline bool wifiIsAssociated(uint activeState)
{
    return activeState == WifiActiveStateActivated;
}

/** Whether NetworkManager is waiting for the user to supply a secret. */
inline bool wifiAwaitingSecret(uint deviceState)
{
    return deviceState == WifiDeviceStateNeedAuth;
}

/**
 * Whether the device has actually started the attempt: prepare, config, asking
 * for a secret, IP, secondaries, activated. Anything in that range means
 * NetworkManager picked the request up.
 *
 * This is what makes it safe to read DISCONNECTED as "the attempt is over" -
 * without it, the moment between asking and NetworkManager reacting looks like
 * an immediate failure.
 */
inline bool wifiDeviceProgressed(uint deviceState)
{
    return deviceState == 40 || deviceState == 50 || deviceState == 60 || deviceState == 70
           || deviceState == 90 || deviceState == 100;
}

/** The attempt is over: disconnected again, or failed outright. */
inline bool wifiDeviceGaveUp(uint deviceState)
{
    return deviceState == WifiDeviceStateDisconnected || deviceState == WifiDeviceStateFailed;
}

/**
 * The access point the panel may mark as connected - the one in use, or none.
 *
 * An access point being associated is not enough: while the WPA handshake runs
 * the device is associated with it and the row would go green before anything
 * was actually joined. This is the single place that decides, so the badge, the
 * header and the model cannot disagree.
 */
inline QString wifiConnectedPath(uint activeState, const QString &activeApPath)
{
    return wifiIsAssociated(activeState) ? activeApPath : QString();
}

/** An SSID split into the network's name and the band its suffix advertises. */
struct WifiBandSplit {
    QString base; // the SSID with any band suffix removed, original casing
    QString band; // "2.4G", "5G", or empty when the name says nothing about it
};

/**
 * Split a band suffix off an SSID: `Anonymous_2.4G`, `Anonymous 5G`,
 * `Anonymous-5GHz` and plain `Anonymous_5g` all reduce to the network name
 * `Anonymous`. Routers hand out one name per band like this far more often than
 * not, and the panel should show one network, not two.
 *
 * Only real band words are stripped - `Wifi_4G` and `Airtel_riya_sinha` keep
 * their names - and a suffix has to leave something behind, so an SSID that is
 * nothing but `5G` survives intact.
 */
inline WifiBandSplit wifiSplitBand(const QString &ssid)
{
    static const QRegularExpression bandSuffix(QStringLiteral(
        "(?i)[\\s._-]?(6\\s?ghz|6g|5\\s?ghz|5g|2\\.4\\s?ghz|2\\.4\\s?g|2\\.4)\\s*$"));

    WifiBandSplit split;
    split.base = ssid;
    const QRegularExpressionMatch match = bandSuffix.match(ssid);
    if (!match.hasMatch()) {
        return split;
    }
    QString base = ssid.left(match.capturedStart());
    // A trailing separator left over from "_5G" belongs to the suffix.
    while (!base.isEmpty()
           && (base.endsWith(u'_') || base.endsWith(u'-') || base.endsWith(u'.')
               || base.endsWith(u' '))) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        return split;
    }
    split.base = base;
    const QString token = match.captured(1);
    split.band = token.startsWith(u'2') ? QStringLiteral("2.4G")
               : token.startsWith(u'5') ? QStringLiteral("5G")
                                        : QStringLiteral("6G");
    return split;
}

/**
 * One row per SSID out of the access points NetworkManager reported.
 *
 * Two things are collapsed here. A dual-band router advertises two access
 * points, one per band, under names that differ only by a band suffix - those
 * become one row carrying both bands. A mesh adds more sightings of the same
 * name, which collapse as before. The sighting in use wins, then the one being
 * joined, then the strongest.
 *
 * The row keeps `ssid` as the network's real name, because that is what a
 * profile matches on, and adds `label` as what to show - the name without its
 * band suffix - plus `bands` for the badge the panel puts beside it.
 *
 * Networks are only merged when they agree on how they are joined: a `Home`
 * that is WPA2-PSK and a `Home_5G` that is WPA3-SAE stay separate rows, because
 * one click could not join both.
 *
 * Each surviving entry gains `connected`, `busy` and `busyConnect`, and the
 * list stops at maxRows because a station or an office can offer a hundred
 * networks.
 */
inline QVariantList collapseNetworks(const QVariantList &aps, const QString &activePath,
                                     const QString &pendingPath, int maxRows,
                                     bool mergeBands = true)
{
    const auto rank = [&activePath, &pendingPath](const QString &path) {
        if (!activePath.isEmpty() && path == activePath) {
            return 3;
        }
        if (!pendingPath.isEmpty() && path == pendingPath) {
            return 2;
        }
        return 1;
    };

    struct Group {
        QString label;
        // One entry per radio. NetworkManager publishes an access point object
        // per BSSID, and a BSSID is the closest thing a Wi-Fi network has to an
        // identity, so two objects for the same BSSID are the same transmitter
        // and must not become two rows.
        QMap<QString, QVariantMap> sightings;
    };
    QMap<QString, Group> groups;

    for (const QVariant &entry : aps) {
        QVariantMap ap = entry.toMap();
        const QString ssid = ap.value(QStringLiteral("ssid")).toString();
        if (ssid.isEmpty()) {
            continue;
        }
        const QString kind = ap.value(QStringLiteral("kind")).toString();
        const WifiBandSplit split = wifiSplitBand(ssid);

        // The band comes from the radio, not from the name. The name is only
        // consulted to decide whether two differently spelled SSIDs are the
        // same network, and even then only if its suffix agrees with the
        // frequency it was actually heard on.
        const uint frequency = ap.value(QStringLiteral("freq")).toUInt();
        const QString realBand = wifiBandFromFrequency(frequency);
        const QString claimedBand = split.band;
        const QString band = realBand.isEmpty() ? claimedBand : realBand;
        const bool nameAgrees = claimedBand.isEmpty() || realBand.isEmpty()
                                || claimedBand == realBand;

        // The key is the name with any band suffix removed, so `DnD` and
        // `DnD_5G` land together whether or not both bands are in range - but
        // only when the suffix is telling the truth. It carries the kind as
        // well, so a PSK and an SAE network of the same name never collapse.
        //
        // With merging off the band is part of the key, so a router sharing one
        // name across two radios shows one row per radio - which is what
        // "list every band separately" has to mean. Sightings of the same
        // network on the same band (a mesh) still collapse, so the list does
        // not fill with identical rows.
        const QString base = (mergeBands && nameAgrees) ? split.base : ssid;
        const QString key = base.toLower() + QChar(u'#') + kind
                            + (mergeBands ? QString() : QChar(u'|') + band);

        Group &group = groups[key];
        if (group.label.isEmpty()) {
            // The row reads "Anonymous", not "Anonymous_5G": the suffix is what
            // the band badge is for.
            group.label = base;
        }

        const QString path = ap.value(QStringLiteral("path")).toString();
        const QString bssid = ap.value(QStringLiteral("bssid")).toString();
        const QString identity = bssid.isEmpty() ? path : bssid;
        const uint strength = ap.value(QStringLiteral("strength")).toUInt();

        ap.insert(QStringLiteral("band"), band);

        auto known = group.sightings.find(identity);
        if (known == group.sightings.end()
            || strength > known->value(QStringLiteral("strength")).toUInt()) {
            group.sightings.insert(identity, ap);
        }
    }

    QVariantList out;
    out.reserve(groups.size());
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        const Group &group = it.value();

        QVariantList members;
        QStringList bands;
        QStringList paths;
        uint strongest = 0;
        for (auto sighting = group.sightings.cbegin(); sighting != group.sightings.cend();
             ++sighting) {
            QVariantMap member = sighting.value();
            const QString path = member.value(QStringLiteral("path")).toString();
            member.insert(QStringLiteral("rank"), rank(path));
            const QString band = member.value(QStringLiteral("band")).toString();
            if (!band.isEmpty() && !bands.contains(band)) {
                bands.append(band);
            }
            paths.append(path);
            strongest = std::max(strongest, member.value(QStringLiteral("strength")).toUInt());
            members.append(member);
        }
        if (members.isEmpty()) {
            continue;
        }

        std::sort(members.begin(), members.end(), [](const QVariant &left, const QVariant &right) {
            const QVariantMap lm = left.toMap();
            const QVariantMap rm = right.toMap();
            const int leftRank = lm.value(QStringLiteral("rank")).toInt();
            const int rightRank = rm.value(QStringLiteral("rank")).toInt();
            if (leftRank != rightRank) {
                return leftRank > rightRank;
            }
            return lm.value(QStringLiteral("strength")).toUInt()
                   > rm.value(QStringLiteral("strength")).toUInt();
        });

        // "2.4G/5G/6G" reads the way the band list should; the order maps come
        // back in is whatever NetworkManager happened to report.
        std::sort(bands.begin(), bands.end(), [](const QString &left, const QString &right) {
            return wifiBandRank(left) < wifiBandRank(right);
        });

        QVariantMap row = members.first().toMap();
        const bool connected = !activePath.isEmpty() && paths.contains(activePath);
        const bool busy = !pendingPath.isEmpty() && paths.contains(pendingPath);

        row.insert(QStringLiteral("members"), members);
        row.insert(QStringLiteral("label"), group.label);
        row.insert(QStringLiteral("bands"), bands);
        row.insert(QStringLiteral("paths"), paths);
        row.insert(QStringLiteral("strength"), strongest);
        row.insert(QStringLiteral("connected"), connected);
        row.insert(QStringLiteral("busy"), busy);
        row.insert(QStringLiteral("busyConnect"), busy);
        out.append(row);
    }

    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap am = a.toMap();
        const QVariantMap bm = b.toMap();
        const bool ac = am.value(QStringLiteral("connected")).toBool();
        const bool bc = bm.value(QStringLiteral("connected")).toBool();
        if (ac != bc) {
            return ac;
        }
        const uint as = am.value(QStringLiteral("strength")).toUInt();
        const uint bs = bm.value(QStringLiteral("strength")).toUInt();
        if (as != bs) {
            return as > bs;
        }
        return am.value(QStringLiteral("label")).toString().localeAwareCompare(
                   bm.value(QStringLiteral("label")).toString())
               < 0;
    });

    if (maxRows > 0 && out.size() > maxRows) {
        out = out.mid(0, maxRows);
    }
    return out;
}
