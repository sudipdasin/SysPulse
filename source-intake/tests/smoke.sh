#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
for f in linux/*.sh tests/*.sh; do bash -n "$f"; done
bash linux/capabilities.sh | grep -E '^wine: (available|unavailable)$'
bash linux/browser-process-summary.sh | grep -E '^Matching browser processes: [0-9]+$'
fixture=$(mktemp -d)
trap 'rm -rf -- "$fixture"' EXIT
for name in df lsblk free zramctl swapon systemctl journalctl xinput wine lutris; do
    printf '#!/bin/sh\nprintf "SYNTHETIC_PRIVATE_MARKER\\n"\n' > "$fixture/$name"
    chmod +x "$fixture/$name"
done
printf '#!/bin/sh\nshift\nexec "$@"\n' > "$fixture/timeout"
chmod +x "$fixture/timeout"
summary=$(PATH="$fixture:$PATH" bash linux/diagnostics.sh --summary)
[[ $summary != *SYNTHETIC_PRIVATE_MARKER* ]]
[[ $summary == *'Storage usage: query completed'* ]]
details=$(PATH="$fixture:$PATH" bash linux/diagnostics.sh --local-detail)
[[ $details == *SYNTHETIC_PRIVATE_MARKER* ]]
if bash linux/diagnostics.sh --invalid >/dev/null 2>&1; then exit 1; fi
printf 'All smoke assertions passed.\n'
