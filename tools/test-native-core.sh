#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Qt-free, read-only core regression runner. No system installation or mutation.
set -euo pipefail
umask 077
[[ $EUID != 0 && $# -le 1 && ( $# == 0 || $1 == --ubsan ) ]] || { echo 'Usage (normal user): bash tools/test-native-core.sh [--ubsan]' >&2; exit 2; }
root=$(cd -- "$(dirname -- "$0")/.." && pwd -P)
command -v g++ >/dev/null || { echo 'Existing C++17 compiler (g++) required' >&2; exit 2; }
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -O2)
if [[ ${1:-} == --ubsan ]]; then
    flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=undefined -fno-sanitize-recover=all)
fi
for name in pressure swap_activity system_memory bounded_line; do
    g++ "${flags[@]}" -I"$root/include" "$root/tests/test_$name.cpp" -o "$work/$name"
    timeout 20s "$work/$name"
done
printf 'All four native core suites passed (%s). No Qt GUI, DEB or privileged actions tested.\n' "${1:-optimized}"
