/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "notificationmodel.h"

#include "app/imagestore.h"
#include "config/config.h"
#include "system/appactivator.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

using namespace Qt::StringLiterals;

namespace
{
constexpr int HistoryLimit = 50;
}

NotificationModel::NotificationModel(Config *config, AppActivator *activator, QObject *parent)
    : QAbstractListModel(parent)
    , m_config(config)
    , m_activator(activator)
{
    m_dnd = m_config->value(u"notifications.dnd"_s, false).toBool();
}

int NotificationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {
        {UidRole, "uid"},
        {DaemonIdRole, "daemonId"},
        {AppNameRole, "appName"},
        {AppIconRole, "appIcon"},
        {SummaryRole, "summary"},
        {BodyRole, "body"},
        {ImageRole, "image"},
        {ActionsRole, "actions"},
        {UrgencyRole, "urgency"},
        {ProgressRole, "progress"},
        {CategoryRole, "category"},
        {AccentRole, "accent"},
        {TimestampRole, "timestamp"},
        {AgeRole, "age"},
    };
}

QVariant NotificationModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const NotificationData &item = m_items.at(index.row());
    switch (role) {
    case UidRole:
        return QVariant::fromValue(item.uid);
    case DaemonIdRole:
        return item.daemonId;
    case AppNameRole:
        return item.appName;
    case AppIconRole:
        return item.appIcon;
    case SummaryRole:
        return item.summary;
    case BodyRole:
        return item.body;
    case ImageRole:
        return item.imageUrl;
    case ActionsRole:
        return item.actions;
    case UrgencyRole:
        return item.urgency;
    case ProgressRole:
        return item.progress;
    case CategoryRole:
        return item.category;
    case AccentRole:
        return item.accent.isValid() ? QVariant::fromValue(item.accent) : QVariant();
    case TimestampRole:
        return item.timestamp;
    case AgeRole:
        return item.timestamp.secsTo(QDateTime::currentDateTime());
    default:
        return {};
    }
}

QVariantMap NotificationModel::toMap(const NotificationData &data) const
{
    return {
        {u"uid"_s, QVariant::fromValue(data.uid)},
        {u"daemonId"_s, data.daemonId},
        {u"appName"_s, data.appName},
        {u"appIcon"_s, data.appIcon},
        {u"summary"_s, data.summary},
        {u"body"_s, data.body},
        {u"image"_s, data.imageUrl},
        {u"actions"_s, data.actions},
        {u"urgency"_s, data.urgency},
        {u"progress"_s, data.progress},
        {u"category"_s, data.category},
        {u"accent"_s, data.accent.isValid() ? QVariant::fromValue(data.accent) : QVariant()},
        {u"transient"_s, data.transient},
        {u"timestamp"_s, data.timestamp},
    };
}

QVariantMap NotificationModel::latest() const
{
    return m_items.isEmpty() ? QVariantMap() : toMap(m_items.first());
}

void NotificationModel::setDoNotDisturb(bool dnd)
{
    if (m_dnd == dnd) {
        return;
    }
    m_dnd = dnd;
    Q_EMIT doNotDisturbChanged();
}

int NotificationModel::indexOfUid(quint64 uid) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).uid == uid) {
            return i;
        }
    }
    return -1;
}

void NotificationModel::onPosted(const NotificationData &notification)
{
    // A replacing notification (same daemon id) updates in place rather than
    // stacking up, which is what progress-bar style notifications rely on.
    if (notification.daemonId != 0) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (m_items.at(i).daemonId == notification.daemonId) {
                NotificationData merged = notification;
                merged.daemonId = m_items.at(i).daemonId;
                m_items[i] = merged;
                const QModelIndex idx = index(i, 0);
                Q_EMIT dataChanged(idx, idx);
                if (i == 0) {
                    Q_EMIT latestChanged();
                }
                if (!m_dnd) {
                    Q_EMIT arrived(toMap(merged));
                }
                return;
            }
        }
    }

    beginInsertRows({}, 0, 0);
    m_items.prepend(notification);
    endInsertRows();

    if (m_items.size() > HistoryLimit) {
        const int last = int(m_items.size()) - 1;
        beginRemoveRows({}, last, last);
        ImageStore::instance().remove(u"notification-%1"_s.arg(m_items.at(last).uid));
        m_items.removeLast();
        endRemoveRows();
    }

    Q_EMIT countChanged();
    Q_EMIT latestChanged();
    if (!m_dnd) {
        Q_EMIT arrived(toMap(notification));
    }
}

