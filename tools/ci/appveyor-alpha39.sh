#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Dedicated AppVeyor evaluation worker; never install the resulting package.
set -euo pipefail
umask 077
export PATH=/usr/bin:/bin GIT_NO_REPLACE_OBJECTS=1 LC_ALL=C
[[ $# == 1 && ( $1 == prepare || $1 == build ) ]] || { echo 'Expected prepare or build.' >&2; exit 2; }
[[ $EUID != 0 && ${APPVEYOR:-} == true ]] || { echo 'Requires ordinary AppVeyor Linux worker.' >&2; exit 2; }
[[ ${APPVEYOR_REPO_NAME:-} == sudipdasin/SysPulse ]] || { echo 'Unexpected repository.' >&2; exit 2; }
case ${APPVEYOR_REPO_BRANCH:-} in build/appveyor-alpha39|integration/alpha39) ;; *) echo 'Unreviewed branch.' >&2; exit 2;; esac
[[ -z ${APPVEYOR_PULL_REQUEST_NUMBER:-} && ${APPVEYOR_REPO_TAG:-false} != true ]] || { echo 'PR and tag builds are not authorized.' >&2; exit 2; }
pin=${APPVEYOR_REPO_COMMIT:-}
[[ $pin =~ ^[0-9a-f]{40}$ ]] || { echo 'Exact commit required.' >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
[[ $(git -C "$root" rev-parse HEAD) == "$pin" ]] || { echo 'Worker HEAD differs from requested commit.' >&2; exit 2; }
if [[ $1 == prepare ]]; then
    . /etc/os-release
    [[ $ID == ubuntu && $VERSION_ID == 24.04 && $(dpkg --print-architecture) == amd64 ]] || { echo 'Unexpected build image.' >&2; exit 2; }
    sudo -n apt-get -o APT::Update::Error-Mode=any update
    sudo -n apt-get -y --no-remove --no-install-recommends install build-essential cmake pkg-config qtbase5-dev dpkg-dev desktop-file-utils python3 openssl gnupg file git ca-certificates fonts-dejavu-core xdg-utils procps iproute2 util-linux dbus-x11
    exit 0
fi
out="$root/syspulse-ci-artifacts"
[[ ! -e $out && ! -L $out ]] || { echo 'Artifact destination already exists.' >&2; exit 2; }
work=$(mktemp -d /tmp/sp-av.XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
mkdir -m 700 "$work/home" "$work/export"
export HOME="$work/home" XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data" XDG_CACHE_HOME="$work/home/cache" XDG_RUNTIME_DIR="$work/home/runtime" SYSPULSE_BUILD_JOBS=2 QT_QPA_PLATFORM=offscreen
mkdir -m 700 "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_RUNTIME_DIR"
git -C "$root" show "$pin:tools/build-alpha39-local.sh" > "$work/builder.sh"
git -C "$root" show "$pin:tools/verify-alpha39-payload.py" > "$work/payload.py"
bash "$work/builder.sh" --build "$pin" "$root" 2>&1 | tee "$work/build.log"
shopt -s nullglob
results=("$HOME"/SysPulse-alpha39-local.*/results)
[[ ${#results[@]} == 1 && -d ${results[0]} && ! -L ${results[0]} ]] || { echo 'Expected one build evidence directory.' >&2; exit 1; }
res=${results[0]}
name=syspulse-3.0.0-alpha.39-ubuntu-24.04-amd64-local-eval.deb
for item in "$name" SHA256SUMS SOURCE-CHECKOUT.txt BUILD-STATUS.txt CTEST.txt OFFLINE-DIAGNOSTICS.txt PAYLOAD-STATUS.txt PACKAGE-VERIFY.txt DETECTION-TUNED.json DETECTION-REPLAY.json; do
    [[ -f $res/$item && ! -L $res/$item && -s $res/$item ]] || { echo "Missing evidence: $item" >&2; exit 1; }
done
[[ $(cat "$res/SOURCE-CHECKOUT.txt") == "$pin" ]]
[[ $(grep -Fc "Source: $pin" "$res/BUILD-STATUS.txt") == 1 ]]
[[ $(cd "$res" && sha256sum -- "$name") == "$(cat "$res/SHA256SUMS")" ]]
python3 "$work/payload.py" "$res/$name" 3.0.0~alpha39~local1 > "$work/verified-payload.txt"
cmp -s "$work/verified-payload.txt" "$res/PAYLOAD-STATUS.txt"
cp -- "$res/$name" "$work/export/$name"
# Only fixed, explicitly selected build evidence is included. No credentials or home directory.
tar -czf "$work/export/evidence.tar.gz" -C "$res" SHA256SUMS SOURCE-CHECKOUT.txt BUILD-STATUS.txt CTEST.txt OFFLINE-DIAGNOSTICS.txt PAYLOAD-STATUS.txt PACKAGE-VERIFY.txt DETECTION-TUNED.json DETECTION-REPLAY.json
mkdir -m 700 "$out"
cp -- "$work/export/$name" "$work/export/evidence.tar.gz" "$out/"
printf 'Verified evaluation artifacts from %s. No installation or production release.\n' "$pin"
