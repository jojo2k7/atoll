// SPDX-License-Identifier: GPL-3.0-or-later
#include "focus/focusservice.h"
#include "config/config.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>
#include <QFile>
#include <QStandardPaths>
class FocusTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void completion() {
        Config config;
        config.setValue("focus.soundEnabled", false);
        config.setValue("focus.rounds", 1);
        config.setValue("focus.longBreakMinutes", 12);
        FocusService initial(&config);
        initial.start(); initial.stop();
        QFile state(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/focus-state.json");
        QVERIFY(state.open(QIODevice::WriteOnly));
        state.write(R"({"phase":"focus","duration":60,"remaining":1,"round":1})"); state.close();
        FocusService restored(&config);
        QVERIFY(restored.state()["paused"].toBool());
        restored.togglePause();
        QTRY_COMPARE_WITH_TIMEOUT(restored.state()["phase"].toString(), "break", 2500);
        QCOMPARE(restored.state()["completed"].toInt(), 1);
        QCOMPARE(restored.state()["remaining"].toInt(), 720);
        restored.stop();
        QVERIFY(state.open(QIODevice::WriteOnly)); state.write("{}"); state.close();
    }
    void lifecycle() {
        Config config;
        config.setValue("focus.soundEnabled", false);
        config.setValue("focus.minutes", 1);
        config.setValue("focus.extensionMinutes", 2);
        FocusService focus(&config);
        QCOMPARE(focus.state()["phase"].toString(), "idle");
        focus.start();
        QCOMPARE(focus.state()["remaining"].toInt(), 60);
        focus.start(); // Idempotent; don't reset a running session.
        QTest::qWait(1150);
        QVERIFY(focus.state()["remaining"].toInt() < 60);
        focus.togglePause();
        const int remaining = focus.state()["remaining"].toInt();
        QTest::qWait(1100);
        QCOMPARE(focus.state()["remaining"].toInt(), remaining);
        focus.extend();
        QCOMPARE(focus.state()["remaining"].toInt(), remaining + 120);
        focus.next();
        QCOMPARE(focus.state()["phase"].toString(), "break");
        QCOMPARE(focus.state()["completed"].toInt(), 0); // Skipping never earns a completed phase.
        focus.next();
        QVERIFY(focus.state()["paused"].toBool());
        focus.togglePause();
        focus.locked(true);
        QVERIFY(focus.state()["paused"].toBool());
        focus.park("  A thought  ");
        focus.park("  ");
        QCOMPARE(focus.state()["notes"].toStringList(), QStringList{"A thought"});
        focus.togglePause();
        { FocusService restored(&config);
          QCOMPARE(restored.state()["phase"].toString(), "focus");
          QVERIFY(restored.state()["paused"].toBool());
          QCOMPARE(restored.state()["notes"].toStringList(), QStringList{"A thought"}); }
        focus.clearNotes();
        focus.stop();
        QVERIFY(!focus.state()["active"].toBool());
        config.setValue("focus.minutes", -200);
        focus.start();
        QCOMPARE(focus.state()["remaining"].toInt(), 60);
        focus.stop();
    }
};
int main(int argc, char **argv) {
    QTemporaryDir temporary;
    qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8());
    qputenv("XDG_DATA_HOME", temporary.path().toUtf8());
    QCoreApplication app(argc, argv);
    app.setApplicationName("atoll-focus-test");
    FocusTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "focustest.moc"
