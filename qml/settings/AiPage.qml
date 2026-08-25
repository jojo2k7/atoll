// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The assistant's settings: which service answers, what it is allowed to touch,
 * and how loudly it announces itself.
 *
 * The permission section is the part worth reading twice. It is written as
 * three plain choices rather than a matrix of checkboxes, because the question
 * it is really asking - how much of my computer does this thing get - is one
 * question, and splitting it into eight would only make it easier to answer
 * wrongly.
 */
SettingsPage {
    id: page

    title: qsTr("Assistant")
    description: qsTr("Hold the island to ask a question. It can answer, and - with your "
                      + "permission, one step at a time - it can act on this machine.")

    readonly property string provider: Cfg.get("ai.provider", "claude-cli")
    /** The client that signs in with an account instead of a pasted key. */
    readonly property bool usingClient: provider === "claude-cli"

    // The answer depends on another program's state, so it is asked again
    // every time this page is looked at rather than remembered from before.
    Component.onCompleted: if (page.usingClient) App.ai.refreshCli()
    onUsingClientChanged: if (page.usingClient) App.ai.refreshCli()

    SettingsGroup {
        title: qsTr("Service")

        BoolSetting {
            label: qsTr("Assistant")
            description: qsTr("Turns the long press, the edge glow and everything below it off.")
            key: "modules.ai"
            defaultValue: true
        }

        ChoiceSetting {
            label: qsTr("Provider")
            description: qsTr("Claude Code signs in with an account you may already have, and needs no key. "
                              + "The others want a key from the service that runs them. Nothing is sent "
                              + "anywhere until you ask a question.")
            key: "ai.provider"
            defaultValue: "claude-cli"
            options: [
                { value: "claude-cli", label: "Claude Code" },
                { value: "anthropic", label: qsTr("Claude API") },
                { value: "gemini", label: "Gemini" },
                { value: "openrouter", label: "OpenRouter" }
            ]
        }

        SettingRow {
            label: qsTr("Sign-in")
            description: App.ai.cliDetail
            wide: true
            visible: page.usingClient

            Column {
                width: parent.width
                spacing: 10

                Text {
                    width: parent.width
                    visible: App.ai.cliState === "missing"
                    text: App.ai.cliInstallCommand()
                    color: Skin.text
                    font.family: "monospace"
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                }

                Row {
                    spacing: 8

                    PushButton {
                        text: {
                            if (App.ai.cliState === "missing")
                                return qsTr("Copy the command")
                            return App.ai.cliState === "ready" ? qsTr("Sign in again")
                                                               : qsTr("Sign in")
                        }
                        onClicked: {
                            if (App.ai.cliState === "missing") {
                                App.copyText(App.ai.cliInstallCommand())
                                hint.text = qsTr("Copied. Paste it into a terminal, then come back "
                                                 + "here and check again.")
                                return
                            }
                            hint.text = App.ai.signInToCli()
                                ? qsTr("A terminal opened. Follow it, then check again here.")
                                : qsTr("No terminal was found. Run `claude auth login` yourself.")
                        }
                    }

                    PushButton {
                        text: qsTr("Check again")
                        onClicked: {
                            hint.text = ""
                            App.ai.refreshCli()
                        }
                    }
                }

                Text {
                    id: hint
                    width: parent.width
                    visible: text.length > 0
                    color: Skin.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        SettingRow {
            visible: !page.usingClient
            label: page.provider === "gemini" ? qsTr("Gemini API key")
                   : page.provider === "openrouter" ? qsTr("OpenRouter API key")
                   : qsTr("Claude API key")
            description: page.provider === "gemini"
                         ? qsTr("From aistudio.google.com. Atoll also reads GEMINI_API_KEY from your environment.")
                         : page.provider === "openrouter"
                         ? qsTr("From openrouter.ai/keys. One key, every model on OpenRouter. Atoll also reads OPENROUTER_API_KEY from your environment.")
                         : qsTr("From console.anthropic.com. Atoll also reads ANTHROPIC_API_KEY from your environment.")
            wide: true

            SecretField {
                id: secret
                width: 420

                // The service is asked rather than the config, because a key
                // can live in three different places and only it knows which.
                property int revision: 0
                stored: revision >= 0 && App.ai.hasKeyFor(page.provider)
                backend: revision >= 0 ? App.ai.keyBackendFor(page.provider) : "none"

                onCommitted: value => {
                    App.ai.setKeyFor(page.provider, value)
                    revision++
                }
                onCleared: {
                    App.ai.setKeyFor(page.provider, "")
                    revision++
                }
            }
        }

        SettingRow {
            visible: !page.usingClient
            label: qsTr("Check the connection")
            description: App.ai.keyTestResult.length > 0
                         ? App.ai.keyTestResult
                         : qsTr("Asks the service one throwaway question to prove the key works.")

            PushButton {
                text: qsTr("Test")
                enabled: App.ai.hasKeyFor(page.provider)
                opacity: enabled ? 1 : 0.4
                onClicked: App.ai.testKey()
            }
        }

        TextSetting {
            label: qsTr("Model")
            description: qsTr("Empty means the best general model the provider offers.")
            key: "ai.model"
            defaultValue: ""
                placeholder: page.usingClient
                             ? "sonnet"
                             : (page.provider === "gemini" ? "gemini-2.5-pro"
                                : page.provider === "openrouter" ? "anthropic/claude-sonnet-4"
                                : "claude-opus-5")
        }

        BoolSetting {
            label: qsTr("Let it search the web")
            description: qsTr("Uses the provider's own search, so no third service is involved.")
            key: "ai.webSearch"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("What it may do")

        ChoiceSetting {
            label: qsTr("Permissions")
            description: qsTr("Reading is always allowed. Anything as administrator always asks, whatever this is set to.")
            key: "ai.permissions.mode"
            defaultValue: "guarded"
            options: [
                { value: "readonly", label: qsTr("Look only") },
                { value: "guarded", label: qsTr("Ask first") },
                { value: "trusted", label: qsTr("Trust it") }
            ]
        }

        BoolSetting {
            label: qsTr("May ask for administrator rights")
            description: qsTr("Needed to install software or update the system. Your desktop asks you to confirm; Atoll never sees your password or touches your security key.")
            key: "ai.permissions.allowRoot"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("May look at your screen")
            description: qsTr("Adds a button to the question box, and lets the assistant ask for a screenshot. Your desktop asks before any picture is taken.")
            key: "ai.allowScreenshots"
            defaultValue: true
        }

        ChoiceSetting {
            label: qsTr("Which screen it looks at")
            description: qsTr("With more than one monitor, asking is usually right: everything sent is "
                              + "scaled down to fit, so one screen is legible where all of them at once "
                              + "is not. This is only the starting point - the assistant asks on the "
                              + "island, and your answer holds for that conversation.")
            key: "ai.screen"
            defaultValue: "ask"
            visible: App.ai.severalScreens
            options: [
                { value: "ask", label: qsTr("Ask me") },
                { value: "current", label: qsTr("The one I am on") },
                { value: "all", label: qsTr("All of them") }
            ]
        }

        NumberSetting {
            label: qsTr("Screenshot detail")
            description: qsTr("The longest edge of the picture, in pixels. 1568 is as much as the "
                              + "services keep; more only costs upload, less loses small text.")
            key: "ai.screenshotMaxEdge"
            defaultValue: 1568
            from: 640
            to: 2560
            step: 64
            enabled: Cfg.get("ai.allowScreenshots", true)
            opacity: enabled ? 1 : 0.45
        }

        NumberSetting {
            label: qsTr("Give up on a command after")
            description: qsTr("Seconds. A system upgrade needs more than a directory listing does.")
            key: "ai.commandTimeout"
            defaultValue: 180
            from: 10
            to: 3600
            step: 10
        }

        TextSetting {
            label: qsTr("Standing instructions")
            description: qsTr("Added to every conversation. \"I use zsh\", \"explain things simply\", that sort of thing.")
            key: "ai.systemPrompt"
            defaultValue: ""
            placeholder: qsTr("Anything it should always know")
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Appearance")

        BoolSetting {
            label: qsTr("Light up the screen edges")
            description: qsTr("While the assistant has your attention. It never covers anything: the desktop underneath stays usable.")
            key: "ai.glow"
            defaultValue: true
        }

        SliderSetting {
            label: qsTr("Glow strength")
            key: "ai.glowIntensity"
            defaultValue: 0.9
            from: 0.15
            to: 1.0
            step: 0.05
            enabled: Cfg.get("ai.glow", true)
            opacity: enabled ? 1 : 0.45
        }

        NumberSetting {
            label: qsTr("Glow width")
            description: qsTr("Pixels from the edge inwards.")
            key: "ai.glowThickness"
            defaultValue: 130
            from: 30
            to: 400
            step: 10
            enabled: Cfg.get("ai.glow", true)
            opacity: enabled ? 1 : 0.45
        }

        ColorSetting {
            label: qsTr("Glow colour")
            description: qsTr("At the island. The light does not follow the music: it is this colour "
                              + "every time the assistant opens, and it stays that colour until it closes.")
            key: "ai.glowColor"
            defaultValue: "#5aa2ff"
            presets: ["#5aa2ff", "#8f5bff", "#4ad9c8", "#ff8f4a", "#ffffff"]
        }

        ColorSetting {
            label: qsTr("Glow colour, far corners")
            description: qsTr("What it fades into by the other end of the screen. Set it to the same "
                              + "colour for one flat shade.")
            key: "ai.glowColorFar"
            defaultValue: "#8f5bff"
            presets: ["#8f5bff", "#5aa2ff", "#4ad9c8", "#ff4a8f", "#ffffff"]
        }

        BoolSetting {
            label: qsTr("Show the face")
            description: qsTr("The little character that blinks while it thinks. Turning it off leaves the text.")
            key: "ai.avatar"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Hold the island for")
            description: qsTr("Milliseconds before a press counts as opening the assistant.")
            key: "ai.longPressMs"
            defaultValue: 450
            from: 150
            to: 1500
            step: 50
        }

        NumberSetting {
            label: qsTr("Panel width")
            description: qsTr("How wide the island grows for a conversation.")
            key: "ai.panelWidth"
            defaultValue: 560
            from: 360
            to: 1000
            step: 20
            last: true
        }
    }
}
