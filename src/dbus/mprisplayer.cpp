/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mprisplayer.h"

#include "app/imagestore.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusArgument>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QFileInfo>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView RootInterface{"org.mpris.MediaPlayer2"};
constexpr QLatin1StringView PlayerInterface{"org.mpris.MediaPlayer2.Player"};
constexpr QLatin1StringView ObjectPath{"/org/mpris/MediaPlayer2"};

QNetworkAccessManager *network()
{
    static QNetworkAccessManager manager;
    return &manager;
}

/**
 * Unwrap a QtDBus value into plain Qt types.
 *
 * Only the outermost container of a reply is demarshalled for us; anything
 * nested - and MPRIS metadata is a map inside a variant, with an array of
 * artists inside that - arrives as a QDBusArgument that QVariant::toMap() and
 * friends quietly turn into nothing.
 */
QVariant demarshall(const QVariant &value)
{
    if (value.canConvert<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    if (value.canConvert<QDBusVariant>()) {
        return demarshall(value.value<QDBusVariant>().variant());
    }
    if (!value.canConvert<QDBusArgument>()) {
        return value;
    }

    const QDBusArgument argument = value.value<QDBusArgument>();
    switch (argument.currentType()) {
    case QDBusArgument::MapType: {
        QVariantMap map;
        argument.beginMap();
        while (!argument.atEnd()) {
            QString key;
            QVariant entry;
            argument.beginMapEntry();
            argument >> key >> entry;
            argument.endMapEntry();
            map.insert(key, demarshall(entry));
        }
        argument.endMap();
        return map;
    }
    case QDBusArgument::ArrayType: {
        QVariantList list;
        argument.beginArray();
        while (!argument.atEnd()) {
            QVariant entry;
            argument >> entry;
            list.append(demarshall(entry));
        }
        argument.endArray();
        return list;
    }
    case QDBusArgument::VariantType:
    case QDBusArgument::BasicType: {
        QVariant inner;
        argument >> inner;
        return demarshall(inner);
    }
    default:
        return value;
    }
}
}

MprisPlayer::MprisPlayer(const QString &service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(m_service,
                QString(ObjectPath),
                u"org.freedesktop.DBus.Properties"_s,
                u"PropertiesChanged"_s,
                this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    bus.connect(m_service, QString(ObjectPath), QString(PlayerInterface), u"Seeked"_s, this, SLOT(onSeeked(qint64)));

    m_positionTimer.setInterval(500);
    connect(&m_positionTimer, &QTimer::timeout, this, &MprisPlayer::refreshPosition);

    fetchAll();
}

QString MprisPlayer::iconName() const
{
    if (!m_desktopEntry.isEmpty()) {
        return m_desktopEntry;
    }
    return m_identity.toLower().replace(u' ', u'-');
}

bool MprisPlayer::playing() const
{
    return m_playbackStatus == u"Playing"_s;
}

void MprisPlayer::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (interface == PlayerInterface) {
        applyPlayerProperties(changed);
    } else if (interface == RootInterface) {
        applyRootProperties(changed);
    }
}

void MprisPlayer::onSeeked(qint64 position)
{
    if (position != m_position) {
        m_position = position;
        Q_EMIT positionChanged();
    }
}

void MprisPlayer::fetchAll()
{
    const auto request = [this](const QString &interface, void (MprisPlayer::*apply)(const QVariantMap &)) {
        auto message = QDBusMessage::createMethodCall(m_service,
                                                      QString(ObjectPath),
                                                      u"org.freedesktop.DBus.Properties"_s,
                                                      u"GetAll"_s);
        message << interface;
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, apply](QDBusPendingCallWatcher *self) {
            self->deleteLater();
            const QDBusPendingReply<QVariantMap> reply = *self;
            if (!reply.isError()) {
                (this->*apply)(reply.value());
            }
        });
    };

    request(QString(RootInterface), &MprisPlayer::applyRootProperties);
    request(QString(PlayerInterface), &MprisPlayer::applyPlayerProperties);
}

void MprisPlayer::applyRootProperties(const QVariantMap &properties)
{
    bool changed = false;
    if (properties.contains(u"Identity"_s)) {
        const QString value = properties.value(u"Identity"_s).toString();
        changed |= std::exchange(m_identity, value) != value;
    }
    if (properties.contains(u"DesktopEntry"_s)) {
        const QString value = properties.value(u"DesktopEntry"_s).toString();
        changed |= std::exchange(m_desktopEntry, value) != value;
    }
    if (m_identity.isEmpty()) {
        m_identity = m_service.section(u'.', -1);
        changed = true;
    }
    if (changed) {
        Q_EMIT identityChanged();
    }
}