void NotificationModel::onIdAssigned(quint64 uid, quint32 daemonId)
{
    const int row = indexOfUid(uid);
    if (row < 0) {
        return;
    }
    m_items[row].daemonId = daemonId;
    const QModelIndex idx = index(row, 0);
    Q_EMIT dataChanged(idx, idx, {DaemonIdRole});
    if (row == 0) {
        Q_EMIT latestChanged();
    }
}

void NotificationModel::onClosed(quint32 daemonId, int reason)
{
    Q_UNUSED(reason)
    if (daemonId == 0) {
        return;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).daemonId != daemonId) {
            continue;
        }
        // Keep it in the history list, but let the island know it is gone by
        // clearing the id so a second close is a no-op.
        m_items[i].daemonId = 0;
        const QModelIndex idx = index(i, 0);
        Q_EMIT dataChanged(idx, idx, {DaemonIdRole});
        if (i == 0) {
            Q_EMIT latestChanged();
        }
        return;
    }
}

void NotificationModel::dismiss(quint64 uid)
{
    const int row = indexOfUid(uid);
    if (row < 0) {
        return;
    }
    beginRemoveRows({}, row, row);
    ImageStore::instance().remove(u"notification-%1"_s.arg(uid));
    m_items.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
    Q_EMIT latestChanged();
}

void NotificationModel::close(quint64 uid)
{
    const int row = indexOfUid(uid);
    if (row >= 0 && m_items.at(row).daemonId != 0) {
        auto message = QDBusMessage::createMethodCall(u"org.freedesktop.Notifications"_s,
                                                      u"/org/freedesktop/Notifications"_s,
                                                      u"org.freedesktop.Notifications"_s,
                                                      u"CloseNotification"_s);
        message << m_items.at(row).daemonId;
        QDBusConnection::sessionBus().asyncCall(message);
    }
    dismiss(uid);
}

void NotificationModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }
    beginResetModel();
    for (const NotificationData &item : std::as_const(m_items)) {
        ImageStore::instance().remove(u"notification-%1"_s.arg(item.uid));
    }
    m_items.clear();
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT latestChanged();
}

bool NotificationModel::invokeAction(quint64 uid, const QString &actionKey)
{
    const int row = indexOfUid(uid);
    if (row < 0 || m_items.at(row).daemonId == 0) {
        return false;
    }

    auto signal = QDBusMessage::createSignal(u"/org/freedesktop/Notifications"_s,
                                             u"org.freedesktop.Notifications"_s,
                                             u"ActionInvoked"_s);
    signal << m_items.at(row).daemonId << actionKey;
    const bool sent = QDBusConnection::sessionBus().send(signal);
    dismiss(uid);
    return sent;
}

void NotificationModel::open(quint64 uid)
{
    const int row = indexOfUid(uid);
    if (row < 0) {
        return;
    }
    const NotificationData item = m_items.at(row);

    // The polite path: Plasma's own daemon answers org.kde.Notifications on
    // the same object, and InvokeAction makes *it* emit ActionInvoked from its
    // well-known name - which senders accept, unlike a re-broadcast of ours.
    bool handedToSender = false;
    if (item.daemonId != 0 && item.actions.contains(u"default"_s)) {
        auto message = QDBusMessage::createMethodCall(u"org.freedesktop.Notifications"_s,
                                                      u"/org/freedesktop/Notifications"_s,
                                                      u"org.kde.Notifications"_s,
                                                      u"InvokeAction"_s);
        message << item.daemonId << u"default"_s;
        const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 300);
        handedToSender = reply.type() == QDBusMessage::ReplyMessage;
    }
    // Everywhere else, shout and hope: some senders take ActionInvoked from
    // anyone who says it loudly enough.
    if (!handedToSender && item.daemonId != 0) {
        auto signal = QDBusMessage::createSignal(u"/org/freedesktop/Notifications"_s,
                                                 u"org.freedesktop.Notifications"_s,
                                                 u"ActionInvoked"_s);
        signal << item.daemonId << u"default"_s;
        QDBusConnection::sessionBus().send(signal);
    }

    // And the practical one: whatever ignored all of that still gets brought
    // forward by name.
    if (m_config->value(u"notifications.openOnClick"_s, true).toBool() && m_activator) {
        m_activator->activate(item.desktopEntry, item.appName);
    }

    dismiss(uid);
}
