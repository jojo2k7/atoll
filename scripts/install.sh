#!/bin/sh
#
# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build and install Atoll from this checkout, and start it.
#
# Written for somebody whose first week on Arch this is: it says what it is
# about to do before it does it, it asks before anything leaves the user's own
# account, and it stops at the first thing that goes wrong rather than carrying
# on and leaving half an install behind.

set -eu

BLUE='\033[1;34m'
DIM='\033[2m'
WARN='\033[1;33m'
BOLD='\033[1m'
OFF='\033[0m'
if [ ! -t 1 ]; then
    BLUE='' DIM='' WARN='' BOLD='' OFF=''
fi

say() { printf '%b==>%b %s\n' "$BLUE" "$OFF" "$1"; }
note() { printf '    %b%s%b\n' "$DIM" "$1" "$OFF"; }
warn() { printf '%b!!%b  %s\n' "$WARN" "$OFF" "$1"; }
die() { printf '%b!!%b  %s\n' "$WARN" "$OFF" "$1" >&2; exit 1; }

ask() {
    # Default no, and a pipe that cannot answer counts as no.
    [ -t 0 ] || return 1
    printf '    %s [y/N] ' "$1"
    read -r answer || return 1
    case "$answer" in
        y | Y | yes | YES) return 0 ;;
        *) return 1 ;;
    esac
}

# ---- where are we --------------------------------------------------------

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
[ -f CMakeLists.txt ] || die "Run this from inside the Atoll source directory."

[ "$(id -u)" -ne 0 ] || die "Do not run this as root. It asks for a password only for the one step that needs it."

command -v sudo >/dev/null 2>&1 || die "This script needs sudo to install into /usr."

# ---- dependencies --------------------------------------------------------

runtime="qt6-multimedia qt6-base qt6-declarative qt6-svg layer-shell-qt kcoreaddons ki18n dbus wayland"
building="cmake ninja gcc pkgconf qt6-shadertools extra-cmake-modules wayland"
optional="libpulse bluez cava wireplumber polkit polkit-kde-agent libsecret xdg-desktop-portal-kde"

if command -v pacman >/dev/null 2>&1; then
    say "Installing what Atoll needs to build and run"
    note "$runtime"
    note "$building"
    # --needed leaves alone anything already installed, so this is safe to
    # re-run and costs nothing on a machine that is already set up.
    sudo pacman -S --needed $runtime $building

    say "Optional extras"
    note "A real spectrum analyser, volume control, the dialog that asks for"
    note "your password, a keyring for API keys, and screen sharing."
    note "$optional"
    if ask "Install those too?"; then
        sudo pacman -S --needed $optional
    fi
else
    warn "This is not an Arch-based system, so nothing was installed for you."
    note "Atoll needs these, by whatever name your distribution gives them:"
    note "$runtime"
    note "and to build: $building"
    ask "Carry on and try to build anyway?" || exit 1
fi

# ---- build ---------------------------------------------------------------

say "Building"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build

say "Installing into /usr"
sudo cmake --install build

# ---- run it --------------------------------------------------------------

say "Starting the island"
systemctl --user daemon-reload
systemctl --user enable --now atoll.service
systemctl --user restart atoll.service
note "It starts with your session from now on. 'systemctl --user disable --now atoll.service' undoes that."

# ---- the assistant -------------------------------------------------------

if ! command -v claude >/dev/null 2>&1 && [ ! -x "$HOME/.local/bin/claude" ]; then
    say "The assistant"
    note "Holding the island opens an assistant that can answer questions and,"
    note "with your permission, do things on this machine. It talks to Claude"
    note "through the Claude Code client, which signs in with an account - there"
    note "is no API key to create."
    note ""
    note "Install it with one of:"
    note "  curl -fsSL https://claude.ai/install.sh | bash"
    note "  yay -S claude-code    (or paru, or whichever AUR helper you use)"
    note ""
    note "Then run 'claude auth login' once, or use the button in Atoll's"
    note "settings under Assistant."
else
    say "The assistant"
    if claude auth status 2>/dev/null | grep -q '"loggedIn": *true'; then
        note "The Claude Code client is installed and signed in. Hold the island to ask it something."
    else
        note "The Claude Code client is installed but not signed in yet."
        note "Run 'claude auth login', or use the button in Atoll's settings under Assistant."
    fi
fi

printf '\n%bAtoll is running.%b\n' "$BOLD" "$OFF"
note "atollctl settings   opens the settings window"
note "atollctl toggle     expands or collapses the island"
note "Hold the island     asks the assistant"
