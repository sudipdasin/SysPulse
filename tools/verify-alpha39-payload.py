#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only contract for locally built Alpha.39 DEBs. No extraction or execution."""
import argparse
import hashlib
import pathlib
import stat
import subprocess
import tarfile

ANCHOR = "1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6"
PROGRAMS = ("syspulse", "tux-steward", "tux-task-manager", "tux-setup", "syspulse-memory-control", "syspulse-volume-scan", "syspulse-admin-session", "syspulse-apt-update", "syspulse-input-control", "syspulse-brave-profile", "syspulse-desktop-recovery", "syspulse-schedule-profile", "syspulse-hdd-profile", "syspulse-desktop-profile", "syspulse-diagnostics")
MODULES = ("syspulse_research.py", "syspulse_hybrid.py", "syspulse_outcomes.py", "syspulse_recovery.py", "syspulse_native_probes.py", "syspulse_integrity.py", "syspulse_smart_evidence.py")
TIMERS = ("syspulse-weekly-clean.service", "syspulse-weekly-clean.timer", "syspulse-daily-clean.timer", "syspulse-monthly-clean.timer")
KEY = "usr/share/syspulse/update-keys/alpha.gpg"
KEY_HASH = "usr/share/syspulse/update-keys/alpha.sha256"
REQUIRED = {**{f"usr/bin/{p}": 0o755 for p in PROGRAMS}, **{f"usr/bin/{p}": 0o644 for p in MODULES}, **{f"usr/lib/systemd/system/{p}": 0o644 for p in TIMERS}, KEY: 0o644, KEY_HASH: 0o644, "usr/share/applications/syspulse.desktop": 0o644, "usr/share/icons/hicolor/scalable/apps/syspulse.svg": 0o644}
PARENTS = {str(parent) for name in REQUIRED for parent in pathlib.PurePosixPath(name).parents if str(parent) != "."}
VERSIONS = ("3.0.0~alpha39~local1", "3.0.0~alpha39~render1")
MAX_PAYLOAD = 64 * 1024 * 1024


def verify_tar(stream, expected_anchor=ANCHOR):
    seen = set()
    present = set()
    directories = set()
    total = 0
    key_bytes = {}
    with tarfile.open(fileobj=stream, mode="r|") as archive:
        for idx, item in enumerate(archive):
            if idx >= 4096:
                raise ValueError("Too many payload entries")
            name = item.name.removeprefix("./").rstrip("/")
            if name in ("", ".") and item.isdir():
                continue
            if not name or name.startswith("/") or any(p in ("", ".", "..") for p in name.split("/")) or name in seen:
                raise ValueError("Unsafe or duplicate payload path")
            seen.add(name)
            total += item.size
            if total > MAX_PAYLOAD:
                raise ValueError("Payload too large")
            if name in PARENTS:
                if not item.isdir() or item.uid != 0 or item.gid != 0 or item.mode & 0o7022:
                    raise ValueError(f"Unsafe required parent: {name}")
                directories.add(name)
            if name not in REQUIRED:
                continue
            if not item.isfile() or item.uid != 0 or item.gid != 0 or item.mode != REQUIRED[name] or not item.size:
                raise ValueError(f"Invalid required payload entry: {name}")
            present.add(name)
            if name in (KEY, KEY_HASH):
                if item.size > 65536:
                    raise ValueError("Oversized trust data")
                key_bytes[name] = archive.extractfile(item).read()
    if PARENTS - directories:
        raise ValueError("Missing required parent directories")
    missing = REQUIRED.keys() - present
    if missing:
        raise ValueError("Missing payload entries: " + ", ".join(sorted(missing)))
    if hashlib.sha256(key_bytes[KEY]).hexdigest() != expected_anchor or key_bytes[KEY_HASH] not in (expected_anchor.encode(), (expected_anchor + "\n").encode()):
        raise ValueError("Embedded public anchor mismatch")
    return len(present)


def verify_deb(path, version, expected_anchor=ANCHOR):
    path = pathlib.Path(path).absolute()
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= MAX_PAYLOAD:
        raise ValueError("Expected a bounded regular DEB")
    if version not in VERSIONS:
        raise ValueError("Unexpected evaluation package version")
    for field, expected in (("Package", "syspulse"), ("Architecture", "amd64"), ("Version", version)):
        actual = subprocess.check_output(["dpkg-deb", "-f", str(path), field], timeout=10, text=True).strip()
        if actual != expected:
            raise ValueError(f"Wrong package {field}")
    # The input is a locally generated package, not an arbitrary remote archive.
    with subprocess.Popen(["dpkg-deb", "--fsys-tarfile", str(path)], stdout=subprocess.PIPE) as proc:
        try:
            count = verify_tar(proc.stdout, expected_anchor)
            # tarfile stops at the end marker; drain bounded trailing padding before waiting.
            trailing = proc.stdout.read(65537)
            if len(trailing) > 65536 or any(trailing):
                raise ValueError("Unexpected trailing payload data")
            if proc.wait(timeout=10):
                raise ValueError("dpkg-deb payload read failed")
        finally:
            if proc.poll() is None:
                proc.kill()
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("deb", type=pathlib.Path)
    parser.add_argument("version", choices=VERSIONS)
    args = parser.parse_args()
    try:
        count = verify_deb(args.deb, args.version)
    except (OSError, ValueError, tarfile.TarError, subprocess.SubprocessError) as exc:
        parser.exit(1, f"Payload rejected: {exc}\n")
    print(f"Verified {count} required payload entries, root ownership, modes, metadata and owner public anchor. No installation or execution performed.")


if __name__ == "__main__":
    main()
