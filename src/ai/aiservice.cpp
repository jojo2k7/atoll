/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aiservice.h"

#include "ai/aitools.h"
#include "ai/anthropicprovider.h"
#include "ai/claudecliprovider.h"
#include "ai/credentialstore.h"
#include "ai/geminiprovider.h"
#include "ai/openrouterprovider.h"
#include "ai/permissionbroker.h"
#include "ai/screencapture.h"
#include "config/config.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
/**
 * How many times the model may ask for tools before Atoll stops it.
 *
 * A model that has misread the situation will happily try the same thing until
 * something gives, and on a desktop the thing that gives is the user's
 * patience or their package database. The ceiling is high enough for a real
 * job - install, configure, verify - and low enough to be noticed.
 */
constexpr int MaxToolRounds = 24;
}

AiService::AiService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_network(new QNetworkAccessManager(this))
    , m_credentials(new CredentialStore(this))
    , m_broker(new PermissionBroker(config, this))
    , m_toolbox(new AiToolbox(config, this))
{
    m_anthropic = new AnthropicProvider(m_network, this);
    m_gemini = new GeminiProvider(m_network, this);
    m_openrouter = new OpenRouterProvider(m_network, this);
    m_cli = new ClaudeCliProvider(this);

    for (AiBackend *provider : {static_cast<AiBackend *>(m_anthropic),
                                static_cast<AiBackend *>(m_gemini),
                                static_cast<AiBackend *>(m_openrouter),
                                static_cast<AiBackend *>(m_cli)}) {
        connect(provider, &AiBackend::textDelta, this, [this](const QString &text) {
            if (m_testing) {
                return;
            }
            if (m_answer.isEmpty() && !text.trimmed().isEmpty()) {
                setState(u"answering"_s);
            }
            m_answer.append(text);
            Q_EMIT answerChanged();
        });
        connect(provider, &AiBackend::thoughtDelta, this, [this](const QString &text) {
            if (m_testing) {
                return;
            }
            // Only the tail is ever shown, so there is no point keeping more.
            m_thought.append(text);
            if (m_thought.size() > 600) {
                m_thought = m_thought.right(600);
            }
            Q_EMIT answerChanged();
        });
        connect(provider, &AiBackend::turnEnded, this, &AiService::onTurnEnded);
        connect(provider, &AiBackend::failed, this, [this](const QString &reason) {
            if (qEnvironmentVariableIntValue("ATOLL_DEBUG_AI") > 0) {
                qWarning("atoll: ai backend failed: %s", qUtf8Printable(reason));
            }
            if (m_testing) {
                m_testing = false;
                m_keyTest = reason;
                Q_EMIT keyTestChanged();
                return;
            }
            fail(reason);
        });
    }

    // A backend that runs its own tools reports them rather than handing them
    // over, so the island's list of steps is filled in from what it says.
    connect(m_cli, &AiBackend::toolStarted, this, [this](const QString &id, const QString &summary) {
        addStep(u"tool"_s, summary, u"running"_s, id);
        setActivity(summary);
        setState(u"working"_s);
    });
    connect(m_cli, &AiBackend::toolFinished, this, [this](const QString &id, bool ok, const QString &) {
        updateStepById(id, ok ? u"done"_s : u"failed"_s);
        setActivity({});
    });
    connect(m_cli, &ClaudeCliProvider::statusChanged, this, [this] {
        if (m_cliTesting) {
            m_cliTesting = false;
            m_keyTest = cliDetail();
            Q_EMIT keyTestChanged();
        }
        Q_EMIT cliChanged();
        Q_EMIT configurationChanged();
    });

    connect(m_toolbox, &AiToolbox::completed, this, &AiService::onToolResult);
    connect(m_toolbox, &AiToolbox::progress, this, [this](const QString &line) {
        if (!line.isEmpty()) {
            setActivity(line);
        }
    });
    connect(m_toolbox, &AiToolbox::messageRequested, this, &AiService::messageRequested);

    connect(m_config, &Config::changed, this, &AiService::configurationChanged);

    // Where the "which screen" question starts from. It is a starting point
    // rather than the answer: the user's own answer, given on the island,
    // holds for the conversation it was given in and no longer.
    m_screenChoice = m_config->value(u"ai.screen"_s, u"ask"_s).toString();

    // Plugging a monitor in changes what "which screen" can be answered with,
    // and the question may be on the island while it happens.
    if (auto *gui = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(gui, &QGuiApplication::screenAdded, this, &AiService::screensChanged);
        connect(gui, &QGuiApplication::screenRemoved, this, &AiService::screensChanged);
        connect(gui, &QGuiApplication::primaryScreenChanged, this, &AiService::screensChanged);
    }
}

AiService::~AiService() = default;

// ---- configuration -------------------------------------------------------

QString AiService::provider() const
{
    const QString configured = m_config->value(u"ai.provider"_s, u"claude-cli"_s).toString();
    if (configured == u"gemini"_s || configured == u"anthropic"_s || configured == u"openrouter"_s) {
        return configured;
    }
    return u"claude-cli"_s;
}

QString AiService::providerLabel() const
{
    const QString name = provider();
    if (name == u"gemini"_s) {
        return u"Gemini"_s;
    }
    if (name == u"openrouter"_s) {
        return u"OpenRouter"_s;
    }
    return u"Claude"_s;
}

AiBackend *AiService::activeBackend() const
{
    const QString name = provider();
    if (name == u"gemini"_s) {
        return m_gemini;
    }
    if (name == u"anthropic"_s) {
        return m_anthropic;
    }
    if (name == u"openrouter"_s) {
        return m_openrouter;
    }
    return m_cli;
}

