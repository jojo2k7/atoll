/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "systemmonitor.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace Qt::StringLiterals;

namespace
{
constexpr int SampleIntervalMs = 1000;
constexpr int MaxProcesses = 6;

long clockTicksPerSecond()
{
#ifdef Q_OS_UNIX
    const long ticks = sysconf(_SC_CLK_TCK);
    return ticks > 0 ? ticks : 100;
#else
    return 100;
#endif
}
}

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(SampleIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitor::sample);
}

SystemMonitor::~SystemMonitor()
{
    // An in-flight nvidia-smi query would otherwise warn about being
    // destroyed while still running.
    if (m_nvidiaProcess) {
        m_nvidiaProcess->kill();
        m_nvidiaProcess->waitForFinished(200);
    }
}

void SystemMonitor::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    Q_EMIT activeChanged();
    if (active) {
        sample();
        m_timer->start();
    } else {
        m_timer->stop();
    }
}

void SystemMonitor::setFocusPid(qint64 pid)
{
    if (m_focusPid == pid) {
        return;
    }
    m_focusPid = pid;
    Q_EMIT focusPidChanged();
}

void SystemMonitor::sampleOnce()
{
    sample();
}

void SystemMonitor::sample()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    readCpu();
    readLoad();
    readMem();
    readTemps();
    readGpu();
    readDisks(now);
    readNet(now);
    readProcesses(now);
    m_prevSampleMs = now;
    Q_EMIT updated();
}

void SystemMonitor::readCpu()
{
    QFile file(u"/proc/stat"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    // Not a loop on atEnd(): /proc files report size() 0, which makes
    // atEnd() true before anything is read. readAll() ignores that.
    const QList<QByteArray> lines = file.readAll().split('\n');

    CpuTicks totalTicks;
    bool haveTotal = false;
    QList<CpuTicks> coreTicks;

    for (const QByteArray &line : lines) {
        if (!line.startsWith("cpu")) {
            break;
        }
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 5) {
            continue;
        }
        qint64 sum = 0;
        for (int i = 1; i < fields.size(); ++i) {
            sum += fields.at(i).toLongLong();
        }
        // idle (field 4) + iowait (field 5): both count as "not busy".
        const qint64 idle = fields.at(3).toLongLong() + (fields.size() > 4 ? fields.at(4).toLongLong() : 0);
        const CpuTicks ticks{idle, sum};
        if (fields.at(0) == "cpu") {
            totalTicks = ticks;
            haveTotal = true;
        } else {
            coreTicks.append(ticks);
        }
    }

    if (haveTotal) {
        const qint64 totalDelta = totalTicks.total - m_prevTotalCpu.total;
        const qint64 idleDelta = totalTicks.idle - m_prevTotalCpu.idle;
        if (m_prevTotalCpu.total > 0 && totalDelta > 0) {
            m_cpu = std::clamp(100.0 * double(totalDelta - idleDelta) / double(totalDelta), 0.0, 100.0);
        }
        m_prevTotalCpu = totalTicks;
    }

    QVariantList cores;
    cores.reserve(coreTicks.size());
    for (int i = 0; i < coreTicks.size(); ++i) {
        double percent = 0;
        if (i < m_prevCoreCpu.size()) {
            const CpuTicks &prev = m_prevCoreCpu.at(i);
            const qint64 totalDelta = coreTicks.at(i).total - prev.total;
            const qint64 idleDelta = coreTicks.at(i).idle - prev.idle;
            if (prev.total > 0 && totalDelta > 0) {
                percent = std::clamp(100.0 * double(totalDelta - idleDelta) / double(totalDelta), 0.0, 100.0);
            }
        }
        cores.append(percent);
    }
    m_prevCoreCpu = coreTicks;
    m_cores = cores;
}

