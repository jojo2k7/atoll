// SPDX-License-Identifier: GPL-3.0-or-later
#include "config/config.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
class ConfigTest : public QObject {
    Q_OBJECT
    void write(const QByteArray &json) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/atoll";
        QDir().mkpath(dir);
        QFile file(dir + "/atoll.json");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(json);
    }
private Q_SLOTS:
    void defaults() {
        write("{}");
        Config config;
        QCOMPARE(config.value("controls.click").toString(), "expand");
        QCOMPARE(config.value("controls.middleClick").toString(), "officeOrPlayPause");
        QCOMPARE(config.value("controls.scrollUp").toString(), "volumeUp");
        QCOMPARE(config.value("controls.hover").toString(), "peek");
        QCOMPARE(config.value("controls.longPressMs").toInt(), 450);
        QVERIFY(config.value("controls.shortcuts").toMap().isEmpty());
    }
    void oldKeys() {
        write(R"({"behavior": {"middleClickAction": "playPause", "scrollAdjustsVolume": false,
                  "hoverPeek": false, "volumeStep": 10, "collapseOnLeave": false},
                  "ai": {"longPressMs": 700}})");
        Config config;
        QCOMPARE(config.value("controls.middleClick").toString(), "playPause");
        QCOMPARE(config.value("controls.scrollUp").toString(), "none");
        QCOMPARE(config.value("controls.scrollDown").toString(), "none");
        QCOMPARE(config.value("controls.hover").toString(), "none");
        QCOMPARE(config.value("controls.volumeStep").toInt(), 10);
        QCOMPARE(config.value("controls.collapseOnLeave").toBool(), false);
        QCOMPARE(config.value("controls.longPressMs").toInt(), 700);
        QCOMPARE(config.value("controls.click").toString(), "expand");
    }
    void newKeysWin() {
        write(R"({"behavior": {"middleClickAction": "playPause", "scrollAdjustsVolume": false, "expandOnHover": true},
                  "controls": {"middleClick": "office", "scrollUp": "next"}})");
        Config config;
        QCOMPARE(config.value("controls.middleClick").toString(), "office");
        QCOMPARE(config.value("controls.scrollUp").toString(), "next");
        QCOMPARE(config.value("controls.scrollDown").toString(), "none");
        QCOMPARE(config.value("controls.hover").toString(), "expand");
    }
};
int main(int argc, char **argv) {
    QTemporaryDir temporary;
    qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8());
    QCoreApplication app(argc, argv);
    ConfigTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "configtest.moc"
