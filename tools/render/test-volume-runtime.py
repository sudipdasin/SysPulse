#!/usr/bin/env python3
"""Disposable root container only. The helper has no mutation mode."""
import json, pathlib, subprocess
exe="/usr/bin/syspulse-volume-scan"
for path in ("/proc","/sys","/dev","/not-a-mounted-volume"):
    assert subprocess.run([exe,path,"standard"],capture_output=True,timeout=10).returncode!=0,path
assert subprocess.run([exe,"/","delete"],capture_output=True,timeout=10).returncode!=0
# Verify the protected fixture appears during a real root-volume scan.
import tempfile, os, base64
with tempfile.TemporaryDirectory(prefix="syspulse-volume-fixture-") as temp:
    folder=pathlib.Path(temp)/"protected";folder.mkdir(mode=0o700)
    fixture=folder/"inspection-only.iso"
    # Large sparse file ensures it ranks among largest entries without consuming disk space.
    with fixture.open("wb") as stream:stream.truncate(16*1024**3)
    fixture.chmod(0o000)
    before=fixture.stat()
    result=subprocess.run([exe,"/","standard"],check=True,capture_output=True,timeout=200)
    data=json.loads(result.stdout)
    assert data["schema"]==1 and data["known"] and int(data["files"])>0
    assert any(base64.b64decode(e["path"])==os.fsencode(fixture) for e in data["largest"]), "protected fixture not found"
    assert fixture.stat().st_ino==before.st_ino and fixture.stat().st_size==before.st_size
    assert "candidates" not in data
print("Actual administrator metadata scan, protected fixture, pseudo-filesystem and mutation rejection passed")
