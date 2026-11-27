#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Disposable Git/Buildx export fixtures: real data-only DEBs; no real Docker, compilation or installation.
set -euo pipefail
[[ $EUID != 0 ]] || { echo 'Run fixture as ordinary user' >&2; exit 2; }
wrapper=$(realpath "$(dirname "$0")/../tools/render/build-local-alpha39.sh")
builder=$(realpath "$(dirname "$0")/../tools/render/build-alpha39.sh")
source_root=$(realpath "$(dirname "$0")/..")
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir "$work/repo" "$work/bin"
cd "$work/repo"
git init -q
git config user.name Fixture
git config user.email fixture@example.invalid
mkdir -p tools/render
printf 'set(SYSPULSE_ALPHA_NUMBER 39)\n' > CMakeLists.txt
printf 'FROM scratch\n' > tools/render/Dockerfile.alpha39-local
mkdir -p assets/update-trust
cp "$source_root/tools/verify-alpha39-payload.py" tools/
cp "$source_root/assets/update-trust/alpha-public.gpg.b64" assets/update-trust/
git add .
git commit -qm fixture
commit=$(git rev-parse HEAD)
printf 'dirty worktree\n' > CMakeLists.txt
touch untracked.txt
cat > "$work/bin/docker" <<'DOCKER'
#!/usr/bin/env bash
set -euo pipefail
[[ $1 == buildx ]]
if [[ $2 == version ]]; then exit 0; fi
[[ $2 == build ]]; shift 2
out= commit= context=${!#}
while (($#)); do
 case $1 in
 --build-arg) commit=${2#SOURCE_COMMIT=}; shift 2;;
 --output) out=${2#type=local,dest=}; shift 2;;
 *) shift;;
 esac
done
[[ $commit == "$FIXTURE_COMMIT" && -n $out ]]
grep -Fxq 'set(SYSPULSE_ALPHA_NUMBER 39)' "$context/CMakeLists.txt"
[[ ! -e $context/untracked.txt ]]
cd "$out"
name=syspulse-3.0.0-alpha.39-x86_64-ubuntu-22.04-render-eval.deb
# Real data-only package; no executable application or private signing key.
python3 - "$context" "$name" <<'PYFIXTURE'
import base64, hashlib, os, pathlib, runpy, subprocess, sys, tempfile
context = pathlib.Path(sys.argv[1]); name = pathlib.Path(sys.argv[2]).absolute()
gate = runpy.run_path(str(context / "tools/verify-alpha39-payload.py"))
key = base64.b64decode((context / "assets/update-trust/alpha-public.gpg.b64").read_bytes())
assert hashlib.sha256(key).hexdigest() == gate["ANCHOR"]
mode = os.environ.get("FIXTURE_MODE", "valid")
with tempfile.TemporaryDirectory() as d:
    root = pathlib.Path(d) / "root"; root.mkdir(mode=0o755); root.chmod(0o755)
    control = root / "DEBIAN"; control.mkdir(); control.chmod(0o755)
    version = "1.0" if mode == "wrong_version" else "3.0.0~alpha39~render1"
    (control / "control").write_text(f"Package: syspulse\nVersion: {version}\nArchitecture: amd64\nMaintainer: Fixture <fixture@example.invalid>\nDescription: Disposable data-only test archive\n")
    (control / "control").chmod(0o644)
    for path, perm in gate["REQUIRED"].items():
        if mode == "missing_required_module" and path == "usr/bin/syspulse_package_evidence.py":
            continue
        target = root / path; target.parent.mkdir(parents=True, exist_ok=True)
        data = key if path == gate["KEY"] else (gate["ANCHOR"] + "\n").encode() if path == gate["KEY_HASH"] else b"fixture data only\n"
        if mode == "bad_anchor" and path == gate["KEY"]: data = b"altered public anchor"
        target.write_bytes(data); target.chmod(perm)
    for path in gate["PARENTS"]: (root / path).chmod(0o755)
    subprocess.run(["dpkg-deb", "--root-owner-group", "--build", str(root), str(name)], check=True, stdout=subprocess.DEVNULL)
pathlib.Path("PAYLOAD-STATUS.txt").write_text(f"Verified {len(gate['REQUIRED'])} required payload entries, root ownership, modes, metadata and owner public anchor. No installation or execution performed.\n")
if mode == "fake_package": name.write_bytes(b"not a Debian archive\n")
PYFIXTURE
printf '%s\n' "$commit" > SOURCE-CHECKOUT.txt
printf 'Source commit: %s\n' "$commit" > BUILD-STATUS.txt
printf 'Synthetic installation evidence\n' > INSTALL-STATUS.txt
sha256sum "$name" > SHA256SUMS
case ${FIXTURE_MODE:-valid} in
 valid) ;;
 wrong_checkout) printf '%040d\n' 0 > SOURCE-CHECKOUT.txt;;
 wrong_report) printf 'Source commit: %040d\n' 0 > BUILD-STATUS.txt;;
 duplicate_report) printf 'Source commit: %s\n' "$commit" >> BUILD-STATUS.txt;;
 tamper) printf 'changed\n' >> "$name";;
 missing_install) rm INSTALL-STATUS.txt;;
 symlink_report) mv BUILD-STATUS.txt other; ln -s other BUILD-STATUS.txt;;
 wrong_manifest) sha256sum BUILD-STATUS.txt > SHA256SUMS;;
 missing_package) rm "$name";;
 missing_payload_report) rm PAYLOAD-STATUS.txt;;
 wrong_payload_report) printf 'Unverified payload\n' > PAYLOAD-STATUS.txt;;
 symlink_payload_report) mv PAYLOAD-STATUS.txt payload-other; ln -s payload-other PAYLOAD-STATUS.txt;;
 wrong_version|missing_required_module|bad_anchor|fake_package) ;;
 *) exit 99;;
