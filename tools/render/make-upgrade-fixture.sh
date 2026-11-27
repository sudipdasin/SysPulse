#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Disposable local APT test fixture. Never publish this repository/key.
set -euo pipefail
umask 022
[[ $# == 2 && $EUID != 0 ]] || { echo 'Usage (non-root): make-upgrade-fixture.sh CANDIDATE_DEB EMPTY_OUTPUT' >&2; exit 1; }
readonly deb=$1 out=$2 expected=3.0.0~alpha23~render1
[[ -f $deb && ! -L $deb && -d $out && ! -L $out && -z $(ls -A -- "$out") ]] || exit 2
[[ $(dpkg-deb -f "$deb" Package) == syspulse && $(dpkg-deb -f "$deb" Architecture) == amd64 && $(dpkg-deb -f "$deb" Version) == "$expected" ]] || exit 3
export GNUPGHOME=$(mktemp -d)
trap 'gpgconf --kill gpg-agent >/dev/null 2>&1 || true; rm -rf -- "$GNUPGHOME"' EXIT
chmod 700 "$GNUPGHOME"
gpg --batch --pinentry-mode loopback --passphrase '' --quick-gen-key 'SysPulse disposable upgrade fixture <test@example.invalid>' rsa2048 sign 1d
gpg --batch --export > "$out/test-only.gpg"
mkdir -p "$out/repo/pool"
cp -- "$deb" "$out/repo/pool/syspulse_3.0.0~alpha23~render1_amd64.deb"
cmp -- "$deb" "$out/repo/pool/syspulse_3.0.0~alpha23~render1_amd64.deb"
(
    cd "$out/repo"
    dpkg-scanpackages --arch amd64 pool /dev/null > Packages
    test -s Packages
    hash=$(sha256sum Packages | cut -d' ' -f1)
    size=$(wc -c < Packages)
    printf 'Origin: SysPulse disposable test\nLabel: SysPulse disposable test\nArchitectures: amd64\nDate: %s\nValid-Until: %s\nSHA256:\n %s %s Packages\n' "$(date -Ru)" "$(date -Ru -d '+1 day')" "$hash" "$size" > Release
    gpg --batch --pinentry-mode loopback --passphrase '' --digest-algo SHA256 --clearsign --output InRelease Release
)
gpgv --keyring "$out/test-only.gpg" "$out/repo/InRelease"
printf 'Disposable signed upgrade fixture generated; not for distribution.\n'