QString AiService::model() const
{
    const QString configured = m_config->value(u"ai.model"_s, QString()).toString();
    return configured.isEmpty() ? activeBackend()->defaultModel() : configured;
}

bool AiService::configured() const
{
    if (!m_config->value(u"ai.enabled"_s, true).toBool()) {
        return false;
    }
    if (provider() == u"claude-cli"_s) {
        // Whether the client is signed in is a question that costs a process
        // to answer, so it is not asked here. A client that is installed is
        // treated as usable, and a login that turns out to be missing is
        // reported the moment something is actually asked of it.
        return m_cli->status().installed;
    }
    return m_credentials->hasKey(provider());
}

bool AiService::hasKeyFor(const QString &name) const
{
    return m_credentials->hasKey(name);
}

QString AiService::keyBackendFor(const QString &name) const
{
    return m_credentials->backendFor(name);
}

void AiService::setKeyFor(const QString &name, const QString &key)
{
    m_credentials->setKey(name, key.trimmed());
    m_keyTest.clear();
    Q_EMIT keyTestChanged();
    Q_EMIT configurationChanged();
}

void AiService::testKey()
{
    if (provider() == u"claude-cli"_s) {
        m_cliTesting = true;
        m_keyTest = tr("Checking…");
        Q_EMIT keyTestChanged();
        m_cli->refreshStatus();
        return;
    }

    auto *target = qobject_cast<AiProvider *>(activeBackend());
    target->setApiKey(m_credentials->key(provider()));

    m_testing = true;
    m_keyTest = tr("Checking…");
    Q_EMIT keyTestChanged();

    AiRequest request;
    request.model = model();
    request.baseUrl = m_config->value(u"ai.baseUrl"_s, QString()).toString();
    request.maxTokens = 64;
    request.systemPrompt = u"Reply with the single word: ready."_s;
    AiTurn turn;
    turn.role = u"user"_s;
    turn.text = u"Are you there?"_s;
    request.history.append(turn);
    target->send(request);
}

// ---- the command-line client ---------------------------------------------

QString AiService::cliState() const
{
    const CliStatus status = m_cli->status();
    if (!status.installed) {
        return u"missing"_s;
    }
    if (!status.loggedIn) {
        // Nothing has been asked yet, so "not signed in" would be a guess.
        return status.error.isEmpty() && status.method.isEmpty() ? u"checking"_s : u"signed-out"_s;
    }
    return u"ready"_s;
}

QString AiService::cliDetail() const
{
    const CliStatus status = m_cli->status();
    if (!status.installed) {
        return tr("The Claude Code client is not installed yet.");
    }
    if (!status.error.isEmpty()) {
        return status.error;
    }
    if (!status.loggedIn) {
        return cliState() == u"checking"_s
            ? tr("Found at %1. Check the sign-in to be sure it can answer.").arg(status.path)
            : tr("Found at %1, but nobody is signed in.").arg(status.path);
    }
    if (!status.account.isEmpty() && !status.plan.isEmpty()) {
        return tr("Signed in as %1, on a %2 plan.").arg(status.account, status.plan);
    }
    if (!status.account.isEmpty()) {
        return tr("Signed in as %1.").arg(status.account);
    }
    return tr("Signed in and ready.");
}

void AiService::refreshCli()
{
    m_cli->setExecutablePath(m_config->value(u"ai.cliPath"_s, QString()).toString());
    m_cli->refreshStatus();
    Q_EMIT cliChanged();
}

QString AiService::cliInstallCommand() const
{
    return u"curl -fsSL https://claude.ai/install.sh | bash"_s;
}

bool AiService::signInToCli()
{
    const CliStatus status = m_cli->status();
    const QString client = status.installed ? status.path : u"claude"_s;
    // Signing in is a conversation with a browser and a code to paste back, so
    // it belongs in a terminal the user can see and type into - not in a
    // process Atoll started behind them.
    const QString command = u"%1 auth login; echo; echo 'You can close this window.'; read -r _"_s
                                .arg(client);

    struct Terminal {
        const char *program;
        QStringList before;
    };
    // xdg-terminal-exec first: it opens whatever this desktop calls its
    // terminal, which on a KDE install is the one the user already knows.
    static const QList<Terminal> terminals = {
        {"xdg-terminal-exec", {}},
        {"konsole", {u"-e"_s}},
        {"ptyxis", {u"--"_s}},
        {"gnome-terminal", {u"--"_s}},
        {"alacritty", {u"-e"_s}},
        {"kitty", {}},
        {"foot", {}},
        {"xterm", {u"-e"_s}},
    };

    for (const Terminal &terminal : terminals) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(terminal.program));
        if (path.isEmpty()) {
            continue;
        }
        QStringList arguments = terminal.before;
        arguments << u"sh"_s << u"-c"_s << command;
        if (QProcess::startDetached(path, arguments)) {
            return true;
        }
    }
    return false;
}

bool AiService::screenAvailable() const
{
    return ScreenCapture::available();
}

void AiService::setShareScreen(bool share)
{
    if (m_shareScreen == share) {
        return;
    }
    m_shareScreen = share;
    Q_EMIT shareScreenChanged();
}

QVariantList AiService::screens() const
{
    return ScreenCapture::outputs();
}

bool AiService::severalScreens() const
{
    return ScreenCapture::hasSeveralOutputs();
}

QString AiService::screenChoice() const
{
    return m_screenChoice;
}

