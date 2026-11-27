#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# User-controlled dependency preparation and pinned SysPulse evaluation build.
# No full system upgrade, automatic application installation, or publication.
set -euo pipefail
umask 077
export PATH=/usr/bin:/bin
readonly revision=a587258ec5cf80fff17d5be37d463f92a0fad229
readonly builder_hash=580bc41811084567cf62f52c412f2cd0e701c209180106741f7df7f682320817
usage() {
    printf '%s\n' 'Usage (normal user):' \
      '  bash SysPulse-Alpha39-prepare-and-build.sh --check' \
      '  bash SysPulse-Alpha39-prepare-and-build.sh --prepare-and-build [LOCAL_GIT_REPO]' \
      'The latter installs missing build prerequisites with your approval, then builds the pinned evaluation DEB. It does not install SysPulse.'
}
mode=${1:---check}
case "$mode:$#" in
    --help:1|-h:1) usage; exit 0;;
    --check:0|--check:1|--prepare-and-build:1|--prepare-and-build:2) ;;
    *) usage >&2; exit 2;;
esac
if [[ $EUID == 0 ]]; then echo 'Run as an ordinary user, not with sudo.' >&2; exit 1; fi
[[ -n ${HOME:-} && -d $HOME && -w $HOME ]] || { echo 'A writable home directory is required.' >&2; exit 1; }
local_repo=()
if [[ $# == 2 ]]; then
    [[ -d $2 ]] || { echo 'Local repository path unavailable.' >&2; exit 2; }
    local_repo=("$2")
fi
work=$(mktemp -d "$HOME/SysPulse-alpha39-setup.XXXXXXXX")
readonly work
phase=extract-builder
trap 'rc=$?; if (( rc != 0 )); then printf "FAILED: %s (exit %s)\n" "$phase" "$rc" > "$work/STATUS.txt"; printf "Stopped at %s. Local evidence: %s\n" "$phase" "$work" >&2; fi' EXIT
printf 'Setup evidence: %s\nPinned source: %s\n' "$work" "$revision"
cat > "$work/builder.sh" <<'SYSPULSE_EXACT_BUILDER_EOF'
#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local evaluation only. No sudo, dependency installation or publication.
set -euo pipefail
umask 077
readonly version=3.0.0-alpha.39
readonly deb_version=3.0.0~alpha39~local1
readonly repository=https://github.com/sudipdasin/SysPulse.git
mode=${1:---check}
source_repo=
readonly build_jobs=${SYSPULSE_BUILD_JOBS:-2}
if [[ $build_jobs != 1 && $build_jobs != 2 ]]; then
    echo 'SYSPULSE_BUILD_JOBS must be 1 or 2.' >&2; exit 2
fi
if [[ $mode == --check && $# -le 1 ]]; then
    readonly revision=not-selected
elif [[ ( $mode == --build || $mode == --diagnostics ) && ( $# == 2 || $# == 3 ) && $2 =~ ^[0-9a-f]{40}$ ]]; then
    readonly revision=$2
    if [[ $# == 3 ]]; then
        source_repo=$(realpath -e -- "$3") || { echo 'Local Git repository unavailable.' >&2; exit 2; }
        [[ -d $source_repo ]] || { echo 'Local Git repository must be a directory.' >&2; exit 2; }
    fi
else
    echo 'Usage: bash tools/build-alpha39-local.sh --check | --diagnostics FULL_COMMIT [LOCAL_GIT_REPO] | --build FULL_COMMIT [LOCAL_GIT_REPO]' >&2
    exit 2
fi
if [[ $EUID == 0 ]]; then echo 'Run as a normal desktop user, not root.' >&2; exit 1; fi
export PATH=/usr/bin:/bin
missing=0
target=diagnostics-only
required=(git g++ python3 dpkg-deb timeout tar sha256sum apt-get apt-cache dpkg dpkg-scanpackages gpg gpgconf realpath tee)
if [[ $mode != --diagnostics ]]; then
    . /etc/os-release
    case "${ID:-}:${VERSION_ID:-}:${UBUNTU_CODENAME:-}" in
        linuxmint:22*:noble) target=linuxmint-22-noble;;
        ubuntu:24.04:*) target=ubuntu-24.04;;
        ubuntu:22.04:*) target=ubuntu-22.04;;
        *) echo 'Unsupported build distribution; use Linux Mint 22.x or Ubuntu 22.04/24.04.'; missing=1; target=unsupported;;
    esac
    required+=(cmake ctest cpack make pkg-config dpkg-shlibdeps openssl file desktop-file-validate)
    if command -v dpkg >/dev/null && [[ $(dpkg --print-architecture) != amd64 ]]; then echo 'amd64 required.'; missing=1; fi
    if ! command -v pkg-config >/dev/null || ! pkg-config --exists 'Qt5Core >= 5.15' Qt5Widgets Qt5Network; then echo 'Missing Qt5 development files (qtbase5-dev).'; missing=1; fi
fi
for tool in "${required[@]}"; do
    if ! command -v "$tool" >/dev/null; then echo "Missing: $tool"; missing=1; fi
done
if [[ $missing == 1 ]]; then
    echo 'Preflight blocked. Review missing prerequisites. No system packages changed.'
    echo 'Typical build prerequisites: build-essential cmake pkg-config qtbase5-dev dpkg-dev desktop-file-utils python3 openssl gnupg file git ca-certificates.'
    exit 1
fi
printf 'Preflight passed: %s, source %s, build jobs %s\n' "$target" "$revision" "$build_jobs"
[[ $mode != --check ]] || exit 0
work=$(mktemp -d "$HOME/SysPulse-alpha39-local.XXXXXXXX")
mkdir -m 700 "$work/source" "$work/results" "$work/home" "$work/git"
export XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data" XDG_CACHE_HOME="$work/home/cache" XDG_STATE_HOME="$work/home/state" XDG_RUNTIME_DIR="$work/home/runtime" QT_QPA_PLATFORM=offscreen
export HOME="$work/home" GIT_CONFIG_NOSYSTEM=1 GIT_CONFIG_GLOBAL=/dev/null GIT_NO_REPLACE_OBJECTS=1 GIT_TERMINAL_PROMPT=0
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_CONFIG_COUNT GIT_CONFIG_PARAMETERS GIT_OBJECT_DIRECTORY GIT_ALTERNATE_OBJECT_DIRECTORIES GIT_COMMON_DIR PYTHONPATH PYTHONHOME
mkdir -m 700 "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR"
printf 'Build directory: %s\n' "$work"
phase=source
trap 'rc=$?; if ((rc != 0)); then printf "FAILED: %s (exit %s)\n" "$phase" "$rc" > "$work/results/FAILURE-STATUS.txt"; printf "Failed phase: %s. Local evidence: %s/results\n" "$phase" "$work" >&2; fi' EXIT
git -C "$work/git" init --quiet
if [[ -n $source_repo ]]; then
    [[ $(git -C "$source_repo" rev-parse --verify "$revision^{commit}") == "$revision" ]] || { echo 'Exact commit unavailable in local repository.' >&2; exit 1; }
    timeout 180s git -C "$work/git" -c protocol.file.allow=always fetch --depth 1 "$source_repo" "$revision"
else
    timeout 180s git -C "$work/git" -c protocol.file.allow=never -c http.sslVerify=true -c http.followRedirects=false fetch --depth 1 "$repository" "$revision"
fi
[[ $(git -C "$work/git" rev-parse FETCH_HEAD) == "$revision" ]]
git -C "$work/git" archive --format=tar "$revision" | tar -xf - -C "$work/source"
grep -Fx 'set(SYSPULSE_ALPHA_NUMBER 39)' "$work/source/CMakeLists.txt" >/dev/null
printf '%s\n' "$revision" > "$work/results/SOURCE-CHECKOUT.txt"
run_logged() { local log=$1; shift; "$@" 2>&1 | tee "$work/results/$log"; }
phase=offline-diagnostics
run_logged OFFLINE-DIAGNOSTICS.txt bash "$work/source/tools/test-offline-alpha39.sh" --full
printf 'Source: %s\nSelected offline diagnostic suites completed; review individual skip reports. No native GUI, application package, installed upgrade or physical hardware validation claimed.\n' "$revision" > "$work/results/DIAGNOSTIC-STATUS.txt"
if [[ $mode == --diagnostics ]]; then
    printf 'Diagnostic evidence: %s/results\nNo compilation of the GUI or installation performed.\n' "$work"
    exit 0
fi
phase=configure
run_logged CONFIGURE.txt cmake -S "$work/source" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSYSPULSE_PACKAGE_TARGET="$target" -DSYSPULSE_USE_OWNER_ALPHA_TRUST=ON -DSYSPULSE_REPORT_ENDPOINT=https://syspulse-diagnostic-relay.onrender.com/diagnostics/v1/report
phase=compile
run_logged COMPILE.txt cmake --build "$work/build" --parallel "$build_jobs"
phase=ctest
run_logged CTEST.txt ctest --test-dir "$work/build" --output-on-failure --no-tests=error
phase=benchmark
python3 "$work/source/tests/benchmark_diagnostic_detection.py" > "$work/results/DETECTION-TUNED.json" 2> "$work/results/DETECTION-TUNED.stderr.txt"
python3 "$work/source/tests/benchmark_diagnostic_holdout.py" > "$work/results/DETECTION-REPLAY.json" 2> "$work/results/DETECTION-REPLAY.stderr.txt"
phase=package
(umask 022; cd "$work/build" && cpack --config CPackConfig.cmake -D "CPACK_DEBIAN_PACKAGE_VERSION=$deb_version") 2>&1 | tee "$work/results/CPACK.txt"
shopt -s nullglob
packages=("$work/build/packages/"*.deb)
[[ ${#packages[@]} == 1 && -f ${packages[0]} && ! -L ${packages[0]} ]]
deb=${packages[0]}
phase=payload
run_logged PAYLOAD-STATUS.txt python3 "$work/source/tools/verify-alpha39-payload.py" "$deb" "$deb_version"
run_logged PACKAGE-VERIFY.txt cmake -DPACKAGE_DIR="$work/build/packages" -DRELEASE_VERSION="$version" -P "$work/source/tests/verify_packages.cmake"
[[ $(dpkg-deb -f "$deb" Package) == syspulse && $(dpkg-deb -f "$deb" Architecture) == amd64 && $(dpkg-deb -f "$deb" Version) == "$deb_version" ]]
mkdir "$work/anchor-check"
dpkg-deb -x "$deb" "$work/anchor-check"
[[ $(sha256sum "$work/anchor-check/usr/share/syspulse/update-keys/alpha.gpg" | cut -d' ' -f1) == 1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6 ]]
phase=export
name="syspulse-${version}-${target}-amd64-local-eval.deb"
cp -- "$deb" "$work/results/$name"
(cd "$work/results" && sha256sum "$name" > SHA256SUMS && sha256sum -c SHA256SUMS)
printf 'Source: %s\nPlatform: %s\nLocal compilation, CTest and package validation passed. No installed upgrade, hardware or production signing validation claimed.\n' "$revision" "$target" > "$work/results/BUILD-STATUS.txt"
printf 'Evaluation DEB and reports: %s/results\nNo installation or publication was performed.\n' "$work"
SYSPULSE_EXACT_BUILDER_EOF
[[ $(sha256sum "$work/builder.sh" | cut -d' ' -f1) == "$builder_hash" ]] || { echo 'Embedded builder checksum failed.' >&2; exit 1; }
export SYSPULSE_BUILD_JOBS=1
run_logged() { local log=$1; shift; "$@" 2>&1 | tee "$work/$log"; }
phase=preflight
if run_logged PREFLIGHT.txt bash "$work/builder.sh" --check; then
    ready=1
else
    ready=0
fi
if [[ $mode == --check ]]; then
    [[ $ready == 1 ]] || exit 1
    printf 'Preflight passed; no packages installed.\n' > "$work/STATUS.txt"
    exit 0
fi
# Refuse unsupported distribution/architecture before any privileged command.
. /etc/os-release
case "${ID:-}:${VERSION_ID:-}:${UBUNTU_CODENAME:-}" in
    linuxmint:22*:noble|ubuntu:24.04:*|ubuntu:22.04:*) ;;
    *) echo 'Unsupported environment. No dependency changes attempted.' >&2; exit 1;;
esac
[[ $(dpkg --print-architecture) == amd64 ]] || { echo 'amd64 required.' >&2; exit 1; }
if [[ $ready == 0 ]]; then
    phase=dependency-preparation
    command -v sudo >/dev/null || { echo 'sudo unavailable. Ask your administrator to prepare the build environment.' >&2; exit 1; }
    [[ -t 0 ]] || { echo 'Use an interactive terminal for administrator approval.' >&2; exit 1; }
    deps=(build-essential cmake pkg-config qtbase5-dev dpkg-dev desktop-file-utils python3 openssl gnupg file git ca-certificates)
    printf '\nOnly the listed build prerequisites will be requested. No distribution upgrade or application installation.\n'
    printf ' %s' "${deps[@]}"; printf '\n'
    printf 'Refresh package indexes and review the dependency installation? [y/N] '
    read -r answer
    case "$answer" in y|Y|yes|YES) ;; *) echo 'Dependency preparation declined.'; exit 1;; esac
    phase=apt-update
    run_logged APT-UPDATE.txt sudo apt-get -o APT::Update::Error-Mode=any update
    phase=apt-preview
    run_logged APT-PLAN.txt apt-get -s --no-remove install "${deps[@]}"
    phase=apt-install
    # No automatic yes. Let APT present its own confirmation and refuse removals.
    run_logged APT-INSTALL.txt sudo apt-get --no-remove -o APT::Get::Assume-Yes=false install "${deps[@]}"
    phase=post-install-preflight
    run_logged PREFLIGHT-AFTER.txt bash "$work/builder.sh" --check
fi
phase=build
run_logged BUILD.txt bash "$work/builder.sh" --build "$revision" "${local_repo[@]}"
printf 'Pinned evaluation build completed; see BUILD.txt for builder results directory. No SysPulse installation or publication performed.\n' > "$work/STATUS.txt"
printf 'Preparation and build completed. Setup evidence: %s\n' "$work"
