/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "officeservice.h"

#include "config/config.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
constexpr int NotifyCoalesceMs = 100;
constexpr int SweepIntervalMs = 60 * 1000;
constexpr qint64 SessionTimeoutMs = 45 * 60 * 1000;
constexpr qint64 SubagentTimeoutMs = 20 * 60 * 1000;
constexpr int LeavingRemovalMs = 6 * 1000;
constexpr int MaxDesks = 8;
constexpr int MaxRecent = 5;
constexpr int MaxFeed = 20;
constexpr qint64 LineSizeCap = 64 * 1024;
constexpr qint64 BusyThresholdMs = 3000;
constexpr int BusyTimerIntervalMs = 1000;

/** `general-purpose` -> `General`; `Explore` -> `Explore`. */
QString typeWord(const QString &agentType)
{
    QString base = agentType.section(u'-', 0, 0);
    if (base.isEmpty()) {
        return u"Agent"_s;
    }
    base[0] = base[0].toUpper();
    return base;
}

QString activityFor(const QString &toolName, const QString &arg)
{
    if (toolName == u"Edit"_s || toolName == u"Write"_s || toolName == u"MultiEdit"_s || toolName == u"NotebookEdit"_s) {
        return u"typing"_s;
    }
    if (toolName == u"Bash"_s) {
        return arg.startsWith(u"graphify"_s) ? u"reading"_s : u"terminal"_s;
    }
    if (toolName == u"BashOutput"_s || toolName == u"KillShell"_s || toolName == u"Monitor"_s) {
        return u"terminal"_s;
    }
    if (toolName == u"Read"_s || toolName == u"Grep"_s || toolName == u"Glob"_s || toolName == u"LSP"_s
        || toolName == u"ToolSearch"_s || toolName == u"Skill"_s) {
        return u"reading"_s;
    }
    if (toolName == u"WebFetch"_s || toolName == u"WebSearch"_s) {
        return u"browsing"_s;
    }
    if (toolName.startsWith(u"mcp__"_s)) {
        const QString lower = toolName.toLower();
        return (lower.contains(u"browser"_s) || lower.contains(u"chrome"_s)) ? u"browsing"_s : u"terminal"_s;
    }
    if (toolName == u"Agent"_s || toolName == u"Task"_s) {
        return u"delegating"_s;
    }
    if (toolName == u"SendMessage"_s) {
        return u"talking"_s;
    }
    if (toolName == u"AskUserQuestion"_s) {
        return u"asking"_s;
    }
    if (toolName == u"TaskCreate"_s || toolName == u"TaskUpdate"_s || toolName == u"TaskList"_s || toolName == u"TaskGet"_s
        || toolName == u"TodoWrite"_s || toolName == u"EnterPlanMode"_s || toolName == u"ExitPlanMode"_s) {
        return u"planning"_s;
    }
    return u"thinking"_s;
}

/** `detail` doubles as the SendMessage recipient, so `talking`'s label needs it. */
QString labelFor(const QString &activity, const QString &detail)
{
    if (activity == u"typing"_s) return u"writing code"_s;
    if (activity == u"terminal"_s) return u"running a command"_s;
    if (activity == u"reading"_s) return u"reading files"_s;
    if (activity == u"browsing"_s) return u"on the web"_s;
    if (activity == u"delegating"_s) return u"briefing a teammate"_s;
    if (activity == u"talking"_s) return detail.isEmpty() ? u"messaging"_s : u"messaging %1"_s.arg(detail);
    if (activity == u"asking"_s) return u"has a question"_s;
    if (activity == u"planning"_s) return u"updating the plan"_s;
    if (activity == u"archiving"_s) return u"compacting memory"_s;
    if (activity == u"error"_s) return u"hit an error"_s;
    if (activity == u"idle"_s) return u"on a break"_s;
    return u"thinking"_s;
}
}

OfficeService::OfficeService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_system = new SystemMonitor(this);

    m_notifyTimer = new QTimer(this);
    m_notifyTimer->setSingleShot(true);
    connect(m_notifyTimer, &QTimer::timeout, this, &OfficeService::changed);

    m_sweepTimer = new QTimer(this);
    connect(m_sweepTimer, &QTimer::timeout, this, &OfficeService::sweepStale);
    m_sweepTimer->start(SweepIntervalMs);

    m_busyTimer = new QTimer(this);
    m_busyTimer->setInterval(BusyTimerIntervalMs);
    connect(m_busyTimer, &QTimer::timeout, this, &OfficeService::scheduleNotify);
}