void AiService::setScreenChoice(const QString &choice)
{
    const QString wanted = choice.isEmpty() ? u"ask"_s : choice;
    if (m_screenChoice == wanted) {
        return;
    }
    m_screenChoice = wanted;
    Q_EMIT screensChanged();
}

QString AiService::screenSummary() const
{
    const QVariantList outputs = ScreenCapture::outputs();
    if (outputs.size() < 2) {
        return {};
    }
    QStringList lines;
    for (const QVariant &entry : outputs) {
        const QVariantMap output = entry.toMap();
        lines.append(u"%1 (%2x%3)"_s.arg(output.value(u"name"_s).toString())
                         .arg(output.value(u"width"_s).toInt())
                         .arg(output.value(u"height"_s).toInt()));
    }
    return lines.join(u", "_s);
}

QVariantList AiService::screenOptions() const
{
    QVariantList options;
    const QVariantList outputs = ScreenCapture::outputs();
    const QString current = ScreenCapture::currentOutputName();
    for (const QVariant &entry : outputs) {
        const QVariantMap output = entry.toMap();
        const QString name = output.value(u"name"_s).toString();
        options.append(QVariantMap{
            {u"id"_s, name},
            // The one the pointer is on is almost always the one meant, so it
            // is the one that looks like the answer.
            {u"label"_s, name == current ? tr("This screen") : name},
            {u"detail"_s, ScreenCapture::labelFor(name)},
            {u"accented"_s, name == current},
        });
    }
    // Last, and deliberately unremarkable: a picture of four monitors at once
    // is a picture in which nothing can be read.
    options.append(QVariantMap{
        {u"id"_s, u"all"_s},
        {u"label"_s, tr("All of them")},
        {u"detail"_s, tr("One wide picture, so everything on it is smaller.")},
    });
    return options;
}

QString AiService::resolveScreen(const QString &choice) const
{
    if (choice.compare(u"current"_s, Qt::CaseInsensitive) == 0) {
        const QString current = ScreenCapture::currentOutputName();
        return current.isEmpty() ? u"all"_s : current;
    }
    if (ScreenCapture::hasOutput(choice)) {
        return choice;
    }
    return u"all"_s;
}

void AiService::withChosenScreen(const QString &requested, std::function<void(const QString &)> then)
{
    QString wanted = requested.trimmed();
    if (wanted.isEmpty()) {
        wanted = m_screenChoice;
    }
    if (wanted.isEmpty()) {
        wanted = u"ask"_s;
    }

    const bool asking = wanted.compare(u"ask"_s, Qt::CaseInsensitive) == 0;
    if (!asking || !ScreenCapture::hasSeveralOutputs()) {
        then(resolveScreen(asking ? u"all"_s : wanted));
        return;
    }

    askChoice(tr("Which screen shall I look at?"), screenOptions(), [this, then](const QString &id) {
        if (id.isEmpty()) {
            then(QString{});
            return;
        }
        // The answer holds for the rest of the conversation: somebody who has
        // said "the right-hand one" once is not asking to be asked again two
        // questions later.
        setScreenChoice(id);
        then(resolveScreen(id));
    });
}

void AiService::takeScreenshot(const QString &screenName,
                               std::function<void(const QByteArray &)> onImage,
                               std::function<void(const QString &)> onError)
{
    auto *capture = new ScreenCapture(this);
    capture->setMaxEdge(m_config->value(u"ai.screenshotMaxEdge"_s, 1568).toInt());
    connect(capture, &ScreenCapture::captured, this, [capture, onImage](const QByteArray &png) {
        capture->deleteLater();
        onImage(png);
    });
    connect(capture, &ScreenCapture::failed, this, [capture, onError](const QString &reason) {
        capture->deleteLater();
        onError(reason);
    });
    capture->capture(screenName);
}

// ---- questions with buttons ----------------------------------------------

void AiService::askChoice(const QString &question,
                          const QVariantList &options,
                          std::function<void(const QString &)> then)
{
    // Two questions cannot share the island. The one already there is older,
    // so it keeps its place and the new one is refused rather than queued -
    // every caller of this can deal with not being answered.
    if (m_choiceThen) {
        then(QString{});
        return;
    }

    m_choiceQuestion = question;
    m_choiceOptions = options;
    m_choiceThen = std::move(then);
    m_stateBeforeChoice = m_state;

    // A question is worth surfacing even when the user has looked away: it is
    // the one thing in the conversation that cannot make progress without them.
    if (m_background) {
        m_background = false;
        m_engaged = true;
    }
    m_engaged = true;
    setState(u"choosing"_s);
    Q_EMIT choiceChanged();
    Q_EMIT stateChanged();
}

void AiService::choose(const QString &id)
{
    if (!m_choiceThen) {
        return;
    }
    // Only an option that was actually offered counts; anything else is the
    // question being walked away from.
    bool offered = false;
    for (const QVariant &entry : std::as_const(m_choiceOptions)) {
        if (entry.toMap().value(u"id"_s).toString() == id) {
            offered = true;
            break;
        }
    }

    auto then = std::move(m_choiceThen);
    m_choiceThen = nullptr;
    m_choiceQuestion.clear();
    m_choiceOptions.clear();
    Q_EMIT choiceChanged();
    if (m_state == u"choosing"_s) {
        setState(m_stateBeforeChoice.isEmpty() ? u"working"_s : m_stateBeforeChoice);
    }
    then(offered ? id : QString{});
}

