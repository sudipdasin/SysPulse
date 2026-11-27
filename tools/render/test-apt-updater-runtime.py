#!/usr/bin/env python3
"""Disposable container only: exercise actual new updater against fixture.
Not installed, no production URL/key overrides in helper CLI."""
import runpy, sys
from pathlib import Path
u=runpy.run_path(sys.argv[1])
key=Path("/upgrade/test-only.gpg")
url="file:/upgrade/repo"
assert u["secure_key"](key), "Fixture key or ancestor not root-owned/readable/nonwritable"
result=u["check"]("alpha",key,url)
assert result=="AVAILABLE 3.0.0~alpha23~render1", result
try:
    u["install"]("stable","3.0.0~alpha23~render1",key,url)
    raise AssertionError("channel crossing accepted")
except ValueError:
    pass
assert u["install"]("alpha","3.0.0~alpha23~render1",key,url)=="INSTALLED 3.0.0~alpha23~render1"
assert u["check"]("alpha",key,url)=="CURRENT 3.0.0~alpha23~render1"
try:
    u["install"]("alpha","3.0.0~alpha22~render1",key,url)
    raise AssertionError("downgrade accepted")
except ValueError:
    pass
print("Actual updater helper: signed discovery, full package install, readback, current version and channel/downgrade guards passed")