void MprisPlayer::applyPlayerProperties(const QVariantMap &properties)
{
    bool capabilities = false;
    bool options = false;

    const auto readBool = [&properties](const QString &key, bool &target, bool &flag) {
        if (properties.contains(key)) {
            const bool value = properties.value(key).toBool();
            if (target != value) {
                target = value;
                flag = true;
            }
        }
    };

    readBool(u"CanGoNext"_s, m_canGoNext, capabilities);
    readBool(u"CanGoPrevious"_s, m_canGoPrevious, capabilities);
    readBool(u"CanPlay"_s, m_canPlay, capabilities);
    readBool(u"CanPause"_s, m_canPause, capabilities);
    readBool(u"CanSeek"_s, m_canSeek, capabilities);
    readBool(u"Shuffle"_s, m_shuffle, options);

    // MPRIS has no "can shuffle"/"can loop" capability flag; a player that
    // supports these exposes the properties at all. Latch on first sight.
    if (!m_canShuffle && properties.contains(u"Shuffle"_s)) {
        m_canShuffle = true;
        options = true;
    }
    if (!m_canLoop && properties.contains(u"LoopStatus"_s)) {
        m_canLoop = true;
        options = true;
    }

    if (properties.contains(u"LoopStatus"_s)) {
        const QString value = properties.value(u"LoopStatus"_s).toString();
        if (m_loopStatus != value) {
            m_loopStatus = value;
            options = true;
        }
    }
    if (properties.contains(u"Volume"_s)) {
        const double value = properties.value(u"Volume"_s).toDouble();
        if (!qFuzzyCompare(m_volume + 1.0, value + 1.0)) {
            m_volume = value;
            Q_EMIT volumeChanged();
        }
    }
    if (properties.contains(u"Metadata"_s)) {
        applyMetadata(demarshall(properties.value(u"Metadata"_s)).toMap());
    }
    if (properties.contains(u"Position"_s)) {
        m_position = properties.value(u"Position"_s).toLongLong();
        Q_EMIT positionChanged();
    }
    if (properties.contains(u"PlaybackStatus"_s)) {
        const QString value = properties.value(u"PlaybackStatus"_s).toString();
        if (m_playbackStatus != value) {
            m_playbackStatus = value;
            if (playing()) {
                m_lastPlayedAt = QDateTime::currentDateTime();
                m_positionTimer.start();
                refreshPosition();
            } else {
                m_positionTimer.stop();
            }
            Q_EMIT playbackChanged();
        }
    }

    if (capabilities) {
        Q_EMIT capabilitiesChanged();
    }
    if (options) {
        Q_EMIT optionsChanged();
    }
}

void MprisPlayer::applyMetadata(const QVariantMap &metadata)
{
    const QString title = metadata.value(u"xesam:title"_s).toString();
    const QStringList artists = metadata.value(u"xesam:artist"_s).toStringList();
    const QString album = metadata.value(u"xesam:album"_s).toString();
    const qint64 length = metadata.value(u"mpris:length"_s).toLongLong();
    const QString trackId = metadata.value(u"mpris:trackid"_s).toString();
    const QString art = metadata.value(u"mpris:artUrl"_s).toString();
    const QString url = metadata.value(u"xesam:url"_s).toString();

    const QString artist = artists.join(u", "_s);
    const bool changed = title != m_title || artist != m_artist || album != m_album || length != m_length
        || url != m_url;

    m_title = title;
    m_artist = artist;
    m_album = album;
    m_length = length;
    m_trackId = trackId;
    m_url = url;

    if (changed) {
        Q_EMIT metadataChanged();
    }
    if (art != m_rawArtUrl) {
        m_rawArtUrl = art;
        resolveArt(art);
    }
}

void MprisPlayer::resolveArt(const QString &rawUrl)
{
    const auto publish = [this](const QString &url, const QImage &image) {
        m_artUrl = url;
        m_accent = image.isNull() ? QColor() : ImageStore::dominantColor(image);
        Q_EMIT artChanged();
    };

    if (rawUrl.isEmpty()) {
        publish({}, {});
        return;
    }

    const QUrl url(rawUrl);
    if (url.isLocalFile() || rawUrl.startsWith(u'/')) {
        const QString local = url.isLocalFile() ? url.toLocalFile() : rawUrl;
        if (!QFileInfo::exists(local)) {
            publish({}, {});
            return;
        }
        publish(QUrl::fromLocalFile(local).toString(), QImage(local));
        return;
    }

    if (url.scheme() != u"http"_s && url.scheme() != u"https"_s) {
        publish({}, {});
        return;
    }

    // Remote artwork (Spotify, web players): fetch once, keep it in the store.
    const QString key = u"art-%1"_s.arg(
        QString::fromLatin1(QCryptographicHash::hash(rawUrl.toUtf8(), QCryptographicHash::Md5).toHex()));

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(8000);
    QNetworkReply *reply = network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, rawUrl, publish] {
        reply->deleteLater();
        if (m_rawArtUrl != rawUrl) {
            return; // The track moved on while we were downloading.
        }
        if (reply->error() != QNetworkReply::NoError) {
            publish({}, {});
            return;
        }
        QImage image;
        if (!image.loadFromData(reply->readAll())) {
            publish({}, {});
            return;
        }
        publish(ImageStore::instance().put(key, image), image);
    });
}

