/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "calendarservice.h"

#include "config/config.h"

#include <QDebug>
#include <QMultiHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr int kMaxOccurrencesPerEvent = 5000;
constexpr int kMaxRuleIterations = 100000;

const QHash<QString, int>& dayCodes()
{
    static const QHash<QString, int> codes = {
        {u"MO"_s, 1}, {u"TU"_s, 2}, {u"WE"_s, 3},
        {u"TH"_s, 4}, {u"FR"_s, 5}, {u"SA"_s, 6}, {u"SU"_s, 7},
    };
    return codes;
}

QDate mondayOfWeek(const QDate &d)
{
    return d.addDays(-(d.dayOfWeek() - 1));
}

} // namespace

CalendarService::CalendarService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    connect(config, &Config::changed, this, [this] {
        m_events.clear();
        applyIntervals();
        refresh();
    });

    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &CalendarService::refresh);

    // Recompute the visible lists periodically so statuses and the
    // "today" boundary stay current without refetching the feeds.
    m_recomputeTimer.setSingleShot(false);
    connect(&m_recomputeTimer, &QTimer::timeout, this, &CalendarService::computeUpcoming);

    applyIntervals();
    refresh();
}

void CalendarService::applyIntervals()
{
    const int fetchMinutes =
        qMax(1, m_config->value(u"calendar.fetchIntervalMinutes"_s, 15).toInt());
    const int recomputeSeconds = qBound(5,
        m_config->value(u"calendar.recomputeIntervalSeconds"_s, 60).toInt(), 3600);

    m_refreshTimer.setInterval(fetchMinutes * 60 * 1000);
    m_recomputeTimer.setInterval(recomputeSeconds * 1000);
    // Restart so a changed interval takes effect immediately.
    m_refreshTimer.start();
    m_recomputeTimer.start();
}

QVariantMap CalendarService::nextEvent() const
{
    return m_upcoming.isEmpty() ? QVariantMap{} : m_upcoming.first().toMap();
}

void CalendarService::refresh()
{
    const QVariantList sources = m_config->value(u"calendar.sources"_s, QVariantList{}).toList();
    if (sources.isEmpty()) {
        if (!m_upcoming.isEmpty() || !m_todayEvents.isEmpty()) {
            m_upcoming.clear();
            m_todayEvents.clear();
            Q_EMIT changed();
        }
        return;
    }

    m_events.clear();
    m_pendingFetches = sources.size();

    for (const QVariant &s : sources) {
        const QVariantMap source = s.toMap();
        const QString name = source.value(u"name"_s).toString();
        QString url = source.value(u"url"_s).toString().trimmed();
        // webcal:// is the same as https:// for our purposes; Qt Network does
        // not handle the scheme natively, so rewrite it here.
        if (url.startsWith(u"webcal://"_s)) {
            url = u"https://"_s + url.mid(9);
        } else if (url.startsWith(u"webcals://"_s)) {
            url = u"https://"_s + url.mid(10);
        }
        fetchSource(name, url);
    }
}

void CalendarService::fetchSource(const QString &name, const QString &url)
{
    if (url.isEmpty()) {
        if (--m_pendingFetches <= 0) {
            computeUpcoming();
        }
        return;
    }

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(10000);

    QNetworkReply *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            parseIcs(name, reply->readAll());
        } else {
            qWarning() << "CalendarService: Failed to fetch source" << name << reply->errorString();
        }
        if (--m_pendingFetches <= 0) {
            computeUpcoming();
        }
    });
}

