#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
umask 022
[[ $# == 3 && $EUID != 0 ]] || { echo 'Usage (non-root): build-candidate.sh SOURCE WORK OUTPUT' >&2; exit 1; }
readonly source_dir=$1 work=$2 output=$3
readonly app_version=3.0.0-alpha.21 deb_version=3.0.0~alpha21~render1
readonly baseline=1da688363bb4aeda8b82de7a32f43ad314bce427
[[ -d $source_dir && -d $work && -d $output && -z $(ls -A -- "$work") && -z $(ls -A -- "$output") ]] || { echo 'Fresh work and output directories required' >&2; exit 1; }
[[ $(dpkg --print-architecture) == amd64 ]] || { echo 'amd64 required' >&2; exit 1; }
. /etc/os-release
[[ ${ID:-} == ubuntu && ${VERSION_ID:-} == 22.04 ]] || { echo 'Ubuntu 22.04 required' >&2; exit 1; }
export HOME="$work/home" XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data" XDG_CACHE_HOME="$work/home/cache" XDG_STATE_HOME="$work/home/state" XDG_RUNTIME_DIR="$work/home/runtime" QT_QPA_PLATFORM=offscreen
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$HOME" "$XDG_RUNTIME_DIR"
bash -n "$source_dir/tools/render/x11-runtime-test.sh"
cmake -S "$source_dir" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSYSPULSE_PACKAGE_TARGET=ubuntu-22.04 -DSYSPULSE_REPORT_ENDPOINT=https://syspulse-diagnostic-relay.onrender.com/diagnostics/v1/report
cmake --build "$work/build" --parallel 2
ctest --test-dir "$work/build" --output-on-failure --no-tests=error
# Print actual baseline execution outcomes without private command output.
ctest --test-dir "$work/build" -V -R "^core$" --no-tests=error
# Additional component suites not registered by the pinned CMake project.
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I"$source_dir/include" "$source_dir/tests/test_pressure.cpp" -o "$work/pressure-test"
"$work/pressure-test"
python3 "$source_dir/tests/test_stage_update.py"
python3 "$source_dir/tests/test_release_assets.py"
python3 "$source_dir/tests/test_apt_snapshot.py"
cpack --config "$work/build/CPackConfig.cmake" -D "CPACK_DEBIAN_PACKAGE_VERSION=$deb_version"
cmake -DPACKAGE_DIR="$work/build/packages" -DRELEASE_VERSION="$app_version" -P "$source_dir/tests/verify_packages.cmake"
shopt -s nullglob
packages=("$work/build/packages/"*.deb)
[[ ${#packages[@]} == 1 && -f ${packages[0]} && ! -L ${packages[0]} ]] || { echo 'Exactly one regular DEB required' >&2; exit 1; }
deb=${packages[0]}
[[ $(dpkg-deb -f "$deb" Package) == syspulse && $(dpkg-deb -f "$deb" Architecture) == amd64 && $(dpkg-deb -f "$deb" Version) == "$deb_version" ]] || { echo 'Wrong DEB metadata' >&2; exit 1; }
# All checks precede export. Never export raw build logs or credentials.
name=syspulse-3.0.0-alpha.21-x86_64-ubuntu-22.04-render-eval.deb
cp -- "$deb" "$output/$name"
(cd "$output" && sha256sum "$name" > SHA256SUMS && sha256sum -c SHA256SUMS && cat SHA256SUMS)
printf 'Source baseline: %s\nApplication version: %s\nDebian package version: %s\nBuild target: Ubuntu 22.04 amd64\nBuild, CTest, extra component tests and package verification: passed in build container\nSee INSTALL-STATUS.txt for separate container installation/upgrade evidence. Physical Mint XFCE and production signing/updates: NOT verified\nEvaluation candidate only; not an official release.\n' "$baseline" "$app_version" "$deb_version" > "$output/BUILD-STATUS.txt"
