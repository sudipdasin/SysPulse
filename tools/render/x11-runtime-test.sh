#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $EUID != 0 ]] || exit 1
# Only disposable test configuration is mutated. Never use the real desktop config.
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
export XDG_CONFIG_HOME="$tmp/config" XDG_CACHE_HOME="$tmp/cache" XDG_DATA_HOME="$tmp/data" XDG_STATE_HOME="$tmp/state"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_STATE_HOME"
export QT_QPA_PLATFORM=xcb XDG_SESSION_TYPE=x11 XDG_CURRENT_DESKTOP=XFCE
dbus-run-session -- xvfb-run -a -s '-screen 0 1366x768x24' bash -euo pipefail -c '
    xfconf-query -c xfwm4 -p /general/use_compositing -n -t bool -s true
    SYSPULSE_RUNTIME_X11=1 /usr/local/bin/syspulse-core-runtime-test
    /usr/local/bin/syspulse-shell-runtime-test
    rc=0
    timeout 4s syspulse >/dev/null 2>&1 || rc=$?
    test "$rc" = 124
    echo "Virtual X11 functional runtime gates passed"
'