void CalendarService::parseIcs(const QString &calendarName, const QByteArray &data)
{
    // RFC 5545 line unfolding: a CRLF followed by a space or tab is a
    // continuation of the previous line, not a new property.
    QString raw = QString::fromUtf8(data);
    raw.replace(u"\r\n "_s, u""_s);
    raw.replace(u"\r\n\t"_s, u""_s);
    raw.replace(u"\n "_s, u""_s);
    raw.replace(u"\n\t"_s, u""_s);

    const QStringList lines = raw.split(u'\n');

    bool inEvent = false;
    Event current;

    const auto finalizeEvent = [&] {
        if (!current.start.isValid()) {
            return;
        }
        if (!current.end.isValid()) {
            // RFC 5545: an all-day event without DTEND occupies one day,
            // a timed event without DTEND is treated as zero length; give
            // both a sensible minimum so they show up in the views.
            current.end = current.allDay ? current.start.addDays(1)
                                         : current.start.addSecs(3600);
        }
        if (current.recurrenceId.isValid()) {
            current.isOverride = true;
        }
        if (current.title.isEmpty()) {
            current.title = u"(no title)"_s;
        }
        m_events.append(current);
    };

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();

        if (line.compare(u"BEGIN:VEVENT"_s, Qt::CaseInsensitive) == 0) {
            inEvent = true;
            current = {};
            current.calendarName = calendarName;
            continue;
        }
        if (line.compare(u"END:VEVENT"_s, Qt::CaseInsensitive) == 0) {
            inEvent = false;
            finalizeEvent();
            current = {};
            continue;
        }
        if (!inEvent) {
            continue;
        }

        const int colonPos = line.indexOf(u':');
        if (colonPos < 0) {
            continue;
        }

        const QString propFull = line.left(colonPos);
        const QString value = line.mid(colonPos + 1);
        const QString propName = propFull.section(u';', 0, 0).trimmed().toUpper();
        const QString params = propFull.section(u';', 1).trimmed();

        if (propName == u"SUMMARY"_s) {
            current.title = unescapeText(value.trimmed());
        } else if (propName == u"UID"_s) {
            current.uid = value.trimmed();
        } else if (propName == u"RRULE"_s) {
            current.rrule = value.trimmed();
        } else if (propName == u"RECURRENCE-ID"_s) {
            current.recurrenceId = parseIcsDateTime(params, value);
        } else if (propName == u"EXDATE"_s) {
            for (const QString &part : value.split(u',')) {
                const QDateTime dt = parseIcsDateTime(params, part);
                if (dt.isValid()) {
                    current.exDates.append(dt);
                }
            }
        } else if (propName == u"DTSTART"_s) {
            const QString v = value.trimmed();
            current.allDay = params.contains(u"VALUE=DATE"_s, Qt::CaseInsensitive)
                             || (v.length() == 8 && !v.contains(u'T'));
            current.start = parseIcsDateTime(params, v);
        } else if (propName == u"DTEND"_s) {
            current.end = parseIcsDateTime(params, value.trimmed());
        }
    }
}

QList<CalendarService::Event> CalendarService::buildPool(const QDateTime &horizon) const
{
    // Starts that have been overridden by a RECURRENCE-ID instance replace
    // the generated occurrence of their series.
    QMultiHash<QString, QDateTime> overridden;
    for (const Event &ev : m_events) {
        if (ev.recurrenceId.isValid()) {
            overridden.insert(ev.uid, ev.recurrenceId);
        }
    }

    QList<Event> pool;
    pool.reserve(m_events.size() * 2);
    for (const Event &ev : m_events) {
        if (ev.rrule.isEmpty()) {
            pool.append(ev);
            continue;
        }
        QList<QDateTime> blocked = ev.exDates;
        blocked += overridden.values(ev.uid);
        pool += expandEvent(ev, horizon, blocked);
    }
    return pool;
}

