// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Dialogs
import Atoll
SettingsPage {
    FileDialog {
        id: audioPicker
        title: "Choose focus audio"
        nameFilters: ["Audio (*.mp3 *.wav *.flac *.ogg *.m4a *.opus)", "All files (*)"]
        onAccepted: Cfg.set("focus.soundFile", selectedFile.toString())
    }
    title: "Focus"
    description: "Your rhythm, your sound. Changes apply immediately; durations apply to the next phase."
    SettingsGroup {
        title: "Session rhythm"
        TextSetting { label: "Intention"; key: "focus.intention"; defaultValue: "One step at a time" }
        NumberSetting { label: "Focus duration (minutes)"; key: "focus.minutes"; defaultValue: 25; from: 1; to: 240; step: 1 }
        NumberSetting { label: "Short break (minutes)"; key: "focus.breakMinutes"; defaultValue: 5; from: 1; to: 120; step: 1 }
        NumberSetting { label: "Long break (minutes)"; key: "focus.longBreakMinutes"; defaultValue: 15; from: 1; to: 120; step: 1 }
        NumberSetting { label: "Long break every N rounds"; key: "focus.rounds"; defaultValue: 4; from: 1; to: 12; step: 1 }
        NumberSetting { label: "Extend by (minutes)"; key: "focus.extensionMinutes"; defaultValue: 5; from: 1; to: 60; step: 1 }
        BoolSetting { label: "Start breaks automatically"; key: "focus.autoBreak"; defaultValue: true }
        BoolSetting { label: "Start the next focus phase automatically"; key: "focus.autoFocus"; defaultValue: false }
    }
    SettingsGroup {
        title: "Audio"
        BoolSetting { label: "Focus audio"; key: "focus.soundEnabled"; defaultValue: true }
        SettingRow { label: "Choose audio"; PushButton { text: "Browse …"; onClicked: audioPicker.open() } }
        TextSetting { label: "Audio file (full path)"; key: "focus.soundFile"; defaultValue: "" }
        NumberSetting { label: "Volume (%)"; key: "focus.volume"; defaultValue: 25; from: 0; to: 100; step: 1 }
        NumberSetting { label: "Fade duration (seconds)"; key: "focus.fadeSeconds"; defaultValue: 3; from: 0; to: 30; step: 1 }
        BoolSetting { label: "Play audio during breaks"; key: "focus.soundDuringBreak"; defaultValue: false }
    }
    SettingsGroup {
        title: "System integration"
        BoolSetting { label: "Pause on screen lock"; key: "focus.pauseOnLock"; defaultValue: true }
        BoolSetting { label: "Pause other media when starting"; key: "focus.pauseMedia"; defaultValue: true }
        BoolSetting { label: "Quiet notifications in the notch"; key: "focus.quietNotifications"; defaultValue: true }
        BoolSetting { label: "Allow critical notifications"; key: "focus.allowCritical"; defaultValue: true }
    }
    SettingsGroup {
        title: "Appearance"
        BoolSetting { label: "Breathing indicator"; key: "focus.animations"; defaultValue: true }
        NumberSetting { label: "Breathing cycle (seconds)"; key: "focus.breathSeconds"; defaultValue: 6; from: 2; to: 20; step: 1 }
        BoolSetting { label: "Show countdown"; key: "focus.showCountdown"; defaultValue: true }
        ColorSetting { label: "Focus accent"; key: "focus.accent"; defaultValue: "#9ae8cb" }
    }
}
