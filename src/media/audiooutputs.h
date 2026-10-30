// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QJsonArray>
#include <functional>
#include <QtQml/qqmlregistration.h>
class Config;

// PulseAudio's JSON protocol works with both PulseAudio and pipewire-pulse.
// BlueZ is used only for explicitly requested connections, never to steal a
// headset back from another computer/phone in the background.
class AudioOutputs : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.audio")
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(QString currentId READ currentId NOTIFY changed)
    Q_PROPERTY(bool bluetoothActive READ bluetoothActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit AudioOutputs(Config *config, QObject *parent = nullptr);
    ~AudioOutputs() override;
    void start();
    QVariantList outputs() const { return m_outputs; }
    QString currentName() const;
    QString currentId() const;
    bool bluetoothActive() const;
    bool busy() const { return m_busy; }
    QString message() const { return m_message; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectOutput(const QString &id);
    Q_INVOKABLE void releaseBluetooth();
    Q_INVOKABLE void renameOutput(const QString &id, const QString &name);
Q_SIGNALS:
    void changed();
    void pauseRequested();
private:
    friend class AudioOutputsTest;
    using Done = std::function<void(bool, QByteArray)>;
    void pactl(const QStringList &args, Done done);
    void bluetoothCall(const QString &path, const QString &method, Done done);
    void refreshBluetooth();
    void rebuild();
    void waitForSink(const QString &id, int attempts);
    void route(const QVariantMap &output, Done done);
    void moveStreams(const QString &sink, Done done);
    void finish(const QString &message = {});
    QVariantMap find(const QString &id) const;
    static bool movableStream(const QJsonObject &stream);
    Config *m_config;
    QTimer m_poll, m_debounce;
    QProcess m_subscription;
    QJsonArray m_sinks;
    QVariantList m_bluetooth, m_outputs;
    QString m_default, m_message;
    bool m_busy = false, m_refreshing = false, m_bluezRefreshing = false;
};