QList<CalendarService::Event> CalendarService::expandEvent(const Event &ev, const QDateTime &horizon,
                                                           const QList<QDateTime> &blocked)
{
    // Parse "FREQ=WEEKLY;INTERVAL=2;COUNT=5;UNTIL=...;BYDAY=MO,WE"
    QHash<QString, QString> rule;
    for (const QString &kv : ev.rrule.toUpper().split(u';')) {
        const int eq = kv.indexOf(u'=');
        if (eq > 0) {
            rule.insert(kv.left(eq).trimmed(), kv.mid(eq + 1).trimmed());
        }
    }

    const QString freq = rule.value(u"FREQ"_s);
    if (freq != u"DAILY"_s && freq != u"WEEKLY"_s && freq != u"MONTHLY"_s
        && freq != u"YEARLY"_s) {
        return {ev};
    }

    int interval = rule.value(u"INTERVAL"_s, u"1"_s).toInt();
    if (interval < 1) {
        interval = 1;
    }
    const int countBound = rule.value(u"COUNT"_s).toInt();

    QDateTime until;
    if (rule.contains(u"UNTIL"_s)) {
        until = parseIcsDateTime(QString(), rule.value(u"UNTIL"_s));
    }

    QList<int> byDays;
    if (rule.contains(u"BYDAY"_s)) {
        for (const QString &d : rule.value(u"BYDAY"_s).split(u',')) {
            const QString code = d.trimmed().right(2);
            if (dayCodes().contains(code)) {
                byDays.append(dayCodes().value(code));
            }
        }
        std::sort(byDays.begin(), byDays.end());
    }

    QList<int> monthDays;
    if (freq == u"MONTHLY"_s && rule.contains(u"BYMONTHDAY"_s)) {
        for (const QString &md : rule.value(u"BYMONTHDAY"_s).split(u',')) {
            monthDays.append(md.trimmed().toInt());
        }
    }

    const qint64 durationMs = qMax<qint64>(0, ev.start.msecsTo(ev.end));
    const QTime time = ev.start.time();
    const QTimeZone tz = ev.start.timeZone();
    const auto atTime = [&time, &tz](const QDate &d) {
        return QDateTime(d, time, tz);
    };

    const auto isBlocked = [&blocked](const QDateTime &t) {
        return std::any_of(blocked.cbegin(), blocked.cend(),
                           [&t](const QDateTime &b) { return b == t; });
    };

    QList<Event> out;
    int generated = 0;
    bool stop = false;

    // Emits one candidate occurrence; returns false once the bound was hit
    // and no further candidates need to be considered.
    const auto consider = [&](const QDateTime &cand) {
        if (stop || cand > horizon) {
            stop = true;
            return;
        }
        if (until.isValid() && cand > until) {
            stop = true;
            return;
        }
        if (cand < ev.start) {
            return;
        }
        generated++;
        if (!isBlocked(cand)) {
            Event occ = ev;
            occ.start = cand;
            occ.end = cand.addMSecs(durationMs);
            occ.rrule.clear();
            occ.exDates.clear();
            out.append(occ);
            if (out.size() >= kMaxOccurrencesPerEvent) {
                stop = true;
            }
        }
        if (countBound > 0 && generated >= countBound) {
            stop = true;
        }
    };

    if (freq == u"DAILY"_s) {
        QDate d = ev.start.date();
        for (int i = 0; i < kMaxRuleIterations && !stop; ++i) {
            consider(atTime(d));
            d = d.addDays(interval);
        }
    } else if (freq == u"WEEKLY"_s) {
        QList<int> dows = byDays;
        if (dows.isEmpty()) {
            dows = {ev.start.date().dayOfWeek()};
        }
        const QDate firstMonday = mondayOfWeek(ev.start.date());
        for (int week = 0; week < kMaxRuleIterations / 7 && !stop; ++week) {
            const QDate anchor = firstMonday.addDays(7 * interval * week);
            for (int dow : dows) {
                consider(atTime(anchor.addDays(dow - 1)));
                if (stop) {
                    break;
                }
            }
        }
    } else if (freq == u"MONTHLY"_s) {
        // BYDAY entries may carry an ordinal prefix ("2TU", "-1FR"); plain
        // codes are not valid for MONTHLY per RFC, so treat them as ordinals
        // of the event's own weekday when none are given via BYMONTHDAY.
        struct NthDay {
            int ordinal = 0;
            int weekday = 0;
        };
        QList<NthDay> nthDays;
        if (rule.contains(u"BYDAY"_s)) {
            for (const QString &d : rule.value(u"BYDAY"_s).split(u',')) {
                const QString s = d.trimmed();
                const QString code = s.right(2);
                int ord = s.chopped(2).toInt();
                if (ord == 0) {
                    ord = 1;
                }
                if (dayCodes().contains(code)) {
                    nthDays.append({ord, dayCodes().value(code)});
                }
            }
        }

        QDate anchor(ev.start.date().year(), ev.start.date().month(), 1);
        for (int i = 0; i < kMaxRuleIterations && !stop; ++i) {
            const auto emitInMonth = [&](const QDate &d) {
                if (d.isValid() && d.month() == anchor.month() && d.year() == anchor.year()) {
                    consider(atTime(d));
                }
            };

            if (!nthDays.isEmpty()) {
                for (const NthDay &nd : nthDays) {
                    const QDate first(anchor.year(), anchor.month(), 1);
                    const int offset = (nd.weekday - first.dayOfWeek() + 7) % 7;
                    const QDate firstHit = first.addDays(offset);
                    if (nd.ordinal > 0) {
                        emitInMonth(firstHit.addDays(7 * (nd.ordinal - 1)));
                    } else {
                        const QDate last(anchor.year(), anchor.month(), first.daysInMonth());
                        const int back = (last.dayOfWeek() - nd.weekday + 7) % 7;
                        emitInMonth(last.addDays(-back + 7 * (nd.ordinal + 1)));
                    }
                    if (stop) {
                        break;
                    }
                }
            } else if (!monthDays.isEmpty()) {
                for (int md : monthDays) {
                    QDate d;
                    if (md > 0) {
                        d = QDate(anchor.year(), anchor.month(), md);
                    } else if (md < 0) {
                        d = QDate(anchor.year(), anchor.month(),
                                  anchor.daysInMonth() + md + 1);
                    }
                    emitInMonth(d);
                    if (stop) {
                        break;
                    }
                }
            } else {
                emitInMonth(QDate(anchor.year(), anchor.month(), ev.start.date().day()));
            }

            anchor = anchor.addMonths(interval);
        }
    } else if (freq == u"YEARLY"_s) {
        QDate d = ev.start.date();
        for (int i = 0; i < kMaxRuleIterations && !stop; ++i) {
            if (d.isValid()) {
                consider(atTime(d));
            }
            d = d.addYears(interval);
        }
    }

    return out;
}

