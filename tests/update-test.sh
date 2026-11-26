#!/usr/bin/env bash
set -euo pipefail
[[ $EUID != 0 ]] || { echo 'Run as non-root'; exit 1; }
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tmp=$(mktemp -d); trap 'rm -rf -- "$tmp"' EXIT
export HOME="$tmp/home" FIXTURE="$tmp/assets"; mkdir -p "$HOME/.config/syspulse" "$HOME/.local/bin" "$FIXTURE" "$tmp/mock"; chmod 700 "$HOME/.local/bin"
updater="$root/tools/syspulse-update"
if "$updater" check 0.1.0; then exit 1; fi
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$tmp/key" 2>/dev/null
openssl pkey -in "$tmp/key" -pubout -out "$HOME/.config/syspulse/update-public.pem"
printf '#!/bin/sh\necho 0.1.0\n' > "$HOME/.local/bin/syspulse"; chmod +x "$HOME/.local/bin/syspulse"
printf '#!/bin/sh\necho 0.2.0\n' > "$FIXTURE/syspulse-linux-x86_64"
cat > "$tmp/mock/curl" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
while (($#)); do case "$1" in --output) out=$2;shift 2;; https://*) url=$1;shift;; *) shift;; esac; done
cp "$FIXTURE/${url##*/}" "$out"
MOCK
chmod +x "$tmp/mock/curl"; export PATH="$tmp/mock:$PATH"
manifest="$FIXTURE/syspulse-linux-x86_64.manifest"
printf 'version=0.2.0\nsha256=%s\n' "$(sha256sum "$FIXTURE/syspulse-linux-x86_64" | cut -d ' ' -f1)" > "$manifest"
openssl dgst -sha256 -sign "$tmp/key" -out "$manifest.sig" "$manifest"
chmod 777 "$HOME/.config/syspulse"
if "$updater" check 0.1.0; then exit 1; fi
chmod 700 "$HOME/.config/syspulse"
"$updater" check 0.1.0
printf 'bad' >> "$FIXTURE/syspulse-linux-x86_64"
if "$updater" install 0.1.0; then exit 1; fi
[[ $("$HOME/.local/bin/syspulse") == 0.1.0 ]]
printf '#!/bin/sh\necho 0.2.0\n' > "$FIXTURE/syspulse-linux-x86_64"
"$updater" install 0.1.0
[[ $("$HOME/.local/bin/syspulse") == 0.2.0 ]]
"$updater" install 0.2.0
cp "$manifest" "$manifest.good"
head -c 4097 /dev/zero > "$manifest"
if "$updater" check 0.2.0; then exit 1; fi
mv "$manifest.good" "$manifest"
printf '\n' >> "$manifest"
if "$updater" check 0.2.0; then exit 1; fi
printf 'Signed update fixture tests passed.\n'
