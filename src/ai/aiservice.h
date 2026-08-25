/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aiprovider.h"
#include "ai/aitypes.h"

#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class AiToolbox;
class AnthropicProvider;
class ClaudeCliProvider;
class Config;
class CredentialStore;
class GeminiProvider;
class OpenRouterProvider;
class PermissionBroker;
class QNetworkAccessManager;

/**
 * The assistant, as the island sees it.
 *
 * One property tells the UI what to draw (`state`), one carries the answer as
 * it arrives, and one describes whatever the assistant is currently asking
 * permission for. Everything else - which service is answering, what the model
 * is allowed to touch, how a long job keeps running once the user has looked
 * away - is arranged behind those three.
 *
 * The loop itself is deliberately plain: send the conversation, let the model
 * ask for tools, put every result back in one message, send it again. What is
 * not plain is who decides whether a tool runs at all, and that lives in
 * PermissionBroker rather than here.
 */
class AiService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.ai")

    /** Whether a provider is configured well enough to be asked anything. */
    Q_PROPERTY(bool configured READ configured NOTIFY configurationChanged)
    Q_PROPERTY(QString provider READ provider NOTIFY configurationChanged)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY configurationChanged)
    Q_PROPERTY(QString model READ model NOTIFY configurationChanged)

    /**
     * setup | composing | thinking | answering | permission | choosing | working | done | failed
     * Only meaningful while `engaged`.
     */
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    /** True from the long press until the user dismisses the assistant. */
    Q_PROPERTY(bool engaged READ engaged NOTIFY stateChanged)
    /** True once the user sent it away; the island keeps the progress only. */
    Q_PROPERTY(bool background READ background NOTIFY stateChanged)
    /** Whether the screen edges should be lit. */
    Q_PROPERTY(bool glowing READ glowing NOTIFY stateChanged)
    /** Whether anything is still in flight. */
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)

    Q_PROPERTY(QString question READ question NOTIFY conversationChanged)
    Q_PROPERTY(QString answer READ answer NOTIFY answerChanged)
    /** The model's own summary of what it is doing, when it offers one. */
    Q_PROPERTY(QString thought READ thought NOTIFY answerChanged)
    /** One line about the step in progress: "Installing firefox…". */
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    /** Everything the assistant has done this turn, for the expanded view. */
    Q_PROPERTY(QVariantList steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(int exchanges READ exchanges NOTIFY conversationChanged)

    // ---- the permission the island is currently showing -------------------
    Q_PROPERTY(QString pendingSummary READ pendingSummary NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingDetail READ pendingDetail NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingTier READ pendingTier NOTIFY pendingChanged)
    /** safe | user | admin */
    Q_PROPERTY(QString pendingRisk READ pendingRisk NOTIFY pendingChanged)
    /** Whether saying yes will bring up a system password prompt. */
    Q_PROPERTY(bool pendingElevated READ pendingElevated NOTIFY pendingChanged)
    /** True once the user has told it to stop asking for this conversation. */
    Q_PROPERTY(bool unattended READ unattended NOTIFY stateChanged)

    // ---- the command-line client, for the settings window -----------------
    /** missing | signed-out | checking | ready */
    Q_PROPERTY(QString cliState READ cliState NOTIFY cliChanged)
    /** One sentence about that state, for the person reading it. */
    Q_PROPERTY(QString cliDetail READ cliDetail NOTIFY cliChanged)

    // ---- a question with buttons under it ---------------------------------
    //
    // Both the assistant and Atoll itself need to put a small choice to the
    // user - "which of these do you want", "which screen shall I look at" -
    // and neither is a permission prompt. One mechanism serves both: a line of
    // text, a handful of options, and whatever should happen with the answer.
    /** What is being asked, while the state is "choosing". */
    Q_PROPERTY(QString choiceQuestion READ choiceQuestion NOTIFY choiceChanged)
    /** `[{id, label, detail, accented}]`, in the order they should be shown. */
    Q_PROPERTY(QVariantList choiceOptions READ choiceOptions NOTIFY choiceChanged)

    /** Whether the next question carries a picture of the screen. */
    Q_PROPERTY(bool shareScreen READ shareScreen WRITE setShareScreen NOTIFY shareScreenChanged)
    Q_PROPERTY(bool screenAvailable READ screenAvailable CONSTANT)
    /** The outputs a picture can be taken of: `[{name, label, width, height}]`. */
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
    /** True when there is more than one, which is when the question is worth asking. */
    Q_PROPERTY(bool severalScreens READ severalScreens NOTIFY screensChanged)
    /**
     * Which output pictures are taken of: an output name, "all", "current", or
     * "ask" to put the question to the user each time. Kept for the
     * conversation, not written to the configuration, so a choice made for one
     * question does not quietly become the setting.
     */
    Q_PROPERTY(QString screenChoice READ screenChoice WRITE setScreenChoice NOTIFY screensChanged)

public:
    explicit AiService(Config *config, QObject *parent = nullptr);
    ~AiService() override;

    bool configured() const;
    QString provider() const;
    QString providerLabel() const;
    QString model() const;

    QString state() const
    {
        return m_state;
    }
    bool engaged() const
    {
        return m_engaged;
    }
    bool background() const
    {
        return m_background;
    }
    bool glowing() const;
    bool busy() const;

    QString question() const
    {
        return m_question;
    }
    QString answer() const
    {
        return m_answer;
    }
    QString thought() const
    {
        return m_thought;
    }
    QString activity() const
    {
        return m_activity;
    }
    QString error() const
    {
        return m_error;
    }
    QVariantList steps() const
    {
        return m_steps;
    }
    int exchanges() const;

    QString pendingSummary() const
    {
        return m_pending.summary;
    }
    QString pendingDetail() const
    {
        return m_pending.detail;
    }
    QString pendingTier() const;
    QString pendingRisk() const
    {
        return aiRiskName(m_pending.risk);
    }
    bool pendingElevated() const
    {
        return m_pending.risk == AiRisk::Admin;
    }
    bool unattended() const;

    QString cliState() const;
    QString cliDetail() const;

    QString choiceQuestion() const
    {
        return m_choiceQuestion;
    }
    QVariantList choiceOptions() const
    {
        return m_choiceOptions;
    }

    bool shareScreen() const
    {
        return m_shareScreen;
    }
    void setShareScreen(bool share);
    bool screenAvailable() const;
    QVariantList screens() const;
    bool severalScreens() const;
    QString screenChoice() const;
    void setScreenChoice(const QString &choice);

    // ---- what the island calls ------------------------------------------
    /** Long press: open the assistant, or the setup prompt if it has no key. */
    Q_INVOKABLE void engage();
    /** Put it away. A job already running keeps running, in the background. */
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void ask(const QString &text);
    /** Stop the current turn and everything it started. */
    Q_INVOKABLE void cancel();
    /** Hide the assistant but keep working; progress stays on the island. */
    Q_INVOKABLE void continueInBackground();
    Q_INVOKABLE void bringToFront();
    /** Answer the permission the island is showing. */
    Q_INVOKABLE void allow(bool rememberForSession);
    /**
     * Allow this one and everything after it, until the conversation ends.
     * The one answer somebody wants to give when they asked for a job rather
     * than for a step.
     */
    Q_INVOKABLE void allowEverything();
    Q_INVOKABLE void deny();
    /** Forget the conversation and every allowance granted in it. */
    Q_INVOKABLE void startOver();
    /**
     * Answer whatever is being asked in the "choosing" state. An id nobody
     * offered - or an empty one - counts as walking away from the question,
     * which is a legitimate answer and is passed on as such.
     */
    Q_INVOKABLE void choose(const QString &id);
    /**
     * Put the "which screen" question up on its own, for somebody who wants to
     * settle it before they ask rather than when they are asked.
     */
    Q_INVOKABLE void pickScreen();

    /**
     * Decide whether a tool call the command-line client is about to make may
     * go ahead. The verdict is sent back through `toolReviewAnswered`, which
     * may be a long time later, because the answer is usually a person's.
     */
    void reviewToolCall(const QString &payload, const QString &token);

    /**
     * Take the picture the assistant asked for and answer with where it went.
     * The path is Atoll's to choose, so the caller does not name one.
     *
     * `screenName` may be an output, "all", "current", or empty - and empty on
     * a machine with several outputs is what puts the question to the user
     * rather than sending back a picture of everything at once.
     */
    void captureScreenFor(const QString &token, const QString &screenName = {});

    /**
     * Put a question with buttons on the island on the assistant's behalf and
     * answer with the option the user picked, or with a line starting
     * "error:" if they closed it instead.
     */
    void askUserFor(const QString &token, const QString &question, const QStringList &options);

    /** The outputs, as a line the assistant can be told about. */
    Q_INVOKABLE QString screenSummary() const;

    // ---- what the settings window calls ----------------------------------
    Q_INVOKABLE bool hasKeyFor(const QString &provider) const;
    Q_INVOKABLE QString keyBackendFor(const QString &provider) const;
    Q_INVOKABLE void setKeyFor(const QString &provider, const QString &key);
    /** Ask the provider a trivial question, to prove the key works. */
    Q_INVOKABLE void testKey();
    /** Look again for the command-line client and its login. */
    Q_INVOKABLE void refreshCli();
    /** Open a terminal on the client's sign-in flow. False if none was found. */
    Q_INVOKABLE bool signInToCli();
    /** The command that installs the client, for the user to run or copy. */
    Q_INVOKABLE QString cliInstallCommand() const;
    Q_PROPERTY(QString keyTestResult READ keyTestResult NOTIFY keyTestChanged)
    QString keyTestResult() const
    {
        return m_keyTest;
    }

Q_SIGNALS:
    void configurationChanged();
    void stateChanged();
    void conversationChanged();
    void answerChanged();
    void activityChanged();
    void stepsChanged();
    void pendingChanged();
    void choiceChanged();
    void shareScreenChanged();
    void screensChanged();
    void keyTestChanged();
    void cliChanged();
    /** `verdictJson` is what the waiting tool call gets told. */
    void toolReviewAnswered(const QString &token, const QString &verdictJson);
    /** The path the picture was written to, or a line starting "error:". */
    void screenCaptureAnswered(const QString &token, const QString &result);
    /** The option the user picked, or a line starting "error:". */
    void userChoiceAnswered(const QString &token, const QString &result);
    /** The assistant wants a line shown on the island. */
    void messageRequested(const QString &summary, const QString &body);
    /** The island should give the input field the keyboard. */
    void focusRequested();
    /** No provider is set up; the island offers to open the settings. */
    void setupRequested();

private:
    AiBackend *activeBackend() const;
    void setState(const QString &state);
    void setActivity(const QString &line);
    void addStep(const QString &kind, const QString &text, const QString &status,
                 const QString &id = {});
    void updateLastStep(const QString &status, const QString &text = {});
    /** Update the step a self-driving backend started under `id`, if it is still shown. */
    void updateStepById(const QString &id, const QString &status);

    /** Send one waiting tool call its verdict. */
    void answerReview(const QString &token, bool allowed, const QString &reason);
    /** Put the next tool call that needs a person in front of them. */
    void showNextReview();

    /**
     * Put a question with buttons in front of the user. `then` is handed the
     * id of the option they picked, or an empty string if the question went
     * away unanswered - every caller has to answer that case, because
     * something is always waiting on the other end of it.
     */
    void askChoice(const QString &question,
                   const QVariantList &options,
                   std::function<void(const QString &)> then);
    /** Take back an unanswered question, telling whoever asked it. */
    void abandonChoice();
    /** The options for "which screen", including everything at once. */
    QVariantList screenOptions() const;
    /**
     * Settle which output a picture is of, asking the user when the answer is
     * "ask" and there is more than one. `then` is given the output's name, or
     * an empty string if the question was walked away from.
     */
    void withChosenScreen(const QString &requested, std::function<void(const QString &)> then);
    /** Resolve "current"/"all"/a name into what ScreenCapture wants. */
    QString resolveScreen(const QString &choice) const;
    /** A capture already decided upon: takes it, and hands the bytes over. */
    void takeScreenshot(const QString &screenName,
                        std::function<void(const QByteArray &)> onImage,
                        std::function<void(const QString &)> onError);

    void runTurn();
    void onTurnEnded(const QString &stopReason, const QList<AiToolCall> &calls, const QJsonArray &raw);
    /** Take the next call off the queue: classify it, ask, or run it. */
    void advanceQueue();
    void executeNow(const AiToolCall &call, const AiVerdict &verdict);
    /** The assistant's own question, put on the island as buttons. */
    void presentQuestion(const AiToolCall &call);
    /** Answer a tool call from here rather than from the toolbox. */
    void finishToolLocally(const AiToolCall &call, const QString &content, bool isError);
    void onToolResult(const AiToolResult &result);
    void finishToolRound();
    void fail(const QString &reason);

    Config *m_config = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    AnthropicProvider *m_anthropic = nullptr;
    GeminiProvider *m_gemini = nullptr;
    OpenRouterProvider *m_openrouter = nullptr;
    ClaudeCliProvider *m_cli = nullptr;
    CredentialStore *m_credentials = nullptr;
    PermissionBroker *m_broker = nullptr;
    AiToolbox *m_toolbox = nullptr;

    QList<AiTurn> m_history;
    QString m_state = QStringLiteral("composing");
    bool m_engaged = false;
    bool m_background = false;
    QString m_question;
    QString m_answer;
    QString m_thought;
    QString m_activity;
    QString m_error;
    QString m_keyTest;
    QVariantList m_steps;
    bool m_shareScreen = false;
    /** "ask", "all", "current", or an output name. */
    QString m_screenChoice = QStringLiteral("ask");

    QString m_choiceQuestion;
    QVariantList m_choiceOptions;
    /** What to do with the answer. Empty when nothing is being asked. */
    std::function<void(const QString &)> m_choiceThen;
    /** The state to go back to if the question is abandoned. */
    QString m_stateBeforeChoice;
    /** Guards against a model that keeps calling tools and never answers. */
    int m_rounds = 0;

    /** A tool call the command-line client is holding, waiting to be judged. */
    struct PendingReview {
        QString token;
        AiToolCall call;
        AiVerdict verdict;
    };
    /**
     * The client can ask about several calls at once, and each one is a
     * process sitting there waiting, so they are answered in turn rather than
     * one of them being quietly forgotten.
     */
    QQueue<PendingReview> m_reviews;
    /** The review currently on the island, if the pending question is one. */
    QString m_reviewToken;
    /** True while the settings window is waiting for a login check. */
    bool m_cliTesting = false;

    QQueue<AiToolCall> m_queue;
    QList<AiToolResult> m_results;
    AiToolCall m_current;
    AiVerdict m_pending;
    bool m_awaitingPermission = false;
    /** True while cancel() is unwinding, so nothing it releases starts a new turn. */
    bool m_cancelling = false;
    /** True while the turn is only a key test, so the UI stays out of it. */
    bool m_testing = false;
};
