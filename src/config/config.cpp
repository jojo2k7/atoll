/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJSValue>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

using namespace Qt::StringLiterals;

Config::Config(QObject *parent)
    : QObject(parent)
{
    m_path = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                 .filePath(u"atoll/atoll.json"_s);
    m_data = defaults();

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        // Editors replace rather than rewrite; give the new inode a moment to land.
        QTimer::singleShot(120, this, &Config::reload);
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        QTimer::singleShot(120, this, &Config::reload);
    });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(220);
    connect(&m_saveTimer, &QTimer::timeout, this, &Config::save);

    reload();
}

QVariantMap Config::defaults()
{
    static const char *json = R"JSON({
  "island": {
    "screens": ["primary"],
    "position": "top-center",
    "shape": "notch",
    "layer": "overlay",
    "overlapPanels": true,
    "exclusiveZone": 0,
    "edgeMargin": 0,
    "sideMargin": 24,
    "collapsedWidth": 168,
    "collapsedHeight": 32,
    "expandedWidth": 460,
    "maxWidth": 620,
    "cornerRadius": 0,
    "idleMode": "auto",
    "alwaysVisible": true,
    "surfaceHeight": 700
  },
  "appearance": {
    "background": "#0b0b0e",
    "backgroundOpacity": 0.97,
    "foreground": "#f4f4f7",
    "muted": "#9a9aa6",
    "accent": "auto",
    "accentFallback": "#5aa2ff",
    "fontFamily": "",
    "fontScale": 1.0,
    "shadow": true,
    "shadowOpacity": 0.45,
    "border": true,
    "borderColor": "#1affffff"
  },
  "effects": {
    "gooey": true,
    "gooeyStrength": 0.62,
    "spring": 4.2,
    "damping": 0.36,
    "animationScale": 1.0
  },
  "modules": {
    "osd": true,
    "notifications": true,
    "media": true,
    "battery": true,
    "bluetooth": true,
    "visualizer": true,
    "clock": true,
    "lyrics": true,
    "sharing": true,
    "ai": true
  },
  "osd": {
    "timeout": 1700,
    "showMediaPlayerVolume": true
  },
  "notifications": {
    "timeout": 5000,
    "trackIds": true,
    "ignoredApps": [],
    "criticalStaysOpen": true,
    "maxBodyLines": 3,
    "dnd": false,
    "showActions": true
  },
  "media": {
    "showOnPlay": true,
    "peekDuration": 4200,
    "preferred": [],
    "blocked": [],
    "visualizerBars": 26,
    "cava": "auto",
    "showArt": true,
    "showAlbum": true,
    "idleBadge": true
  },
  "lockScreen": {
    "enabled": true,
    "showMedia": true,
    "showNotifications": false,
    "allowExpanding": false
  },
  "lyrics": {
    "enabled": true,
    "showInIsland": true,
    "showInExpanded": true,
    "seekOnClick": true,
    "offsetMs": 0,
    "cache": true
  },
  "sharing": {
    "alias": "",
    "receive": true,
    "autoAccept": false,
    "saveDirectory": "",
    "port": 53317,
    "multicast": "224.0.0.167"
  },
  "bluetooth": {
    "showInExpanded": false
  },
  "ai": {
    "enabled": true,
    "provider": "claude-cli",
    "cliPath": "",
    "model": "",
    "effort": "high",
    "maxTokens": 16000,
    "baseUrl": "",
    "webSearch": true,
    "allowScreenshots": true,
    "systemPrompt": "",
    "commandTimeout": 180,
    "longPressMs": 450,
    "panelWidth": 560,
    "glow": true,
    "glowIntensity": 0.8,
    "glowThickness": 130,
    "avatar": true,
    "permissions": {
      "mode": "guarded",
      "allowRoot": true
    }
  },
  "behavior": {
    "expandOnHover": false,
    "hoverPeek": true,
    "hoverDelay": 180,
    "collapseOnLeave": true,
    "scrollAdjustsVolume": true,
    "volumeStep": 5,
    "clickAction": "expand",
    "middleClickAction": "playPause",
    "rightClickAction": "settings"
  },
  "clock": {
    "timeFormat": "HH:mm",
    "dateFormat": "ddd d MMM"
  },
  "idle": {
    "showClock": true,
    "showDate": false,
    "showMediaBadge": true,
    "showNotificationDot": true,
    "showBatteryDot": true,
    "showCalendarHint": true
  }
})JSON";

    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(QByteArray(json), &err);
    Q_ASSERT_X(err.error == QJsonParseError::NoError, "Config::defaults", qPrintable(err.errorString()));
    return doc.object().toVariantMap();
}

