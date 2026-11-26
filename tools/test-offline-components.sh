#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local component gate; not a replacement for native Qt or target VM tests.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
export PYTHONDONTWRITEBYTECODE=1
for test in health_triage health_probe pressure; do
    g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined -I"$root/include" "$root/tests/test_$test.cpp" -o "$work/$test"
    "$work/$test"
done
for test in signed_package release_policy stage_update; do
    python3 "$root/tests/test_$test.py" -q
done
printf 'Offline component gate passed; native Qt validation remains separate.\n'
