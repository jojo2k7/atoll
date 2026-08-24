/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

/**
 * The scriptable side of Atoll: `org.atoll.Atoll` on the session bus.
 *
 * Everything the island can be told to do from the outside lives here, which
 * makes the island usable as a general purpose heads-up display for scripts,
 * hotkeys and other desktop tooling - not just for the events it watches.
 */
class IpcService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.atoll.Atoll")
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.ipc")

public:
    explicit IpcService(QObject *parent = nullptr);

    /** Claim the bus name. Returns false if another instance already has it. */
    bool registerOnBus();

    /**
     * Claim the settings window's own bus name. The settings page runs as a
     * second process - it wants a normal window, and layer-shell is a
     * process-wide decision - so it needs a name of its own to be raised
     * through instead of being started twice.
     */
    bool registerSettingsOnBus();

public Q_SLOTS:
    Q_SCRIPTABLE void expand();
    Q_SCRIPTABLE void collapse();
    Q_SCRIPTABLE void toggle();
    Q_SCRIPTABLE void showText(const QString &icon, const QString &text);
    Q_SCRIPTABLE void showProgress(const QString &icon, int percent, const QString &text);
    /** Offer these files to nearby devices, as if they had been dropped. */
    Q_SCRIPTABLE void share(const QStringList &paths);
    /** Open the assistant with an empty box, as a long press does. */
    Q_SCRIPTABLE void assistant();
    /** Put a question to the assistant, as if it had been typed on the island. */
    Q_SCRIPTABLE void ask(const QString &prompt);
    Q_SCRIPTABLE void dismiss();
    Q_SCRIPTABLE void reloadConfig();
    Q_SCRIPTABLE void settings();
    Q_SCRIPTABLE void raise();
    Q_SCRIPTABLE QString version() const;
    Q_SCRIPTABLE void quit();

    /**
     * Ask the island whether a tool call may go ahead.
     *
     * `payload` is the request as the command-line client describes it, and
     * the reply is `{"decision": "allow"|"deny", "reason": "..."}`. The caller
     * is left waiting, sometimes for as long as it takes a person to notice
     * the question, so the reply is sent later rather than returned here.
     */
    Q_SCRIPTABLE QString reviewToolCall(const QString &payload);

    /**
     * Take one picture of the screen and answer with the path it was written
     * to, or with a line starting "error:".
     *
     * The screen may be named - `screen` is an output, "all", or "current" -
     * but the path is not: where the file lands is Atoll's decision, because a
     * caller that could name it could hand a screenshot's worth of bytes to
     * any file the session can write. An empty name on a machine with several
     * outputs puts the question to the user instead of guessing.
     */
    Q_SCRIPTABLE QString captureScreen(const QString &screen);

    /**
     * Put a question with up to five answers on the island and reply with the
     * one the user tapped, or with a line starting "error:".
     *
     * It exists for the assistant running as a command-line client, which can
     * write a question into its answer but has no way to put buttons in front
     * of anybody.
     */
    Q_SCRIPTABLE QString askUser(const QString &question, const QStringList &options);

    /** The outputs a picture can be taken of, one per line: "DP-1 2560x1440". */
    Q_SCRIPTABLE QString listScreens();

public:
    /** Answer a review that reviewToolCall left open. */
    void answerToolReview(const QString &token, const QString &verdictJson);
    /** Answer a picture that captureScreen left open. */
    void answerScreenCapture(const QString &token, const QString &result);
    /** Answer a question that askUser left open. */
    void answerUserChoice(const QString &token, const QString &result);
    /** Whether anything is still waiting for an answer. */
    bool hasOpenReviews() const
    {
        return !m_reviews.isEmpty();
    }

Q_SIGNALS:
    void expandRequested();
    void collapseRequested();
    void toggleRequested();
    void dismissRequested();
    void textRequested(const QString &icon, const QString &text);
    void progressRequested(const QString &icon, int percent, const QString &text);
    void shareRequested(const QStringList &paths);
    void assistantRequested();
    void askRequested(const QString &prompt);
    void reloadRequested();
    void settingsRequested();
    void raiseRequested();
    /** A tool call is waiting for a verdict; answer it with `token`. */
    void toolReviewRequested(const QString &payload, const QString &token);
    /** Somebody is waiting for a picture of the screen. */
    void screenCaptureRequested(const QString &token, const QString &screen);
    /** Somebody is waiting for the user to answer a question. */
    void userChoiceRequested(const QString &token, const QString &question, const QStringList &options);

private:
    /** One caller left hanging on reviewToolCall, and how to reach it. */
    struct OpenReview {
        // Never used as it stands - every entry is assigned a real connection
        // - but a hash needs to be able to hand back an empty one, and the
        // empty one is what says "there was nothing waiting under that name".
        QDBusConnection connection = QDBusConnection::sessionBus();
        QDBusMessage request;
    };

    /** Park a caller and hand back a token to answer it with later. */
    QString park(const QString &kind);
    void sendReply(const QString &token, const QString &value);

    QHash<QString, OpenReview> m_reviews;
    int m_reviewCounter = 0;
};