void MprisPlayer::refreshPosition()
{
    auto message = QDBusMessage::createMethodCall(m_service,
                                                  QString(ObjectPath),
                                                  u"org.freedesktop.DBus.Properties"_s,
                                                  u"Get"_s);
    message << QString(PlayerInterface) << u"Position"_s;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *self;
        if (reply.isError()) {
            return;
        }
        const qint64 value = reply.value().variant().toLongLong();
        if (value != m_position) {
            m_position = value;
            Q_EMIT positionChanged();
        }
    });
}

void MprisPlayer::call(const QString &method, const QVariantList &arguments)
{
    auto message = QDBusMessage::createMethodCall(m_service, QString(ObjectPath), QString(PlayerInterface), method);
    message.setArguments(arguments);
    QDBusConnection::sessionBus().asyncCall(message);
}

void MprisPlayer::playPause()
{
    call(u"PlayPause"_s);
}

void MprisPlayer::play()
{
    call(u"Play"_s);
}

void MprisPlayer::pause()
{
    call(u"Pause"_s);
}

void MprisPlayer::stop()
{
    call(u"Stop"_s);
}

void MprisPlayer::next()
{
    call(u"Next"_s);
}

void MprisPlayer::previous()
{
    call(u"Previous"_s);
}

void MprisPlayer::seek(qint64 offsetMicroseconds)
{
    call(u"Seek"_s, {QVariant::fromValue(offsetMicroseconds)});
}

void MprisPlayer::setPositionRatio(double ratio)
{
    if (m_length <= 0 || m_trackId.isEmpty()) {
        return;
    }
    const qint64 target = qint64(qBound(0.0, ratio, 1.0) * double(m_length));
    auto message = QDBusMessage::createMethodCall(m_service, QString(ObjectPath), QString(PlayerInterface), u"SetPosition"_s);
    message << QVariant::fromValue(QDBusObjectPath(m_trackId)) << QVariant::fromValue(target);
    QDBusConnection::sessionBus().asyncCall(message);
    m_position = target;
    Q_EMIT positionChanged();
}

void MprisPlayer::raise()
{
    auto message = QDBusMessage::createMethodCall(m_service, QString(ObjectPath), QString(RootInterface), u"Raise"_s);
    QDBusConnection::sessionBus().asyncCall(message);
}

void MprisPlayer::setVolume(double volume)
{
    const double clamped = qBound(0.0, volume, 1.0);
    auto message = QDBusMessage::createMethodCall(m_service,
                                                  QString(ObjectPath),
                                                  u"org.freedesktop.DBus.Properties"_s,
                                                  u"Set"_s);
    message << QString(PlayerInterface) << u"Volume"_s << QVariant::fromValue(QDBusVariant(clamped));
    QDBusConnection::sessionBus().asyncCall(message);
    m_volume = clamped;
    Q_EMIT volumeChanged();
}

void MprisPlayer::setShuffle(bool shuffle)
{
    auto message = QDBusMessage::createMethodCall(m_service,
                                                  QString(ObjectPath),
                                                  u"org.freedesktop.DBus.Properties"_s,
                                                  u"Set"_s);
    message << QString(PlayerInterface) << u"Shuffle"_s << QVariant::fromValue(QDBusVariant(shuffle));
    QDBusConnection::sessionBus().asyncCall(message);
    m_shuffle = shuffle;
    Q_EMIT optionsChanged();
}

void MprisPlayer::setLoopStatus(const QString &status)
{
    auto message = QDBusMessage::createMethodCall(m_service,
                                                  QString(ObjectPath),
                                                  u"org.freedesktop.DBus.Properties"_s,
                                                  u"Set"_s);
    message << QString(PlayerInterface) << u"LoopStatus"_s << QVariant::fromValue(QDBusVariant(status));
    QDBusConnection::sessionBus().asyncCall(message);
    m_loopStatus = status;
    Q_EMIT optionsChanged();
}
