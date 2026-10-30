/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "office/systemmonitor.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Config;
class QLocalServer;
class QLocalSocket;
class QTimer;

/**
 * Owns the office socket and the model behind `App.office`: every known
 * Claude Code session, the lead and subagents in each, and what they are
 * doing right now.
 *
 * Sessions and agents are created lazily, the first time an event mentions
 * them - a session usually starts before Atoll does. Everything here is
 * driven by `ingest()`; the socket server just turns lines into calls to it.
 */
class OfficeService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.office")

    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool open READ open WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY changed)
    Q_PROPERTY(QString currentSession READ currentSession WRITE selectSession NOTIFY changed)
    Q_PROPERTY(QVariantList agents READ agents NOTIFY changed)
    Q_PROPERTY(QVariantList feed READ feed NOTIFY changed)
    Q_PROPERTY(int working READ working NOTIFY changed)
    Q_PROPERTY(int resting READ resting NOTIFY changed)
    Q_PROPERTY(int waiting READ waiting NOTIFY changed)
    Q_PROPERTY(int toolCalls READ toolCalls NOTIFY changed)
    Q_PROPERTY(qint64 sessionStart READ sessionStart NOTIFY changed)
    /** Open asks, oldest first. See docs/office-v2.md section 3. */
    Q_PROPERTY(QVariantList questions READ questions NOTIFY changed)
    /** The question id the island should bring to front, or empty. */
    Q_PROPERTY(QString focusQuestion READ focusQuestion NOTIFY focusQuestionChanged)
    Q_PROPERTY(SystemMonitor *system READ system CONSTANT)

public:
    explicit OfficeService(Config *config, QObject *parent = nullptr);
    ~OfficeService() override;

    bool active() const;
    bool open() const
    {
        return m_open;
    }
    void setOpen(bool open);
    QVariantList sessions() const;
    QString currentSession() const
    {
        return m_currentSession;
    }
    QVariantList agents() const;
    QVariantList feed() const;
    int working() const;
    int resting() const;
    int waiting() const;
    int toolCalls() const;
    qint64 sessionStart() const;
    QVariantList questions() const;
    QString focusQuestion() const
    {
        return m_focusQuestion;
    }
    SystemMonitor *system() const
    {
        return m_system;
    }

    /**
     * Start listening, if `office.enabled` allows it. Safe to call once.
     * `socketPath`, when given, is used verbatim instead of the computed
     * `$XDG_RUNTIME_DIR/atoll/claude.sock` - the test-only way to point a
     * service at a throwaway socket.
     */
    void start(const QString &socketPath = {});

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void selectSession(const QString &id);
    /** What the socket calls for every line it reads; public for tests. */
    Q_INVOKABLE void ingest(const QJsonObject &event);
    /** Reply to an open ask and drop it. `answers` is `{question text: answer}`. */
    Q_INVOKABLE void answerQuestion(const QString &id, const QVariantMap &answers);
    /** Tell the asking process to fall back to the terminal, and drop the ask. */
    Q_INVOKABLE void cancelQuestion(const QString &id);
    /**
     * One of the scene files bundled with the office art (`manifest.json` or
     * `layout.json` under data/office), parsed; null for anything else. QML's
     * XMLHttpRequest refuses to read resources, hence this.
     */
    Q_INVOKABLE QVariant sceneData(const QString &file) const;

Q_SIGNALS:
    /** Coalesced (at most every 100 ms): something a model property exposes changed. */
    void changed();
    void openChanged();
    void focusQuestionChanged();
    void attention(const QString &session, const QString &agent, const QString &text);

private:
    struct Agent {
        QString key;
        QString name;
        QString type;
        bool lead = false;
        QString state;
        QString activity;
        QString label;
        QString detail;
        int desk = -1;
        int look = 0;
        qint64 since = 0;
        int toolCalls = 0;
        QString waitingFor;
        QVariantList recent;
        qint64 lastEventAt = 0;
        /** Last `pv` seen for this agent: `{kind, title, text, at}`. */
        QVariantMap screen;
        /** A Bash PreToolUse without its Post yet. */
        bool running = false;
        qint64 runningSince = 0;
        /** The claude process that reported this agent's events (event `ppid`). */
        qint64 pid = 0;
    };

    struct Session {
        QString id;
        QString cwd;
        QString user;
        qint64 startedAt = 0;
        qint64 lastAt = 0;
        int toolCalls = 0;
        QHash<QString, Agent> agents;
        QStringList order;
        QVariantList feed;
        QSet<int> usedDesks;
        QHash<QString, int> typeCounters;
        /** The lead's claude process pid, kept as the session's default `focusPid`. */
        qint64 pid = 0;
    };

    /** One open `Ask`: the question text plus the connection waiting on a reply. */
    struct Question {
        QString id;
        QString session;
        QString agentKey;
        QString agentName;
        QString project;
        QJsonArray questions;
        qint64 at = 0;
        QLocalSocket *socket = nullptr;
    };

    Session &sessionFor(const QString &id, const QJsonObject &event);
    Agent &agentFor(Session &session, const QString &key, bool lead, const QString &agentType);
    QString nameForSubagent(Session &session, const QString &agentType) const;
    int allocateDesk(Session &session) const;
    void setAgentState(Agent &agent, const QString &state, const QString &activity);
    void setActivity(Agent &agent, const QString &activity);
    void refreshLabel(Agent &agent) const;
    void pushRecent(Agent &agent, const QString &tool, const QString &arg, qint64 at) const;
    void updateRecentOk(Agent &agent, bool ok) const;
    void appendFeed(Session &session, const QString &from, const QString &to, const QString &text, qint64 at) const;
    QVariantMap agentToVariant(const Agent &agent) const;
    int countState(const QString &state) const;
    void scheduleAgentRemoval(const QString &sessionId, const QString &agentKey);
    void scheduleSessionRemoval(const QString &sessionId);
    void removeSession(const QString &id);
    void scheduleNotify();
    void sweepStale();
    /** Updates `agent.screen` from the event's `pv`, if it carries one. */
    void applyPv(Agent &agent, const QJsonObject &event, qint64 at) const;
    /** Updates `agent.pid`, and `session.pid` when the agent is the lead, from event `ppid`. */
    void updatePid(Session &session, Agent &agent, const QJsonObject &event) const;
    /** Starts or stops the 1 s `busy`-crossing timer depending on whether anything is `running`. */
    void updateBusyTimer();

    void acceptConnection();
    void onReadyRead(QLocalSocket *socket);
    void handleAsk(const QJsonObject &event, QLocalSocket *socket);
    void writeReply(const Question &question, const QJsonObject &reply) const;
    /** Clears a `waitingFor: "user"` left over from an Ask that just got answered, cancelled, or dropped. */
    void clearWaiting(const Question &question);
    /** A client disconnected; if it was holding an open ask, drop it. */
    void dropQuestionFor(QLocalSocket *socket);

    Config *m_config;
    QLocalServer *m_server = nullptr;
    QHash<QLocalSocket *, QByteArray> m_buffers;

    QHash<QString, Session> m_sessions;
    QString m_currentSession;
    bool m_currentPinned = false;
    bool m_open = false;

    QHash<QString, Question> m_questions;
    QString m_focusQuestion;
    SystemMonitor *m_system;

    QTimer *m_notifyTimer;
    QTimer *m_sweepTimer;
    /** Ticks at 1 Hz, only while something is `running`, so `busy` can flip without a new event. */
    QTimer *m_busyTimer;
};
