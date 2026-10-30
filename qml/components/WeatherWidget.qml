// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The current temperature, for the resting island. Off unless
 * `idle.showWeather` is on; nothing is fetched before that. The forecast comes
 * from open-meteo.com, and without `weather.latitude` / `weather.longitude`
 * the rough position is looked up once from the IP address at ipwho.is.
 */
Item {
    id: root

    readonly property bool weatherEnabled: Cfg.get("idle.showWeather", false)
    readonly property bool fahrenheit: {
        const unit = Cfg.get("weather.unit", "auto")
        return unit === "fahrenheit" || (unit === "auto" && Qt.locale().measurementSystem === Locale.ImperialUSSystem)
    }

    property string temperature: ""
    property var iconNames: []
    property var cachedLat: null
    property var cachedLon: null

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    visible: weatherEnabled && temperature.length > 0

    onWeatherEnabledChanged: if (!weatherEnabled) {
        temperature = ""
    }
    onFahrenheitChanged: refresh()

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        IconImage {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.size(14)
            height: width
            names: root.iconNames
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.temperature
            color: Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(12)
            font.weight: Font.DemiBold
            opacity: 0.95
        }
    }

    /** WMO weather code to freedesktop icon names, most specific first. */
    function icons(code, isDay) {
        const night = isDay ? "" : "-night"
        if (code === 0) return ["weather-clear" + night, "weather-clear"]
        if (code <= 2) return ["weather-few-clouds" + night, "weather-few-clouds"]
        if (code === 3) return ["weather-overcast", "weather-clouds"]
        if (code <= 48) return ["weather-fog", "weather-overcast"]
        if (code <= 67 || (code >= 80 && code <= 82)) return ["weather-showers", "weather-showers-scattered"]
        if (code <= 77 || code === 85 || code === 86) return ["weather-snow", "weather-snow-scattered"]
        if (code >= 95) return ["weather-storm", "weather-showers"]
        return ["weather-few-clouds" + night, "weather-few-clouds"]
    }

    function fetchWeather(lat, lon) {
        const url = "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon
                    + "&current=temperature_2m,weather_code,is_day&timezone=auto"
                    + (root.fahrenheit ? "&temperature_unit=fahrenheit" : "")
        const req = new XMLHttpRequest()
        req.open("GET", url, true)
        req.onreadystatechange = function() {
            if (req.readyState !== XMLHttpRequest.DONE || req.status !== 200 || !root.weatherEnabled) {
                return
            }
            try {
                const current = JSON.parse(req.responseText).current
                root.temperature = Math.round(current.temperature_2m) + (root.fahrenheit ? "°F" : "°C")
                root.iconNames = root.icons(current.weather_code, current.is_day)
            } catch (e) {
                console.warn("atoll: weather reply could not be read:", e)
            }
        }
        req.send()
    }

    function refresh() {
        if (!weatherEnabled) {
            return
        }
        const lat = Cfg.get("weather.latitude", null)
        const lon = Cfg.get("weather.longitude", null)
        if (lat !== null && lon !== null) {
            fetchWeather(lat, lon)
            return
        }
        if (cachedLat !== null && cachedLon !== null) {
            fetchWeather(cachedLat, cachedLon)
            return
        }
        const geo = new XMLHttpRequest()
        geo.open("GET", "https://ipwho.is/", true)
        geo.onreadystatechange = function() {
            if (geo.readyState !== XMLHttpRequest.DONE || geo.status !== 200) {
                return
            }
            try {
                const place = JSON.parse(geo.responseText)
                if (place.success && place.latitude !== undefined && place.longitude !== undefined) {
                    root.cachedLat = place.latitude
                    root.cachedLon = place.longitude
                    root.fetchWeather(place.latitude, place.longitude)
                }
            } catch (e) {
                console.warn("atoll: location lookup could not be read:", e)
            }
        }
        geo.send()
    }

    Timer {
        interval: 15 * 60 * 1000
        repeat: true
        running: root.weatherEnabled
        triggeredOnStart: true
        onTriggered: root.refresh()
    }
}
