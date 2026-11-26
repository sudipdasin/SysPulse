#!/usr/bin/env bash
# Inventory installed commands only.
set -u
(( $# == 0 )) || { printf 'Usage: %s\n' "${0##*/}" >&2; exit 2; }
for cmd in systemctl journalctl lsblk findmnt smartctl hdparm fstrim e2fsck fsck.exfat ntfsfix apt-get flatpak zramctl tlp xfconf-query xinput wine wineboot wineserver lutris vulkaninfo brave-browser pkexec; do
    if command -v "$cmd" >/dev/null 2>&1; then
        printf '%s: available\n' "$cmd"
    else
        printf '%s: unavailable\n' "$cmd"
    fi
done
