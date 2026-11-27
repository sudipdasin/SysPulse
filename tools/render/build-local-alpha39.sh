#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the tracked source of one explicit commit; never publish or install.
set -euo pipefail
umask 077
# A reviewed commit must resolve to its original objects, never a local replacement.
export GIT_NO_REPLACE_OBJECTS=1
[[ $# == 2 && $EUID != 0 ]] || { echo 'Usage (ordinary user): bash tools/render/build-local-alpha39.sh COMMIT NEW_OUTPUT_DIR' >&2; exit 2; }
commit=$1
[[ $commit =~ ^[0-9a-f]{40}$ ]] || { echo 'Exact full Git commit required' >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
[[ $(git -C "$root" rev-parse --verify "$commit^{commit}") == "$commit" ]] || exit 2
git -C "$root" show "$commit:CMakeLists.txt" | grep -Fx 'set(SYSPULSE_ALPHA_NUMBER 39)' >/dev/null
git -C "$root" cat-file -e "$commit:tools/render/Dockerfile.alpha39-local"
git -C "$root" cat-file -e "$commit:tools/verify-alpha39-payload.py"
for tool in python3 dpkg-deb cmp; do
 command -v "$tool" >/dev/null || { echo "Missing export verification prerequisite: $tool" >&2; exit 2; }
done
[[ ! -e $2 && ! -L $2 ]] || { echo 'Output directory must not already exist' >&2; exit 2; }
command -v docker >/dev/null && docker buildx version >/dev/null || { echo 'Existing Docker with Buildx is required; no dependencies will be installed automatically.' >&2; exit 2; }
out=$(realpath -m -- "$2")
[[ $out != / && $out != "$root" && $out != "$root/"* ]] || { echo 'Output must be outside the source tree' >&2; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
git -C "$root" archive --format=tar "$commit" | tar -xf - -C "$tmp"
mkdir -- "$out"
docker buildx build --platform linux/amd64 --target artifacts \
  --build-arg "SOURCE_COMMIT=$commit" \
  --file "$tmp/tools/render/Dockerfile.alpha39-local" \
  --output "type=local,dest=$out" "$tmp"
(
 cd "$out"
 name=syspulse-3.0.0-alpha.39-x86_64-ubuntu-22.04-render-eval.deb
 for file in SOURCE-CHECKOUT.txt BUILD-STATUS.txt SHA256SUMS INSTALL-STATUS.txt PAYLOAD-STATUS.txt "$name"; do
   [[ -f $file && ! -L $file && -s $file ]] || { echo "Missing or unsafe artifact: $file" >&2; exit 1; }
 done
 [[ $(cat SOURCE-CHECKOUT.txt) == "$commit" ]]
 [[ $(grep '^Source commit:' BUILD-STATUS.txt) == "Source commit: $commit" ]]
 # The export contract contains exactly one known package, not arbitrary manifest paths.
 [[ $(sha256sum -- "$name") == "$(cat SHA256SUMS)" ]]
 # Revalidate the actual exported bytes with the verifier from the pinned source.
 python3 "$tmp/tools/verify-alpha39-payload.py" "$name" 3.0.0~alpha39~render1 > "$tmp/verified-payload.txt"
 cmp -s "$tmp/verified-payload.txt" PAYLOAD-STATUS.txt || { echo "Exported payload evidence differs from independent verification" >&2; exit 1; }
)
printf 'Local evaluation artifacts: %s\nNo production publication or installation performed.\n' "$out"