OfficeService::~OfficeService() = default;

void OfficeService::start(const QString &socketPath)
{
    if (m_server) {
        return;
    }
    if (!m_config->value(u"office.enabled"_s, true).toBool()) {
        return;
    }

    QString path = socketPath;
    if (path.isEmpty()) {
        const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (runtimeDir.isEmpty()) {
            return;
        }
        const QString dir = runtimeDir + u"/atoll"_s;
        QDir().mkpath(dir);
        QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        path = dir + u"/claude.sock"_s;
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
    }

    QLocalServer::removeServer(path);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(path)) {
        qWarning("atoll: could not open the office socket at %s (%s)",
                 qUtf8Printable(path),
                 qUtf8Printable(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return;
    }
    connect(m_server, &QLocalServer::newConnection, this, &OfficeService::acceptConnection);
}

bool OfficeService::active() const
{
    return !m_sessions.isEmpty();
}

void OfficeService::setOpen(bool open)
{
    if (m_open == open) {
        return;
    }
    m_open = open;
    Q_EMIT openChanged();
}

void OfficeService::toggle()
{
    setOpen(!m_open);
}

void OfficeService::show()
{
    setOpen(true);
}

void OfficeService::hide()
{
    setOpen(false);
}

QVariant OfficeService::sceneData(const QString &file) const
{
    if (file != u"manifest.json"_s && file != u"layout.json"_s) {
        return {};
    }
    QFile in(u":/qt/qml/Atoll/data/office/"_s + file);
    if (!in.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(in.readAll()).toVariant();
}

void OfficeService::selectSession(const QString &id)
{
    if (!m_sessions.contains(id)) {
        return;
    }
    m_currentSession = id;
    m_currentPinned = true;
    scheduleNotify();
}

QVariantList OfficeService::sessions() const
{
    QList<const Session *> ordered;
    ordered.reserve(m_sessions.size());
    for (const Session &session : m_sessions) {
        ordered.append(&session);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Session *a, const Session *b) {
        return a->lastAt > b->lastAt;
    });

    QVariantList result;
    result.reserve(ordered.size());
    for (const Session *session : ordered) {
        bool waitingOnUser = false;
        for (const Agent &agent : session->agents) {
            if (agent.state == u"waiting"_s) {
                waitingOnUser = true;
                break;
            }
        }
        result.append(QVariantMap{
            {u"id"_s, session->id},
            {u"project"_s, QFileInfo(session->cwd).fileName()},
            {u"cwd"_s, session->cwd},
            {u"user"_s, session->user},
            {u"startedAt"_s, session->startedAt},
            {u"lastAt"_s, session->lastAt},
            {u"toolCalls"_s, session->toolCalls},
            {u"waiting"_s, waitingOnUser},
            {u"pid"_s, session->pid},
        });
    }
    return result;
}

QVariantList OfficeService::agents() const
{
    const auto it = m_sessions.constFind(m_currentSession);
    if (it == m_sessions.cend()) {
        return {};
    }
    QVariantList result;
    result.reserve(it->order.size());
    for (const QString &key : it->order) {
        result.append(agentToVariant(it->agents.value(key)));
    }
    return result;
}

QVariantList OfficeService::feed() const
{
    const auto it = m_sessions.constFind(m_currentSession);
    return it == m_sessions.cend() ? QVariantList() : it->feed;
}

int OfficeService::countState(const QString &state) const
{
    const auto it = m_sessions.constFind(m_currentSession);
    if (it == m_sessions.cend()) {
        return 0;
    }
    int count = 0;
    for (const Agent &agent : it->agents) {
        if (agent.state == state) {
            ++count;
        }
    }
    return count;
}

int OfficeService::working() const
{
    return countState(u"working"_s);
}

int OfficeService::resting() const
{
    return countState(u"idle"_s);
}

int OfficeService::waiting() const
{
    return countState(u"waiting"_s);
}

int OfficeService::toolCalls() const
{
    const auto it = m_sessions.constFind(m_currentSession);
    return it == m_sessions.cend() ? 0 : it->toolCalls;
}

qint64 OfficeService::sessionStart() const
{
    const auto it = m_sessions.constFind(m_currentSession);
    return it == m_sessions.cend() ? 0 : it->startedAt;
}

OfficeService::Session &OfficeService::sessionFor(const QString &id, const QJsonObject &event)
{
    const auto it = m_sessions.find(id);
    if (it != m_sessions.end()) {
        return it.value();
    }
    Session session;
    session.id = id;
    session.cwd = event.value(u"cwd"_s).toString();
    session.user = event.value(u"user"_s).toString();
    const qint64 ts = event.value(u"ts"_s).toVariant().toLongLong();
    session.startedAt = ts != 0 ? ts : QDateTime::currentMSecsSinceEpoch();
    session.lastAt = session.startedAt;
    return m_sessions.insert(id, session).value();
}

QString OfficeService::nameForSubagent(Session &session, const QString &agentType) const
{
    const QString word = typeWord(agentType);
    int &counter = session.typeCounters[word];
    ++counter;
    return u"%1 %2"_s.arg(word).arg(counter);
}

int OfficeService::allocateDesk(Session &session) const
{
    for (int i = 0; i < MaxDesks; ++i) {
        if (!session.usedDesks.contains(i)) {
            session.usedDesks.insert(i);
            return i;
        }
    }
    return -2;
}

OfficeService::Agent &OfficeService::agentFor(Session &session, const QString &key, bool lead, const QString &agentType)
{
    const auto it = session.agents.find(key);
    if (it != session.agents.end()) {
        return it.value();
    }

    Agent agent;
    agent.key = key;
    agent.lead = lead;
    agent.type = agentType;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    agent.since = now;
    agent.lastEventAt = now;
    agent.state = u"idle"_s;
    agent.activity = u"idle"_s;

    if (lead) {
        agent.name = u"Lead"_s;
        agent.desk = -1;
        agent.look = 0;
    } else {
        agent.name = nameForSubagent(session, agentType);
        agent.desk = allocateDesk(session);
        agent.look = 1 + int(qHash(key) % 7);
    }
    refreshLabel(agent);

    session.order.append(key);
    return session.agents.insert(key, agent).value();
}

void OfficeService::setAgentState(Agent &agent, const QString &state, const QString &activity)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (agent.state != state) {
        agent.since = now;
    }
    agent.state = state;
    agent.activity = activity;
    agent.lastEventAt = now;
}

