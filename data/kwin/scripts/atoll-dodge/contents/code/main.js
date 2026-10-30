// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tells Atoll whether an application window has the focus, so the island can
// step aside while you work. Atoll loads this only while it is set to dodge
// windows, and unloads it again otherwise.

function isAppActive() {
    var win = workspace.activeWindow;
    if (!win || win.desktopWindow || win.dock) {
        return false;
    }
    var cls = (win.resourceClass || "").toLowerCase();
    if (cls.indexOf("plasmashell") !== -1 || cls.indexOf("spectacle") !== -1 || cls.indexOf("atoll") !== -1) {
        return false;
    }
    if (win.minimized || win.hidden) {
        return false;
    }
    return win.normalWindow === true || win.dialog === true;
}

var last = null;

function update() {
    var active = isAppActive();
    if (active === last) {
        return;
    }
    last = active;
    callDBus("org.atoll.Atoll", "/Atoll", "org.atoll.Atoll", "setWindowActive", active);
}

function watch(win) {
    if (win && win.minimizedChanged) {
        win.minimizedChanged.connect(update);
    }
}

workspace.windowActivated.connect(update);
workspace.windowAdded.connect(function (win) { watch(win); update(); });
workspace.windowRemoved.connect(update);
workspace.currentDesktopChanged.connect(update);
workspace.windowList().forEach(watch);

update();
