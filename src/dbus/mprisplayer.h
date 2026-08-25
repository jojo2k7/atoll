/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * One MPRIS2 media player, flattened into properties QML can bind to.
 *
 * Everything is driven by PropertiesChanged; the only polling is the playback
 * position, which the spec deliberately does not push.
 */
class MprisPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App.media")

    Q_PROPERTY(QString service READ service CONSTANT)
    Q_PROPERTY(QString identity READ identity NOTIFY identityChanged)
    Q_PROPERTY(QString desktopEntry READ desktopEntry NOTIFY identityChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY identityChanged)

    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY artChanged)
    /** xesam:url of the current track, which is a local path for local files. */
    Q_PROPERTY(QString url READ url NOTIFY metadataChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY artChanged)
    Q_PROPERTY(qint64 length READ length NOTIFY metadataChanged)

    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY playbackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY optionsChanged)
    Q_PROPERTY(QString loopStatus READ loopStatus WRITE setLoopStatus NOTIFY optionsChanged)

    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canPlay READ canPlay NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canPause READ canPause NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY capabilitiesChanged)
    /** True once the player has been seen exposing a Shuffle property at all. */
    Q_PROPERTY(bool canShuffle READ canShuffle NOTIFY optionsChanged)
    /** True once the player has been seen exposing a LoopStatus property at all. */
    Q_PROPERTY(bool canLoop READ canLoop NOTIFY optionsChanged)

public:
    explicit MprisPlayer(const QString &service, QObject *parent = nullptr);

    QString service() const
    {
        return m_service;
    }
    QString identity() const
    {
        return m_identity;
    }
    QString desktopEntry() const
    {
        return m_desktopEntry;
    }
    QString iconName() const;

    QString title() const
    {
        return m_title;
    }
    QString artist() const
    {
        return m_artist;
    }
    QString album() const
    {
        return m_album;
    }
    QString artUrl() const
    {
        return m_artUrl;
    }
    QString url() const
    {
        return m_url;
    }
    QColor accent() const
    {
        return m_accent;
    }
    qint64 length() const
    {
        return m_length;
    }

    QString playbackStatus() const
    {
        return m_playbackStatus;
    }
    bool playing() const;
    qint64 position() const
    {
        return m_position;
    }
    double volume() const
    {
        return m_volume;
    }
    Q_INVOKABLE void setVolume(double volume);
    bool shuffle() const
    {
        return m_shuffle;
    }
    Q_INVOKABLE void setShuffle(bool shuffle);
    QString loopStatus() const
    {
        return m_loopStatus;
    }
    Q_INVOKABLE void setLoopStatus(const QString &status);

    bool canGoNext() const
    {
        return m_canGoNext;
    }
    bool canGoPrevious() const
    {
        return m_canGoPrevious;
    }
    bool canPlay() const
    {
        return m_canPlay;
    }
    bool canPause() const
    {
        return m_canPause;
    }
    bool canSeek() const
    {
        return m_canSeek;
    }
    bool canShuffle() const
    {
        return m_canShuffle;
    }
    bool canLoop() const
    {
        return m_canLoop;
    }

    /** Wall-clock time this player last started playing, for pick-the-active. */
    QDateTime lastPlayedAt() const
    {
        return m_lastPlayedAt;
    }

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 offsetMicroseconds);
    Q_INVOKABLE void setPositionRatio(double ratio);
    Q_INVOKABLE void raise();

private Q_SLOTS:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    void onSeeked(qint64 position);

Q_SIGNALS:
    void identityChanged();
    void metadataChanged();
    void artChanged();
    void playbackChanged();
    void positionChanged();
    void volumeChanged();
    void optionsChanged();
    void capabilitiesChanged();

private:
    void fetchAll();
    void applyPlayerProperties(const QVariantMap &properties);
    void applyRootProperties(const QVariantMap &properties);
    void applyMetadata(const QVariantMap &metadata);
    void resolveArt(const QString &rawUrl);
    void call(const QString &method, const QVariantList &arguments = {});
    void refreshPosition();

    QString m_service;
    QString m_identity;
    QString m_desktopEntry;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_artUrl;
    QString m_url;
    QString m_rawArtUrl;
    QString m_trackId;
    QColor m_accent;
    qint64 m_length = 0;
    qint64 m_position = 0;
    double m_volume = 1.0;
    QString m_playbackStatus = QStringLiteral("Stopped");
    QString m_loopStatus;
    bool m_shuffle = false;
    bool m_canShuffle = false;
    bool m_canLoop = false;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
    bool m_canPlay = false;
    bool m_canPause = false;
    bool m_canSeek = false;
    QDateTime m_lastPlayedAt;
    QTimer m_positionTimer;
};
