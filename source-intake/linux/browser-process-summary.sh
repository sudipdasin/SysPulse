#!/usr/bin/env bash
# Read-only aggregate; no usernames, PID, arguments or paths emitted.
set -uo pipefail
export LC_ALL=C
(( $# == 0 )) || { printf 'Usage: %s\n' "${0##*/}" >&2; exit 2; }
command -v ps >/dev/null 2>&1 || { printf 'ps unavailable\n' >&2; exit 1; }
ps -eo comm=,rss= | awk '$1 ~ /^(brave|brave-browser|brave-browser-stable)$/ && $2 ~ /^[0-9]+$/ {n++; rss+=$2} END {printf "Matching browser processes: %d\nAggregate RSS (KiB, may double-count shared pages): %.0f\n", n, rss}'
