/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "clock.h"

#include "config/config.h"

#include <QLocale>

using namespace Qt::StringLiterals;

Clock::Clock(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        Q_EMIT tick();
        scheduleNext();
    });
    connect(m_config, &Config::changed, this, &Clock::tick);
    scheduleNext();
}

void Clock::scheduleNext()
{
    const QTime current = QTime::currentTime();
    // Land just after the minute rolls over, never just before it.
    const int msUntilNextMinute = (60 - current.second()) * 1000 - current.msec() + 20;
    m_timer.start(qMax(200, msUntilNextMinute));
}

QString Clock::time() const
{
    const QString format = m_config->value(u"clock.timeFormat"_s, u"HH:mm"_s).toString();
    // A format with h but no ap/AP means 12-hour without the am/pm suffix
    // (Qt would otherwise treat a bare h as 24-hour).
    if (format.contains(u'h') && !format.contains(u"ap"_s, Qt::CaseInsensitive)) {
        const QString withSuffix = QLocale::system().toString(QDateTime::currentDateTime(), format + u" ap"_s);
        return withSuffix.left(withSuffix.lastIndexOf(u' '));
    }
    return QLocale::system().toString(QDateTime::currentDateTime(), format);
}

QString Clock::date() const
{
    const QString format = m_config->value(u"clock.dateFormat"_s, u"ddd d MMM"_s).toString();
    return QLocale::system().toString(QDateTime::currentDateTime(), format);
}
