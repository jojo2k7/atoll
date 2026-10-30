// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
class Config;
class FocusService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.focus")
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
    explicit FocusService(Config *config, QObject *parent = nullptr);
    QVariantMap state() const;
    Q_INVOKABLE void start();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void extend();
    Q_INVOKABLE void next();
    Q_INVOKABLE void park(const QString &text);
    Q_INVOKABLE void clearNotes();
    void locked(bool locked);
    void activate() { m_authoritative = true; }
Q_SIGNALS:
    void changed();
    void started();
private:
    void tick();
    void audio();
    void save();
    void phase(const QString &name, int seconds);
    int number(const char *key, int fallback, int low, int high) const;
    bool option(const char *key, bool fallback) const;
    Config *m_config;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    QMediaPlayer m_player;
    QAudioOutput m_output;
    QString m_phase = "idle", m_error, m_path;
    QStringList m_notes;
    bool m_paused = false;
    bool m_authoritative = false;
    int m_remaining = 0, m_duration = 0, m_round = 0, m_completed = 0;
    qint64 m_remainder = 0;
};
