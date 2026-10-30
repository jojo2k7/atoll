// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

Item {
    id: root

    readonly property bool weatherEnabled: Cfg.get("idle.showWeather", true)
    property string weatherText: ""
    property bool loaded: false
    property var cachedLat: null
    property var cachedLon: null

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    visible: weatherEnabled && weatherText.length > 0

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.weatherText
            color: Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(12)
            font.weight: Font.DemiBold
            opacity: 0.95
        }
    }

    function getSymbol(code, isDay) {
        if (code === 0) return isDay ? "☀️" : "🌙"
        if (code <= 3) return "⛅"
        if (code <= 48) return "🌫️"
        if (code <= 67 || (code >= 80 && code <= 82)) return "🌧️"
        if (code <= 77 || code === 85 || code === 86) return "❄️"
        if (code >= 95) return "⚡"
        return "🌤️"
    }

    function doFetch(lat, lon) {
        const url = "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon + "&current=temperature_2m,weather_code,is_day&timezone=auto"
        const req = new XMLHttpRequest()
        req.open("GET", url, true)
        req.onreadystatechange = function() {
            if (req.readyState === 4 && req.status === 200) {
                try {
                    const data = JSON.parse(req.responseText)
                    const cur = data.current
                    const temp = Math.round(cur.temperature_2m)
                    const symbol = getSymbol(cur.weather_code, cur.is_day)
                    root.weatherText = symbol + " " + temp + "°C"
                    root.loaded = true
                } catch (e) {
                    console.warn("atoll: weather parse error:", e)
                }
            }
        }
        req.send()
    }

    function refresh() {
        if (!weatherEnabled) {
            weatherText = ""
            return
        }

        const configuredLat = Cfg.get("weather.latitude", null)
        const configuredLon = Cfg.get("weather.longitude", null)
        if (configuredLat !== null && configuredLon !== null) {
            doFetch(configuredLat, configuredLon)
            return
        }

        if (cachedLat !== null && cachedLon !== null) {
            doFetch(cachedLat, cachedLon)
            return
        }

        // Auto-detect location by IP
        const geoReq = new XMLHttpRequest()
        geoReq.open("GET", "https://ipwho.is/", true)
        geoReq.onreadystatechange = function() {
            if (geoReq.readyState === 4) {
                if (geoReq.status === 200) {
                    try {
                        const geo = JSON.parse(geoReq.responseText)
                        if (geo.success && geo.latitude !== undefined && geo.longitude !== undefined) {
                            root.cachedLat = geo.latitude
                            root.cachedLon = geo.longitude
                            doFetch(geo.latitude, geo.longitude)
                            return
                        }
                    } catch (e) {}
                }

                // Secondary fallback
                const fallbackReq = new XMLHttpRequest()
                fallbackReq.open("GET", "http://ip-api.com/json/", true)
                fallbackReq.onreadystatechange = function() {
                    if (fallbackReq.readyState === 4 && fallbackReq.status === 200) {
                        try {
                            const geo2 = JSON.parse(fallbackReq.responseText)
                            if (geo2.status === "success" && geo2.lat !== undefined && geo2.lon !== undefined) {
                                root.cachedLat = geo2.lat
                                root.cachedLon = geo2.lon
                                doFetch(geo2.lat, geo2.lon)
                            }
                        } catch (e) {}
                    }
                }
                fallbackReq.send()
            }
        }
        geoReq.send()
    }

    Timer {
        id: timer
        interval: 900000 // 15 mins
        repeat: true
        running: root.weatherEnabled
        triggeredOnStart: true
        onTriggered: root.refresh()
    }
}