void CalendarService::computeUpcoming()
{
    const QDateTime now = QDateTime::currentDateTime();
    const int lookaheadHours = m_config->value(u"calendar.lookaheadHours"_s, 24).toInt();
    const QDateTime cutoff = now.addSecs(static_cast<qint64>(lookaheadHours) * 3600);

    const QDate today = QDate::currentDate();
    const QDateTime todayEnd(today, QTime(23, 59, 59), QTimeZone::systemTimeZone());
    const QDateTime horizon = cutoff > todayEnd ? cutoff : todayEnd;

    const QList<Event> pool = buildPool(horizon);

    QList<Event> filtered;
    for (const Event &ev : pool) {
        const QDateTime start = ev.start.toTimeZone(QTimeZone::systemTimeZone());
        const QDateTime end = ev.end.toTimeZone(QTimeZone::systemTimeZone());

        // Include events that start within the lookahead window, or are ongoing.
        if (start <= cutoff && end > now) {
            filtered.append(ev);
        }
    }

    std::sort(filtered.begin(), filtered.end(), [](const Event &a, const Event &b) {
        return a.start < b.start;
    });

    if (filtered.size() > 5) {
        filtered.resize(5);
    }

    QVariantList result;
    for (const Event &ev : std::as_const(filtered)) {
        const QDateTime start = ev.start.toTimeZone(QTimeZone::systemTimeZone());
        const qint64 minsUntil = qMax<qint64>(0, now.secsTo(start) / 60);
        QVariantMap item;
        item[u"title"_s] = ev.title;
        item[u"startIso"_s] = start.toString(Qt::ISODate);
        item[u"minutesUntil"_s] = minsUntil;
        item[u"calendarName"_s] = ev.calendarName;
        result.append(item);
    }

    m_upcoming = result;
    computeToday(pool);
    Q_EMIT changed();
}

