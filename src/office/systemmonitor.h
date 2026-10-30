/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QProcess;
class QTimer;

/**
 * Samples system load at 1 Hz, only while `active` is true, for the office's
 * monitor overlay. See docs/office-v2.md section 4 for the property table.
 *
 * Reading is defensive throughout: every `/proc` and `/sys` path is optional,
 * missing ones just leave the property at its last (or default) value rather
 * than ever failing a sample.
 */
class SystemMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.office.system")

    /** Sampling runs only while this is true, set by whatever is showing the monitor. */
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    /** The claude process to list descendants of; the office sets this to the agent being looked at. */
    Q_PROPERTY(qint64 focusPid READ focusPid WRITE setFocusPid NOTIFY focusPidChanged)

    Q_PROPERTY(double cpu READ cpu NOTIFY updated)
    Q_PROPERTY(QVariantList cores READ cores NOTIFY updated)
    Q_PROPERTY(double load1 READ load1 NOTIFY updated)
    Q_PROPERTY(qint64 memUsed READ memUsed NOTIFY updated)
    Q_PROPERTY(qint64 memTotal READ memTotal NOTIFY updated)
    Q_PROPERTY(qint64 swapUsed READ swapUsed NOTIFY updated)
    Q_PROPERTY(qint64 swapTotal READ swapTotal NOTIFY updated)
    /** Degrees Celsius, -1 when no k10temp/coretemp hwmon was found. */
    Q_PROPERTY(double cpuTemp READ cpuTemp NOTIFY updated)
    /** `{name, util, memUsed, memTotal, temp, power, fan}`, empty when there is no GPU reading. */
    Q_PROPERTY(QVariantMap gpu READ gpu NOTIFY updated)
    Q_PROPERTY(qint64 diskRead READ diskRead NOTIFY updated)
    Q_PROPERTY(qint64 diskWrite READ diskWrite NOTIFY updated)
    Q_PROPERTY(QVariantList disks READ disks NOTIFY updated)
    /** Degrees Celsius, -1 when no nvme hwmon was found. */
    Q_PROPERTY(double diskTemp READ diskTemp NOTIFY updated)
    Q_PROPERTY(qint64 netRx READ netRx NOTIFY updated)
    Q_PROPERTY(qint64 netTx READ netTx NOTIFY updated)
    /** Top 6 by CPU among descendants of `focusPid`: `{pid, name, cpu, rss}`. */
    Q_PROPERTY(QVariantList processes READ processes NOTIFY updated)

public:
    explicit SystemMonitor(QObject *parent = nullptr);
    ~SystemMonitor() override;

    bool active() const
    {
        return m_active;
    }
    void setActive(bool active);
    qint64 focusPid() const
    {
        return m_focusPid;
    }
    void setFocusPid(qint64 pid);

    double cpu() const
    {
        return m_cpu;
    }
    QVariantList cores() const
    {
        return m_cores;
    }
    double load1() const
    {
        return m_load1;
    }
    qint64 memUsed() const
    {
        return m_memUsed;
    }
    qint64 memTotal() const
    {
        return m_memTotal;
    }
    qint64 swapUsed() const
    {
        return m_swapUsed;
    }
    qint64 swapTotal() const
    {
        return m_swapTotal;
    }
    double cpuTemp() const
    {
        return m_cpuTemp;
    }
    QVariantMap gpu() const
    {
        return m_gpu;
    }
    qint64 diskRead() const
    {
        return m_diskRead;
    }
    qint64 diskWrite() const
    {
        return m_diskWrite;
    }
    QVariantList disks() const
    {
        return m_disks;
    }
    double diskTemp() const
    {
        return m_diskTemp;
    }
    qint64 netRx() const
    {
        return m_netRx;
    }
    qint64 netTx() const
    {
        return m_netTx;
    }
    QVariantList processes() const
    {
        return m_processes;
    }

    /** Takes one sample right now, regardless of `active`. Used by tests. */
    Q_INVOKABLE void sampleOnce();

Q_SIGNALS:
    void activeChanged();
    void focusPidChanged();
    /** One per sample. */
    void updated();

private:
    void sample();
    void readCpu();
    void readLoad();
    void readMem();
    void readTemps();
    void readGpu();
    void readNvidia();
    void readAmdGpu();
    void readDisks(qint64 now);
    void readNet(qint64 now);
    void readProcesses(qint64 now);

    QTimer *m_timer;
    bool m_active = false;
    qint64 m_focusPid = 0;

    double m_cpu = 0;
    QVariantList m_cores;
    double m_load1 = 0;
    qint64 m_memUsed = 0;
    qint64 m_memTotal = 0;
    qint64 m_swapUsed = 0;
    qint64 m_swapTotal = 0;
    double m_cpuTemp = -1;
    QVariantMap m_gpu;
    qint64 m_diskRead = 0;
    qint64 m_diskWrite = 0;
    QVariantList m_disks;
    double m_diskTemp = -1;
    qint64 m_netRx = 0;
    qint64 m_netTx = 0;
    QVariantList m_processes;

    // State needed to turn cumulative counters into per-second deltas.
    struct CpuTicks {
        qint64 idle = 0;
        qint64 total = 0;
    };
    CpuTicks m_prevTotalCpu;
    QList<CpuTicks> m_prevCoreCpu;
    QHash<QString, QPair<qint64, qint64>> m_prevDiskSectors;
    qint64 m_prevNetRxBytes = 0;
    qint64 m_prevNetTxBytes = 0;
    qint64 m_prevSampleMs = 0;
    QHash<qint64, qint64> m_prevProcTicks;

    bool m_nvidiaChecked = false;
    QString m_nvidiaSmiPath;
    QProcess *m_nvidiaProcess = nullptr;
};