void OfficeService::setActivity(Agent &agent, const QString &activity)
{
    agent.activity = activity;
    agent.lastEventAt = QDateTime::currentMSecsSinceEpoch();
}

void OfficeService::refreshLabel(Agent &agent) const
{
    // A permission wait looks like any other wait except for its label - an
    // AskUserQuestion wait already carries its own ("has a question").
    if (agent.state == u"waiting"_s && agent.waitingFor == u"user"_s && agent.activity != u"asking"_s) {
        agent.label = u"needs permission"_s;
    } else {
        agent.label = labelFor(agent.activity, agent.detail);
    }
}

void OfficeService::pushRecent(Agent &agent, const QString &tool, const QString &arg, qint64 at) const
{
    agent.recent.prepend(QVariantMap{{u"t"_s, tool}, {u"arg"_s, arg}, {u"ok"_s, true}, {u"at"_s, at}});
    while (agent.recent.size() > MaxRecent) {
        agent.recent.removeLast();
    }
}

void OfficeService::updateRecentOk(Agent &agent, bool ok) const
{
    if (agent.recent.isEmpty()) {
        return;
    }
    QVariantMap entry = agent.recent.first().toMap();
    entry[u"ok"_s] = ok;
    agent.recent[0] = entry;
}

void OfficeService::appendFeed(Session &session, const QString &from, const QString &to, const QString &text, qint64 at) const
{
    session.feed.prepend(QVariantMap{{u"from"_s, from}, {u"to"_s, to}, {u"text"_s, text}, {u"at"_s, at}});
    while (session.feed.size() > MaxFeed) {
        session.feed.removeLast();
    }
}

