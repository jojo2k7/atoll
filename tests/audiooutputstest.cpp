// SPDX-License-Identifier: GPL-3.0-or-later
#include "media/audiooutputs.h"
#include "config/config.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class AudioOutputsTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QByteArray originalPath;
    void write(const QString &name, const QByteArray &data) {
        QFile file(dir.filePath(name)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(data);
    }
    QByteArray read(const QString &name) { QFile f(dir.filePath(name)); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    void seed(AudioOutputs &audio) {
        audio.m_sinks = QJsonDocument::fromJson(R"([
          {"name":"alsa_output.test.stereo","description":"Analog output","properties":{}},
          {"name":"bluez_output.AA_BB_CC_DD_EE_FF.1","description":"Headphones","properties":{}},
          {"name":"easyeffects_sink","description":"Effects","properties":{}}
        ])").array();
        audio.m_bluetooth = {QVariantMap{{"id", "bt:AA:BB:CC:DD:EE:FF"}, {"label", "Headphones"},
            {"path", "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"}, {"bluetooth", true}, {"connected", false}, {"physical", true}}};
        audio.m_default = "alsa_output.test.stereo";
        audio.rebuild();
        write("sinks", QJsonDocument(audio.m_sinks).toJson());
        write("default", "alsa_output.test.stereo\n");
    }
private Q_SLOTS:
    void initTestCase() {
        originalPath = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8());
        qputenv("ATOLL_AUDIO_TEST_DIR", dir.path().toUtf8());
        write("pactl", R"SH(#!/bin/sh
printf '%s\n' "$*" >> "$ATOLL_AUDIO_TEST_DIR/calls"
case "$*" in
 'subscribe') exit 0 ;;
 '--format=json list sinks') /bin/cat "$ATOLL_AUDIO_TEST_DIR/sinks" ;;
 'get-default-sink') /bin/cat "$ATOLL_AUDIO_TEST_DIR/default" ;;
 '--format=json list sink-inputs') /bin/cat "$ATOLL_AUDIO_TEST_DIR/streams" ;;
 set-default-sink*)
   if [ -f "$ATOLL_AUDIO_TEST_DIR/fail" ]; then echo 'Rejected output' >&2; exit 1; fi
   printf '%s\n' "$2" > "$ATOLL_AUDIO_TEST_DIR/default" ;;
 move-sink-input*) exit 0 ;;
 *) exit 1 ;;
esac
)SH");
        QFile::setPermissions(dir.filePath("pactl"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    void cleanupTestCase() { qputenv("PATH", originalPath); }
    void init() {
        QFile::remove(dir.filePath("fail")); write("calls", {}); write("streams", "[]");
    }
    void mergeAndDisconnect() {
        Config config; AudioOutputs audio(&config); seed(audio);
        QCOMPARE(audio.outputs().size(), 3);
        QCOMPARE(audio.currentId(), "sink:alsa_output.test.stereo");
        const auto bt = audio.find("bt:AA:BB:CC:DD:EE:FF");
        QVERIFY(bt["connected"].toBool()); QVERIFY(!bt["path"].toString().isEmpty());
        audio.renameOutput("sink:alsa_output.test.stereo", "My headphones · AUX");
        QCOMPARE(audio.currentName(), "My headphones · AUX");
        audio.m_default = "bluez_output.AA_BB_CC_DD_EE_FF.1"; audio.rebuild();
        QVERIFY(audio.bluetoothActive());
        audio.m_sinks.removeAt(1); audio.rebuild();
        QVERIFY(!audio.bluetoothActive());
        QVERIFY(audio.find("bt:AA:BB:CC:DD:EE:FF")["sink"].toString().isEmpty());
        QCOMPARE(audio.outputs().size(), 3); // Paired headphones stay selectable.
    }
    void routeRunningPlayersWithoutMovingFilters() {
        Config config; AudioOutputs audio(&config); seed(audio);
        write("streams", R"([
          {"index":8,"properties":{"application.name":"Music"}},
          {"index":9,"properties":{"media.role":"filter"}},
          {"index":10,"properties":{"node.virtual":"true"}}
        ])");
        audio.selectOutput("sink:alsa_output.test.stereo");
        QVERIFY(audio.busy());
        audio.selectOutput("bt:AA:BB:CC:DD:EE:FF"); // A double click cannot race the pending operation.
        QTRY_VERIFY_WITH_TIMEOUT(!audio.busy(), 3000);
        QCOMPARE(audio.currentId(), "sink:alsa_output.test.stereo");
        QVERIFY(audio.message().isEmpty());
        const auto calls = read("calls");
        QVERIFY(calls.contains("move-sink-input 8 alsa_output.test.stereo"));
        QVERIFY(!calls.contains("move-sink-input 9")); QVERIFY(!calls.contains("move-sink-input 10"));
        QVERIFY(!calls.contains("set-default-sink bluez"));
        QCOMPARE(config.value("audio.fallbackOutput").toString(), "sink:alsa_output.test.stereo");
    }
    void failedSwitchLeavesRoutingAlone() {
        Config config; AudioOutputs audio(&config); seed(audio); write("fail", "1");
        audio.selectOutput("bt:AA:BB:CC:DD:EE:FF");
        QTRY_VERIFY_WITH_TIMEOUT(!audio.busy(), 3000);
        QCOMPARE(audio.currentId(), "sink:alsa_output.test.stereo");
        QVERIFY(audio.message().contains("Rejected")); QVERIFY(!read("calls").contains("move-sink-input"));
    }
    void malformedServerReply() {
        Config config; AudioOutputs audio(&config); seed(audio); write("sinks", "not json");
        audio.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(audio.message().contains("unavailable"), 3000);
        QVERIFY(audio.currentId().isEmpty());
    }
    void vanishedDevice() {
        Config config; AudioOutputs audio(&config); seed(audio);
        audio.selectOutput("sink:gone");
        QVERIFY(!audio.busy()); QVERIFY(audio.message().contains("no longer"));
        QVERIFY(!read("calls").contains("set-default-sink"));
    }
};
int main(int argc, char **argv) {
    QTemporaryDir configDir;
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
    // No test touches the desktop's real Bluetooth service.
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent-atoll-audio-test-bus");
    QCoreApplication app(argc, argv);
    app.setApplicationName("atoll-audio-test");
    AudioOutputsTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "audiooutputstest.moc"
