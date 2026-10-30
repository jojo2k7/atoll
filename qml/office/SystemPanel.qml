// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * What the machine is going through (docs/office-v2.md section 4), drawn on
 * the `sysroom` PC cut-away: a LED column per CPU core, RAM sticks filling
 * up, GPU fans spinning with the fan speed, disk and network LEDs blinking
 * with traffic, a thermometer, and the processes under the agent's claude.
 */
FocusScope {
    id: panel

    required property Item view

    readonly property var sys: App.office.system
    readonly property var room: OfficeArt.ready ? OfficeArt.sheet("sysroom") : undefined
    readonly property var reg: room ? room.regions : ({})
    readonly property var gpu: sys.gpu ?? ({})
    readonly property bool hasGpu: gpu.name !== undefined
    readonly property real ramFrac: sys.memTotal > 0 ? sys.memUsed / sys.memTotal : 0
    readonly property real swapFrac: sys.swapTotal > 0 ? sys.swapUsed / sys.swapTotal : 0
    readonly property var focusAgent: App.office.agents.find(a => a.pid > 0 && a.pid === sys.focusPid)

    width: 4 + (room ? room.frameWidth : 232) + 4 + 128 + 6
    height: 18 + (room ? room.frameHeight : 150) + 5

    Keys.onEscapePressed: view.systemOpen = false

    function level(frac) {
        return frac > 0.8 ? "hot" : (frac > 0.55 ? "warn" : "ok")
    }

    NineSlice {
        anchors.fill: parent
        sheet: "ui_panel_dark"
    }

    MouseArea {
        anchors.fill: parent
    }

    // ---- header ------------------------------------------------------------
    Sprite {
        sheet: "icons"
        frame: "cpu"
        px: 6
        py: 5
    }
    PixelText {
        x: 18
        y: 6
        text: qsTr("System") + "   " + qsTr("load %1").arg(panel.sys.load1.toFixed(1))
              + "   " + qsTr("cpu %1%").arg(Math.round(panel.sys.cpu))
              + (panel.sys.cpuTemp >= 0 ? "   " + Math.round(panel.sys.cpuTemp) + "°C" : "")
    }
    PixelButton {
        x: panel.width - width - 4
        y: 3
        icon: "close"
        onClicked: panel.view.systemOpen = false
    }

    // ---- the case ----------------------------------------------------------
    Item {
        id: caseView
        x: 4
        y: 18
        width: panel.room ? panel.room.frameWidth : 232
        height: panel.room ? panel.room.frameHeight : 150

        Sprite {
            sheet: "sysroom"
        }

        // CPU: one LED column per core, filled bottom-up.
        Item {
            id: cpu
            readonly property var r: panel.reg.cpuCores ?? [0, 0, 0, 0]
            readonly property var cores: panel.sys.cores ?? []
            // Up to 8 columns of 5 px LEDs; more cores get 2 px dots, grouped by their maximum.
            readonly property bool wide: cores.length <= 8
            readonly property int colW: wide ? 5 : 2
            readonly property int columns: Math.max(1, Math.min(cores.length, Math.floor((r[2] - 2) / (colW + 1))))
            readonly property var loads: {
                const out = []
                const per = Math.ceil(cores.length / columns)
                for (let c = 0; c < columns; ++c) {
                    let m = 0
                    for (let k = c * per; k < Math.min(cores.length, (c + 1) * per); ++k) {
                        m = Math.max(m, cores[k])
                    }
                    out.push(m)
                }
                return out
            }
            readonly property int segs: Math.floor((r[3] - 2) / 3)
            readonly property int leftX: r[0] + Math.floor((r[2] - columns * (colW + 1) + 1) / 2)
            Repeater {
                model: cpu.columns * cpu.segs
                Sprite {
                    required property int index
                    readonly property int col: index % cpu.columns
                    readonly property int seg: Math.floor(index / cpu.columns)
                    readonly property real load: (cpu.loads[col] ?? 0) / 100
                    readonly property bool lit: seg < Math.round(load * cpu.segs)
                    sheet: cpu.wide ? "ui_led" : "ui_led_dot"
                    frame: !lit ? "off" : (seg >= cpu.segs * 0.8 ? "hot" : (seg >= cpu.segs * 0.55 ? "warn" : "ok"))
                    px: cpu.leftX + col * (cpu.colW + 1)
                    py: cpu.r[1] + cpu.r[3] - 3 - seg * 3
                }
            }
        }
        PixelText {
            x: (panel.reg.cpuLabel ?? [0, 0])[0]
            y: (panel.reg.cpuLabel ?? [0, 0])[1]
            text: "CPU " + Math.round(panel.sys.cpu) + "%"
            color: OfficeArt.dim
        }

        // RAM: used memory spread over the sticks, each filled bottom-up.
        Repeater {
            model: panel.reg.ramSticks ?? []
            Item {
                id: stick
                required property var modelData
                required property int index
                readonly property int n: (panel.reg.ramSticks ?? []).length
                readonly property real fill: Math.max(0, Math.min(1, panel.ramFrac * n - index))
                readonly property int segs: Math.floor(modelData[3] / 3)
                Repeater {
                    model: stick.segs
                    Item {
                        required property int index
                        x: stick.modelData[0]
                        y: stick.modelData[1] + stick.modelData[3] - 3 - index * 3
                        width: 4
                        height: 2
                        clip: true
                        Sprite {
                            sheet: "ui_led"
                            frame: parent.index < Math.round(stick.fill * stick.segs) ? "info" : "off"
                        }
                    }
                }
            }
        }
        PixelText {
            x: (panel.reg.ramLabel ?? [0, 0])[0]
            y: (panel.reg.ramLabel ?? [0, 0])[1]
            text: "RAM " + Math.round(panel.ramFrac * 100) + "%"
            color: OfficeArt.dim
        }

        // Disks: a LED per bay, blinking with that disk's I/O.
        Repeater {
            model: Math.min(6, (panel.sys.disks ?? []).length)
            Sprite {
                required property int index
                readonly property var d: panel.sys.disks[index]
                readonly property real rate: d ? d.read + d.write : 0
                readonly property var r: panel.reg.disk ?? [0, 0, 48, 8]
                sheet: "ui_led_dot"
                frame: rate <= 0 ? "ok" : (Math.floor(OfficeArt.clock * (6 + index)) % 2 === 0
                                           ? (d.write > d.read ? "warn" : "info") : "off")
                px: r[0] + Math.floor(r[2] / 6) * index + 3
                py: r[1] + 3
            }
        }
        Sprite {
            readonly property bool io: panel.sys.diskRead + panel.sys.diskWrite > 0
            sheet: "ui_led_dot"
            frame: io && Math.floor(OfficeArt.clock * 9) % 2 === 0 ? "warn" : "off"
            px: (panel.reg.diskLed ?? [0, 0])[0]
            py: (panel.reg.diskLed ?? [0, 0])[1]
        }
        PixelText {
            x: (panel.reg.diskLabel ?? [0, 0])[0]
            y: (panel.reg.diskLabel ?? [0, 0])[1]
            text: "R " + OfficeArt.fmtBytes(panel.sys.diskRead) + " W " + OfficeArt.fmtBytes(panel.sys.diskWrite)
            color: OfficeArt.dim
        }

        // GPU: fans spin with the fan speed (or the load), bars for util and VRAM.
        Repeater {
            model: panel.reg.gpuFans ?? []
            FanSprite {
                required property var modelData
                px: modelData[0]
                py: modelData[1]
                speed: panel.hasGpu ? Math.max(panel.gpu.fan ?? 0, (panel.gpu.util ?? 0) * 0.6) / 100 : 0
            }
        }
        FanSprite {
            px: (panel.reg.psuFan ?? [0, 0])[0]
            py: (panel.reg.psuFan ?? [0, 0])[1]
            speed: 0.25 + panel.sys.cpu / 140
        }
        Repeater {
            model: [
                { r: panel.reg.gpuBar, frac: (panel.gpu.util ?? 0) / 100, frame: "warn" },
                { r: panel.reg.gpuMemBar, frac: panel.gpu.memTotal > 0 ? panel.gpu.memUsed / panel.gpu.memTotal : 0, frame: "info" }
            ].filter(m => m.r !== undefined)
            Item {
                required property var modelData
                visible: modelData.r !== undefined
                NineSlice {
                    x: modelData.r[0]
                    y: modelData.r[1]
                    width: modelData.r[2]
                    height: 7
                    sheet: "ui_bar_bg"
                }
                NineSlice {
                    visible: modelData.frac > 0
                    x: modelData.r[0] + 1
                    y: modelData.r[1] + 1
                    width: Math.max(2, Math.round((modelData.r[2] - 2) * Math.min(1, modelData.frac)))
                    height: 5
                    sheet: "ui_bar_fill"
                    frame: modelData.frame
                }
            }
        }
        PixelText {
            x: (panel.reg.gpuLabel ?? [0, 0])[0]
            y: (panel.reg.gpuLabel ?? [0, 0])[1] - 4
            text: panel.hasGpu ? "GPU " + Math.round(panel.gpu.util ?? 0) + "%"
                                 + (panel.gpu.temp !== undefined ? " " + Math.round(panel.gpu.temp) + "°" : "")
                                 + (panel.gpu.power !== undefined ? " " + Math.round(panel.gpu.power) + "W" : "")
                               : qsTr("no GPU")
            color: OfficeArt.dim
        }

        // Network: the first port blinks with rx/tx, the second shows the link.
        Repeater {
            model: [
                { key: "netRxLed", rate: panel.sys.netRx, frame: "ok" },
                { key: "netTxLed", rate: panel.sys.netTx, frame: "warn" },
                { key: "net2RxLed", rate: -1, frame: "info" },
                { key: "net2TxLed", rate: -2, frame: "off" }
            ]
            Sprite {
                required property var modelData
                readonly property var at: panel.reg[modelData.key] ?? [0, 0]
                readonly property real hz: modelData.rate > 0 ? Math.min(12, 2 + Math.log(1 + modelData.rate / 1024) * 1.5) : 0
                sheet: "ui_led_dot"
                frame: modelData.rate === -1 ? "info" : modelData.rate === -2 ? "off"
                       : (hz > 0 && Math.floor(OfficeArt.clock * hz) % 2 === 0 ? modelData.frame : "off")
                px: at[0]
                py: at[1]
            }
        }
        PixelText {
            x: (panel.reg.netLabel ?? [0, 0])[0]
            y: (panel.reg.netLabel ?? [0, 0])[1] + 3
            text: "↓" + OfficeArt.fmtBytes(panel.sys.netRx) + " ↑" + OfficeArt.fmtBytes(panel.sys.netTx)
            color: OfficeArt.dim
        }

        // Thermometer: CPU temperature, 30-95 °C, filled bottom-up.
        Rectangle {
            readonly property var r: panel.reg.thermoTube ?? [0, 0, 0, 0]
            readonly property real frac: panel.sys.cpuTemp < 0 ? 0
                                         : Math.max(0, Math.min(1, (panel.sys.cpuTemp - 30) / 65))
            x: r[0]
            width: r[2]
            height: Math.round(r[3] * frac)
            y: r[1] + r[3] - height
            color: OfficeArt.hot
            Behavior on height {
                NumberAnimation {
                    duration: 600
                }
            }
        }
        PixelText {
            x: (panel.reg.tempLabel ?? [0, 0])[0] - 4
            y: (panel.reg.tempLabel ?? [0, 0])[1] - 13
            text: (panel.sys.cpuTemp >= 0 ? Math.round(panel.sys.cpuTemp) + "°" : "-")
                  + (panel.sys.diskTemp >= 0 ? " nvme " + Math.round(panel.sys.diskTemp) + "°" : "")
            color: OfficeArt.dim
        }
    }

    // ---- figures and processes ---------------------------------------------
    Column {
        x: caseView.x + caseView.width + 5
        y: 19
        width: 126
        spacing: 1

        Repeater {
            model: [
                { icon: "ram", text: OfficeArt.fmtBytes(panel.sys.memUsed) + " / " + OfficeArt.fmtBytes(panel.sys.memTotal),
                  frac: panel.ramFrac },
                { icon: "ram", text: "swap " + OfficeArt.fmtBytes(panel.sys.swapUsed) + " / " + OfficeArt.fmtBytes(panel.sys.swapTotal),
                  frac: panel.swapFrac },
                { icon: "gpu", text: panel.hasGpu ? "vram " + OfficeArt.fmtBytes(panel.gpu.memUsed ?? 0)
                                                    + " / " + OfficeArt.fmtBytes(panel.gpu.memTotal ?? 0)
                                                  : "no GPU reading",
                  frac: panel.gpu.memTotal > 0 ? panel.gpu.memUsed / panel.gpu.memTotal : -1 },
                { icon: "disk", text: "r " + OfficeArt.fmtBytes(panel.sys.diskRead) + "/s  w " + OfficeArt.fmtBytes(panel.sys.diskWrite) + "/s",
                  frac: -1 },
                { icon: "net", text: "rx " + OfficeArt.fmtBytes(panel.sys.netRx) + "/s  tx " + OfficeArt.fmtBytes(panel.sys.netTx) + "/s",
                  frac: -1 }
            ]
            Item {
                required property var modelData
                width: 126
                height: 11
                Sprite {
                    sheet: "icons"
                    frame: modelData.icon
                    px: 0
                    py: 1
                }
                PixelText {
                    x: 12
                    y: 2
                    width: 114
                    elide: Text.ElideRight
                    text: modelData.text
                    color: modelData.frac > 0.85 ? OfficeArt.hot : (modelData.frac > 0.65 ? OfficeArt.warn : OfficeArt.text)
                }
            }
        }

        Item {
            width: 126
            height: 5
            Rectangle {
                y: 2
                width: parent.width
                height: 1
                color: "#3a3046"
            }
        }

        Item {
            width: 126
            height: 11
            Sprite {
                sheet: "icons"
                frame: "terminal"
                px: 0
                py: 1
            }
            PixelText {
                x: 12
                y: 2
                width: 114
                elide: Text.ElideRight
                text: panel.focusAgent ? panel.focusAgent.name + "  pid " + panel.sys.focusPid
                      : (panel.sys.focusPid > 0 ? "pid " + panel.sys.focusPid : qsTr("no claude process"))
                color: OfficeArt.accent
            }
        }

        Repeater {
            model: panel.sys.processes ?? []
            Item {
                required property var modelData
                width: 126
                height: 10
                PixelText {
                    x: 2
                    y: 1
                    width: 60
                    elide: Text.ElideRight
                    text: modelData.name
                }
                PixelText {
                    x: 64
                    y: 1
                    width: 26
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(modelData.cpu) + "%"
                    color: modelData.cpu > 80 ? OfficeArt.hot : (modelData.cpu > 40 ? OfficeArt.warn : OfficeArt.ok)
                }
                PixelText {
                    x: 94
                    y: 1
                    width: 32
                    horizontalAlignment: Text.AlignRight
                    text: OfficeArt.fmtBytes(modelData.rss)
                    color: OfficeArt.dim
                }
            }
        }

        PixelText {
            visible: (panel.sys.processes ?? []).length === 0
            x: 2
            text: qsTr("nothing running")
            color: OfficeArt.dim
        }
    }
}