esac
DOCKER
chmod +x "$work/bin/docker"
export PATH="$work/bin:$PATH" FIXTURE_COMMIT="$commit"
passed=0
for mode in valid wrong_checkout wrong_report duplicate_report tamper missing_install symlink_report wrong_manifest missing_package missing_payload_report wrong_payload_report symlink_payload_report wrong_version missing_required_module bad_anchor fake_package; do
 rc=0
 FIXTURE_MODE=$mode bash "$wrapper" "$commit" "$work/out-$mode" > "$work/$mode.log" 2>&1 || rc=$?
 if [[ $mode == valid ]]; then
   [[ $rc == 0 ]] || { cat "$work/$mode.log"; exit 1; }
 else
   [[ $rc != 0 ]] || { echo "Unexpected acceptance: $mode"; exit 1; }
   ! grep -q '^Local evaluation artifacts:' "$work/$mode.log" || exit 1
 fi
 printf 'PASS: %s\n' "$mode"; passed=$((passed+1))
done
rc=0; bash "$wrapper" short "$work/invalid" > "$work/invalid.log" 2>&1 || rc=$?
[[ $rc == 2 && ! -e $work/invalid ]]; passed=$((passed+1)); echo 'PASS: invalid commit'
rc=0; bash "$wrapper" "$commit" "$work/out-valid" > "$work/existing.log" 2>&1 || rc=$?
[[ $rc == 2 ]]; passed=$((passed+1)); echo 'PASS: existing output'
# A local replacement of the reviewed commit must not affect the archive.
printf 'set(SYSPULSE_ALPHA_NUMBER 38)\n' > CMakeLists.txt
git add CMakeLists.txt; git commit -qm replacement
replacement=$(git rev-parse HEAD)
git replace "$commit" "$replacement"
FIXTURE_MODE=valid bash "$wrapper" "$commit" "$work/out-replaced" > "$work/replaced.log" 2>&1 || { cat "$work/replaced.log"; exit 1; }
git replace -d "$commit" >/dev/null
passed=$((passed+1)); echo 'PASS: local replacement ref ignored'
mkdir "$work/source" "$work/build" "$work/artifacts"
for value in '' bad; do
 rc=0; SOURCE_COMMIT=$value bash "$builder" "$work/source" "$work/build" "$work/artifacts" > "$work/builder.log" 2>&1 || rc=$?
 [[ $rc == 1 ]] && grep -Fxq 'Exact SOURCE_COMMIT required' "$work/builder.log"
 [[ -z $(ls -A "$work/artifacts") && -z $(ls -A "$work/build") ]]
 passed=$((passed+1)); echo 'PASS: missing or malformed source provenance rejected'
done
printf '%s fixture cases passed (no real Docker run)\n' "$passed"
