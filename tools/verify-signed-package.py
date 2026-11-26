#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline signed DEB admission check; never installs, extracts or executes a package."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_bounded(path, limit):
    path = Path(path)
    require(not path.is_symlink() and stat.S_ISREG(path.stat().st_mode), "not a regular file")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    require(0 < len(data) <= limit, "file size rejected")
    return data


def unique_pairs(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, "duplicate manifest key")
        result[name] = value
    return result


def verify(package, manifest, signature, key, key_sha256, version, target, commit):
    require(re.fullmatch(r"[a-f0-9]{64}", key_sha256), "invalid trusted key digest")
    require(re.fullmatch(r"[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}(?:-alpha\.[0-9]{1,6}|-beta\.[0-9]{1,6})?", version), "invalid expected version")
    require(target in ("linux-mint-22.3-amd64", "ubuntu-22.04-amd64", "ubuntu-24.04-amd64"), "unsupported target")
    require(re.fullmatch(r"[a-f0-9]{40}", commit), "invalid expected source commit")
    key_bytes = read_bounded(key, 16384)
    require(hashlib.sha256(key_bytes).hexdigest() == key_sha256, "untrusted public key")
    raw = read_bounded(manifest, 16384)
    sig = read_bounded(signature, 8192)
    with tempfile.TemporaryDirectory(prefix="syspulse-verify-") as work:
        directory = Path(work)
        for name, content in (("key", key_bytes), ("manifest", raw), ("signature", sig)):
            (directory / name).write_bytes(content)
        result = subprocess.run(["/usr/bin/openssl", "dgst", "-sha256", "-verify", str(directory / "key"), "-signature", str(directory / "signature"), str(directory / "manifest")], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10, check=False)
        require(result.returncode == 0, "signature rejected")
    record = json.loads(raw, object_pairs_hook=unique_pairs)
    require(isinstance(record, dict) and set(record) == {"schema", "version", "target", "commit", "filename", "size", "sha256"}, "unexpected manifest fields")
    require(type(record["schema"]) is int and record["schema"] == 1, "unsupported manifest schema")
    require((record["version"], record["target"], record["commit"]) == (version, target, commit), "release identity mismatch")
    package = Path(package)
    require(isinstance(record["filename"], str) and re.fullmatch(r"syspulse-[a-zA-Z0-9._-]+\.deb", record["filename"]) and package.name == record["filename"], "package filename mismatch")
    size = record["size"]
    require(type(size) is int and 0 < size <= 512 * 1024 * 1024, "package size rejected")
    require(isinstance(record["sha256"], str) and re.fullmatch(r"[a-f0-9]{64}", record["sha256"]), "invalid package digest")
    require(not package.is_symlink() and stat.S_ISREG(package.stat().st_mode), "not a regular package")
    digest = hashlib.sha256()
    actual = 0
    with package.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            actual += len(block)
            require(actual <= size, "package length mismatch")
            digest.update(block)
    require(actual == size and digest.hexdigest() == record["sha256"], "package integrity mismatch")
    return {"version": version, "target": target, "commit": commit, "sha256": digest.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("package", "manifest", "signature", "key", "key-sha256", "version", "target", "commit"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(**vars(args)), sort_keys=True))
    except (OSError, ValueError, TypeError, subprocess.SubprocessError) as error:
        print("Package rejected: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