void Config::deepMerge(QVariantMap &target, const QVariantMap &overlay)
{
    for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) {
        const QVariant &incoming = it.value();
        const QVariant existing = target.value(it.key());
        if (incoming.typeId() == QMetaType::QVariantMap && existing.typeId() == QMetaType::QVariantMap) {
            QVariantMap sub = existing.toMap();
            deepMerge(sub, incoming.toMap());
            target.insert(it.key(), sub);
        } else {
            target.insert(it.key(), incoming);
        }
    }
}

void Config::reload()
{
    QVariantMap merged = defaults();

    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning("atoll: %s is not valid JSON (%s at offset %d) - using defaults",
                     qUtf8Printable(m_path), qUtf8Printable(err.errorString()), err.offset);
        } else if (doc.isObject()) {
            deepMerge(merged, doc.object().toVariantMap());
        }
    }

    applyWatch();

    if (merged != m_data) {
        m_data = merged;
        Q_EMIT changed();
    }
}

void Config::applyWatch()
{
    const QString dir = QFileInfo(m_path).absolutePath();
    if (!m_watcher.directories().contains(dir) && QDir(dir).exists()) {
        m_watcher.addPath(dir);
    }
    if (!m_watcher.files().contains(m_path) && QFile::exists(m_path)) {
        m_watcher.addPath(m_path);
    }
}

QVariant Config::value(const QString &dottedKey, const QVariant &fallback) const
{
    const QStringList parts = dottedKey.split(u'.', Qt::SkipEmptyParts);
    QVariant cursor = m_data;
    for (const QString &part : parts) {
        if (cursor.typeId() != QMetaType::QVariantMap) {
            return fallback;
        }
        const QVariantMap map = cursor.toMap();
        if (!map.contains(part)) {
            return fallback;
        }
        cursor = map.value(part);
    }
    return cursor.isValid() ? cursor : fallback;
}

QVariant Config::defaultValue(const QString &dottedKey) const
{
    static const QVariantMap builtin = defaults();
    const QStringList parts = dottedKey.split(u'.', Qt::SkipEmptyParts);
    QVariant cursor = builtin;
    for (const QString &part : parts) {
        if (cursor.typeId() != QMetaType::QVariantMap) {
            return {};
        }
        cursor = cursor.toMap().value(part);
    }
    return cursor;
}

bool Config::insertAt(QVariantMap &target, const QStringList &parts, const QVariant &value)
{
    if (parts.isEmpty()) {
        return false;
    }
    if (parts.size() == 1) {
        if (target.value(parts.first()) == value && target.contains(parts.first())) {
            return false;
        }
        target.insert(parts.first(), value);
        return true;
    }

    QVariantMap sub = target.value(parts.first()).toMap();
    if (!insertAt(sub, parts.mid(1), value)) {
        return false;
    }
    target.insert(parts.first(), sub);
    return true;
}

void Config::setValue(const QString &dottedKey, const QVariant &value)
{
    const QStringList parts = dottedKey.split(u'.', Qt::SkipEmptyParts);

    // A JS array or object arrives wrapped in a QJSValue, and QJsonValue has
    // no idea what that is - it would quietly write null and take the setting
    // with it. Unwrap first, then normalise, so a slider does not persist as a
    // double where an int was meant.
    QVariant plain = value;
    if (plain.userType() == qMetaTypeId<QJSValue>()) {
        plain = plain.value<QJSValue>().toVariant();
    }

    const QVariant normalised = QJsonValue::fromVariant(plain).toVariant();
    if (plain.isValid() && normalised.isNull()) {
        qWarning("atoll: refusing to store %s, its value cannot be written as JSON",
                 qUtf8Printable(dottedKey));
        return;
    }
    if (!insertAt(m_data, parts, normalised)) {
        return;
    }
    Q_EMIT changed();
    m_saveTimer.start();
}

void Config::resetValue(const QString &dottedKey)
{
    setValue(dottedKey, defaultValue(dottedKey));
}

void Config::resetAll()
{
    const QVariantMap builtin = defaults();
    if (builtin == m_data) {
        return;
    }
    m_data = builtin;
    Q_EMIT changed();
    m_saveTimer.start();
}

void Config::save()
{
    const QFileInfo info(m_path);
    QDir().mkpath(info.absolutePath());

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("atoll: cannot write %s", qUtf8Printable(m_path));
        return;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(m_data)).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning("atoll: cannot commit %s", qUtf8Printable(m_path));
        return;
    }
    applyWatch();
}

void Config::ensureUserFile()
{
    if (QFile::exists(m_path)) {
        return;
    }
    save();
}
