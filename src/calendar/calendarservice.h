/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class Config;

/**
 * Fetches and parses ICS calendar feeds (Google Calendar, Apple iCloud, etc.)
 * and exposes upcoming events to QML.
 *
 * Sources are configured as an array under "calendar.sources" in atoll.json,
 * each entry being {"name": "...", "url": "..."} . Both https:// and webcal://
 * URLs are accepted.
 */
class CalendarService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App.calendar")

    Q_PROPERTY(bool hasEvents READ hasEvents NOTIFY changed)
    Q_PROPERTY(QVariantMap nextEvent READ nextEvent NOTIFY changed)
    Q_PROPERTY(QVariantList upcomingEvents READ upcomingEvents NOTIFY changed)
    Q_PROPERTY(QVariantList todayEvents READ todayEvents NOTIFY changed)

public:
    explicit CalendarService(Config *config, QObject *parent = nullptr);

    bool hasEvents() const
    {
        return !m_upcoming.isEmpty();
    }
    QVariantMap nextEvent() const;
    QVariantList upcomingEvents() const
    {
        return m_upcoming;
    }
    QVariantList todayEvents() const
    {
        return m_todayEvents;
    }

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    struct Event {
        QString title;
        QString uid;
        QDateTime start;
        QDateTime end;
        QDateTime recurrenceId;
        QString calendarName;
        bool allDay = false;
        bool isOverride = false;
        QString rrule;
        QList<QDateTime> exDates;
    };

    void fetchSource(const QString &name, const QString &url);
    void parseIcs(const QString &calendarName, const QByteArray &data);
    void computeUpcoming();
    void computeToday(const QList<Event> &pool);
    void applyIntervals();

    QList<Event> buildPool(const QDateTime &horizon) const;
    static QList<Event> expandEvent(const Event &ev, const QDateTime &horizon,
                                    const QList<QDateTime> &blocked);

    static QDateTime parseIcsDateTime(const QString &params, const QString &value);
    static QString unescapeText(const QString &s);

    Config *m_config = nullptr;
    QNetworkAccessManager m_network;
    QTimer m_refreshTimer;
    QTimer m_recomputeTimer;
    QList<Event> m_events;
    QVariantList m_upcoming;
    QVariantList m_todayEvents;
    int m_pendingFetches = 0;
};

