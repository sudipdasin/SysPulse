#!/usr/bin/env python3
"""Decode an explicitly approved public release key, never secret material."""
import base64
import hashlib
from pathlib import Path
import re
import sys

def main():
    source, target, digest = sys.argv[1:]
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise ValueError("invalid digest")
    raw = Path(source).read_bytes()
    if len(raw) > 90000:
        raise ValueError("oversized public anchor")
    key = base64.b64decode(raw.strip(), validate=True)
    if not 0 < len(key) <= 65536 or hashlib.sha256(key).hexdigest() != digest:
        raise ValueError("public anchor digest mismatch")
    Path(target).write_bytes(key)

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        sys.exit(str(error))