QVariantMap OfficeService::agentToVariant(const Agent &agent) const
{
    const bool busy = agent.running && (QDateTime::currentMSecsSinceEpoch() - agent.runningSince) > BusyThresholdMs;
    return QVariantMap{
        {u"key"_s, agent.key},
        {u"name"_s, agent.name},
        {u"type"_s, agent.type},
        {u"lead"_s, agent.lead},
        {u"state"_s, agent.state},
        {u"activity"_s, agent.activity},
        {u"label"_s, agent.label},
        {u"detail"_s, agent.detail},
        {u"desk"_s, agent.desk},
        {u"look"_s, agent.look},
        {u"since"_s, agent.since},
        {u"toolCalls"_s, agent.toolCalls},
        {u"waitingFor"_s, agent.waitingFor},
        {u"recent"_s, agent.recent},
        {u"screen"_s, agent.screen},
        {u"running"_s, agent.running},
        {u"runningSince"_s, agent.runningSince},
        {u"busy"_s, busy},
        {u"pid"_s, agent.pid},
    };
}

QVariantList OfficeService::questions() const
{
    QList<const Question *> ordered;
    ordered.reserve(m_questions.size());
    for (const Question &question : m_questions) {
        ordered.append(&question);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Question *a, const Question *b) {
        return a->at < b->at;
    });

    QVariantList result;
    result.reserve(ordered.size());
    for (const Question *question : ordered) {
        result.append(QVariantMap{
            {u"id"_s, question->id},
            {u"session"_s, question->session},
            {u"agentKey"_s, question->agentKey},
            {u"agentName"_s, question->agentName},
            {u"project"_s, question->project},
            {u"questions"_s, question->questions.toVariantList()},
            {u"at"_s, question->at},
        });
    }
    return result;
}

void OfficeService::applyPv(Agent &agent, const QJsonObject &event, qint64 at) const
{
    const QJsonValue pv = event.value(u"pv"_s);
    if (!pv.isObject()) {
        return;
    }
    const QJsonObject pvObject = pv.toObject();
    agent.screen = QVariantMap{
        {u"kind"_s, pvObject.value(u"kind"_s).toString()},
        {u"title"_s, pvObject.value(u"title"_s).toString()},
        {u"text"_s, pvObject.value(u"text"_s).toString()},
        {u"at"_s, at},
    };
}

void OfficeService::updatePid(Session &session, Agent &agent, const QJsonObject &event) const
{
    const qint64 ppid = event.value(u"ppid"_s).toVariant().toLongLong();
    if (ppid <= 0) {
        return;
    }
    agent.pid = ppid;
    if (agent.lead) {
        session.pid = ppid;
    }
}

void OfficeService::updateBusyTimer()
{
    bool anyRunning = false;
    for (const Session &session : m_sessions) {
        for (const Agent &agent : session.agents) {
            if (agent.running) {
                anyRunning = true;
                break;
            }
        }
        if (anyRunning) {
            break;
        }
    }
    if (anyRunning && !m_busyTimer->isActive()) {
        m_busyTimer->start();
    } else if (!anyRunning && m_busyTimer->isActive()) {
        m_busyTimer->stop();
    }
}

