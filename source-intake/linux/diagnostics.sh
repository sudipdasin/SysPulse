#!/usr/bin/env bash
# Read-only local diagnostics. No network or privilege escalation.
set -u
export LC_ALL=C
usage() { printf 'Usage: %s [--summary|--local-detail]\n' "${0##*/}"; }
mode=${1:---summary}
case "$mode" in --summary|--local-detail) ;; --help) usage; exit 0;; *) usage >&2; exit 2;; esac
(( $# <= 1 )) || { usage >&2; exit 2; }
probe() {
    local label=$1; shift
    if ! command -v "$1" >/dev/null 2>&1; then printf '%s: unavailable\n' "$label"; return; fi
    if ! command -v timeout >/dev/null 2>&1; then printf '%s: timeout utility unavailable\n' "$label"; return; fi
    if [[ $mode == --local-detail ]]; then
        printf '\n--- %s (private local output) ---\n' "$label"
        timeout 5 "$@" || printf '[probe failed or timed out]\n'
    else
        if timeout 5 "$@" >/dev/null 2>&1; then printf '%s: query completed\n' "$label"; else printf '%s: query failed or timed out\n' "$label"; fi
    fi
}
printf 'SysPulse diagnostics | mode=%s\n' "$mode"
[[ $mode != --local-detail ]] || printf 'PRIVATE: review all output before sharing.\n'
probe 'Storage usage' df -h
probe 'Block devices' lsblk -o NAME,TYPE,SIZE,FSTYPE,MOUNTPOINTS
probe 'Memory' free -h
probe 'Compressed swap' zramctl
probe 'Swap configuration' swapon --show
probe 'Failed system services' systemctl --failed --no-pager
probe 'Current boot kernel journal' journalctl -k -b -n 100 --no-pager
probe 'Input devices (X11)' xinput list
probe 'Wine' wine --version
probe 'Lutris' lutris --version