void CalendarService::computeToday(const QList<Event> &pool)
{
    const QDate today = QDate::currentDate();
    const QDateTime dayStart(today, QTime(0, 0), QTimeZone::systemTimeZone());
    const QDateTime dayEnd(today, QTime(23, 59, 59), QTimeZone::systemTimeZone());
    const QDateTime now = QDateTime::currentDateTime();

    QList<Event> filtered;
    for (const Event &ev : pool) {
        const QDateTime start = ev.start.toTimeZone(QTimeZone::systemTimeZone());
        const QDateTime end = ev.end.toTimeZone(QTimeZone::systemTimeZone());

        if (start <= dayEnd && end > dayStart) {
            filtered.append(ev);
        }
    }

    // All-day events first, then by start time.
    std::sort(filtered.begin(), filtered.end(), [](const Event &a, const Event &b) {
        if (a.allDay != b.allDay) {
            return a.allDay;
        }
        return a.start < b.start;
    });

    QVariantList result;
    for (const Event &ev : std::as_const(filtered)) {
        const QDateTime start = ev.start.toTimeZone(QTimeZone::systemTimeZone());
        const QDateTime end = ev.end.toTimeZone(QTimeZone::systemTimeZone());

        const QString status = end <= now ? u"past"_s
                             : start <= now ? u"ongoing"_s : u"upcoming"_s;
        const qint64 minsUntil = qMax<qint64>(0, now.secsTo(start) / 60);

        QVariantMap item;
        item[u"title"_s]        = ev.title;
        item[u"allDay"_s]       = ev.allDay;
        item[u"startTime"_s]    = ev.allDay ? QStringLiteral("All day")
                                            : start.time().toString(u"HH:mm"_s);
        item[u"endTime"_s]      = (ev.allDay || !ev.end.isValid())
                                  ? QString()
                                  : end.time().toString(u"HH:mm"_s);
        item[u"calendarName"_s] = ev.calendarName;
        item[u"status"_s]       = status;
        item[u"minutesUntil"_s] = minsUntil;
        result.append(item);
    }

    m_todayEvents = result;
}

QDateTime CalendarService::parseIcsDateTime(const QString &params, const QString &value)
{
    QString v = value.trimmed().toUpper();
    if (v.isEmpty()) {
        return {};
    }

    const bool isUtc = v.endsWith(u'Z');
    if (isUtc) {
        v.chop(1);
    }

    // Some providers (Outlook/Office365) emit fractional seconds: .000
    const int dotPos = v.indexOf(u'.');
    if (dotPos > 0) {
        v.truncate(dotPos);
    }

    QDateTime dt;
    if (!v.contains(u'T')) {
        const QDate d = QDate::fromString(v.remove(u'-'), u"yyyyMMdd"_s);
        if (!d.isValid()) {
            return {};
        }
        dt = QDateTime(d, QTime(0, 0), QTimeZone::systemTimeZone());
    } else {
        // Compact RFC 5545 form first, then the extended ISO forms some
        // non-conforming feeds use.
        dt = QDateTime::fromString(v, u"yyyyMMddTHHmmss"_s);
        if (!dt.isValid()) {
            dt = QDateTime::fromString(v, u"yyyyMMddTHHmm"_s);
        }
        if (!dt.isValid()) {
            dt = QDateTime::fromString(v, u"yyyy-MM-ddTHH:mm:ss"_s);
        }
        if (!dt.isValid()) {
            dt = QDateTime::fromString(v, u"yyyy-MM-ddTHH:mm"_s);
        }
        if (!dt.isValid()) {
            return {};
        }
    }

    if (isUtc) {
        dt.setTimeZone(QTimeZone::utc());
        return dt;
    }

    // Try to honour TZID if the system knows the timezone.
    const QString tzidMarker = u"TZID="_s;
    const int tzidPos = params.indexOf(tzidMarker, 0, Qt::CaseInsensitive);
    if (tzidPos >= 0) {
        QString tzid = params.mid(tzidPos + tzidMarker.length()).section(u';', 0, 0).trimmed();
        if (tzid.startsWith(u'"') && tzid.endsWith(u'"') && tzid.size() >= 2) {
            tzid = tzid.mid(1, tzid.size() - 2);
        }
        const QTimeZone tz(tzid.toLatin1());
        if (tz.isValid()) {
            dt.setTimeZone(tz);
            return dt;
        }
    }

    dt.setTimeZone(QTimeZone::systemTimeZone());
    return dt;
}

QString CalendarService::unescapeText(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] == u'\\' && i + 1 < s.size()) {
            const QChar next = s[i + 1];
            if (next == u'n' || next == u'N') {
                out += u'\n';
                ++i;
            } else if (next == u'\\') {
                out += u'\\';
                ++i;
            } else if (next == u',') {
                out += u',';
                ++i;
            } else if (next == u';') {
                out += u';';
                ++i;
            } else {
                out += s[i];
            }
        } else {
            out += s[i];
        }
    }
    return out;
}
