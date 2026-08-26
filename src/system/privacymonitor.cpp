/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "privacymonitor.h"

#include "config/config.h"

#ifdef ATOLL_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/utils/dict.h>
#endif

using namespace Qt::StringLiterals;

PrivacyMonitor::PrivacyMonitor(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_recompute = new QTimer(this);
    m_recompute->setSingleShot(true);
    m_recompute->setInterval(0);
    // Graph changes arrive as bursts of registry events; recomputing once per
    // event loop turn keeps that to one walk instead of one per global.
    connect(m_recompute, &QTimer::timeout, this, &PrivacyMonitor::recompute);

    connect(m_config, &Config::changed, this, &PrivacyMonitor::syncEnabled);
    syncEnabled();
}

PrivacyMonitor::~PrivacyMonitor()
{
    stop();
}

void PrivacyMonitor::syncEnabled()
{
    const bool enabled = m_config->value(u"modules.privacy"_s, true).toBool();
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    if (enabled) {
        start();
    } else {
        stop();
    }
}

bool PrivacyMonitor::start()
{
#ifdef ATOLL_PIPEWIRE
    if (m_notifier) {
        return true;
    }

    static bool initialised = false;
    if (!initialised) {
        pw_init(nullptr, nullptr);
        initialised = true;
    }

    m_loop = pw_loop_new(nullptr);
    m_context = m_loop ? pw_context_new(m_loop, nullptr, 0) : nullptr;
    m_core = m_context ? pw_context_connect(m_context, nullptr, 0) : nullptr;
    if (!m_core) {
        qWarning("atoll: cannot reach PipeWire; camera, microphone and "
                 "screen-share indicators stay dark");
        stop();
        return false;
    }

    m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
    m_registryListener = new spa_hook{};
    static const pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY,
        .global = &PrivacyMonitor::globalEvent,
        .global_remove = &PrivacyMonitor::globalRemoveEvent,
    };
    pw_registry_add_listener(m_registry, m_registryListener, &registryEvents, this);

    m_notifier = new QSocketNotifier(pw_loop_get_fd(m_loop), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        if (pw_loop_iterate(m_loop, 0) < 0) {
            qWarning("atoll: PipeWire loop failed; privacy indicators stop here");
            stop();
            return;
        }
        scheduleRecompute();
    });
    return true;
#else
    return false;
#endif
}

void PrivacyMonitor::stop()
{
#ifdef ATOLL_PIPEWIRE
    delete m_notifier;
    m_notifier = nullptr;

    for (auto *hook : std::as_const(m_streamHooks)) {
        delete hook;
    }
    m_streamHooks.clear();
    for (auto *proxy : std::as_const(m_streamProxies)) {
        pw_proxy_destroy(proxy);
    }
    m_streamProxies.clear();

    if (m_registry) {
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_registry));
        m_registry = nullptr;
    }
    delete m_registryListener;
    m_registryListener = nullptr;
    if (m_core) {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }
    if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }
    if (m_loop) {
        pw_loop_destroy(m_loop);
        m_loop = nullptr;
    }
#endif

    if (!m_nodes.isEmpty() || !m_links.isEmpty() || !m_devices.isEmpty() || !m_streamStates.isEmpty()) {
        m_nodes.clear();
        m_links.clear();
        m_devices.clear();
        m_streamStates.clear();
        recompute();
    }
}

void PrivacyMonitor::onGlobal(quint32 id, const char *type, const struct spa_dict *props)
{
#ifdef ATOLL_PIPEWIRE
    QHash<QByteArray, QString> map;
    const struct spa_dict_item *item = nullptr;
    spa_dict_for_each(item, props)
    {
        map.insert(QByteArray(item->key), QString::fromUtf8(item->value));
    }

    if (strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
        Link link;
        link.outputNode = map.value(PW_KEY_LINK_OUTPUT_NODE).toUInt();
        link.inputNode = map.value(PW_KEY_LINK_INPUT_NODE).toUInt();
        if (link.outputNode != 0 && link.inputNode != 0) {
            m_links.insert(id, link);
            scheduleRecompute();
        }
        return;
    }
    if (strcmp(type, PW_TYPE_INTERFACE_Device) == 0) {
        m_devices.insert(id, map);
        scheduleRecompute();
        return;
    }
    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        m_nodes.insert(id, map);
        // Capture streams are watched for their state: only a running one
        // means something is really being recorded right now.
        if (map.value(PW_KEY_MEDIA_CLASS).startsWith(u"Stream/Input"_s) && !m_streamProxies.contains(id)) {
            static const pw_node_events nodeEvents = [] {
                pw_node_events events{};
                events.version = PW_VERSION_NODE_EVENTS;
                events.info = &PrivacyMonitor::nodeInfoEvent;
                return events;
            }();
            auto *proxy = static_cast<pw_proxy *>(
                    pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
            if (proxy) {
                auto *hook = new spa_hook{};
                pw_node_add_listener(reinterpret_cast<struct pw_node *>(proxy), hook, &nodeEvents, this);
                m_streamProxies.insert(id, proxy);
                m_streamHooks.insert(id, hook);
            }
        }
        scheduleRecompute();
    }
#else
    Q_UNUSED(id)
    Q_UNUSED(type)
    Q_UNUSED(props)
#endif
}

