#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
umask 022
[[ $# == 3 && $EUID != 0 ]] || { echo 'Usage (non-root): build-candidate.sh SOURCE WORK OUTPUT' >&2; exit 1; }
readonly source_dir=$1 work=$2 output=$3
readonly app_version=3.0.0-alpha.39 deb_version=3.0.0~alpha39~render1
readonly baseline=e2800b09ab587c55ba4e1267b4a38619d89f31f0
[[ -d $source_dir && -d $work && -d $output && -z $(ls -A -- "$work") && -z $(ls -A -- "$output") ]] || { echo 'Fresh work and output directories required' >&2; exit 1; }
[[ $(dpkg --print-architecture) == amd64 ]] || { echo 'amd64 required' >&2; exit 1; }
. /etc/os-release
[[ ${ID:-} == ubuntu && ${VERSION_ID:-} == 22.04 ]] || { echo 'Ubuntu 22.04 required' >&2; exit 1; }
export HOME="$work/home" XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data" XDG_CACHE_HOME="$work/home/cache" XDG_STATE_HOME="$work/home/state" XDG_RUNTIME_DIR="$work/home/runtime" QT_QPA_PLATFORM=offscreen
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$HOME" "$XDG_RUNTIME_DIR"
bash -n "$source_dir/tools/render/x11-runtime-test.sh"
cmake -S "$source_dir" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSYSPULSE_PACKAGE_TARGET=ubuntu-22.04 -DSYSPULSE_USE_OWNER_ALPHA_TRUST=ON -DSYSPULSE_REPORT_ENDPOINT=https://syspulse-diagnostic-relay.onrender.com/diagnostics/v1/report
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
python3 "$source_dir/tests/benchmark_diagnostic_detection.py" > "$output/DETECTION39-TUNED.json"
python3 "$source_dir/tests/benchmark_diagnostic_holdout.py" > "$output/DETECTION39-REPLAY.json" || { cat "$output/DETECTION39-REPLAY.json"; exit 1; }
cat "$output/DETECTION39-TUNED.json" "$output/DETECTION39-REPLAY.json"
cpack --config "$work/build/CPackConfig.cmake" -D "CPACK_DEBIAN_PACKAGE_VERSION=$deb_version"
cmake -DPACKAGE_DIR="$work/build/packages" -DRELEASE_VERSION="$app_version" -P "$source_dir/tests/verify_packages.cmake"
shopt -s nullglob
packages=("$work/build/packages/"*.deb)
[[ ${#packages[@]} == 1 && -f ${packages[0]} && ! -L ${packages[0]} ]] || { echo 'Exactly one regular DEB required' >&2; exit 1; }
deb=${packages[0]}
# Confirm the public anchor is present in the actual DEB, not merely in CMake configuration.
trust_dir="$work/trust-package-check"
mkdir "$trust_dir"
dpkg-deb -x "$deb" "$trust_dir"
test "$(sha256sum "$trust_dir/usr/share/syspulse/update-keys/alpha.gpg" | cut -d' ' -f1)" = '1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6'
test "$(cat "$trust_dir/usr/share/syspulse/update-keys/alpha.sha256")" = '1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6'
rm -rf -- "$trust_dir"
echo 'Owner Alpha public key embedded and exact digest verified in DEB'
[[ $(dpkg-deb -f "$deb" Package) == syspulse && $(dpkg-deb -f "$deb" Architecture) == amd64 && $(dpkg-deb -f "$deb" Version) == "$deb_version" ]] || { echo 'Wrong DEB metadata' >&2; exit 1; }
# All checks precede export. Never export raw build logs or credentials.
name=syspulse-3.0.0-alpha.39-x86_64-ubuntu-22.04-render-eval.deb
cp -- "$deb" "$output/$name"
(cd "$output" && sha256sum "$name" > SHA256SUMS && sha256sum -c SHA256SUMS && cat SHA256SUMS)
printf 'Source baseline: %s\nApplication version: %s\nDebian package version: %s\nBuild target: Ubuntu 22.04 amd64\nBuild, CTest, extra component tests and package verification: passed in build container\nSee INSTALL-STATUS.txt for separate container installation/upgrade evidence. Physical Mint XFCE and production signing/updates: NOT verified\nEvaluation candidate only; not an official release.\n' "$baseline" "$app_version" "$deb_version" > "$output/BUILD-STATUS.txt"