void AiService::pickScreen()
{
    if (!ScreenCapture::hasSeveralOutputs()) {
        return;
    }
    m_engaged = true;
    m_background = false;
    // "ask" whatever is set, because being asked is the point of the button.
    askChoice(tr("Which screen shall I look at?"), screenOptions(), [this](const QString &id) {
        if (!id.isEmpty()) {
            setScreenChoice(id);
        }
        if (m_state == u"choosing"_s) {
            setState(u"composing"_s);
        }
        Q_EMIT focusRequested();
    });
}

void AiService::abandonChoice()
{
    if (!m_choiceThen) {
        return;
    }
    auto then = std::move(m_choiceThen);
    m_choiceThen = nullptr;
    m_choiceQuestion.clear();
    m_choiceOptions.clear();
    Q_EMIT choiceChanged();
    then(QString{});
}

// ---- state ---------------------------------------------------------------

bool AiService::glowing() const
{
    return m_engaged && !m_background;
}

bool AiService::busy() const
{
    return m_state == u"thinking"_s || m_state == u"answering"_s || m_state == u"working"_s
        || m_state == u"permission"_s || m_state == u"choosing"_s;
}

int AiService::exchanges() const
{
    int count = 0;
    for (const AiTurn &turn : m_history) {
        if (turn.role == u"user"_s && !turn.text.isEmpty()) {
            ++count;
        }
    }
    return count;
}

bool AiService::unattended() const
{
    return m_broker->grantedEverything();
}

QString AiService::pendingTier() const
{
    return PermissionBroker::tierTitle(m_pending.risk);
}

void AiService::setState(const QString &state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged();
}

void AiService::setActivity(const QString &line)
{
    const QString trimmed = line.left(160);
    if (m_activity == trimmed) {
        return;
    }
    m_activity = trimmed;
    Q_EMIT activityChanged();
}

void AiService::addStep(const QString &kind,
                        const QString &text,
                        const QString &status,
                        const QString &id)
{
    m_steps.append(QVariantMap{{u"kind"_s, kind},
                               {u"text"_s, text},
                               {u"status"_s, status},
                               {u"id"_s, id}});
    if (m_steps.size() > 40) {
        m_steps.removeFirst();
    }
    Q_EMIT stepsChanged();
}

void AiService::updateLastStep(const QString &status, const QString &text)
{
    if (m_steps.isEmpty()) {
        return;
    }
    QVariantMap step = m_steps.last().toMap();
    step.insert(u"status"_s, status);
    if (!text.isEmpty()) {
        step.insert(u"text"_s, text);
    }
    m_steps[m_steps.size() - 1] = step;
    Q_EMIT stepsChanged();
}

void AiService::updateStepById(const QString &id, const QString &status)
{
    if (id.isEmpty()) {
        return;
    }
    for (int index = m_steps.size() - 1; index >= 0; --index) {
        QVariantMap step = m_steps.at(index).toMap();
        if (step.value(u"id"_s).toString() != id) {
            continue;
        }
        step.insert(u"status"_s, status);
        m_steps[index] = step;
        Q_EMIT stepsChanged();
        return;
    }
}

// ---- the island's verbs --------------------------------------------------

void AiService::engage()
{
    m_engaged = true;
    m_background = false;

    if (!configured()) {
        setState(u"setup"_s);
        Q_EMIT stateChanged();
        Q_EMIT setupRequested();
        return;
    }

    if (!busy()) {
        m_error.clear();
        setState(u"composing"_s);
        Q_EMIT focusRequested();
    }
    Q_EMIT stateChanged();
}

void AiService::dismiss()
{
    m_engaged = false;
    // Work that is under way is not thrown out just because the panel closed;
    // it moves to the island, which is what "continue in background" does by
    // hand and what closing the panel should do by itself.
    m_background = busy();
    if (!busy()) {
        setState(u"composing"_s);
    }
    Q_EMIT stateChanged();
}

void AiService::continueInBackground()
{
    if (!busy()) {
        dismiss();
        return;
    }
    m_background = true;
    Q_EMIT stateChanged();
}

void AiService::bringToFront()
{
    m_engaged = true;
    m_background = false;
    Q_EMIT stateChanged();
}

void AiService::startOver()
{
    cancel();
    m_history.clear();
    m_steps.clear();
    m_answer.clear();
    m_thought.clear();
    m_question.clear();
    m_error.clear();
    m_rounds = 0;
    m_broker->revokeAll();
    setScreenChoice(m_config->value(u"ai.screen"_s, u"ask"_s).toString());
    setState(u"composing"_s);
    Q_EMIT stepsChanged();
    Q_EMIT answerChanged();
    Q_EMIT conversationChanged();
}

void AiService::cancel()
{
    // Whoever is waiting on a question - the model, or a command sitting on
    // the other end of the bus - has to be told, but nothing they are told may
    // start the conversation up again.
    m_cancelling = true;
    abandonChoice();

    m_anthropic->abort();
    m_gemini->abort();
    m_cli->abort();
    m_toolbox->cancel();
    m_queue.clear();
    m_results.clear();
    // Every tool call the client is holding open has to be told something, or
    // it sits there until its own patience runs out.
    if (!m_reviewToken.isEmpty()) {
        answerReview(m_reviewToken, false, tr("The user stopped the assistant."));
        m_reviewToken.clear();
    }
    while (!m_reviews.isEmpty()) {
        answerReview(m_reviews.dequeue().token, false, tr("The user stopped the assistant."));
    }
    m_awaitingPermission = false;
    m_pending = {};
    m_background = false;
    setActivity({});
    Q_EMIT pendingChanged();
    if (m_state != u"composing"_s && m_state != u"setup"_s) {
        setState(m_answer.isEmpty() ? u"composing"_s : u"done"_s);
    }
    m_cancelling = false;
}