void PrivacyMonitor::onGlobalRemove(quint32 id)
{
#ifdef ATOLL_PIPEWIRE
    bool removed = m_links.remove(id);
    removed = m_nodes.remove(id) || removed;
    removed = m_devices.remove(id) || removed;
    if (m_streamStates.remove(id)) {
        removed = true;
    }
    if (auto *hook = m_streamHooks.take(id)) {
        delete hook;
    }
    if (auto *proxy = m_streamProxies.take(id)) {
        pw_proxy_destroy(proxy);
        removed = true;
    }
    if (removed) {
        scheduleRecompute();
    }
#else
    Q_UNUSED(id)
#endif
}

#ifdef ATOLL_PIPEWIRE
void PrivacyMonitor::nodeInfoEvent(void *data, const struct pw_node_info *info)
{
    auto *self = static_cast<PrivacyMonitor *>(data);
    if (!info) {
        return;
    }
    const int state = info->state >= 0 ? int(info->state) : -1;
    if (self->m_streamStates.value(info->id, -2) != state) {
        self->m_streamStates.insert(info->id, state);
        self->scheduleRecompute();
    }
}
#endif

void PrivacyMonitor::scheduleRecompute()
{
    m_recompute->start();
}

PrivacyMonitor::Source PrivacyMonitor::classify(const QHash<QByteArray, QString> &node,
                                                 const QHash<QByteArray, QString> &sink) const
{
    const QString mediaClass = node.value(PW_KEY_MEDIA_CLASS);

    if (mediaClass.startsWith(u"Video/Source"_s)) {
        // Hardware cameras announce their driver; screencast sources made by
        // the compositor for portal clients carry no such device.
        const QString api = node.value(PW_KEY_DEVICE_API);
        if (api == u"v4l2"_s || api == u"libcamera"_s) {
            return Source::Camera;
        }
        const QString deviceId = node.value(PW_KEY_DEVICE_ID);
        if (!deviceId.isEmpty()) {
            const QString deviceApi = m_devices.value(deviceId.toUInt()).value(PW_KEY_DEVICE_API);
            if (deviceApi == u"v4l2"_s || deviceApi == u"libcamera"_s) {
                return Source::Camera;
            }
        }
        return Source::Screen;
    }

    // Compositors push a screencast as their own output stream rather than as
    // a source: KWin's side of a screen share is "Stream/Output/Video".
    if (mediaClass == u"Stream/Output/Video"_s) {
        return Source::Screen;
    }

    if (mediaClass.startsWith(u"Audio/Source"_s)) {
        // Sink monitors feed visualisers and recorders of what is playing -
        // nobody is being listened to through one. Streams say so themselves,
        // and the ALSA monitor nodes carry the tell-tale name.
        if (sink.value(PW_KEY_STREAM_MONITOR) == u"true"_s) {
            return Source::None;
        }
        const bool monitor = node.value(PW_KEY_NODE_NAME).endsWith(u".monitor"_s);
        return monitor ? Source::None : Source::Microphone;
    }

    return Source::None;
}

void PrivacyMonitor::recompute()
{
#ifdef ATOLL_PIPEWIRE
    QStringList cameras;
    QStringList microphones;
    QStringList shares;

    for (const auto &[id, link] : m_links.asKeyValueRange()) {
        Q_UNUSED(id)
        const auto source = m_nodes.constFind(link.outputNode);
        const auto sink = m_nodes.constFind(link.inputNode);
        if (source == m_nodes.cend() || sink == m_nodes.cend()) {
            continue;
        }
        // Only capture streams count: something is recording or receiving.
        if (!sink->value(PW_KEY_MEDIA_CLASS).startsWith(u"Stream/Input"_s)) {
            continue;
        }
        // And only ones actually flowing. A suspended or idle stream is an
        // open tap with nothing coming out - Plasma keeps several of those
        // lying around for its own previews.
        if (m_streamStates.value(link.inputNode, -1) != PW_NODE_STATE_RUNNING) {
            continue;
        }
        const QString app = [&] {
            for (const QByteArray key : {PW_KEY_APP_NAME, PW_KEY_APP_PROCESS_BINARY, PW_KEY_NODE_DESCRIPTION,
                                         PW_KEY_NODE_NAME}) {
                const QString value = sink->value(key);
                if (!value.isEmpty()) {
                    return value;
                }
            }
            return QString();
        }();

        switch (classify(*source, *sink)) {
        case Source::Camera:
            cameras << app;
            break;
        case Source::Microphone:
            microphones << app;
            break;
        case Source::Screen:
            shares << app;
            break;
        case Source::None:
            break;
        }
    }

    cameras.removeDuplicates();
    cameras.sort();
    microphones.removeDuplicates();
    microphones.sort();
    shares.removeDuplicates();
    shares.sort();

    if (cameras != m_cameraUsers) {
        m_cameraUsers = cameras;
        Q_EMIT cameraChanged();
    }
    if (microphones != m_microphoneUsers) {
        m_microphoneUsers = microphones;
        Q_EMIT microphoneChanged();
    }
    if (shares != m_shareUsers) {
        m_shareUsers = shares;
        Q_EMIT shareChanged();
    }
#endif
}

#ifdef ATOLL_PIPEWIRE
void PrivacyMonitor::globalEvent(void *data, quint32 id, quint32 permissions, const char *type, quint32 version,
                                 const struct spa_dict *props)
{
    Q_UNUSED(permissions)
    Q_UNUSED(version)
    static_cast<PrivacyMonitor *>(data)->onGlobal(id, type, props);
}

void PrivacyMonitor::globalRemoveEvent(void *data, quint32 id)
{
    static_cast<PrivacyMonitor *>(data)->onGlobalRemove(id);
}
#endif
