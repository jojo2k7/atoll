/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "shortcutservice.h"

#include "config/config.h"

#include <QAction>
#include <QKeySequence>

#ifdef ATOLL_HAVE_GLOBALACCEL
#include <KGlobalAccel>
#endif

using namespace Qt::StringLiterals;

namespace
{
/** The actions in qml/Actions.qml that have `shortcut: true`, in its order. */
struct ShortcutAction {
    const char *id;
    const char *label;
};

constexpr ShortcutAction kActions[] = {
    {"expand", "Open dashboard"},
    {"collapse", "Collapse"},
    {"dismiss", "Dismiss current activity"},
    {"settings", "Open settings"},
    {"office", "Claude office"},
    {"officeOrPlayPause", "Claude office, else play / pause"},
    {"assistant", "Ask the assistant"},
    {"playPause", "Play / pause"},
    {"next", "Next track"},
    {"previous", "Previous track"},
    {"volumeUp", "Volume up"},
    {"volumeDown", "Volume down"},
    {"focus", "Start or stop focus"},
    {"dnd", "Do not disturb"},
};
}

ShortcutService::ShortcutService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

bool ShortcutService::available() const
{
#ifdef ATOLL_HAVE_GLOBALACCEL
    return true;
#else
    return false;
#endif
}

void ShortcutService::start()
{
#ifdef ATOLL_HAVE_GLOBALACCEL
    if (!m_actions.isEmpty()) {
        return;
    }
    for (const auto &entry : kActions) {
        const QString id = QString::fromLatin1(entry.id);
        auto *action = new QAction(QString::fromLatin1(entry.label), this);
        action->setObjectName(id);
        action->setProperty("componentName", u"atoll"_s);
        action->setProperty("componentDisplayName", u"Atoll"_s);
        connect(action, &QAction::triggered, this, [this, id] {
            Q_EMIT activated(id);
        });
        m_actions.insert(id, action);
    }

    // A change made in System Settings lands in the config, which is what
    // the settings window shows and what the next start applies again.
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this,
            [this](QAction *action, const QKeySequence &sequence) {
                const QString id = action->objectName();
                if (!m_actions.contains(id)) {
                    return;
                }
                const QString text = sequence.toString(QKeySequence::PortableText);
                if (text == m_bound.value(id)) {
                    return;
                }
                m_bound.insert(id, text);
                m_requested.insert(id, text);
                m_config->setValue(u"controls.shortcuts."_s + id, text);
            });

    connect(m_config, &Config::changed, this, &ShortcutService::apply);
    apply();
#endif
}

void ShortcutService::apply()
{
#ifdef ATOLL_HAVE_GLOBALACCEL
    const QVariantMap shortcuts = m_config->value(u"controls.shortcuts"_s).toMap();
    for (auto it = m_actions.cbegin(); it != m_actions.cend(); ++it) {
        const QString wanted = shortcuts.value(it.key()).toString().trimmed();
        if (m_requested.contains(it.key()) && m_requested.value(it.key()) == wanted) {
            continue;
        }
        m_requested.insert(it.key(), wanted);

        QList<QKeySequence> sequence;
        if (!wanted.isEmpty()) {
            const QKeySequence parsed = QKeySequence::fromString(wanted, QKeySequence::PortableText);
            if (parsed.isEmpty()) {
                qWarning("atoll: cannot read shortcut \"%s\" for %s", qUtf8Printable(wanted), qUtf8Printable(it.key()));
            } else {
                sequence.append(parsed);
            }
        }
        KGlobalAccel::self()->setShortcut(it.value(), sequence, KGlobalAccel::NoAutoloading);

        // A combination another application already owns is dropped, so
        // remember what was really bound rather than what was asked for.
        const QList<QKeySequence> bound = KGlobalAccel::self()->shortcut(it.value());
        m_bound.insert(it.key(), bound.isEmpty() ? QString() : bound.first().toString(QKeySequence::PortableText));
    }
#endif
}