void AiService::ask(const QString &text)
{
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_AI") > 0) {
        qWarning("atoll: ai ask '%s' provider=%s configured=%d key=%d baseUrl='%s'",
                 qUtf8Printable(text.trimmed()), qUtf8Printable(provider()),
                 configured(), m_credentials->hasKey(provider()),
                 qUtf8Printable(m_config->value(u"ai.baseUrl"_s, QString()).toString()));
    }
    const QString question = text.trimmed();
    if (question.isEmpty()) {
        return;
    }
    if (!configured()) {
        // A question asked from outside - a shortcut, a script - should still
        // put the island in front of the user, or the request vanishes with no
        // sign that anything was wrong with it.
        engage();
        return;
    }

    m_engaged = true;
    m_background = false;
    m_error.clear();
    m_question = question;
    m_answer.clear();
    m_thought.clear();
    m_steps.clear();
    m_rounds = 0;
    Q_EMIT stepsChanged();
    Q_EMIT answerChanged();
    Q_EMIT conversationChanged();

    AiTurn turn;
    turn.role = u"user"_s;
    turn.text = question;
    m_history.append(turn);

    if (m_shareScreen) {
        // The picture has to be taken before the question is sent, which may
        // mean asking which screen is meant and then waiting on a portal
        // dialog. Both are worth showing as work rather than as a pause.
        setState(u"working"_s);
        withChosenScreen({}, [this](const QString &screenName) {
            if (m_cancelling) {
                return;
            }
            if (screenName.isEmpty()) {
                // The screen question was closed. The question that was typed
                // is still a question, so it goes without the picture rather
                // than being thrown away with it.
                setShareScreen(false);
                setActivity({});
                addStep(u"note"_s, tr("Asked without a picture of the screen."), u"denied"_s);
                runTurn();
                return;
            }

            setActivity(tr("Taking a picture of the screen…"));
            takeScreenshot(
                screenName,
                [this, screenName](const QByteArray &png) {
                    if (m_cancelling) {
                        return;
                    }
                    if (!m_history.isEmpty()) {
                        const QString what = screenName == u"all"_s
                            ? tr("every screen")
                            : ScreenCapture::labelFor(screenName);
                        if (activeBackend()->drivesTools()) {
                            // The client reads pictures off disk rather than
                            // out of a message, so the screenshot is left
                            // somewhere it can open and the question says where.
                            const QString path =
                                QDir(ClaudeCliProvider::workspacePath()).filePath(u"screen.png"_s);
                            QFile file(path);
                            if (file.open(QIODevice::WriteOnly)) {
                                file.write(png);
                                file.close();
                                m_history.last().text +=
                                    tr("\n\n(A picture of %1 as it is right now has been saved to "
                                       "%2. Open it to see what I am looking at.)")
                                        .arg(what, path);
                            }
                        } else {
                            m_history.last().image = png;
                            m_history.last().imageMediaType = u"image/png"_s;
                            m_history.last().text +=
                                tr("\n\n(The picture attached is of %1.)").arg(what);
                        }
                    }
                    setShareScreen(false);
                    setActivity({});
                    runTurn();
                },
                [this](const QString &reason) {
                    if (m_cancelling) {
                        return;
                    }
                    setShareScreen(false);
                    addStep(u"note"_s, reason, u"failed"_s);
                    setActivity({});
                    runTurn();
                });
        });
        return;
    }

    runTurn();
}

// ---- the loop ------------------------------------------------------------

void AiService::runTurn()
{
    AiBackend *target = activeBackend();
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_AI") > 0) {
        qWarning("atoll: ai turn backend=%s model=%s url='%s'",
                 qUtf8Printable(target->id()), qUtf8Printable(model()),
                 qUtf8Printable(m_config->value(u"ai.baseUrl"_s, QString()).toString()));
    }
    if (auto *keyed = qobject_cast<AiProvider *>(target)) {
        keyed->setApiKey(m_credentials->key(provider()));
    }
    if (auto *client = qobject_cast<ClaudeCliProvider *>(target)) {
        client->setExecutablePath(m_config->value(u"ai.cliPath"_s, QString()).toString());
    }

    AiRequest request;
    request.model = model();
    request.maxTokens = m_config->value(u"ai.maxTokens"_s, 16000).toInt();
    request.effort = m_config->value(u"ai.effort"_s, u"high"_s).toString();
    request.webSearch = m_config->value(u"ai.webSearch"_s, true).toBool();
    request.baseUrl = m_config->value(u"ai.baseUrl"_s, QString()).toString();
    request.systemPrompt =
        AiToolbox::systemPrompt(m_config->value(u"ai.systemPrompt"_s, QString()).toString());
    request.tools = AiToolbox::definitions(m_config->value(u"ai.allowScreenshots"_s, true).toBool()
                                           && ScreenCapture::available());
    request.history = m_history;

    // A backend that runs its own tools was given them when it started, so the
    // catalogue above is not its business and neither is the round counter:
    // it stops itself.
    if (target->drivesTools()) {
        request.tools = {};
        request.systemPrompt +=
            AiToolbox::clientAddendum(m_config->value(u"ai.allowScreenshots"_s, true).toBool()
                                      && ScreenCapture::available());
    }

    setState(m_answer.isEmpty() ? u"thinking"_s : u"answering"_s);
    target->send(request);
}