void OfficeService::ingest(const QJsonObject &event)
{
    const QString eventName = event.value(u"e"_s).toString();
    const QString sessionId = event.value(u"s"_s).toString();
    if (eventName.isEmpty() || sessionId.isEmpty()) {
        return;
    }

    Session &session = sessionFor(sessionId, event);
    const qint64 ts = event.value(u"ts"_s).toVariant().toLongLong();
    session.lastAt = ts != 0 ? ts : QDateTime::currentMSecsSinceEpoch();

    const QString agentId = event.value(u"a"_s).toString();
    const QString agentType = event.value(u"at"_s).toString();
    const QString toolName = event.value(u"t"_s).toString();
    const QString arg = event.value(u"arg"_s).toString();
    const bool isLead = agentId.isEmpty();
    const QString agentKey = isLead ? u"lead"_s : agentId;

    if (eventName == u"SessionStart"_s) {
        Agent &lead = agentFor(session, u"lead"_s, true, QString());
        lead.waitingFor.clear();
        setAgentState(lead, u"idle"_s, u"idle"_s);
        refreshLabel(lead);
        updatePid(session, lead, event);
    } else if (eventName == u"UserPromptSubmit"_s) {
        Agent &lead = agentFor(session, u"lead"_s, true, QString());
        setAgentState(lead, u"working"_s, u"thinking"_s);
        refreshLabel(lead);
        updatePid(session, lead, event);
    } else if (eventName == u"PreToolUse"_s) {
        Agent &agent = agentFor(session, agentKey, isLead, agentType);
        agent.detail = arg;
        ++agent.toolCalls;
        ++session.toolCalls;
        pushRecent(agent, toolName, arg, session.lastAt);
        applyPv(agent, event, session.lastAt);
        updatePid(session, agent, event);
        if (toolName == u"Bash"_s) {
            agent.running = true;
            // Real wall-clock time, not the event's `ts`: `busy` compares this
            // against `QDateTime::currentMSecsSinceEpoch()`, the same way
            // `since`/`lastEventAt` already do.
            agent.runningSince = QDateTime::currentMSecsSinceEpoch();
        }

        const QString activity = activityFor(toolName, arg);
        if (toolName == u"AskUserQuestion"_s) {
            setAgentState(agent, u"waiting"_s, activity);
            agent.waitingFor = u"user"_s;
            Q_EMIT attention(sessionId, agentKey, event.value(u"msg"_s).toString());
        } else {
            setAgentState(agent, u"working"_s, activity);
        }
        refreshLabel(agent);

        if (toolName == u"SendMessage"_s) {
            appendFeed(session, agent.name, event.value(u"to"_s).toString(), event.value(u"msg"_s).toString(), session.lastAt);
        }
    } else if (eventName == u"PostToolUse"_s || eventName == u"PostToolUseFailure"_s) {
        Agent &agent = agentFor(session, agentKey, isLead, agentType);
        const bool ok = eventName == u"PostToolUseFailure"_s ? false : event.value(u"ok"_s).toBool(true);
        agent.waitingFor.clear();
        applyPv(agent, event, session.lastAt);
        updatePid(session, agent, event);
        if (toolName == u"Bash"_s) {
            agent.running = false;
        }
        setAgentState(agent, u"working"_s, ok ? u"thinking"_s : u"error"_s);
        refreshLabel(agent);
        updateRecentOk(agent, ok);
    } else if (eventName == u"PermissionRequest"_s) {
        Agent &agent = agentFor(session, agentKey, isLead, agentType);
        setAgentState(agent, u"waiting"_s, agent.activity);
        agent.waitingFor = u"user"_s;
        refreshLabel(agent);
        updatePid(session, agent, event);
        Q_EMIT attention(sessionId, agentKey, event.value(u"msg"_s).toString());
    } else if (eventName == u"Notification"_s) {
        const QString nt = event.value(u"nt"_s).toString();
        if (nt == u"permission_prompt"_s) {
            Agent &lead = agentFor(session, u"lead"_s, true, QString());
            setAgentState(lead, u"waiting"_s, lead.activity);
            lead.waitingFor = u"user"_s;
            refreshLabel(lead);
            Q_EMIT attention(sessionId, u"lead"_s, event.value(u"msg"_s).toString());
        } else if (nt == u"idle_prompt"_s) {
            Agent &lead = agentFor(session, u"lead"_s, true, QString());
            lead.waitingFor.clear();
            setAgentState(lead, u"idle"_s, u"idle"_s);
            refreshLabel(lead);
        } else {
            Q_EMIT attention(sessionId, agentKey, event.value(u"msg"_s).toString());
        }
    } else if (eventName == u"PreCompact"_s) {
        Agent &lead = agentFor(session, u"lead"_s, true, QString());
        setActivity(lead, u"archiving"_s);
        refreshLabel(lead);
    } else if (eventName == u"Stop"_s) {
        Agent &lead = agentFor(session, u"lead"_s, true, QString());
        lead.waitingFor.clear();
        setAgentState(lead, u"idle"_s, u"idle"_s);
        refreshLabel(lead);
    } else if (eventName == u"SubagentStart"_s) {
        Agent &agent = agentFor(session, agentKey, false, agentType);
        setAgentState(agent, u"working"_s, u"thinking"_s);
        refreshLabel(agent);
        updatePid(session, agent, event);
    } else if (eventName == u"SubagentStop"_s) {
        Agent &agent = agentFor(session, agentKey, false, agentType);
        setAgentState(agent, u"leaving"_s, agent.activity);
        refreshLabel(agent);
        scheduleAgentRemoval(sessionId, agentKey);
    } else if (eventName == u"SessionEnd"_s) {
        for (auto it = session.agents.begin(); it != session.agents.end(); ++it) {
            setAgentState(it.value(), u"leaving"_s, it.value().activity);
            refreshLabel(it.value());
        }
        scheduleSessionRemoval(sessionId);
    }

    if (!m_currentPinned) {
        m_currentSession = sessionId;
    }
    updateBusyTimer();
    scheduleNotify();
}