void SystemMonitor::readLoad()
{
    QFile file(u"/proc/loadavg"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QList<QByteArray> fields = file.readAll().simplified().split(' ');
    if (!fields.isEmpty()) {
        m_load1 = fields.at(0).toDouble();
    }
}

void SystemMonitor::readMem()
{
    QFile file(u"/proc/meminfo"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    qint64 total = 0, available = 0, free = 0, buffers = 0, cached = 0, swapTotal = 0, swapFree = 0;
    const QList<QByteArray> memLines = file.readAll().split('\n');
    for (const QByteArray &rawLine : memLines) {
        const QList<QByteArray> fields = rawLine.simplified().split(' ');
        if (fields.size() < 2) {
            continue;
        }
        const QByteArray &key = fields.at(0);
        const qint64 value = fields.at(1).toLongLong() * 1024;
        if (key == "MemTotal:") total = value;
        else if (key == "MemAvailable:") available = value;
        else if (key == "MemFree:") free = value;
        else if (key == "Buffers:") buffers = value;
        else if (key == "Cached:") cached = value;
        else if (key == "SwapTotal:") swapTotal = value;
        else if (key == "SwapFree:") swapFree = value;
    }
    m_memTotal = total;
    const qint64 freeBasis = available > 0 ? available : (free + buffers + cached);
    m_memUsed = std::max<qint64>(0, total - freeBasis);
    m_swapTotal = swapTotal;
    m_swapUsed = std::max<qint64>(0, swapTotal - swapFree);
}

void SystemMonitor::readTemps()
{
    m_cpuTemp = -1;
    m_diskTemp = -1;

    QDir hwmonRoot(u"/sys/class/hwmon"_s);
    const QFileInfoList hwmons = hwmonRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &hwmon : hwmons) {
        QFile nameFile(hwmon.absoluteFilePath() + u"/name"_s);
        if (!nameFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromUtf8(nameFile.readAll()).trimmed();
        if (name == u"k10temp"_s || name == u"coretemp"_s) {
            QFile tempFile(hwmon.absoluteFilePath() + u"/temp1_input"_s);
            if (tempFile.open(QIODevice::ReadOnly)) {
                m_cpuTemp = tempFile.readAll().trimmed().toDouble() / 1000.0;
            }
        } else if (name == u"nvme"_s) {
            QFile tempFile(hwmon.absoluteFilePath() + u"/temp1_input"_s);
            if (tempFile.open(QIODevice::ReadOnly)) {
                m_diskTemp = tempFile.readAll().trimmed().toDouble() / 1000.0;
            }
        }
    }
}

void SystemMonitor::readGpu()
{
    if (!m_nvidiaChecked) {
        m_nvidiaSmiPath = QStandardPaths::findExecutable(u"nvidia-smi"_s);
        m_nvidiaChecked = true;
    }
    if (!m_nvidiaSmiPath.isEmpty()) {
        readNvidia();
        return;
    }
    readAmdGpu();
}

void SystemMonitor::readNvidia()
{
    if (m_nvidiaProcess) {
        // A previous query is still in flight (nvidia-smi can be slow); skip
        // rather than pile up processes, the next sample tries again.
        return;
    }
    m_nvidiaProcess = new QProcess(this);
    connect(m_nvidiaProcess, &QProcess::finished, this, [this] {
        const QByteArray out = m_nvidiaProcess->readAllStandardOutput();
        m_nvidiaProcess->deleteLater();
        m_nvidiaProcess = nullptr;

        const QString line = QString::fromUtf8(out).split(u'\n').value(0).trimmed();
        if (line.isEmpty()) {
            return;
        }
        const QStringList fields = line.split(u", "_s);
        if (fields.size() < 6) {
            return;
        }
        // The property only picks this up on the next sample's `updated()`,
        // which keeps the "one NOTIFY per sample" rule even though the
        // query itself is async.
        QVariantMap gpu;
        gpu[u"name"_s] = fields.at(0).trimmed();
        gpu[u"util"_s] = fields.at(1).trimmed().toDouble();
        gpu[u"memUsed"_s] = fields.at(2).trimmed().toLongLong() * 1024 * 1024;
        gpu[u"memTotal"_s] = fields.at(3).trimmed().toLongLong() * 1024 * 1024;
        gpu[u"temp"_s] = fields.at(4).trimmed().toDouble();
        gpu[u"power"_s] = fields.at(5).trimmed().toDouble();
        gpu[u"fan"_s] = fields.size() > 6 ? fields.at(6).trimmed().toDouble() : 0.0;
        m_gpu = gpu;
    });
    m_nvidiaProcess->start(m_nvidiaSmiPath,
                            {u"--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw,fan.speed"_s,
                             u"--format=csv,noheader,nounits"_s});
}

void SystemMonitor::readAmdGpu()
{
    QDir drm(u"/sys/class/drm"_s);
    const QFileInfoList cards = drm.entryInfoList({u"card[0-9]*"_s}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &card : cards) {
        if (card.fileName().contains(u'-')) {
            continue; // a connector entry (card0-DP-1), not a card
        }
        const QString devicePath = card.absoluteFilePath() + u"/device"_s;
        QFile busyFile(devicePath + u"/gpu_busy_percent"_s);
        if (!busyFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const double util = busyFile.readAll().trimmed().toDouble();

        qint64 used = 0, total = 0;
        QFile usedFile(devicePath + u"/mem_info_vram_used"_s);
        if (usedFile.open(QIODevice::ReadOnly)) {
            used = usedFile.readAll().trimmed().toLongLong();
        }
        QFile totalFile(devicePath + u"/mem_info_vram_total"_s);
        if (totalFile.open(QIODevice::ReadOnly)) {
            total = totalFile.readAll().trimmed().toLongLong();
        }

        QVariantMap gpu;
        gpu[u"name"_s] = u"AMD GPU"_s;
        gpu[u"util"_s] = util;
        gpu[u"memUsed"_s] = used;
        gpu[u"memTotal"_s] = total;
        gpu[u"temp"_s] = 0.0;
        gpu[u"power"_s] = 0.0;
        gpu[u"fan"_s] = 0.0;
        m_gpu = gpu;
        return;
    }
    m_gpu = QVariantMap();
}

void SystemMonitor::readDisks(qint64 now)
{
    const double dt = m_prevSampleMs > 0 ? std::max(0.001, double(now - m_prevSampleMs) / 1000.0) : 0.0;

    QFile file(u"/proc/diskstats"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QVariantList disks;
    qint64 totalRead = 0, totalWrite = 0;
    const QList<QByteArray> diskLines = file.readAll().split('\n');
    for (const QByteArray &rawLine : diskLines) {
        const QList<QByteArray> fields = rawLine.simplified().split(' ');
        if (fields.size() < 10) {
            continue;
        }
        const QString name = QString::fromUtf8(fields.at(2));
        if (name.startsWith(u"loop"_s) || name.startsWith(u"ram"_s) || name.startsWith(u"zram"_s)
            || name.startsWith(u"dm-"_s)) {
            continue;
        }
        if (!QFile::exists(u"/sys/block/"_s + name)) {
            continue; // a partition, not a whole disk
        }

        const qint64 readSectors = fields.at(5).toLongLong();
        const qint64 writeSectors = fields.at(9).toLongLong();
        qint64 readBps = 0, writeBps = 0;
        const auto prev = m_prevDiskSectors.constFind(name);
        if (prev != m_prevDiskSectors.constEnd() && dt > 0) {
            readBps = qint64(double((readSectors - prev.value().first) * 512) / dt);
            writeBps = qint64(double((writeSectors - prev.value().second) * 512) / dt);
        }
        m_prevDiskSectors.insert(name, {readSectors, writeSectors});

        totalRead += readBps;
        totalWrite += writeBps;
        disks.append(QVariantMap{{u"name"_s, name}, {u"read"_s, readBps}, {u"write"_s, writeBps}});
    }
    m_disks = disks;
    m_diskRead = totalRead;
    m_diskWrite = totalWrite;
}

void SystemMonitor::readNet(qint64 now)
{
    const double dt = m_prevSampleMs > 0 ? std::max(0.001, double(now - m_prevSampleMs) / 1000.0) : 0.0;

    QFile file(u"/proc/net/dev"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QList<QByteArray> netLines = file.readAll().split('\n');

    qint64 rxTotal = 0, txTotal = 0;
    for (int i = 2; i < netLines.size(); ++i) {
        const QString line = QString::fromUtf8(netLines.at(i));
        const int colon = line.indexOf(u':');
        if (colon < 0) {
            continue;
        }
        const QString name = line.left(colon).trimmed();
        if (name == u"lo"_s) {
            continue;
        }
        const QStringList fields = line.mid(colon + 1).simplified().split(u' ');
        if (fields.size() < 9) {
            continue;
        }
        rxTotal += fields.at(0).toLongLong();
        txTotal += fields.at(8).toLongLong();
    }

    if (dt > 0) {
        m_netRx = qint64(double(std::max<qint64>(0, rxTotal - m_prevNetRxBytes)) / dt);
        m_netTx = qint64(double(std::max<qint64>(0, txTotal - m_prevNetTxBytes)) / dt);
    }
    m_prevNetRxBytes = rxTotal;
    m_prevNetTxBytes = txTotal;
}

void SystemMonitor::readProcesses(qint64 now)
{
    m_processes = QVariantList();
    if (m_focusPid <= 0) {
        return;
    }

    struct ProcInfo {
        QString name;
        qint64 ticks = 0;
        qint64 rss = 0;
    };
    QHash<qint64, ProcInfo> procs;
    QHash<qint64, QList<qint64>> children;

    QDir procRoot(u"/proc"_s);
    const QFileInfoList entries = procRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        bool isPid = false;
        const qint64 pid = entry.fileName().toLongLong(&isPid);
        if (!isPid || pid <= 0) {
            continue;
        }

        QFile statFile(entry.absoluteFilePath() + u"/stat"_s);
        if (!statFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray statLine = statFile.readAll();
        const int nameStart = statLine.indexOf('(');
        const int nameEnd = statLine.lastIndexOf(')');
        if (nameStart < 0 || nameEnd < nameStart) {
            continue;
        }
        const QString name = QString::fromUtf8(statLine.mid(nameStart + 1, nameEnd - nameStart - 1));
        const QList<QByteArray> fields = statLine.mid(nameEnd + 2).simplified().split(' ');
        // Past "(name) ": [0]=state [1]=ppid ... [11]=utime [12]=stime.
        if (fields.size() < 14) {
            continue;
        }
        const qint64 ppid = fields.at(1).toLongLong();
        const qint64 ticks = fields.at(11).toLongLong() + fields.at(12).toLongLong();

        qint64 rss = 0;
        QFile statusFile(entry.absoluteFilePath() + u"/status"_s);
        if (statusFile.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> statusLines = statusFile.readAll().split('\n');
            for (const QByteArray &line : statusLines) {
                if (line.startsWith("VmRSS:")) {
                    rss = line.mid(6).trimmed().split(' ').value(0).toLongLong() * 1024;
                    break;
                }
            }
        }

        procs.insert(pid, {name, ticks, rss});
        children[ppid].append(pid);
    }

    QList<qint64> descendants;
    QList<qint64> queue = children.value(m_focusPid);
    while (!queue.isEmpty()) {
        const qint64 pid = queue.takeFirst();
        descendants.append(pid);
        queue.append(children.value(pid));
    }

    const double dt = m_prevSampleMs > 0 ? std::max(0.001, double(now - m_prevSampleMs) / 1000.0) : 0.0;
    const double ticksPerSecond = double(clockTicksPerSecond());

    QList<QVariantMap> rows;
    QHash<qint64, qint64> currentTicks;
    for (qint64 pid : descendants) {
        const ProcInfo &info = procs.value(pid);
        currentTicks.insert(pid, info.ticks);
        double cpuPercent = 0;
        const auto prev = m_prevProcTicks.constFind(pid);
        if (prev != m_prevProcTicks.constEnd() && dt > 0) {
            const double deltaSeconds = double(info.ticks - prev.value()) / ticksPerSecond;
            cpuPercent = std::clamp(100.0 * deltaSeconds / dt, 0.0, 100.0);
        }
        rows.append(QVariantMap{{u"pid"_s, pid}, {u"name"_s, info.name}, {u"cpu"_s, cpuPercent}, {u"rss"_s, info.rss}});
    }
    m_prevProcTicks = currentTicks;

    std::sort(rows.begin(), rows.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(u"cpu"_s).toDouble() > b.value(u"cpu"_s).toDouble();
    });
    if (rows.size() > MaxProcesses) {
        rows = rows.mid(0, MaxProcesses);
    }

    QVariantList result;
    result.reserve(rows.size());
    for (const QVariantMap &row : rows) {
        result.append(row);
    }
    m_processes = result;
}