void AiService::onTurnEnded(const QString &stopReason,
                            const QList<AiToolCall> &calls,
                            const QJsonArray &raw)
{
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_AI") > 0) {
        qWarning("atoll: ai turn ended: %s (%d tool call(s), %d raw block(s))",
                 qUtf8Printable(stopReason), calls.size(), raw.size());
    }
    if (m_testing) {
        m_testing = false;
        m_keyTest = tr("The key works.");
        Q_EMIT keyTestChanged();
        return;
    }

    AiTurn turn;
    turn.role = u"assistant"_s;
    turn.text = m_answer;
    turn.toolCalls = calls;
    turn.rawContent = raw;
    turn.rawProvider = activeBackend()->id();
    m_history.append(turn);

    if (stopReason == u"pause_turn"_s) {
        // The provider's own tools ran and it wants to keep going; the history
        // it needs is already back in place.
        runTurn();
        return;
    }

    if (stopReason == u"refusal"_s) {
        fail(tr("The assistant declined to answer that."));
        return;
    }

    if (calls.isEmpty()) {
        setActivity({});
        setState(u"done"_s);
        if (m_background) {
            Q_EMIT messageRequested(tr("%1 finished").arg(providerLabel()),
                                    m_answer.left(120));
        }
        return;
    }

    if (++m_rounds > MaxToolRounds) {
        fail(tr("The assistant kept going without reaching an answer, so Atoll stopped it."));
        return;
    }

    m_results.clear();
    m_queue.clear();
    for (const AiToolCall &call : calls) {
        m_queue.enqueue(call);
    }
    setState(u"working"_s);
    advanceQueue();
}

void AiService::advanceQueue()
{
    if (m_queue.isEmpty()) {
        finishToolRound();
        return;
    }

    m_current = m_queue.dequeue();
    const AiVerdict verdict = m_broker->classify(m_current);

    if (verdict.risk == AiRisk::Forbidden || !m_broker->tierEnabled(verdict.risk)) {
        const QString reason = verdict.risk == AiRisk::Forbidden
            ? (verdict.refusal.isEmpty() ? tr("Atoll refuses this action.") : verdict.refusal)
            : tr("The user's settings do not allow this (%1). Suggest what they could change, or "
                 "find another way.")
                  .arg(PermissionBroker::tierTitle(verdict.risk));
        addStep(u"tool"_s, verdict.summary.isEmpty() ? m_current.name : verdict.summary, u"denied"_s);

        AiToolResult result;
        result.id = m_current.id;
        result.content = reason;
        result.isError = true;
        m_results.append(result);
        advanceQueue();
        return;
    }

    if (m_broker->isPreApproved(verdict)) {
        executeNow(m_current, verdict);
        return;
    }

    m_pending = verdict;
    m_awaitingPermission = true;
    // A question is worth surfacing even when the user has looked away.
    if (m_background) {
        m_background = false;
        m_engaged = true;
    }
    setState(u"permission"_s);
    Q_EMIT pendingChanged();
    Q_EMIT stateChanged();
}

void AiService::executeNow(const AiToolCall &call, const AiVerdict &verdict)
{
    // Putting a question to the user is not something the toolbox can carry
    // out: it ends with a button being tapped on the island, and the toolbox
    // has no island. So it is answered here instead.
    if (call.name == u"ask_user"_s) {
        presentQuestion(call);
        return;
    }

    // Which screen, when the machine has more than one and nobody has said.
    // Asking beats guessing: a picture of the wrong monitor is a wasted turn,
    // and a picture of all of them is one nothing can be read on.
    if (call.name == u"take_screenshot"_s) {
        AiToolCall wanted = call;
        withChosenScreen(call.input.value(u"screen"_s).toString(),
                         [this, wanted, verdict](const QString &screenName) mutable {
                             if (screenName.isEmpty()) {
                                 finishToolLocally(wanted,
                                                   tr("The user closed the question about which "
                                                      "screen to look at, so no picture was taken."),
                                                   true);
                                 return;
                             }
                             wanted.input.insert(u"screen"_s, screenName);
                             const QString what = screenName == u"all"_s
                                 ? tr("Look at every screen")
                                 : tr("Look at %1").arg(screenName);
                             addStep(u"tool"_s, what, u"running"_s);
                             setActivity(what);
                             setState(u"working"_s);
                             m_toolbox->execute(wanted, verdict.risk);
                         });
        return;
    }

    addStep(u"tool"_s,
            verdict.summary.isEmpty() ? call.name : verdict.summary,
            verdict.risk == AiRisk::Admin ? u"elevated"_s : u"running"_s);
    setActivity(verdict.summary.isEmpty() ? call.name : verdict.summary);
    setState(u"working"_s);
    m_toolbox->execute(call, verdict.risk);
}

void AiService::presentQuestion(const AiToolCall &call)
{
    const QString question = call.input.value(u"question"_s).toString().trimmed();
    QStringList labels;
    const QStringList given = call.input.value(u"options"_s).toStringList();
    for (const QString &label : given) {
        const QString trimmed = label.trimmed();
        // The island is a pill: five short answers is already a lot, and a
        // paragraph on a button is not an answer anybody can pick out.
        if (!trimmed.isEmpty() && labels.size() < 5) {
            labels.append(trimmed.left(48));
        }
    }

    if (question.isEmpty() || labels.size() < 2) {
        finishToolLocally(call,
                          tr("A question needs one line of text and between two and five options. "
                             "Ask again with both, or just say what you were going to ask."),
                          true);
        return;
    }

    QVariantList options;
    for (int index = 0; index < labels.size(); ++index) {
        options.append(QVariantMap{{u"id"_s, u"option-%1"_s.arg(index)},
                                   {u"label"_s, labels.at(index)},
                                   // The first one is the assistant's own
                                   // suggestion, and looks like it.
                                   {u"accented"_s, index == 0}});
    }

    // No step goes in while the question is up: it is already the headline on
    // the island, and a list underneath repeating it back is the same sentence
    // twice. What is worth keeping is the answer, so that is what is written
    // down, once there is one.
    askChoice(question, options, [this, call, question, labels](const QString &id) {
        const int index = id.startsWith(u"option-"_s) ? id.mid(7).toInt() : -1;
        if (index < 0 || index >= labels.size()) {
            addStep(u"question"_s, question, u"denied"_s);
            finishToolLocally(call,
                              tr("The user closed the question instead of answering it. Do not ask "
                                 "it again; carry on with what you can, or stop and say why you "
                                 "cannot."),
                              true);
            return;
        }
        addStep(u"question"_s, u"%1 → %2"_s.arg(question, labels.at(index)), u"done"_s);
        finishToolLocally(call, tr("The user chose: %1").arg(labels.at(index)), false);
    });
}

