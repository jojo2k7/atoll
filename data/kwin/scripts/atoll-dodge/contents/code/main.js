function isAppActive() {
    var win = workspace.activeWindow;
    if (!win) {
        return false;
    }
    if (win.desktopWindow || win.dock) {
        return false;
    }
    var cls = (win.resourceClass || "").toLowerCase();
    if (cls.indexOf("plasmashell") !== -1 || cls.indexOf("plasma") !== -1) {
        return false;
    }
    if (cls.indexOf("spectacle") !== -1 || cls.indexOf("atoll") !== -1) {
        return false;
    }
    if (win.minimized || win.hidden || win.hiddenByShowDesktop) {
        return false;
    }
    return win.normalWindow === true || win.dialog === true;
}

function updateActive() {
    var active = isAppActive();
    callDBus("org.atoll.Atoll", "/Atoll", "org.atoll.Atoll", "setWindowActive", active);
}

workspace.windowActivated.connect(updateActive);
workspace.windowRemoved.connect(updateActive);
if (workspace.currentDesktopChanged) {
    workspace.currentDesktopChanged.connect(updateActive);
}

var timer = new QTimer();
timer.interval = 300;
timer.timeout.connect(updateActive);
timer.start();

updateActive();
