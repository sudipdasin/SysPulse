#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a pinned SysPulse Qt source candidate on Linux Mint 22.3 amd64.
set -euo pipefail
umask 077
readonly REPO='https://github.com/sudipdasin/SysPulse.git'
readonly PIN='1d7bbabab067863ea2769efd13646b35bd7f4115'
readonly APP_VERSION='3.0.0-alpha.3'
readonly DEB_VERSION='3.0.0~alpha3~local.1d7bbab'
fail() { printf 'SysPulse: %s\n' "$*" >&2; exit 1; }
usage() {
    printf '%s\n' 'Usage: bash SysPulse-alpha3-XFCE-Installer.sh [--install|--help]' \
      'Default: build, test and create a local DEB without installing.' \
      '--install: after successful checks, ask before running sudo apt install.' \
      'Target: Linux Mint 22.3 amd64; XFCE desktop recommended.' \
      'Source candidate only; no official signed binary release.'
}
check_platform() {
    [[ -r /etc/os-release ]] || fail 'Cannot identify distribution.'
    # System-owned distribution metadata, not a downloaded configuration.
    . /etc/os-release
    [[ ${ID:-} == linuxmint && ${VERSION_ID:-} == 22.3 && $(dpkg --print-architecture) == amd64 ]] || fail 'Requires Linux Mint 22.3 amd64.'
}
check_dependencies() {
    local cmd
    for cmd in git cmake ctest cpack g++ dpkg dpkg-deb dpkg-query dpkg-shlibdeps python3 openssl pkg-config sha256sum file timeout mktemp; do
        command -v "$cmd" >/dev/null || fail "Missing $cmd. Install the documented build dependencies first."
    done
    pkg-config --atleast-version=5.15 Qt5Core && pkg-config --atleast-version=5.15 Qt5Widgets && pkg-config --atleast-version=5.15 Qt5Network || fail 'Qt 5.15 development packages are required.'
}
main() (
    [[ $# -le 1 ]] || fail 'Unexpected arguments.'
    local mode=build
    if [[ $# == 1 ]]; then
        case "$1" in --help) usage; exit 0;; --install) mode=install;; *) fail 'Unknown option.';; esac
    fi
    [[ $EUID != 0 ]] || fail 'Run as your normal desktop user, not root.'
    check_platform
    check_dependencies
    local work output deb installed digest
    work=$(mktemp -d)
    trap 'rm -rf -- "$work"' EXIT
    printf 'Building SysPulse Qt %s, pinned commit %s\n' "$APP_VERSION" "$PIN"
    git init --quiet "$work/source"
    git -C "$work/source" remote add origin "$REPO"
    git -C "$work/source" -c protocol.file.allow=never fetch --no-tags --depth=1 origin "$PIN"
    git -C "$work/source" -c core.hooksPath=/dev/null checkout --quiet --detach FETCH_HEAD
    [[ $(git -C "$work/source" rev-parse HEAD) == "$PIN" ]] || fail 'Source revision mismatch.'
    # The enclosing mktemp directory remains private; package files need normal modes.
    umask 022
    cmake -S "$work/source" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSYSPULSE_PACKAGE_TARGET=linux-mint-22.3-amd64
    cmake --build "$work/build" --parallel 2
    mkdir -p "$work/test-home/config" "$work/test-home/data" "$work/test-home/cache" "$work/test-home/state" "$work/test-home/runtime"
    chmod 700 "$work/test-home/runtime"
    env HOME="$work/test-home" XDG_CONFIG_HOME="$work/test-home/config" XDG_DATA_HOME="$work/test-home/data" XDG_CACHE_HOME="$work/test-home/cache" XDG_STATE_HOME="$work/test-home/state" XDG_RUNTIME_DIR="$work/test-home/runtime" QT_QPA_PLATFORM=offscreen ctest --test-dir "$work/build" --output-on-failure --no-tests=error
    g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I"$work/source/include" "$work/source/tests/test_pressure.cpp" -o "$work/pressure-test"
    "$work/pressure-test"
    env HOME="$work/test-home" XDG_CONFIG_HOME="$work/test-home/config" XDG_DATA_HOME="$work/test-home/data" XDG_CACHE_HOME="$work/test-home/cache" XDG_STATE_HOME="$work/test-home/state" python3 "$work/source/tests/test_stage_update.py"
    cpack --config "$work/build/CPackConfig.cmake" -D "CPACK_DEBIAN_PACKAGE_VERSION=$DEB_VERSION"
    cmake -DPACKAGE_DIR="$work/build/packages" -DRELEASE_VERSION="$APP_VERSION" -P "$work/source/tests/verify_packages.cmake"
    shopt -s nullglob
    set -- "$work/build/packages/"*.deb
    [[ $# == 1 && -f $1 && ! -L $1 ]] || fail 'Expected exactly one regular DEB.'
    deb=$1
    [[ $(dpkg-deb --field "$deb" Package) == syspulse && $(dpkg-deb --field "$deb" Architecture) == amd64 && $(dpkg-deb --field "$deb" Version) == "$DEB_VERSION" ]] || fail 'Unexpected package metadata.'
    umask 077
    output=$(mktemp -d "$PWD/SysPulse-XFCE.XXXXXXXX")
    digest=$(sha256sum < "$deb" | cut -d ' ' -f1)
    cp -- "$deb" "$output/"
    deb="$output/$(basename -- "$deb")"
    [[ $(sha256sum < "$deb" | cut -d ' ' -f1) == "$digest" ]] || fail 'Package copy integrity mismatch.'
    printf '%s  %s\n' "$digest" "$(basename -- "$deb")" > "$output/SHA256SUMS"
    printf '%s\n' "$PIN" > "$output/SOURCE_COMMIT"
    printf 'Local candidate package: %s\n' "$deb"
    printf 'Local package version: %s (official alpha.3 sorts newer)\n' "$DEB_VERSION"
    if [[ $mode == install ]]; then
        installed=$(dpkg-query -W -f='${Version}' syspulse 2>/dev/null || true)
        if [[ -n $installed ]] && dpkg --compare-versions "$installed" gt "$DEB_VERSION"; then fail 'Installed version is newer; refusing downgrade.'; fi
        printf '%s\n' 'Close running SysPulse/Tux Steward windows. APT is configured to refuse removals; if legacy packages conflict, stop and review the conflict separately.'
        printf 'Type INSTALL to authorize the local unsigned candidate: '
        local answer
        read -r answer || fail 'No confirmation received; package retained.'
        [[ $answer == INSTALL ]] || fail 'Installation cancelled; package retained.'
        [[ $(sha256sum < "$deb" | cut -d ' ' -f1) == "$digest" ]] || fail 'Package changed after verification.'
        sudo apt install --no-remove --reinstall "$deb"
        printf 'APT completed. Open SysPulse from the XFCE menu.\n'
    fi
)
if [[ $0 == "$BASH_SOURCE" ]]; then main "$@"; fi
