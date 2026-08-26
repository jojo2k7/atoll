/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include "notificationmonitor.h"

class AppActivator;
class Config;

/**
 * The notification history the island draws from: newest first, capped, with
 * the freshest entry doubling as the toast the collapsed island morphs into.
 */
class NotificationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.notifications")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)
    /** Set when a notification arrived that the island has not shown yet. */
    Q_PROPERTY(QVariantMap latest READ latest NOTIFY latestChanged)

public:
    enum Roles {
        UidRole = Qt::UserRole + 1,
        DaemonIdRole,
        AppNameRole,
        AppIconRole,
        SummaryRole,
        BodyRole,
        ImageRole,
        ActionsRole,
        UrgencyRole,
        ProgressRole,
        CategoryRole,
        AccentRole,
        TimestampRole,
        AgeRole,
    };
    Q_ENUM(Roles)

    explicit NotificationModel(Config *config, AppActivator *activator, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool doNotDisturb() const
    {
        return m_dnd;
    }
    void setDoNotDisturb(bool dnd);
    QVariantMap latest() const;

    /** Remove from the island only; the real daemon keeps its copy. */
    Q_INVOKABLE void dismiss(quint64 uid);
    /** Ask the notification daemon to close it for everyone. */
    Q_INVOKABLE void close(quint64 uid);
    Q_INVOKABLE void clear();
    /**
     * Best-effort action activation. Atoll is only a bus observer, so it can
     * re-broadcast ActionInvoked but cannot make senders that filter on the
     * daemon's bus name accept it. See docs/notifications.md.
     */
    Q_INVOKABLE bool invokeAction(quint64 uid, const QString &actionKey);
    /**
     * What a click on a notification means: hand the sender its default
     * action - through the daemon where that is possible, re-broadcast where
     * it is not - and then raise or start the application itself, so chat
     * clients like Discord land you where the message came from.
     */
    Q_INVOKABLE void open(quint64 uid);

public Q_SLOTS:
    void onPosted(const NotificationData &notification);
    void onIdAssigned(quint64 uid, quint32 daemonId);
    void onClosed(quint32 daemonId, int reason);

Q_SIGNALS:
    void countChanged();
    void latestChanged();
    void doNotDisturbChanged();
    /** Emitted for notifications the island should actually pop for. */
    void arrived(const QVariantMap &notification);

private:
    int indexOfUid(quint64 uid) const;
    QVariantMap toMap(const NotificationData &data) const;

    Config *m_config;
    AppActivator *m_activator;
    QList<NotificationData> m_items;
    bool m_dnd = false;
};