void OfficeService::scheduleAgentRemoval(const QString &sessionId, const QString &agentKey)
{
    QTimer::singleShot(LeavingRemovalMs, this, [this, sessionId, agentKey] {
        const auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) {
            return;
        }
        const Agent agent = it.value().agents.value(agentKey);
        if (agent.desk >= 0) {
            it.value().usedDesks.remove(agent.desk);
        }
        it.value().agents.remove(agentKey);
        it.value().order.removeAll(agentKey);
        scheduleNotify();
    });
}

void OfficeService::scheduleSessionRemoval(const QString &sessionId)
{
    QTimer::singleShot(LeavingRemovalMs, this, [this, sessionId] {
        removeSession(sessionId);
    });
}

void OfficeService::removeSession(const QString &id)
{
    if (!m_sessions.remove(id)) {
        return;
    }
    if (m_currentSession == id) {
        m_currentPinned = false;
        m_currentSession.clear();
        qint64 latestAt = -1;
        for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
            if (it.value().lastAt > latestAt) {
                latestAt = it.value().lastAt;
                m_currentSession = it.key();
            }
        }
    }
    scheduleNotify();
}

void OfficeService::scheduleNotify()
{
    if (!m_notifyTimer->isActive()) {
        m_notifyTimer->start(NotifyCoalesceMs);
    }
}

void OfficeService::sweepStale()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList expiredSessions;

    for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
        Session &session = sit.value();
        if (now - session.lastAt > SessionTimeoutMs) {
            expiredSessions.append(sit.key());
            continue;
        }
        for (auto ait = session.agents.begin(); ait != session.agents.end(); ++ait) {
            Agent &agent = ait.value();
            if (!agent.lead && agent.state != u"leaving"_s && now - agent.lastEventAt > SubagentTimeoutMs) {
                setAgentState(agent, u"leaving"_s, agent.activity);
                refreshLabel(agent);
                scheduleAgentRemoval(sit.key(), agent.key);
            }
        }
    }

    for (const QString &id : expiredSessions) {
        removeSession(id);
    }
    scheduleNotify();
}

void OfficeService::acceptConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            onReadyRead(socket);
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            m_buffers.remove(socket);
            dropQuestionFor(socket);
            socket->deleteLater();
        });
    }
}

void OfficeService::onReadyRead(QLocalSocket *socket)
{
    const auto it = m_buffers.find(socket);
    if (it == m_buffers.end()) {
        return;
    }
    QByteArray &buffer = it.value();
    buffer += socket->readAll();

    int newline;
    while ((newline = buffer.indexOf('\n')) != -1) {
        const QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        if (line.size() > LineSizeCap) {
            continue;
        }
        const QJsonObject event = QJsonDocument::fromJson(line).object();
        if (event.isEmpty()) {
            continue;
        }
        if (event.value(u"e"_s).toString() == u"Ask"_s) {
            // Unlike every other event, an Ask's connection stays open: the
            // reply is written straight back down this same socket by
            // answerQuestion()/cancelQuestion(), not closed here.
            handleAsk(event, socket);
        } else {
            ingest(event);
        }
    }
    // A line that never ends would otherwise grow the buffer forever.
    if (buffer.size() > LineSizeCap) {
        buffer.clear();
    }
}

