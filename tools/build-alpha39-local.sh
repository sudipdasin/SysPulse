#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local evaluation only. No sudo, dependency installation or publication.
set -euo pipefail
umask 077
readonly revision=f913e2a0506aaaf9bff3101af517f24d318bb2a2
readonly version=3.0.0-alpha.39
readonly deb_version=3.0.0~alpha39~local1
readonly repository=https://github.com/sudipdasin/SysPulse.git
mode=${1:---check}
[[ $# -le 1 && ( $mode == --check || $mode == --build ) ]] || { echo 'Usage: bash SysPulse-Alpha39-local-build.sh [--check|--build]' >&2; exit 2; }
missing=0
if [[ $EUID == 0 ]]; then echo 'Run as a normal desktop user, not root.'; missing=1; fi
. /etc/os-release
case "${ID:-}:${VERSION_ID:-}:${UBUNTU_CODENAME:-}" in
    linuxmint:22*:noble) target=linuxmint-22-noble;;
    ubuntu:24.04:*) target=ubuntu-24.04;;
    ubuntu:22.04:*) target=ubuntu-22.04;;
    *) echo 'Unsupported build distribution; use Linux Mint 22.x or Ubuntu 22.04/24.04.'; missing=1; target=unsupported;;
esac
for tool in git g++ cmake ctest cpack pkg-config dpkg dpkg-deb dpkg-shlibdeps python3 openssl gpg file desktop-file-validate sha256sum; do
    if ! command -v "$tool" >/dev/null; then echo "Missing: $tool"; missing=1; fi
done
if command -v dpkg >/dev/null && [[ $(dpkg --print-architecture) != amd64 ]]; then echo 'amd64 required.'; missing=1; fi
if ! command -v pkg-config >/dev/null || ! pkg-config --exists 'Qt5Core >= 5.15' Qt5Widgets Qt5Network; then echo 'Missing Qt5 development files (qtbase5-dev).'; missing=1; fi
if [[ $missing == 1 ]]; then
    echo 'Preflight blocked. Review missing prerequisites. No system packages changed.'
    echo 'Typical build prerequisites: build-essential cmake pkg-config qtbase5-dev dpkg-dev desktop-file-utils python3 openssl gnupg file git ca-certificates.'
    exit 1
fi
printf 'Preflight passed: %s, amd64, source %s\n' "$target" "$revision"
[[ $mode == --build ]] || exit 0
work=$(mktemp -d "$HOME/SysPulse-alpha39-local.XXXXXXXX")
mkdir -m 700 "$work/source" "$work/results" "$work/home"
export XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data" XDG_CACHE_HOME="$work/home/cache" XDG_STATE_HOME="$work/home/state" XDG_RUNTIME_DIR="$work/home/runtime" QT_QPA_PLATFORM=offscreen
export HOME="$work/home"
mkdir -m 700 "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR"
printf 'Build directory: %s\n' "$work"
git -C "$work/source" init --quiet
git -C "$work/source" -c protocol.file.allow=never -c http.sslVerify=true -c http.followRedirects=false fetch --depth 1 "$repository" "$revision"
git -C "$work/source" checkout --quiet --detach FETCH_HEAD
[[ $(git -C "$work/source" rev-parse HEAD) == "$revision" ]]
cmake -S "$work/source" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSYSPULSE_PACKAGE_TARGET="$target" -DSYSPULSE_USE_OWNER_ALPHA_TRUST=ON -DSYSPULSE_REPORT_ENDPOINT=https://syspulse-diagnostic-relay.onrender.com/diagnostics/v1/report
cmake --build "$work/build" --parallel 2
ctest --test-dir "$work/build" --output-on-failure --no-tests=error | tee "$work/results/CTEST.txt"
python3 "$work/source/tests/benchmark_diagnostic_detection.py" > "$work/results/DETECTION-TUNED.json"
python3 "$work/source/tests/benchmark_diagnostic_holdout.py" > "$work/results/DETECTION-REPLAY.json"
(cd "$work/build" && cpack --config CPackConfig.cmake -D "CPACK_DEBIAN_PACKAGE_VERSION=$deb_version")
cmake -DPACKAGE_DIR="$work/build/packages" -DRELEASE_VERSION="$version" -P "$work/source/tests/verify_packages.cmake"
shopt -s nullglob
packages=("$work/build/packages/"*.deb)
[[ ${#packages[@]} == 1 && -f ${packages[0]} && ! -L ${packages[0]} ]]
deb=${packages[0]}
[[ $(dpkg-deb -f "$deb" Package) == syspulse && $(dpkg-deb -f "$deb" Architecture) == amd64 && $(dpkg-deb -f "$deb" Version) == "$deb_version" ]]
mkdir "$work/anchor-check"
dpkg-deb -x "$deb" "$work/anchor-check"
[[ $(sha256sum "$work/anchor-check/usr/share/syspulse/update-keys/alpha.gpg" | cut -d' ' -f1) == 1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6 ]]
name="syspulse-${version}-${target}-amd64-local-eval.deb"
cp -- "$deb" "$work/results/$name"
(cd "$work/results" && sha256sum "$name" > SHA256SUMS && sha256sum -c SHA256SUMS)
printf 'Source: %s\nPlatform: %s\nLocal compilation, CTest and package validation passed. No installed upgrade, hardware or production signing validation claimed.\n' "$revision" "$target" > "$work/results/BUILD-STATUS.txt"
printf 'Evaluation DEB and reports: %s/results\nNo installation or publication was performed.\n' "$work"