void AiService::finishToolLocally(const AiToolCall &call, const QString &content, bool isError)
{
    if (m_cancelling) {
        return; // The conversation this belonged to is being taken down.
    }
    AiToolResult result;
    result.id = call.id;
    result.content = content;
    result.isError = isError;
    m_results.append(result);
    setActivity({});
    setState(u"working"_s);
    advanceQueue();
}

void AiService::allow(bool rememberForSession)
{
    if (!m_awaitingPermission) {
        return;
    }
    m_awaitingPermission = false;
    if (rememberForSession) {
        m_broker->grantForSession(m_pending.grantKey);
    }
    const AiVerdict verdict = m_pending;
    m_pending = {};
    Q_EMIT pendingChanged();

    if (!m_reviewToken.isEmpty()) {
        // The client is holding the call and will make it itself; all it needs
        // from here is the word.
        const QString token = m_reviewToken;
        m_reviewToken.clear();
        answerReview(token, true, tr("Allowed on the island."));
        setState(u"working"_s);
        showNextReview();
        return;
    }

    executeNow(m_current, verdict);
}

void AiService::allowEverything()
{
    if (!m_awaitingPermission) {
        return;
    }
    m_broker->grantEverythingForSession();
    // Everything queued behind this one is now pre-approved as well, and
    // allow() drains the queue for us.
    allow(false);
    Q_EMIT stateChanged();
}

void AiService::deny()
{
    if (!m_awaitingPermission) {
        return;
    }
    m_awaitingPermission = false;
    const QString summary = m_pending.summary.isEmpty() ? m_current.name : m_pending.summary;
    m_pending = {};
    Q_EMIT pendingChanged();

    if (!m_reviewToken.isEmpty()) {
        const QString token = m_reviewToken;
        m_reviewToken.clear();
        updateStepById(m_current.id, u"denied"_s);
        answerReview(token,
                     false,
                     tr("The user did not allow this. Do not try it again. Continue without it, "
                        "or tell them what you would need."));
        setState(u"working"_s);
        showNextReview();
        return;
    }

    addStep(u"tool"_s, summary, u"denied"_s);

    AiToolResult result;
    result.id = m_current.id;
    // Phrased as a fact rather than an error: the model should carry on and
    // suggest something else, not treat the refusal as a fault to work around.
    result.content = tr("The user did not allow this. Do not try it again. Continue without it, or "
                        "tell them what you would need.");
    result.isError = true;
    m_results.append(result);

    setState(u"working"_s);
    advanceQueue();
}

void AiService::onToolResult(const AiToolResult &result)
{
    updateLastStep(result.isError ? u"failed"_s : u"done"_s);
    m_results.append(result);
    setActivity({});
    advanceQueue();
}

void AiService::finishToolRound()
{
    AiTurn turn;
    turn.role = u"user"_s;
    turn.toolResults = m_results;
    m_results.clear();
    m_history.append(turn);
    runTurn();
}

// ---- judging what the client wants to do ---------------------------------

void AiService::reviewToolCall(const QString &payload, const QString &token)
{
    const QJsonObject request = QJsonDocument::fromJson(payload.toUtf8()).object();
    const QString tool = request.value(u"tool_name"_s).toString();
    const QVariantMap input = request.value(u"tool_input"_s).toObject().toVariantMap();
    const QString useId = request.value(u"tool_use_id"_s).toString();

    // Nothing else on this machine has any business asking, and a question
    // with no conversation behind it cannot be shown to anybody either.
    if (!m_cli->busy()) {
        answerReview(token, false, tr("Atoll has no assistant session waiting for this."));
        return;
    }

    const AiToolCall call = ClaudeCliProvider::toAtollCall(useId, tool, input);
    const AiVerdict verdict = m_broker->classify(call);

    if (verdict.risk == AiRisk::Forbidden || !m_broker->tierEnabled(verdict.risk)) {
        updateStepById(useId, u"denied"_s);
        answerReview(token,
                     false,
                     verdict.risk == AiRisk::Forbidden
                         ? (verdict.refusal.isEmpty() ? tr("Atoll refuses this action.")
                                                      : verdict.refusal)
                         : tr("The user's settings do not allow this (%1). Suggest what they could "
                              "change, or find another way.")
                               .arg(PermissionBroker::tierTitle(verdict.risk)));
        return;
    }

    if (m_broker->isPreApproved(verdict)) {
        answerReview(token, true, tr("Allowed without asking, by the user's settings."));
        return;
    }

    m_reviews.enqueue(PendingReview{token, call, verdict});
    showNextReview();
}

