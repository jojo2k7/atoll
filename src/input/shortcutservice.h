/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class Config;
class QAction;

/**
 * System-wide key combinations for the island's actions, registered with
 * KDE's global shortcut service under the component "atoll".
 *
 * `controls.shortcuts` in the config is the source of truth; a combination
 * changed in System Settings is written back to it, so both places agree.
 * Only the island registers anything - the settings window just asks whether
 * the service exists, to decide whether to offer the section at all. Without
 * KGlobalAccel at build time `available` is false and nothing happens.
 */
class ShortcutService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.shortcuts")

    /** Whether global shortcuts can be registered at all. */
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit ShortcutService(Config *config, QObject *parent = nullptr);

    bool available() const;

    /** Register the actions and bind them as the config says. Island only. */
    void start();

Q_SIGNALS:
    /** A bound combination was pressed; `id` is an action from Actions.qml. */
    void activated(const QString &id);

private:
    void apply();

    Config *m_config = nullptr;
    QHash<QString, QAction *> m_actions;
    /** What the config asked for, per action, when it was last applied. */
    QHash<QString, QString> m_requested;
    /** What KGlobalAccel actually bound, which can be less after a clash. */
    QHash<QString, QString> m_bound;
};
