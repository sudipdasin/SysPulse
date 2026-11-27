#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only fixture/core tests; no package installation or hosted CI.
set -euo pipefail
umask 077
if [[ $EUID == 0 || $# -ne 1 ]]; then
    echo 'Usage (ordinary user): bash tools/test-offline-alpha39.sh --components|--full' >&2
    exit 2
fi
case $1 in --components|--full) mode=$1;; *) echo 'Unknown test mode' >&2; exit 2;; esac
root=$(cd -- "$(dirname -- "$0")/.." && pwd -P)
export PATH=/usr/bin:/bin
for exe in python3 g++ timeout git dpkg-deb; do
    command -v "$exe" >/dev/null || { echo "Missing prerequisite: $exe" >&2; exit 2; }
done
files=(tools/test-native-core.sh tools/syspulse-desktop-profile tools/syspulse-desktop-recovery tests/test_alpha39_build_provenance.sh tools/render/build-alpha39.sh tools/render/build-local-alpha39.sh)
files+=(tools/syspulse_native_probes.py tools/syspulse_integrity.py tools/syspulse_package_evidence.py tools/syspulse_hybrid.py tools/syspulse_smart_evidence.py tools/syspulse_outcomes.py tools/syspulse_recovery.py tools/verify-alpha39-payload.py)
for name in pressure swap_activity system_memory bounded_line; do files+=("include/syspulse/$name.hpp" "tests/test_$name.cpp"); done
pytests=(native_probes native_probe_hardening integrity package_evidence smart_evidence smart_scan_integration outcome_verification recovery_observations alpha39_payload)
for suite in "${pytests[@]}"; do files+=("tests/test_$suite.py"); done
if [[ $mode == --full ]]; then
    files+=(tools/syspulse-diagnostics tools/syspulse_research.py tests/test_hybrid_engine.py tests/test_research_adapters.py tools/build-alpha39-local.sh tests/test_local_builder_recovery.py tests/test_build_observability.py tools/recover-integration-workspace.py tests/test_workspace_recovery.py tools/transfer-integration-bundle.py tests/test_bundle_transfer.py tools/syspulse-apt-update tools/validate-update-key.py tests/test_apt_updater.py tests/test_update_plan.py tests/test_update_plan_signed_fixture.py)
    for exe in apt-get apt-cache dpkg dpkg-scanpackages gpg gpgconf; do
        command -v "$exe" >/dev/null || { echo "Missing updater test prerequisite: $exe" >&2; exit 2; }
    done
fi
for file in "${files[@]}"; do
    [[ -f $root/$file && ! -L $root/$file ]] || { echo "Required test/source missing: $file" >&2; exit 2; }
done
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
export HOME="$work/home" XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache" XDG_STATE_HOME="$work/state" XDG_RUNTIME_DIR="$work/runtime" TMPDIR="$work/tmp"
export LC_ALL=C PYTHONDONTWRITEBYTECODE=1 GIT_CONFIG_NOSYSTEM=1 GIT_CONFIG_GLOBAL=/dev/null
mkdir -m 700 "$HOME" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR" "$TMPDIR"
unset PYTHONPATH PYTHONHOME GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_CONFIG_COUNT GIT_CONFIG_PARAMETERS
run() { printf '\n== %s ==\n' "$1"; shift; timeout 180s "$@"; }
for suite in "${pytests[@]}"; do run "$suite" python3 "$root/tests/test_$suite.py"; done
run desktop-profile python3 "$root/tools/syspulse-desktop-profile" --self-test
run desktop-recovery python3 "$root/tools/syspulse-desktop-recovery" --self-test
run build-provenance bash "$root/tests/test_alpha39_build_provenance.sh"
run native-core bash "$root/tools/test-native-core.sh"
run native-core-ubsan bash "$root/tools/test-native-core.sh" --ubsan
if [[ $mode == --full ]]; then
    run diagnostic-self-test python3 "$root/tools/syspulse-diagnostics" self-test
    run hybrid-engine python3 "$root/tests/test_hybrid_engine.py"
    run research-adapters python3 "$root/tests/test_research_adapters.py"
    run local-builder-recovery python3 "$root/tests/test_local_builder_recovery.py"
    run build-observability python3 "$root/tests/test_build_observability.py"
    run workspace-recovery python3 "$root/tests/test_workspace_recovery.py"
    run bundle-transfer python3 "$root/tests/test_bundle_transfer.py"
    run apt-updater python3 "$root/tests/test_apt_updater.py"
    run update-plan python3 "$root/tests/test_update_plan.py"
    run signed-plan-fixture python3 "$root/tests/test_update_plan_signed_fixture.py"
    printf '\nFull selected offline diagnostic suite passed (see individual skip reports).\n'
else
    printf '\nComponent suite passed. Full diagnostic helper/research integration NOT tested.\n'
fi
printf 'No Qt GUI, actual application DEB installation, production update or physical hardware acceptance claimed.\n'
