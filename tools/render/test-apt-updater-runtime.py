#!/usr/bin/env python3
"""Disposable container only: exercise actual new updater against fixture.
Not installed, no production URL/key overrides in helper CLI."""
import runpy, sys
from pathlib import Path
u=runpy.run_path(sys.argv[1])
key=Path("/upgrade/test-only.gpg")
url="file:/upgrade/repo"
assert u["secure_key"](key), "Fixture key or ancestor not root-owned/readable/nonwritable"
# Test-only local installation of a package-style public anchor, then remove it.
import hashlib, shutil
bundled=Path("/usr/share/syspulse/update-keys")
assert not bundled.exists(),"Evaluation package must not contain a production or test key"
bundled.mkdir(parents=True,mode=0o755)
try:
    shipped=bundled/"alpha.gpg"
    shutil.copyfile(key,shipped);shipped.chmod(0o644)
    pin=bundled/"alpha.sha256"
    pin.write_text(hashlib.sha256(shipped.read_bytes()).hexdigest()+"\n");pin.chmod(0o644)
    assert u["resolve_key"]("alpha")==shipped
    assert u["check"]("alpha",u["resolve_key"]("alpha"),url)=="AVAILABLE 3.0.0~alpha25~render1"
    shipped.write_bytes(b"altered fixture")
    try:
        u["resolve_key"]("alpha")
        raise AssertionError("altered packaged key accepted")
    except ValueError:
        pass
finally:
    shutil.rmtree(bundled)
print("Automatic package-managed trust discovery, signed metadata and tamper rejection passed")
result=u["check"]("alpha",key,url)
assert result=="AVAILABLE 3.0.0~alpha25~render1", result
try:
    u["install"]("stable","3.0.0~alpha25~render1",key,url)
    raise AssertionError("channel crossing accepted")
except ValueError:
    pass
assert u["install"]("alpha","3.0.0~alpha25~render1",key,url)=="INSTALLED 3.0.0~alpha25~render1"
assert u["check"]("alpha",key,url)=="CURRENT 3.0.0~alpha25~render1"
try:
    u["install"]("alpha","3.0.0~alpha24~render1",key,url)
    raise AssertionError("downgrade accepted")
except ValueError:
    pass
print("Actual updater helper: signed discovery, full package install, readback, current version and channel/downgrade guards passed")
