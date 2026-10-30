#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 The Atoll contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Register `atoll --claude-hook` for every Claude Code event the office uses.
# Usage: claude-office-hooks.py <atoll binary> <settings.json> [...]
# Idempotent; existing hooks are kept and a .pre-office backup is written once.

import json
import os
import shutil
import sys

EVENTS = [
    "SessionStart", "SessionEnd", "UserPromptSubmit",
    "PreToolUse", "PostToolUse", "PostToolUseFailure", "PermissionRequest",
    "Notification", "PreCompact", "Stop", "SubagentStart", "SubagentStop",
]


def install(binary, path):
    settings = {}
    if os.path.exists(path):
        with open(path) as f:
            settings = json.load(f)
        backup = path + ".pre-office"
        if not os.path.exists(backup):
            shutil.copy2(path, backup)

    command = f"{binary} --claude-hook"
    hooks = settings.setdefault("hooks", {})
    for event in EVENTS:
        groups = hooks.setdefault(event, [])
        for group in groups:
            group["hooks"] = [h for h in group.get("hooks", [])
                              if "--claude-hook" not in h.get("command", "")]
        groups[:] = [g for g in groups if g.get("hooks")]
        groups.append({"hooks": [{"type": "command", "command": command,
                                  "async": True, "timeout": 5}]})

    # A second, synchronous entry for AskUserQuestion only: the async hook
    # above still fires for it too (that is fine, it just sets `waiting`),
    # but this one is what lets the office answer the question itself.
    ask_command = f"{binary} --claude-ask"
    pre_groups = hooks.setdefault("PreToolUse", [])
    for group in pre_groups:
        group["hooks"] = [h for h in group.get("hooks", [])
                          if "--claude-ask" not in h.get("command", "")]
    pre_groups[:] = [g for g in pre_groups if g.get("hooks")]
    pre_groups.append({"matcher": "AskUserQuestion",
                        "hooks": [{"type": "command", "command": ask_command, "timeout": 600}]})

    with open(path + ".tmp", "w") as f:
        json.dump(settings, f, indent=2)
        f.write("\n")
    st = os.stat(path) if os.path.exists(path) else None
    os.replace(path + ".tmp", path)
    if st:
        os.chown(path, st.st_uid, st.st_gid)
    print(f"{path}: {len(EVENTS)} events -> {command}; AskUserQuestion -> {ask_command}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__ or "usage: claude-office-hooks.py <atoll binary> <settings.json> [...]")
    for target in sys.argv[2:]:
        install(sys.argv[1], target)