void OfficeService::handleAsk(const QJsonObject &event, QLocalSocket *socket)
{
    const QString id = event.value(u"id"_s).toString();
    const QString sessionId = event.value(u"s"_s).toString();
    if (id.isEmpty() || sessionId.isEmpty()) {
        return;
    }

    Session &session = sessionFor(sessionId, event);
    const QString agentId = event.value(u"a"_s).toString();
    const QString agentType = event.value(u"at"_s).toString();
    const bool isLead = agentId.isEmpty();
    const QString agentKey = isLead ? u"lead"_s : agentId;
    Agent &agent = agentFor(session, agentKey, isLead, agentType);
    updatePid(session, agent, event);

    Question question;
    question.id = id;
    question.session = sessionId;
    question.agentKey = agentKey;
    question.agentName = agent.name;
    question.project = QFileInfo(session.cwd).fileName();
    question.questions = event.value(u"questions"_s).toArray();
    const qint64 ts = event.value(u"ts"_s).toVariant().toLongLong();
    question.at = ts != 0 ? ts : QDateTime::currentMSecsSinceEpoch();
    question.socket = socket;
    m_questions.insert(id, question);

    // Ack immediately, before anything else: it is the only proof the
    // caller has that this island understands "Ask" at all, rather than
    // leaving it blocked for up to 590 s against one that doesn't (a v1
    // build, say).
    QJsonObject ack;
    ack[u"id"_s] = id;
    ack[u"ack"_s] = true;
    socket->write(QJsonDocument(ack).toJson(QJsonDocument::Compact) + '\n');
    socket->flush();

    agent.waitingFor = u"user"_s;
    setAgentState(agent, u"waiting"_s, u"asking"_s);
    refreshLabel(agent);

    session.lastAt = question.at;
    if (!m_currentPinned) {
        m_currentSession = sessionId;
    }

    QString text;
    if (!question.questions.isEmpty()) {
        text = question.questions.first().toObject().value(u"question"_s).toString();
    }
    Q_EMIT attention(sessionId, agentKey, text);

    if (m_config->value(u"office.askInIsland"_s, true).toBool()) {
        m_focusQuestion = id;
        Q_EMIT focusQuestionChanged();
        show();
    }

    scheduleNotify();
}

void OfficeService::writeReply(const Question &question, const QJsonObject &reply) const
{
    if (!question.socket) {
        return;
    }
    const QByteArray line = QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n';
    question.socket->write(line);
    question.socket->flush();
}

void OfficeService::clearWaiting(const Question &question)
{
    const auto sessionIt = m_sessions.find(question.session);
    if (sessionIt == m_sessions.end()) {
        return;
    }
    const auto agentIt = sessionIt.value().agents.find(question.agentKey);
    if (agentIt == sessionIt.value().agents.end()) {
        return;
    }
    Agent &agent = agentIt.value();
    if (agent.waitingFor == u"user"_s) {
        agent.waitingFor.clear();
        setAgentState(agent, u"working"_s, u"thinking"_s);
        refreshLabel(agent);
    }
}

void OfficeService::dropQuestionFor(QLocalSocket *socket)
{
    for (auto it = m_questions.begin(); it != m_questions.end(); ++it) {
        if (it.value().socket != socket) {
            continue;
        }
        const QString id = it.key();
        clearWaiting(it.value());
        m_questions.erase(it);
        if (m_focusQuestion == id) {
            m_focusQuestion.clear();
            Q_EMIT focusQuestionChanged();
        }
        scheduleNotify();
        return;
    }
}

void OfficeService::answerQuestion(const QString &id, const QVariantMap &answers)
{
    const auto it = m_questions.find(id);
    if (it == m_questions.end()) {
        return;
    }
    QJsonObject reply;
    reply[u"id"_s] = id;
    reply[u"answers"_s] = QJsonObject::fromVariantMap(answers);
    writeReply(it.value(), reply);
    clearWaiting(it.value());

    QLocalSocket *socket = it.value().socket;
    m_questions.erase(it);
    if (m_focusQuestion == id) {
        m_focusQuestion.clear();
        Q_EMIT focusQuestionChanged();
    }
    if (socket) {
        socket->disconnectFromServer();
    }
    scheduleNotify();
}

void OfficeService::cancelQuestion(const QString &id)
{
    const auto it = m_questions.find(id);
    if (it == m_questions.end()) {
        return;
    }
    QJsonObject reply;
    reply[u"id"_s] = id;
    reply[u"cancel"_s] = true;
    writeReply(it.value(), reply);
    clearWaiting(it.value());

    QLocalSocket *socket = it.value().socket;
    m_questions.erase(it);
    if (m_focusQuestion == id) {
        m_focusQuestion.clear();
        Q_EMIT focusQuestionChanged();
    }
    if (socket) {
        socket->disconnectFromServer();
    }
    scheduleNotify();
}
