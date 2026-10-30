// SPDX-License-Identifier: GPL-3.0-or-later
#include "office/claudeask.h"
#include "office/officeevent.h"
#include "office/officeservice.h"
#include "office/systemmonitor.h"
#include "config/config.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace Qt::StringLiterals;

class OfficeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void compactBash()
    {
        const QJsonObject withDescription{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_input", QJsonObject{{"command", "ls -la"}, {"description", "List files"}}},
        };
        QCOMPARE(OfficeEvent::compact(withDescription).value("arg").toString(), QStringLiteral("List files"));

        const QJsonObject withoutDescription{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_input", QJsonObject{{"command", "ls -la"}}},
        };
        QCOMPARE(OfficeEvent::compact(withoutDescription).value("arg").toString(), QStringLiteral("ls -la"));
    }

    void compactEditBasename()
    {
        const QJsonObject hook{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Edit"},
            {"tool_input", QJsonObject{{"file_path", "/home/user/project/src/main.cpp"}}},
        };
        QCOMPARE(OfficeEvent::compact(hook).value("arg").toString(), QStringLiteral("main.cpp"));
    }

    void compactSendMessage()
    {
        const QJsonObject hook{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "SendMessage"},
            {"tool_input", QJsonObject{{"to", "alice"}, {"message", "hello there"}}},
        };
        const QJsonObject event = OfficeEvent::compact(hook);
        QCOMPARE(event.value("arg").toString(), QStringLiteral("alice"));
        QCOMPARE(event.value("to").toString(), QStringLiteral("alice"));
        QCOMPARE(event.value("msg").toString(), QStringLiteral("hello there"));
    }

    void compactAskUserQuestion()
    {
        const QJsonObject hook{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "AskUserQuestion"},
            {"tool_input", QJsonObject{{"questions", QJsonArray{QJsonObject{{"question", "Which one?"}}}}}},
        };
        const QJsonObject event = OfficeEvent::compact(hook);
        QCOMPARE(event.value("arg").toString(), QStringLiteral("Which one?"));
        QCOMPARE(event.value("msg").toString(), QStringLiteral("Which one?"));
    }

    void compactPostToolUseFailure()
    {
        const QJsonObject failure{
            {"hook_event_name", "PostToolUseFailure"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
        };
        QCOMPARE(OfficeEvent::compact(failure).value("ok").toBool(), false);

        const QJsonObject errorResponse{
            {"hook_event_name", "PostToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_response", QJsonObject{{"is_error", true}}},
        };
        QCOMPARE(OfficeEvent::compact(errorResponse).value("ok").toBool(), false);

        const QJsonObject okResponse{
            {"hook_event_name", "PostToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_response", QJsonObject{{"output", "done"}}},
        };
        QCOMPARE(OfficeEvent::compact(okResponse).value("ok").toBool(), true);
    }

    void compactWebFetchHost()
    {
        const QJsonObject hook{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "WebFetch"},
            {"tool_input", QJsonObject{{"url", "https://example.com/path?x=1"}}},
        };
        QCOMPARE(OfficeEvent::compact(hook).value("arg").toString(), QStringLiteral("example.com"));
    }

    void compactPvEditDiff()
    {
        const QJsonObject hook{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Edit"},
            {"tool_input",
             QJsonObject{
                 {"file_path", "/home/user/project/src/main.cpp"},
                 {"old_string", "int a = 1;"},
                 {"new_string", "int a = 2;\nint b = 3;"},
             }},
        };
        const QJsonObject pv = OfficeEvent::compact(hook).value("pv").toObject();
        QCOMPARE(pv.value("kind").toString(), QStringLiteral("diff"));
        QCOMPARE(pv.value("title").toString(), QStringLiteral("main.cpp"));
        const QString text = pv.value("text").toString();
        QVERIFY(text.contains(QStringLiteral("-int a = 1;")));
        QVERIFY(text.contains(QStringLiteral("+int a = 2;")));
        QVERIFY(text.contains(QStringLiteral("+int b = 3;")));
    }

    void compactPvBashPrePost()
    {
        const QJsonObject pre{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_input", QJsonObject{{"command", "ls -la"}, {"description", "List files"}}},
        };
        const QJsonObject prePv = OfficeEvent::compact(pre).value("pv").toObject();
        QCOMPARE(prePv.value("kind").toString(), QStringLiteral("terminal"));
        QCOMPARE(prePv.value("title").toString(), QStringLiteral("List files"));
        QCOMPARE(prePv.value("text").toString(), QStringLiteral("$ ls -la"));

        const QJsonObject post{
            {"hook_event_name", "PostToolUse"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_input", QJsonObject{{"command", "ls -la"}, {"description", "List files"}}},
            {"tool_response", QJsonObject{{"stdout", "a.txt\nb.txt"}, {"stderr", ""}, {"interrupted", false}, {"isImage", false}}},
        };
        const QString postText = OfficeEvent::compact(post).value("pv").toObject().value("text").toString();
        QVERIFY(postText.contains(QStringLiteral("$ ls -la")));
        QVERIFY(postText.contains(QStringLiteral("a.txt")));
        QVERIFY(postText.contains(QStringLiteral("b.txt")));
    }

    void compactPvPostToolUseFailure()
    {
        const QJsonObject failure{
            {"hook_event_name", "PostToolUseFailure"},
            {"session_id", "s1"},
            {"tool_name", "Bash"},
            {"tool_input", QJsonObject{{"command", "false"}}},
            {"error", "Exit code 1\nsomething broke"},
        };
        const QJsonObject pv = OfficeEvent::compact(failure).value("pv").toObject();
        QCOMPARE(pv.value("kind").toString(), QStringLiteral("terminal"));
        const QString text = pv.value("text").toString();
        QVERIFY(text.contains(QStringLiteral("Exit code 1")));
        QVERIFY(text.contains(QStringLiteral("something broke")));
    }

    void compactPvAskUserQuestion()
    {
        const QJsonObject hook{
            {"hook_event_name", "PreToolUse"},
            {"session_id", "s1"},
            {"tool_name", "AskUserQuestion"},
            {"tool_input", QJsonObject{{"questions", QJsonArray{QJsonObject{{"question", "Which one?"}, {"header", "Pick"}}}}}},
        };
        const QJsonObject pv = OfficeEvent::compact(hook).value("pv").toObject();
        QCOMPARE(pv.value("kind").toString(), QStringLiteral("question"));
        QCOMPARE(pv.value("title").toString(), QStringLiteral("Pick"));
        QCOMPARE(pv.value("text").toString(), QStringLiteral("Which one?"));
    }

    void sessionAndSubagents()
    {
        Config config;
        OfficeService office(&config);

        // Lazy session creation: SessionStart on an unknown session id
        // creates it, with an idle lead.
        office.ingest(QJsonObject{{"e", "SessionStart"}, {"s", "sess1"}, {"cwd", "/home/user/project"}, {"ts", 1000}});
        QCOMPARE(office.sessions().size(), 1);
        QCOMPARE(office.currentSession(), QStringLiteral("sess1"));
        QCOMPARE(office.agents().size(), 1);
        const QVariantMap lead = office.agents().constFirst().toMap();
        QCOMPARE(lead["name"].toString(), QStringLiteral("Lead"));
        QCOMPARE(lead["state"].toString(), QStringLiteral("idle"));
        QCOMPARE(lead["desk"].toInt(), -1);

        // First subagent: lowest free desk (0), named "Explore 1".
        office.ingest(QJsonObject{{"e", "SubagentStart"}, {"s", "sess1"}, {"a", "agent-a"}, {"at", "Explore"}, {"ts", 1100}});
        QVariantList agents = office.agents();
        QCOMPARE(agents.size(), 2);
        QVariantMap first = agents.at(1).toMap();
        QCOMPARE(first["name"].toString(), QStringLiteral("Explore 1"));
        QCOMPARE(first["desk"].toInt(), 0);
        QCOMPARE(first["state"].toString(), QStringLiteral("working"));

        // Second subagent: next free desk (1), named "Explore 2".
        office.ingest(QJsonObject{{"e", "SubagentStart"}, {"s", "sess1"}, {"a", "agent-b"}, {"at", "Explore"}, {"ts", 1200}});
        agents = office.agents();
        QCOMPARE(agents.size(), 3);
        QVariantMap second = agents.at(2).toMap();
        QCOMPARE(second["name"].toString(), QStringLiteral("Explore 2"));
        QCOMPARE(second["desk"].toInt(), 1);

        QCOMPARE(office.working(), 2);
        QCOMPARE(office.resting(), 1);
        QCOMPARE(office.waiting(), 0);
    }

    void askUserQuestionAndClear()
    {
        Config config;
        OfficeService office(&config);
        QSignalSpy attentionSpy(&office, &OfficeService::attention);

        office.ingest(QJsonObject{{"e", "SessionStart"}, {"s", "sess1"}, {"cwd", "/tmp"}, {"ts", 1000}});
        office.ingest(QJsonObject{
            {"e", "PreToolUse"},
            {"s", "sess1"},
            {"t", "AskUserQuestion"},
            {"arg", "Which editor?"},
            {"msg", "Which editor?"},
            {"ts", 1100},
        });

        QCOMPARE(attentionSpy.count(), 1);
        const QVariantList signalArgs = attentionSpy.takeFirst();
        QCOMPARE(signalArgs.at(0).toString(), QStringLiteral("sess1"));
        QCOMPARE(signalArgs.at(2).toString(), QStringLiteral("Which editor?"));

        QVariantMap lead = office.agents().constFirst().toMap();
        QCOMPARE(lead["state"].toString(), QStringLiteral("waiting"));
        QCOMPARE(lead["waitingFor"].toString(), QStringLiteral("user"));
        QCOMPARE(office.waiting(), 1);

        office.ingest(QJsonObject{{"e", "PostToolUse"}, {"s", "sess1"}, {"t", "AskUserQuestion"}, {"ok", true}, {"ts", 1200}});
        lead = office.agents().constFirst().toMap();
        QCOMPARE(lead["state"].toString(), QStringLiteral("working"));
        QVERIFY(lead["waitingFor"].toString().isEmpty());
        QCOMPARE(office.waiting(), 0);
    }

    void subagentStopLeaves()
    {
        Config config;
        OfficeService office(&config);

        office.ingest(QJsonObject{{"e", "SessionStart"}, {"s", "sess1"}, {"cwd", "/tmp"}, {"ts", 1000}});
        office.ingest(QJsonObject{{"e", "SubagentStart"}, {"s", "sess1"}, {"a", "agent-a"}, {"at", "Explore"}, {"ts", 1100}});
        office.ingest(QJsonObject{{"e", "SubagentStop"}, {"s", "sess1"}, {"a", "agent-a"}, {"at", "Explore"}, {"ts", 1200}});

        bool found = false;
        for (const QVariant &entry : office.agents()) {
            const QVariantMap map = entry.toMap();
            if (map["key"].toString() == QStringLiteral("agent-a")) {
                QCOMPARE(map["state"].toString(), QStringLiteral("leaving"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void stopSetsLeadIdle()
    {
        Config config;
        OfficeService office(&config);

        office.ingest(QJsonObject{{"e", "SessionStart"}, {"s", "sess1"}, {"cwd", "/tmp"}, {"ts", 1000}});
        office.ingest(QJsonObject{{"e", "UserPromptSubmit"}, {"s", "sess1"}, {"ts", 1100}});
        QCOMPARE(office.agents().constFirst().toMap()["state"].toString(), QStringLiteral("working"));

        office.ingest(QJsonObject{{"e", "Stop"}, {"s", "sess1"}, {"ts", 1200}});
        QCOMPARE(office.agents().constFirst().toMap()["state"].toString(), QStringLiteral("idle"));
        QCOMPARE(office.resting(), 1);
        QCOMPARE(office.waiting(), 0);
    }

    void bashSetsRunningAndPostClears()
    {
        Config config;
        OfficeService office(&config);

        office.ingest(QJsonObject{{"e", "SessionStart"}, {"s", "sess1"}, {"cwd", "/tmp"}, {"ts", 1000}});
        office.ingest(QJsonObject{{"e", "PreToolUse"}, {"s", "sess1"}, {"t", "Bash"}, {"arg", "sleep 5"}, {"ppid", 4242}, {"ts", 1100}});

        QVariantMap lead = office.agents().constFirst().toMap();
        QCOMPARE(lead["running"].toBool(), true);
        // Just started: not yet past the 3 s busy threshold.
        QCOMPARE(lead["busy"].toBool(), false);
        QCOMPARE(lead["pid"].toLongLong(), qint64(4242));
        QCOMPARE(office.sessions().constFirst().toMap()["pid"].toLongLong(), qint64(4242));

        office.ingest(QJsonObject{{"e", "PostToolUse"}, {"s", "sess1"}, {"t", "Bash"}, {"ok", true}, {"ts", 1200}});
        lead = office.agents().constFirst().toMap();
        QCOMPARE(lead["running"].toBool(), false);
    }

    void askRoundTrip()
    {
        QTemporaryDir socketDir;
        QVERIFY(socketDir.isValid());
        const QString socketPath = socketDir.path() + QStringLiteral("/claude.sock");

        Config config;
        OfficeService office(&config);
        office.start(socketPath);

        QLocalSocket client;
        client.connectToServer(socketPath);
        QVERIFY(client.waitForConnected(1000));

        const QJsonObject ask{
            {"v", 1},
            {"e", "Ask"},
            {"id", "ask-1"},
            {"s", "sess1"},
            {"a", QString()},
            {"at", QString()},
            {"cwd", "/tmp/project"},
            {"user", "julius"},
            {"ts", 1000},
            {"questions",
             QJsonArray{QJsonObject{
                 {"question", "Which?"},
                 {"header", "Pick"},
                 {"options", QJsonArray{QJsonObject{{"label", "A"}, {"description", "first"}}}},
                 {"multiSelect", false},
             }}},
        };
        client.write(QJsonDocument(ask).toJson(QJsonDocument::Compact) + "\n");
        QVERIFY(client.waitForBytesWritten(1000));

        // QTRY_COMPARE is what actually pumps the event loop far enough for
        // the server to accept, read and process the line (the low-level
        // waitFor* calls above only poll this socket's own fd). The ack is
        // written before any of that state change, so it is already
        // sitting in the client's buffer by the time this is satisfied.
        QTRY_COMPARE(office.questions().size(), 1);

        // QTRY_COMPARE's own polling already pumped the readyRead signal for
        // the ack into Qt's internal buffer, so there is no *new* signal
        // left to wait for - only check if it isn't already sitting there.
        if (!client.canReadLine()) {
            QVERIFY(client.waitForReadyRead(1000));
        }
        const QJsonObject ack = QJsonDocument::fromJson(client.readLine()).object();
        QCOMPARE(ack.value("id").toString(), QStringLiteral("ask-1"));
        QCOMPARE(ack.value("ack").toBool(), true);

        const QVariantMap question = office.questions().constFirst().toMap();
        QCOMPARE(question["id"].toString(), QStringLiteral("ask-1"));
        QCOMPARE(question["session"].toString(), QStringLiteral("sess1"));

        const QVariantMap lead = office.agents().constFirst().toMap();
        QCOMPARE(lead["state"].toString(), QStringLiteral("waiting"));
        QCOMPARE(lead["waitingFor"].toString(), QStringLiteral("user"));

        office.answerQuestion(QStringLiteral("ask-1"), QVariantMap{{"Which?", "A"}});

        QVERIFY(client.waitForReadyRead(1000));
        const QJsonObject reply = QJsonDocument::fromJson(client.readLine()).object();
        QCOMPARE(reply.value("id").toString(), QStringLiteral("ask-1"));
        QCOMPARE(reply.value("answers").toObject().value("Which?").toString(), QStringLiteral("A"));

        QCOMPARE(office.questions().size(), 0);
        QVERIFY(office.agents().constFirst().toMap()["waitingFor"].toString().isEmpty());
    }

    void askCancelPath()
    {
        QTemporaryDir socketDir;
        QVERIFY(socketDir.isValid());
        const QString socketPath = socketDir.path() + QStringLiteral("/claude.sock");

        Config config;
        OfficeService office(&config);
        office.start(socketPath);

        QLocalSocket client;
        client.connectToServer(socketPath);
        QVERIFY(client.waitForConnected(1000));

        const QJsonObject ask{
            {"v", 1},
            {"e", "Ask"},
            {"id", "ask-2"},
            {"s", "sess2"},
            {"a", QString()},
            {"at", QString()},
            {"cwd", "/tmp"},
            {"user", "julius"},
            {"ts", 1000},
            {"questions", QJsonArray{QJsonObject{{"question", "Which?"}}}},
        };
        client.write(QJsonDocument(ask).toJson(QJsonDocument::Compact) + "\n");
        QVERIFY(client.waitForBytesWritten(1000));
        QTRY_COMPARE(office.questions().size(), 1);
        if (!client.canReadLine()) {
            QVERIFY(client.waitForReadyRead(1000));
        }
        client.readLine(); // the ack; already covered by askRoundTrip()

        office.cancelQuestion(QStringLiteral("ask-2"));

        QVERIFY(client.waitForReadyRead(1000));
        const QJsonObject reply = QJsonDocument::fromJson(client.readLine()).object();
        QCOMPARE(reply.value("id").toString(), QStringLiteral("ask-2"));
        QCOMPARE(reply.value("cancel").toBool(), true);
        QCOMPARE(office.questions().size(), 0);
    }

    void askClientDisconnectDropsQuestion()
    {
        QTemporaryDir socketDir;
        QVERIFY(socketDir.isValid());
        const QString socketPath = socketDir.path() + QStringLiteral("/claude.sock");

        Config config;
        OfficeService office(&config);
        office.start(socketPath);

        auto *client = new QLocalSocket();
        client->connectToServer(socketPath);
        QVERIFY(client->waitForConnected(1000));

        const QJsonObject ask{
            {"v", 1},
            {"e", "Ask"},
            {"id", "ask-3"},
            {"s", "sess3"},
            {"a", QString()},
            {"at", QString()},
            {"cwd", "/tmp"},
            {"user", "julius"},
            {"ts", 1000},
            {"questions", QJsonArray{QJsonObject{{"question", "Which?"}}}},
        };
        client->write(QJsonDocument(ask).toJson(QJsonDocument::Compact) + "\n");
        QVERIFY(client->waitForBytesWritten(1000));
        QTRY_COMPARE(office.questions().size(), 1);

        client->disconnectFromServer();
        if (client->state() != QLocalSocket::UnconnectedState) {
            client->waitForDisconnected(1000);
        }
        delete client;

        QTRY_COMPARE(office.questions().size(), 0);
    }

    void systemMonitorSmoke()
    {
        SystemMonitor monitor;
        monitor.sampleOnce();
        QVERIFY(monitor.cpu() >= 0.0 && monitor.cpu() <= 100.0);
        QVERIFY(monitor.memTotal() > 0);
    }

    void claudeAskNeverAcksGivesUpQuickly()
    {
        // In-process, not the binary: stands in for an island that does not
        // understand "Ask" at all (e.g. a v1 build) - it accepts the
        // connection but never sends anything back.
        QTemporaryDir socketDir;
        QVERIFY(socketDir.isValid());
        const QString socketPath = socketDir.path() + QStringLiteral("/claude.sock");

        QLocalServer server;
        QVERIFY(server.listen(socketPath));

        QLocalSocket client;
        client.connectToServer(socketPath);
        QVERIFY(client.waitForConnected(1000));
        QVERIFY(server.waitForNewConnection(1000));
        std::unique_ptr<QLocalSocket> serverSide(server.nextPendingConnection());
        QVERIFY(serverSide != nullptr);

        const QJsonObject ask{
            {"v", 1},
            {"e", "Ask"},
            {"id", "ask-noack"},
            {"s", "sess-noack"},
            {"a", QString()},
            {"at", QString()},
            {"cwd", "/tmp"},
            {"user", "julius"},
            {"ts", 1000},
            {"questions", QJsonArray{QJsonObject{{"question", "Which?"}}}},
        };

        QElapsedTimer elapsed;
        elapsed.start();
        bool acked = true; // must flip to false
        const QJsonObject reply = ClaudeAsk::negotiate(client, ask, /*ackTimeoutMs=*/200, /*replyTimeoutMs=*/50000, &acked);

        QVERIFY2(elapsed.elapsed() < 2000, "negotiate() must give up at the ack timeout, not wait for the reply timeout");
        QCOMPARE(acked, false);
        QVERIFY(reply.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir temporary;
    qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8());
    qputenv("XDG_DATA_HOME", temporary.path().toUtf8());
    QCoreApplication app(argc, argv);
    app.setApplicationName("atoll-office-test");
    OfficeTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "officetest.moc"
