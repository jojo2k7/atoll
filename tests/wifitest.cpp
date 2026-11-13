// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The two decisions the Wi-Fi panel makes that are worth pinning down: what a
// network is, and which access points get a row. Both are pure functions over
// what NetworkManager reports, so they can be tested without a radio.
#include "system/wifimodel.h"

#include <QCoreApplication>
#include <QTest>

#include <tuple>

namespace
{
QVariantMap ap(const QString &ssid, const QString &path, uint strength, uint freq = 0,
               const QString &bssid = QString(), const QString &kind = QStringLiteral("wpa-psk"))
{
    return QVariantMap{
        {QStringLiteral("ssid"), ssid},
        {QStringLiteral("path"), path},
        {QStringLiteral("strength"), strength},
        {QStringLiteral("freq"), freq},
        {QStringLiteral("bssid"), bssid},
        {QStringLiteral("kind"), kind},
    };
}
} // namespace

class WifiModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- classification ------------------------------------------------

    void openNetwork()
    {
        const WifiApKind kind = classifyAccessPoint(0, 0, 0);
        QCOMPARE(kind, WifiApKind::Open);
        QVERIFY(wifiKindSupported(kind));
        QVERIFY(wifiKeyMgmt(kind).isEmpty());
    }

    void wpa2Psk()
    {
        // PRIVACY flag set, RSN carries PSK.
        const WifiApKind kind = classifyAccessPoint(0x1, 0, 0x100);
        QCOMPARE(kind, WifiApKind::WpaPsk);
        QVERIFY(wifiKindSupported(kind));
        QCOMPARE(wifiKeyMgmt(kind), QStringLiteral("wpa-psk"));
    }

    void wpa1AndWpa2Psk()
    {
        // Both flag sets carry PSK; still one plain wpa-psk profile.
        QCOMPARE(classifyAccessPoint(0x1, 0x100, 0x100), WifiApKind::WpaPsk);
    }

    void saeOnly()
    {
        const WifiApKind kind = classifyAccessPoint(0x1, 0, 0x400);
        QCOMPARE(kind, WifiApKind::Sae);
        QCOMPARE(wifiKeyMgmt(kind), QStringLiteral("sae"));
    }

    void transitionPrefersPsk()
    {
        // WPA2/WPA3 transition advertises PSK and SAE together; PSK is the
        // compatible choice and must win, or half the clients fail to join.
        QCOMPARE(classifyAccessPoint(0x1, 0, 0x100 | 0x400), WifiApKind::WpaPsk);
    }

    void oweNeedsNoKey()
    {
        // Enhanced Open: no PRIVACY flag, but still secured and joinable.
        const WifiApKind kind = classifyAccessPoint(0, 0, 0x800);
        QCOMPARE(kind, WifiApKind::Owe);
        QVERIFY(wifiKindSupported(kind));
        QCOMPARE(wifiKeyMgmt(kind), QStringLiteral("owe"));
    }

    void enterpriseIsNotAttempted()
    {
        // 802.1X: needs an identity, a certificate and usually a domain.
        const WifiApKind kind = classifyAccessPoint(0x1, 0, 0x8);
        QCOMPARE(kind, WifiApKind::Enterprise);
        QVERIFY(!wifiKindSupported(kind));
        // No key management may be offered: writing wpa-psk here is what made
        // these joins fail with a password prompt that could never work.
        QVERIFY(wifiKeyMgmt(kind).isEmpty());
        QCOMPARE(wifiKindName(kind), QStringLiteral("enterprise"));
    }

    void enterpriseOfferingPskIsJoinable()
    {
        // A network that also advertises PSK is a normal PSK network.
        QCOMPARE(classifyAccessPoint(0x1, 0, 0x8 | 0x100), WifiApKind::WpaPsk);
    }

    void wepIsNotAttempted()
    {
        // PRIVACY with no WPA/RSN key management at all is WEP.
        const WifiApKind kind = classifyAccessPoint(0x1, 0, 0);
        QCOMPARE(kind, WifiApKind::Wep);
        QVERIFY(!wifiKindSupported(kind));
        QVERIFY(wifiKeyMgmt(kind).isEmpty());
        QCOMPARE(wifiKindName(kind), QStringLiteral("wep"));
    }

    // ---- one row per SSID ----------------------------------------------

    void dualBandCollapsesToOneRow()
    {
        const QVariantList aps{ap("home", "/ap/1", 60), ap("home", "/ap/2", 40)};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("strength")).toUInt(), 60u);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/ap/1"));
    }

    void meshCollapsesToStrongestSighting()
    {
        const QVariantList aps{ap("office", "/ap/1", 20), ap("office", "/ap/2", 70),
                               ap("office", "/ap/3", 45)};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/ap/2"));
    }

    void connectedSightingWinsOverStronger()
    {
        // The row has to keep pointing at the access point in use, or the panel
        // would show a network as available while the machine is on it.
        const QVariantList aps{ap("home", "/ap/1", 90), ap("home", "/ap/2", 30)};
        const QVariantList rows = collapseNetworks(aps, QStringLiteral("/ap/2"), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/ap/2"));
        QVERIFY(rows.first().toMap().value(QStringLiteral("connected")).toBool());
    }

    void pendingSightingWinsOverStronger()
    {
        // While joining, the row must stay on the access point being joined.
        const QVariantList aps{ap("home", "/ap/1", 90), ap("home", "/ap/2", 30)};
        const QVariantList rows = collapseNetworks(aps, QString(), QStringLiteral("/ap/2"), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/ap/2"));
        QVERIFY(rows.first().toMap().value(QStringLiteral("busy")).toBool());
        QVERIFY(rows.first().toMap().value(QStringLiteral("busyConnect")).toBool());
    }

    void connectedRowSortsFirst()
    {
        const QVariantList aps{ap("weak", "/ap/1", 20), ap("strong", "/ap/2", 95),
                               ap("mine", "/ap/3", 40)};
        const QVariantList rows = collapseNetworks(aps, QStringLiteral("/ap/3"), QString(), 12);
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.at(0).toMap().value(QStringLiteral("ssid")).toString(),
                 QStringLiteral("mine"));
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("ssid")).toString(),
                 QStringLiteral("strong"));
        QCOMPARE(rows.at(2).toMap().value(QStringLiteral("ssid")).toString(),
                 QStringLiteral("weak"));
    }

    void denseAreaIsCapped()
    {
        QVariantList aps;
        for (int i = 0; i < 40; ++i) {
            aps.append(ap(QStringLiteral("net%1").arg(i, 2, 10, QLatin1Char('0')),
                          QStringLiteral("/ap/%1").arg(i), uint(i)));
        }
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 12);
        // The cap keeps the strongest, not an arbitrary twelve.
        QCOMPARE(rows.first().toMap().value(QStringLiteral("ssid")).toString(),
                 QStringLiteral("net39"));
    }

    void capOfZeroMeansNoCap()
    {
        QVariantList aps;
        for (int i = 0; i < 5; ++i) {
            aps.append(ap(QStringLiteral("n%1").arg(i), QStringLiteral("/ap/%1").arg(i), uint(i)));
        }
        QCOMPARE(collapseNetworks(aps, QString(), QString(), 0).size(), 5);
    }

    void hiddenSsidIsSkipped()
    {
        // NetworkManager reports a blank SSID for hidden networks it has not
        // seen named; an empty row is not something to show or click.
        const QVariantList aps{ap(QString(), "/ap/1", 80), ap("home", "/ap/2", 40)};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("ssid")).toString(),
                 QStringLiteral("home"));
    }

    void emptyListStaysEmpty()
    {
        QVERIFY(collapseNetworks({}, QString(), QString(), 12).isEmpty());
    }

    // ---- one row per network, both bands -------------------------------

    void bandSplit()
    {
        // The suffixed spellings routers actually ship.
        QCOMPARE(wifiSplitBand(QStringLiteral("Anonymous_2.4G")).base, QStringLiteral("Anonymous"));
        QCOMPARE(wifiSplitBand(QStringLiteral("Anonymous_2.4G")).band, QStringLiteral("2.4G"));
        QCOMPARE(wifiSplitBand(QStringLiteral("Anonymous_5G")).base, QStringLiteral("Anonymous"));
        QCOMPARE(wifiSplitBand(QStringLiteral("Anonymous_5G")).band, QStringLiteral("5G"));
        QCOMPARE(wifiSplitBand(QStringLiteral("MyNet 5GHz")).base, QStringLiteral("MyNet"));
        QCOMPARE(wifiSplitBand(QStringLiteral("MyNet-2.4GHz")).base, QStringLiteral("MyNet"));
        QCOMPARE(wifiSplitBand(QStringLiteral("MyNet5g")).base, QStringLiteral("MyNet"));
        QCOMPARE(wifiSplitBand(QStringLiteral("MyNet5g")).band, QStringLiteral("5G"));
        // Wi-Fi 6E advertises 6 GHz, and routers name it the same way.
        QCOMPARE(wifiSplitBand(QStringLiteral("Chander PAPA 6G")).base,
                 QStringLiteral("Chander PAPA"));
        QCOMPARE(wifiSplitBand(QStringLiteral("Chander PAPA 6G")).band, QStringLiteral("6G"));
    }

    void threeBandsSortByFrequency()
    {
        QVariantList aps;
        for (const auto &[ssid, path, strength] :
             {std::tuple{QStringLiteral("Papa_5G"), QStringLiteral("/ap/1"), 50u},
              std::tuple{QStringLiteral("Papa_6G"), QStringLiteral("/ap/2"), 60u},
              std::tuple{QStringLiteral("Papa_2.4G"), QStringLiteral("/ap/3"), 70u}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("strength"), strength);
            m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
            aps.append(m);
        }
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Papa"));
        QCOMPARE(rows.first().toMap().value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("2.4G"), QStringLiteral("5G"), QStringLiteral("6G")}));
    }

    void bandSplitLeavesOrdinaryNamesAlone()
    {
        // 4G is a phone network, not a Wi-Fi band, and this one is not a suffix
        // either.
        QVERIFY(wifiSplitBand(QStringLiteral("Wifi_4G B")).band.isEmpty());
        QCOMPARE(wifiSplitBand(QStringLiteral("Wifi_4G B")).base, QStringLiteral("Wifi_4G B"));
        QVERIFY(wifiSplitBand(QStringLiteral("Airtel__riya_sinha")).band.isEmpty());
        QVERIFY(wifiSplitBand(QStringLiteral("JAI MEENA")).band.isEmpty());
        // An SSID that is only a band name must survive whole.
        QCOMPARE(wifiSplitBand(QStringLiteral("5G")).base, QStringLiteral("5G"));
        QVERIFY(wifiSplitBand(QStringLiteral("5G")).band.isEmpty());
    }

    void bothBandsBecomeOneRow()
    {
        // The case that prompted this: the same router on 2.4 and 5 GHz.
        const QVariantList aps{ap(QStringLiteral("Anonymous_5G"), QStringLiteral("/ap/1"), 65),
                               ap(QStringLiteral("Anonymous_2.4G"), QStringLiteral("/ap/2"), 75)};
        QVariantList withKind;
        for (const QVariant &entry : aps) {
            QVariantMap m = entry.toMap();
            m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
            withKind.append(m);
        }

        const QVariantList rows = collapseNetworks(withKind, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        const QVariantMap row = rows.first().toMap();
        QCOMPARE(row.value(QStringLiteral("label")).toString(), QStringLiteral("Anonymous"));
        QCOMPARE(row.value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("2.4G"), QStringLiteral("5G")}));
        // The stronger sighting represents the network, but the SSID stays real
        // - a profile match and a join both need the true name.
        QCOMPARE(row.value(QStringLiteral("path")).toString(), QStringLiteral("/ap/2"));
        QCOMPARE(row.value(QStringLiteral("ssid")).toString(), QStringLiteral("Anonymous_2.4G"));
        QCOMPARE(row.value(QStringLiteral("strength")).toUInt(), 75u);
    }

    void mergedRowStaysConnectedWhenEitherBandIsActive()
    {
        // Connected on 5 GHz, while 2.4 GHz is the stronger sighting.
        QVariantList aps;
        for (const auto &[ssid, path, strength] :
             {std::tuple{QStringLiteral("Anonymous_5G"), QStringLiteral("/ap/1"), 40u},
              std::tuple{QStringLiteral("Anonymous_2.4G"), QStringLiteral("/ap/2"), 80u}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("strength"), strength);
            m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
            aps.append(m);
        }
        const QVariantList rows = collapseNetworks(aps, QStringLiteral("/ap/1"), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QVERIFY(rows.first().toMap().value(QStringLiteral("connected")).toBool());
        QCOMPARE(rows.first().toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/ap/1"));
    }

    void unsuffixedTwinMergesToo()
    {
        // Plenty of routers advertise "DnD" on 2.4 and "DnD_5G" on 5.
        QVariantList aps;
        for (const auto &[ssid, path, strength] :
             {std::tuple{QStringLiteral("DnD"), QStringLiteral("/ap/1"), 57u},
              std::tuple{QStringLiteral("DnD_5G"), QStringLiteral("/ap/2"), 32u}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("strength"), strength);
            m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
            aps.append(m);
        }
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("DnD"));
        QCOMPARE(rows.first().toMap().value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("5G")}));
    }

    void singleBandKeepsItsBadge()
    {
        QVariantMap m;
        m.insert(QStringLiteral("ssid"), QStringLiteral("Anonymous_5G"));
        m.insert(QStringLiteral("path"), QStringLiteral("/ap/1"));
        m.insert(QStringLiteral("strength"), 65u);
        m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
        const QVariantList rows = collapseNetworks({m}, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Anonymous"));
        QCOMPARE(rows.first().toMap().value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("5G")}));
        // Still joinable by its real name.
        QCOMPARE(rows.first().toMap().value(QStringLiteral("ssid")).toString(),
                 QStringLiteral("Anonymous_5G"));
    }

    void differentNetworksAreNotMerged()
    {
        QVariantList aps;
        for (const auto &[ssid, path] :
             {std::tuple{QStringLiteral("Home"), QStringLiteral("/ap/1")},
              std::tuple{QStringLiteral("HomeGuest"), QStringLiteral("/ap/2")}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("strength"), 50u);
            m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
            aps.append(m);
        }
        QCOMPARE(collapseNetworks(aps, QString(), QString(), 12).size(), 2);
    }

    void differentKeyManagementIsNotMerged()
    {
        // A WPA2-PSK "Home" and a WPA3-SAE "Home_5G" cannot be joined by one
        // click, so they are two networks however alike they look.
        QVariantList aps;
        for (const auto &[ssid, path, kind] :
             {std::tuple{QStringLiteral("Home"), QStringLiteral("/ap/1"), QStringLiteral("wpa-psk")},
              std::tuple{QStringLiteral("Home_5G"), QStringLiteral("/ap/2"), QStringLiteral("sae")}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("strength"), 50u);
            m.insert(QStringLiteral("kind"), kind);
            aps.append(m);
        }
        QCOMPARE(collapseNetworks(aps, QString(), QString(), 12).size(), 2);
    }

    void separateBandsSplitEvenWithOneName()
    {
        // With merging off, one name on two radios is two rows - otherwise the
        // switch would do nothing for a router that shares its name.
        const QVariantList aps{
            ap(QStringLiteral("Home"), QStringLiteral("/ap/1"), 40, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Home"), QStringLiteral("/ap/2"), 80, 2437,
               QStringLiteral("aa:bb:cc:dd:ee:02"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12, false);
        QCOMPARE(rows.size(), 2);
        QStringList seen;
        for (const QVariant &entry : rows) {
            const QVariantMap row = entry.toMap();
            QCOMPARE(row.value(QStringLiteral("ssid")).toString(), QStringLiteral("Home"));
            seen.append(row.value(QStringLiteral("bands")).toStringList().value(0));
        }
        seen.sort();
        QCOMPARE(seen, QStringList({QStringLiteral("2.4G"), QStringLiteral("5G")}));
    }

    void separateBandsStillCollapseAMesh()
    {
        // Same network, same band, several radios: still one row each, or a
        // mesh would fill the panel with duplicates.
        const QVariantList aps{
            ap(QStringLiteral("Office"), QStringLiteral("/ap/1"), 40, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Office"), QStringLiteral("/ap/2"), 70, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:02"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12, false);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("strength")).toUInt(), 70u);
    }

    void mergingCanBeTurnedOff()
    {
        QVariantList aps;
        for (const auto &[ssid, path, strength] :
             {std::tuple{QStringLiteral("Anonymous_5G"), QStringLiteral("/ap/1"), 65u},
              std::tuple{QStringLiteral("Anonymous_2.4G"), QStringLiteral("/ap/2"), 75u}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            m.insert(QStringLiteral("strength"), strength);
            m.insert(QStringLiteral("kind"), QStringLiteral("wpa-psk"));
            aps.append(m);
        }
        QCOMPARE(collapseNetworks(aps, QString(), QString(), 12, false).size(), 2);
    }

    void joinPrefersTheBandWithAProfile()
    {
        // Strongest first, but the saved profile is on 5G - reconnecting must
        // not ask for a password that is already stored for the sibling band.
        QVariantList members;
        for (const auto &[ssid, path] :
             {std::tuple{QStringLiteral("Anonymous_2.4G"), QStringLiteral("/ap/2")},
              std::tuple{QStringLiteral("Anonymous_5G"), QStringLiteral("/ap/1")}}) {
            QVariantMap m;
            m.insert(QStringLiteral("ssid"), ssid);
            m.insert(QStringLiteral("path"), path);
            members.append(m);
        }
        QCOMPARE(wifiSavedMember(members, {QStringLiteral("Anonymous_5G")}), 1);
        QCOMPARE(wifiSavedMember(members, {QStringLiteral("Anonymous_2.4G"),
                                           QStringLiteral("Anonymous_5G")}),
                 0);
        // Nothing saved: -1 means a fresh profile, and the agent may ask.
        QCOMPARE(wifiSavedMember(members, {}), -1);
        QCOMPARE(wifiSavedMember(members, {QStringLiteral("Other")}), -1);
    }

    // ---- the band comes from the radio, not the name ---------------------

    void bandFromFrequency()
    {
        QCOMPARE(wifiBandFromFrequency(2412), QStringLiteral("2.4G"));
        QCOMPARE(wifiBandFromFrequency(2437), QStringLiteral("2.4G"));
        QCOMPARE(wifiBandFromFrequency(2484), QStringLiteral("2.4G"));
        QCOMPARE(wifiBandFromFrequency(5180), QStringLiteral("5G"));
        QCOMPARE(wifiBandFromFrequency(5745), QStringLiteral("5G"));
        QCOMPARE(wifiBandFromFrequency(6135), QStringLiteral("6G"));
        QCOMPARE(wifiBandFromFrequency(5955), QStringLiteral("6G"));
        // Not a band at all - an AP NetworkManager has not finished describing.
        QVERIFY(wifiBandFromFrequency(0).isEmpty());
    }

    void badgeUsesTheFrequencyNotTheSuffix()
    {
        // Names agree with the radios here, and the badge follows the radios.
        const QVariantList aps{
            ap(QStringLiteral("Anonymous_5G"), QStringLiteral("/ap/1"), 65, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Anonymous_2.4G"), QStringLiteral("/ap/2"), 75, 2437,
               QStringLiteral("aa:bb:cc:dd:ee:02"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Anonymous"));
        QCOMPARE(rows.first().toMap().value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("2.4G"), QStringLiteral("5G")}));
    }

    void sameNameOnBothBandsIsOneRow()
    {
        // One SSID, two radios - what a modern router does by default. No name
        // heuristic could ever have found this; the frequencies do.
        const QVariantList aps{
            ap(QStringLiteral("Home"), QStringLiteral("/ap/1"), 40, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Home"), QStringLiteral("/ap/2"), 80, 2437,
               QStringLiteral("aa:bb:cc:dd:ee:02"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        const QVariantMap row = rows.first().toMap();
        QCOMPARE(row.value(QStringLiteral("label")).toString(), QStringLiteral("Home"));
        QCOMPARE(row.value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("2.4G"), QStringLiteral("5G")}));
        // Strongest sighting represents it, but both remain joinable.
        QCOMPARE(row.value(QStringLiteral("members")).toList().size(), 2);
    }

    void sixGhzBandsAreShown()
    {
        const QVariantList aps{
            ap(QStringLiteral("Papa_6G"), QStringLiteral("/ap/1"), 60, 6135,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Papa_5G"), QStringLiteral("/ap/2"), 50, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:02"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("5G"), QStringLiteral("6G")}));
    }

    void aLyingSuffixIsNotMerged()
    {
        // 'Foo_5G' actually transmitting on 2.4 GHz is not a 5 GHz network, and
        // it is not the same network as 'Foo_2.4G' on 5 GHz either. The names
        // would merge them; the frequencies refuse.
        const QVariantList aps{
            ap(QStringLiteral("Foo_5G"), QStringLiteral("/ap/1"), 60, 2437,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Foo_2.4G"), QStringLiteral("/ap/2"), 60, 5180,
               QStringLiteral("aa:bb:cc:dd:ee:02"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 2);
        // Each keeps its own real band in the badge.
        for (const QVariant &entry : rows) {
            const QVariantMap row = entry.toMap();
            const QStringList bands = row.value(QStringLiteral("bands")).toStringList();
            QCOMPARE(bands.size(), 1);
            const QString ssid = row.value(QStringLiteral("ssid")).toString();
            QCOMPARE(bands.first(),
                     ssid == QStringLiteral("Foo_5G") ? QStringLiteral("2.4G")
                                                      : QStringLiteral("5G"));
        }
    }

    void unknownFrequencyFallsBackToTheName()
    {
        // An AP NetworkManager has not filled in yet still gets a plausible
        // badge rather than none.
        const QVariantList rows =
            collapseNetworks({ap(QStringLiteral("DnD_5G"), QStringLiteral("/ap/1"), 50, 0)}, {}, {},
                             12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("bands")).toStringList(),
                 QStringList({QStringLiteral("5G")}));
    }

    void oneBssidIsOneRadio()
    {
        // Two bus objects for the same transmitter are not two networks.
        const QVariantList aps{
            ap(QStringLiteral("Home"), QStringLiteral("/ap/1"), 20, 2437,
               QStringLiteral("aa:bb:cc:dd:ee:01")),
            ap(QStringLiteral("Home"), QStringLiteral("/ap/9"), 70, 2437,
               QStringLiteral("aa:bb:cc:dd:ee:01"))};
        const QVariantList rows = collapseNetworks(aps, QString(), QString(), 12);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("members")).toList().size(), 1);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("strength")).toUInt(), 70u);
    }

    // ---- when a connection is really up ---------------------------------

    void associatingIsNotTheConnectedRow()
    {
        // The bug behind "connected, although the password was never entered":
        // an access point is associated for the whole handshake, so the row
        // must only be handed the path once ACTIVATED.
        QCOMPARE(wifiConnectedPath(1, QStringLiteral("/ap/1")), QString());
        QCOMPARE(wifiConnectedPath(0, QStringLiteral("/ap/1")), QString());
        QCOMPARE(wifiConnectedPath(3, QStringLiteral("/ap/1")), QString());
        QCOMPARE(wifiConnectedPath(2, QStringLiteral("/ap/1")), QStringLiteral("/ap/1"));
    }

    void associatingRowShowsBusyNotConnected()
    {
        // What the panel actually renders during a join: the pending row is
        // busy, and no row claims to be connected.
        const QVariantList aps{
            ap(QStringLiteral("Praction_Lalit"), QStringLiteral("/ap/1"), 60, 2412,
               QStringLiteral("aa:bb:cc:dd:ee:01"))};
        const QVariantList rows = collapseNetworks(aps, wifiConnectedPath(1, QStringLiteral("/ap/1")),
                                                  QStringLiteral("/ap/1"), 12);
        QCOMPARE(rows.size(), 1);
        const QVariantMap row = rows.first().toMap();
        QVERIFY(!row.value(QStringLiteral("connected")).toBool());
        QVERIFY(row.value(QStringLiteral("busy")).toBool());
        QVERIFY(row.value(QStringLiteral("busyConnect")).toBool());
    }

    void needAuthMeansTheUserIsBeingAsked()
    {
        // NM_DEVICE_STATE_NEED_AUTH is the password dialog being open, not a
        // stalled join: the deadline must not expire behind it.
        QVERIFY(wifiAwaitingSecret(60));
        QVERIFY(!wifiAwaitingSecret(0));
        QVERIFY(!wifiAwaitingSecret(40));   // prepare
        QVERIFY(!wifiAwaitingSecret(50));   // config
        QVERIFY(!wifiAwaitingSecret(100));  // activated
        QVERIFY(!wifiAwaitingSecret(120));  // failed
    }

    void anAttemptIsOverWhenTheDeviceGivesUp()
    {
        // The device picks an attempt up in this range; only then is a
        // DISCONNECTED reading the end of the attempt rather than the instant
        // before it started.
        for (uint state : {40u, 50u, 60u, 70u, 90u, 100u}) {
            QVERIFY(wifiDeviceProgressed(state));
            QVERIFY(!wifiDeviceGaveUp(state));
        }
        for (uint state : {0u, 10u, 20u, 30u, 110u, 120u}) {
            QVERIFY(!wifiDeviceProgressed(state));
        }
        // Disconnected and failed are the outcomes that free the panel.
        QVERIFY(wifiDeviceGaveUp(30));
        QVERIFY(wifiDeviceGaveUp(120));
        QVERIFY(!wifiDeviceGaveUp(0));
        QVERIFY(!wifiDeviceGaveUp(100));
    }

    void onlyActivatedCountsAsConnected()
    {
        // The bug that showed "Connected" for two seconds on a wrong password:
        // ACTIVATING (1) must not read as connected, nor must the states after
        // ACTIVATED.
        QVERIFY(!wifiIsAssociated(0)); // unknown
        QVERIFY(!wifiIsAssociated(1)); // activating - still handshaking
        QVERIFY(wifiIsAssociated(2));  // activated
        QVERIFY(!wifiIsAssociated(3)); // deactivating
        QVERIFY(!wifiIsAssociated(4)); // deactivated
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    WifiModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "wifitest.moc"
