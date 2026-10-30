// SPDX-License-Identifier: GPL-3.0-or-later
#include "focusservice.h"
#include "config/config.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

FocusService::FocusService(Config *config, QObject *parent) : QObject(parent), m_config(config) {
    m_path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/focus-state.json";
    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) {
        const auto s = QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
        m_notes = s.value("notes").toStringList();
        m_completed = s.value("completed").toInt();
        const QString p = s.value("phase").toString();
        if (p == "focus" || p == "break") {
            m_phase = p; m_paused = true;
            m_duration = qBound(1, s.value("duration").toInt(), 86400);
            m_remaining = qBound(1, s.value("remaining").toInt(), m_duration);
            m_round = qMax(1, s.value("round").toInt());
        }
    }
    m_player.setAudioOutput(&m_output);
    m_player.setLoops(QMediaPlayer::Infinite);
    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &error) {
        m_error = error; Q_EMIT changed();
    });
    connect(config, &Config::changed, this, [this] { audio(); Q_EMIT changed(); });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { if (m_authoritative) save(); });
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &FocusService::tick);
    m_elapsed.start(); m_timer.start();
}
int FocusService::number(const char *key, int fallback, int low, int high) const {
    return qBound(low, m_config->value(QString("focus.") + key, fallback).toInt(), high);
}
bool FocusService::option(const char *key, bool fallback) const {
    return m_config->value(QString("focus.") + key, fallback).toBool();
}
QVariantMap FocusService::state() const {
    return {{"phase", m_phase}, {"active", m_phase != "idle"}, {"paused", m_paused},
        {"remaining", m_remaining}, {"progress", m_duration ? 1.0 - double(m_remaining)/m_duration : 0.0},
        {"round", m_round}, {"completed", m_completed}, {"notes", m_notes}, {"error", m_error},
        {"quiet", m_phase == "focus" && !m_paused && option("quietNotifications", true)}};
}
void FocusService::save() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    auto s = state(); s.insert("duration", m_duration);
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) { file.write(QJsonDocument(QJsonObject::fromVariantMap(s)).toJson()); file.commit(); }
}
void FocusService::phase(const QString &name, int seconds) {
    m_phase = name; m_duration = seconds; m_remaining = seconds;
    m_remainder = 0; m_elapsed.restart();
    audio(); save(); Q_EMIT changed();
}
void FocusService::start() {
    if (m_phase != "idle") return;
    m_paused = false; m_round = 1; m_error.clear();
    phase("focus", number("minutes", 25, 1, 240) * 60); Q_EMIT started();
}
void FocusService::togglePause() {
    if (m_phase == "idle") return;
    m_paused = !m_paused; m_elapsed.restart(); audio(); save(); Q_EMIT changed();
}
void FocusService::stop() { m_paused = false; phase("idle", 0); }
void FocusService::extend() {
    if (m_phase == "idle") return;
    const int extra = number("extensionMinutes", 5, 1, 60) * 60;
    m_remaining = qMin(86400, m_remaining + extra); m_duration = qMin(86400, m_duration + extra);
    save(); Q_EMIT changed();
}
void FocusService::next() {
    if (m_phase == "idle") return;
    if (m_phase == "focus") {
        const bool longBreak = m_round % number("rounds", 4, 1, 12) == 0;
        m_paused = !option("autoBreak", true);
        phase("break", number(longBreak ? "longBreakMinutes" : "breakMinutes", longBreak ? 15 : 5, 1, 120) * 60);
    } else {
        ++m_round; m_paused = !option("autoFocus", false);
        phase("focus", number("minutes", 25, 1, 240) * 60);
    }
}
void FocusService::park(const QString &text) {
    if (text.trimmed().isEmpty()) return;
    m_notes.append(text.trimmed().left(500));
    while (m_notes.size() > 100) m_notes.removeFirst();
    save(); Q_EMIT changed();
}
void FocusService::clearNotes() { m_notes.clear(); save(); Q_EMIT changed(); }
void FocusService::locked(bool locked) {
    if (locked && m_phase != "idle" && !m_paused && option("pauseOnLock", true)) togglePause();
}
void FocusService::tick() {
    const qint64 elapsed = m_elapsed.restart();
    if (m_phase != "idle" && !m_paused) {
        // Suspend is a pause, never silently count hours of sleep as focus.
        if (elapsed > 10000) { togglePause(); return; }
        m_remainder += elapsed;
        if (m_remainder >= 1000) {
            m_remaining = qMax(0, m_remaining - int(m_remainder / 1000)); m_remainder %= 1000;
            if (!m_remaining) { if (m_phase == "focus") ++m_completed; next(); }
            else { if (m_remaining % 10 == 0) save(); Q_EMIT changed(); }
        }
    }
    const bool playing = m_phase != "idle" && !m_paused && option("soundEnabled", true)
                         && (m_phase == "focus" || option("soundDuringBreak", false));
    const float target = playing ? number("volume", 25, 0, 100) / 100.f : 0.f;
    const float step = 0.1f / qMax(0.1f, number("fadeSeconds", 3, 0, 30) * 1.f);
    const float current = m_output.volume();
    m_output.setVolume(current < target ? qMin(target, current + step) : qMax(target, current - step));
    if (!playing && m_output.volume() < .001f) m_player.pause();
}
void FocusService::audio() {
    const QString path = m_config->value("focus.soundFile").toString();
    const QUrl source = path.startsWith("file:") ? QUrl(path) : QUrl::fromLocalFile(path);
    if (source != m_player.source()) { m_player.setSource(source); m_error.clear(); }
    if (m_phase != "idle" && !m_paused && option("soundEnabled", true)
        && (m_phase == "focus" || option("soundDuringBreak", false))) {
        if (!QFileInfo::exists(source.toLocalFile())) m_error = tr("Choose an existing audio file in Focus settings.");
        else { m_error.clear(); if (m_player.playbackState() != QMediaPlayer::PlayingState) { m_output.setVolume(0); m_player.play(); } }
    }
}
