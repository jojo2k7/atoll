// SPDX-License-Identifier: GPL-3.0-or-later
#include "audiooutputs.h"
#include "config/config.h"
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusMetaType>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <memory>

using Interfaces = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, Interfaces>;
Q_DECLARE_METATYPE(Interfaces)
Q_DECLARE_METATYPE(ManagedObjects)

AudioOutputs::AudioOutputs(Config *config, QObject *parent) : QObject(parent), m_config(config) {
    qDBusRegisterMetaType<Interfaces>();
    qDBusRegisterMetaType<ManagedObjects>();
    m_poll.setInterval(4000);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    connect(&m_poll, &QTimer::timeout, this, &AudioOutputs::refresh);
    connect(&m_debounce, &QTimer::timeout, this, &AudioOutputs::refresh);
    connect(m_config, &Config::changed, this, &AudioOutputs::rebuild);
    connect(&m_subscription, &QProcess::readyReadStandardOutput, this, [this] {
        m_subscription.readAllStandardOutput();
        if (!m_debounce.isActive()) m_debounce.start();
    });
}
AudioOutputs::~AudioOutputs() {
    m_subscription.kill();
    m_subscription.waitForFinished(500);
    for (auto *process : findChildren<QProcess *>()) {
        process->disconnect(this);
        process->kill();
        process->waitForFinished(500);
    }
}
void AudioOutputs::start() { refresh(); m_poll.start(); }
QString AudioOutputs::currentId() const {
    for (const auto &v : m_outputs) if (v.toMap().value("active").toBool()) return v.toMap().value("id").toString();
    return {};
}
QString AudioOutputs::currentName() const {
    return find(currentId()).value("label", tr("Audio output")).toString();
}
bool AudioOutputs::bluetoothActive() const { return find(currentId()).value("bluetooth").toBool(); }
QVariantMap AudioOutputs::find(const QString &id) const {
    for (const auto &v : m_outputs) if (v.toMap().value("id").toString() == id) return v.toMap();
    return {};
}
void AudioOutputs::pactl(const QStringList &args, Done done) {
    const auto executable = QStandardPaths::findExecutable("pactl");
    if (executable.isEmpty()) { done(false, "Install pactl (libpulse) to select audio outputs."); return; }
    auto *process = new QProcess(this);
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    auto completed = std::make_shared<bool>(false);
    auto finishProcess = [process, timer, completed, done](bool ok, QByteArray data) {
        if (*completed) return;
        *completed = true;
        timer->stop();
        done(ok, data);
        process->deleteLater();
    };
    connect(process, &QProcess::finished, this, [process, finishProcess](int code, QProcess::ExitStatus status) {
        const bool ok = code == 0 && status == QProcess::NormalExit;
        finishProcess(ok, ok ? process->readAllStandardOutput() : process->readAllStandardError());
    });
    connect(process, &QProcess::errorOccurred, this, [process, finishProcess](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finishProcess(false, process->errorString().toUtf8());
    });
    connect(timer, &QTimer::timeout, this, [process, finishProcess] {
        process->kill();
        finishProcess(false, "Audio server did not respond in time.");
    });
    process->start(executable, args);
    timer->start(5000);
}
void AudioOutputs::bluetoothCall(const QString &path, const QString &method, Done done) {
    auto message = QDBusMessage::createMethodCall("org.bluez", path, "org.bluez.Device1", method);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 20000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, done] {
        QDBusPendingReply<> reply = *watcher;
        done(!reply.isError(), reply.isError() ? reply.error().message().toUtf8() : QByteArray{});
        watcher->deleteLater();
    });
}
void AudioOutputs::refreshBluetooth() {
    if (m_bluezRefreshing) return;
    m_bluezRefreshing = true;
    auto message = QDBusMessage::createMethodCall("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        QDBusPendingReply<ManagedObjects> reply = *watcher;
        m_bluezRefreshing = false;
        m_bluetooth.clear();
        if (!reply.isError()) {
            const auto objects = reply.value();
            for (auto it = objects.begin(); it != objects.end(); ++it) {
                const auto device = it.value().value("org.bluez.Device1");
                const auto uuids = qdbus_cast<QStringList>(device.value("UUIDs"));
                if (!device.value("Paired").toBool() || !uuids.contains("0000110b-0000-1000-8000-00805f9b34fb")) continue;
                const auto address = device.value("Address").toString();
                m_bluetooth.append(QVariantMap{{"id", "bt:" + address}, {"address", address}, {"path", it.key().path()},
                    {"label", device.value("Alias", device.value("Name"))}, {"bluetooth", true},
                    {"connected", device.value("Connected")}, {"sink", ""}, {"active", false}, {"physical", true}});
            }
        }
        watcher->deleteLater();
        rebuild();
    });
}
void AudioOutputs::refresh() {
    refreshBluetooth();
    if (m_subscription.state() == QProcess::NotRunning) {
        const auto executable = QStandardPaths::findExecutable("pactl");
        if (!executable.isEmpty()) m_subscription.start(executable, {"subscribe"});
    }
    if (m_refreshing) return;
    m_refreshing = true;
    pactl({"--format=json", "list", "sinks"}, [this](bool ok, const QByteArray &data) {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(data, &error);
        if (!ok || !doc.isArray() || error.error != QJsonParseError::NoError) {
            m_refreshing = false;
            m_sinks = {}; m_default.clear(); rebuild();
            if (!m_busy) { m_message = tr("Audio outputs unavailable. Check that PipeWire/PulseAudio and pactl are installed and running."); emit changed(); }
            return;
        }
        const auto sinks = doc.array();
        pactl({"get-default-sink"}, [this, sinks](bool success, const QByteArray &name) {
            m_refreshing = false;
            if (!success) return;
            m_sinks = sinks;
            m_default = QString::fromUtf8(name).trimmed();
            if (m_message.startsWith(tr("Audio outputs unavailable."))) m_message.clear();
            rebuild();
        });
    });
}
void AudioOutputs::rebuild() {
    QVariantList outputs = m_bluetooth;
    const auto aliases = m_config->value("audio.outputNames").toMap();
    for (const auto &entry : m_sinks) {
        const auto sink = entry.toObject();
        const auto props = sink["properties"].toObject();
        const auto name = sink["name"].toString();
        auto address = props["api.bluez5.address"].toString();
        if (address.isEmpty() && name.startsWith("bluez_output.")) address = name.section('.', 1, 1).replace('_', ':');
        const bool bt = !address.isEmpty();
        QVariantMap row{{"id", bt ? "bt:" + address : "sink:" + name}, {"sink", name},
            {"label", sink["description"].toString(name)}, {"bluetooth", bt}, {"connected", true},
            {"active", name == m_default}, {"physical", bt || (name.startsWith("alsa_output.") && !name.contains("snd_aloop"))}};
        bool merged = false;
        for (auto &existing : outputs) {
            auto map = existing.toMap();
            if (map["id"] != row["id"]) continue;
            map["sink"] = name; map["active"] = name == m_default; map["connected"] = true;
            existing = map; merged = true; break;
        }
        if (!merged) outputs.append(row);
    }
    for (auto &entry : outputs) {
        auto row = entry.toMap();
        const auto alias = aliases.value(row["id"].toString()).toString().trimmed();
        if (!alias.isEmpty()) row["label"] = alias;
        entry = row;
    }
    std::stable_sort(outputs.begin(), outputs.end(), [](const QVariant &a, const QVariant &b) {
        const auto x = a.toMap(), y = b.toMap();
        if (x["physical"] != y["physical"]) return x["physical"].toBool();
        return x["label"].toString().localeAwareCompare(y["label"].toString()) < 0;
    });
    if (m_outputs != outputs) { m_outputs = outputs; emit changed(); }
}
void AudioOutputs::renameOutput(const QString &id, const QString &name) {
    if (find(id).isEmpty()) return;
    auto aliases = m_config->value("audio.outputNames").toMap();
    if (name.trimmed().isEmpty()) aliases.remove(id);
    else aliases[id] = name.trimmed().left(80);
    m_config->setValue("audio.outputNames", aliases);
}
void AudioOutputs::finish(const QString &message) {
    m_busy = false; m_message = message; emit changed(); refresh();
}
void AudioOutputs::selectOutput(const QString &id) {
    if (m_busy) return;
    const auto row = find(id);
    if (row.isEmpty()) { finish(tr("This output is no longer available.")); return; }
    m_busy = true; m_message = tr("Switching to %1…").arg(row["label"].toString()); emit changed();
    if (!row["sink"].toString().isEmpty()) {
        route(row, [this](bool ok, const QByteArray &error) { finish(ok ? QString{} : QString::fromUtf8(error)); });
    } else if (!row["path"].toString().isEmpty()) {
        bluetoothCall(row["path"].toString(), "Connect", [this, id](bool ok, const QByteArray &error) {
            if (!ok) { finish(tr("Could not connect. Release the headphones on the other device and try again. %1").arg(QString::fromUtf8(error))); return; }
            waitForSink(id, 30);
        });
    } else finish(tr("No audio output available for this device."));
}
void AudioOutputs::waitForSink(const QString &id, int attempts) {
    const auto row = find(id);
    if (!row["sink"].toString().isEmpty()) {
        route(row, [this](bool ok, const QByteArray &error) { finish(ok ? QString{} : QString::fromUtf8(error)); });
        return;
    }
    if (attempts <= 0) { finish(tr("Connected, but no audio output appeared. Check the Bluetooth audio service and try again.")); return; }
    refresh();
    QTimer::singleShot(500, this, [this, id, attempts] { waitForSink(id, attempts - 1); });
}
bool AudioOutputs::movableStream(const QJsonObject &stream) {
    const auto p = stream["properties"].toObject();
    // Effect/filter/loopback links are routing infrastructure, not players.
    const auto truthy = [](const QJsonValue &v) { return v.toBool() || v.toString() == "true"; };
    return p["media.role"].toString() != "filter" && !truthy(p["node.virtual"])
        && !p["application.id"].toString().contains("easyeffects")
        && !p["application.name"].toString().contains("EasyEffects", Qt::CaseInsensitive)
        && !p["media.name"].toString().contains("loopback", Qt::CaseInsensitive);
}
void AudioOutputs::moveStreams(const QString &sink, Done done) {
    pactl({"--format=json", "list", "sink-inputs"}, [this, sink, done](bool ok, const QByteArray &data) {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(data, &error);
        if (!ok || !doc.isArray() || error.error != QJsonParseError::NoError) { done(false, "Output selected, but existing playback could not be moved."); return; }
        QStringList inputs;
        for (const auto &v : doc.array()) if (movableStream(v.toObject())) inputs.append(QString::number(v.toObject()["index"].toInt()));
        if (inputs.isEmpty()) { done(true, {}); return; }
        auto remaining = std::make_shared<int>(inputs.size());
        auto failures = std::make_shared<bool>(false);
        for (const auto &input : inputs) pactl({"move-sink-input", input, sink}, [remaining, failures, done](bool success, const QByteArray &error) {
            // Players can stop between enumeration and move. That is harmless.
            if (!success && !error.contains("No such entity")) *failures = true;
            if (--*remaining == 0) done(!*failures, *failures ? QByteArray("Output selected; some applications kept their own routing. Select the output in those applications.") : QByteArray{});
        });
    });
}
void AudioOutputs::route(const QVariantMap &output, Done done) {
    const auto sink = output["sink"].toString();
    pactl({"set-default-sink", sink}, [this, sink, output, done](bool ok, const QByteArray &error) {
        if (!ok) { done(false, error); return; }
        m_default = sink;
        if (!output["bluetooth"].toBool() && output["physical"].toBool()) m_config->setValue("audio.fallbackOutput", output["id"]);
        rebuild();
        moveStreams(sink, done);
    });
}
void AudioOutputs::releaseBluetooth() {
    if (m_busy) return;
    const auto active = find(currentId());
    if (!active["bluetooth"].toBool() || active["path"].toString().isEmpty()) return;
    auto fallback = find(m_config->value("audio.fallbackOutput").toString());
    if (fallback["bluetooth"].toBool() || fallback["sink"].toString().isEmpty()) {
        fallback.clear();
        for (const auto &v : m_outputs) {
            auto row = v.toMap();
            if (!row["bluetooth"].toBool() && row["physical"].toBool() && !row["sink"].toString().isEmpty()) { fallback = row; break; }
        }
    }
    m_busy = true; m_message = tr("Releasing headphones…"); emit changed(); emit pauseRequested();
    const auto disconnect = [this, active](bool ok, const QByteArray &error) {
        if (!ok) { finish(QString::fromUtf8(error)); return; }
        bluetoothCall(active["path"].toString(), "Disconnect", [this](bool success, const QByteArray &reason) {
            finish(success ? tr("Headphones released. Select them on your Mac or iPhone.") : QString::fromUtf8(reason));
        });
    };
    if (fallback.isEmpty()) disconnect(true, {});
    else route(fallback, disconnect);
}
