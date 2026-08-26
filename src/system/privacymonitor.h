/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QSocketNotifier>
#include <QStringList>
#include <QTimer>

class Config;

struct pw_loop;
struct pw_context;
struct pw_core;
struct pw_registry;
struct pw_proxy;
struct pw_node_info;
struct spa_hook;

/**
 * Watches the PipeWire graph for the three things a privacy indicator is
 * about: an application capturing from a camera, one recording a microphone,
 * and one receiving your screen.
 *
 * Nothing here polls. The registry pushes every global as it appears, so the
 * moment a voice call opens its capture stream - or the browser starts pulling
 * frames off the webcam, or KWin begins feeding a screencast to a portal
 * client - the links that WirePlumber made between the stream and its source
 * say so, and the island grows the matching dot until the link is gone.
 *
 * Classification walks each link backwards: the consumer must be a capture
 * stream ("Stream/Input/*"), and the source it drinks from decides the kind -
 * a v4l2 or libcamera device means camera, any other non-monitor audio source
 * means microphone, and a synthetic video source (KWin's screencast) means the
 * screen is being shared. Sink monitors are deliberately ignored: a music
 * visualiser reading the desktop audio is not a privacy event.
 *
 * Requires libpipewire; without it every property simply stays false.
 */
class PrivacyMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool cameraActive READ cameraActive NOTIFY cameraChanged)
    Q_PROPERTY(bool microphoneActive READ microphoneActive NOTIFY microphoneChanged)
    Q_PROPERTY(bool shareActive READ shareActive NOTIFY shareChanged)
    /** Application names behind each indicator, for tooltips. */
    Q_PROPERTY(QStringList cameraUsers READ cameraUsers NOTIFY cameraChanged)
    Q_PROPERTY(QStringList microphoneUsers READ microphoneUsers NOTIFY microphoneChanged)
    Q_PROPERTY(QStringList shareUsers READ shareUsers NOTIFY shareChanged)

public:
    explicit PrivacyMonitor(Config *config, QObject *parent = nullptr);
    ~PrivacyMonitor() override;

    bool cameraActive() const
    {
        return !m_cameraUsers.isEmpty();
    }
    bool microphoneActive() const
    {
        return !m_microphoneUsers.isEmpty();
    }
    bool shareActive() const
    {
        return !m_shareUsers.isEmpty();
    }
    QStringList cameraUsers() const
    {
        return m_cameraUsers;
    }
    QStringList microphoneUsers() const
    {
        return m_microphoneUsers;
    }
    QStringList shareUsers() const
    {
        return m_shareUsers;
    }

private:
    enum class Source { None, Camera, Microphone, Screen };

    void syncEnabled();
    bool start();
    void stop();
    void onGlobal(quint32 id, const char *type, const struct spa_dict *props);
    void onGlobalRemove(quint32 id);
    void scheduleRecompute();
    void recompute();
    Source classify(const QHash<QByteArray, QString> &node, const QHash<QByteArray, QString> &sink) const;

#ifdef ATOLL_PIPEWIRE
    static void globalEvent(void *data, quint32 id, quint32 permissions, const char *type, quint32 version,
                            const struct spa_dict *props);
    static void globalRemoveEvent(void *data, quint32 id);
    static void nodeInfoEvent(void *data, const struct pw_node_info *info);
#endif

    Config *m_config;

#ifdef ATOLL_PIPEWIRE
    pw_loop *m_loop = nullptr;
    pw_context *m_context = nullptr;
    pw_core *m_core = nullptr;
    pw_registry *m_registry = nullptr;
    spa_hook *m_registryListener = nullptr;
#endif
    QSocketNotifier *m_notifier = nullptr;
    QTimer *m_recompute = nullptr;
    bool m_enabled = false;

    QHash<quint32, QHash<QByteArray, QString>> m_nodes;
    QHash<quint32, QHash<QByteArray, QString>> m_devices;
    /**
     * Capture-stream nodes get their own proxy, because the registry only
     * hands out properties: whether a stream is actually flowing is state,
     * and Plasma keeps suspended screencast links lying around that would
     * otherwise light the screen-share dot forever.
     */
    QHash<quint32, pw_proxy *> m_streamProxies;
    QHash<quint32, spa_hook *> m_streamHooks;
    QHash<quint32, int> m_streamStates; //!< pw_node_state per capture stream
    struct Link {
        quint32 outputNode = 0;
        quint32 inputNode = 0;
    };
    QHash<quint32, Link> m_links;

    QStringList m_cameraUsers;
    QStringList m_microphoneUsers;
    QStringList m_shareUsers;

Q_SIGNALS:
    void cameraChanged();
    void microphoneChanged();
    void shareChanged();
};