void AiService::captureScreenFor(const QString &token, const QString &screenName)
{
    // Exactly one answer goes back, whichever way this ends. The caller is a
    // command sitting there waiting, and a screenshot that neither arrives nor
    // fails would hold the whole conversation open behind it.
    auto answered = std::make_shared<bool>(false);
    const auto answer = [this, token, answered](const QString &result) {
        if (*answered) {
            return;
        }
        *answered = true;
        setActivity({});
        Q_EMIT screenCaptureAnswered(token, result);
    };
    const auto fail = [answer](const QString &reason) {
        answer(u"error: "_s + reason);
    };

    if (!m_config->value(u"ai.allowScreenshots"_s, true).toBool()) {
        fail(tr("the user has switched off letting the assistant look at the screen."));
        return;
    }
    if (!ScreenCapture::available()) {
        fail(tr("nothing on this machine can take a screenshot."));
        return;
    }
    if (!screenName.isEmpty() && !ScreenCapture::hasOutput(screenName)
        && screenName.compare(u"all"_s, Qt::CaseInsensitive) != 0
        && screenName.compare(u"current"_s, Qt::CaseInsensitive) != 0) {
        fail(tr("there is no screen called %1. There is %2.")
                 .arg(screenName, screenSummary()));
        return;
    }

    // Always the same file. The assistant reads it straight after asking for
    // it, so keeping one around beats leaving a trail of pictures of somebody's
    // screen in a temporary directory.
    const QString target = QDir(ClaudeCliProvider::workspacePath()).filePath(u"screen.png"_s);

    withChosenScreen(screenName, [this, target, answer, fail](const QString &chosen) {
        if (chosen.isEmpty()) {
            fail(tr("the user closed the question about which screen to look at."));
            return;
        }

        setActivity(tr("Taking a picture of the screen…"));
        takeScreenshot(
            chosen,
            [target, answer, fail](const QByteArray &png) {
                QFile file(target);
                if (!file.open(QIODevice::WriteOnly)) {
                    fail(QCoreApplication::translate("AiService",
                                                     "the picture could not be written to %1.")
                             .arg(target));
                    return;
                }
                file.write(png);
                file.close();
                answer(target);
            },
            fail);

        // The consent dialog belongs to the desktop and there is no telling
        // whether anybody is in front of it. Waiting a minute for an answer is
        // generous; waiting for ever is a hung assistant.
        QTimer::singleShot(60000, this, [fail] {
            fail(QCoreApplication::translate(
                "AiService", "nobody answered the desktop's request to share the screen."));
        });
    });
}

void AiService::askUserFor(const QString &token,
                           const QString &question,
                           const QStringList &options)
{
    const auto answer = [this, token](const QString &result) {
        Q_EMIT userChoiceAnswered(token, result);
    };

    QStringList labels;
    for (const QString &option : options) {
        const QString trimmed = option.trimmed();
        if (!trimmed.isEmpty() && labels.size() < 5) {
            labels.append(trimmed.left(48));
        }
    }
    if (question.trimmed().isEmpty() || labels.size() < 2) {
        answer(u"error: "_s
               + tr("a question needs one line of text and between two and five options."));
        return;
    }

    QVariantList choices;
    for (int index = 0; index < labels.size(); ++index) {
        choices.append(QVariantMap{{u"id"_s, u"option-%1"_s.arg(index)},
                                   {u"label"_s, labels.at(index)},
                                   {u"accented"_s, index == 0}});
    }

    const QString asked = question.trimmed();
    askChoice(asked, choices, [this, answer, labels, asked](const QString &picked) {
        const int index = picked.startsWith(u"option-"_s) ? picked.mid(7).toInt() : -1;
        if (index < 0 || index >= labels.size()) {
            addStep(u"question"_s, asked, u"denied"_s);
            answer(u"error: "_s + tr("the user closed the question without answering it."));
            return;
        }
        addStep(u"question"_s, u"%1 → %2"_s.arg(asked, labels.at(index)), u"done"_s);
        answer(labels.at(index));
    });
}

void AiService::showNextReview()
{
    if (m_awaitingPermission || m_reviews.isEmpty()) {
        return;
    }

    const PendingReview review = m_reviews.dequeue();
    m_reviewToken = review.token;
    m_current = review.call;
    m_pending = review.verdict;
    m_awaitingPermission = true;
    if (m_background) {
        m_background = false;
        m_engaged = true;
    }
    setState(u"permission"_s);
    Q_EMIT pendingChanged();
    Q_EMIT stateChanged();
}

void AiService::answerReview(const QString &token, bool allowed, const QString &reason)
{
    const QJsonObject verdict{{u"decision"_s, allowed ? u"allow"_s : u"deny"_s},
                              {u"reason"_s, reason}};
    Q_EMIT toolReviewAnswered(
        token, QString::fromUtf8(QJsonDocument(verdict).toJson(QJsonDocument::Compact)));
}

void AiService::fail(const QString &reason)
{
    m_error = reason;
    m_queue.clear();
    m_results.clear();
    if (!m_reviewToken.isEmpty()) {
        answerReview(m_reviewToken, false, tr("The assistant stopped."));
        m_reviewToken.clear();
    }
    while (!m_reviews.isEmpty()) {
        answerReview(m_reviews.dequeue().token, false, tr("The assistant stopped."));
    }
    m_awaitingPermission = false;
    m_pending = {};
    setActivity({});
    setState(u"failed"_s);
    Q_EMIT pendingChanged();
    if (m_background) {
        Q_EMIT messageRequested(tr("%1 stopped").arg(providerLabel()), reason);
    }
}
