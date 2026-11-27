#!/usr/bin/env python3
"""Disposable root container ONLY. Exercise real fixed-action broker without PolicyKit."""
import json, os, pathlib, socket, subprocess
assert os.geteuid()==0
exe="/usr/bin/syspulse-admin-session"
def connect():
    parent, child=socket.socketpair()
    parent.settimeout(20)
    p=subprocess.Popen([exe],stdin=child,stdout=child,stderr=subprocess.DEVNULL,close_fds=True)
    child.close()
    stream=parent.makefile("rb")
    assert stream.readline(128)==b"SYSPULSE_ADMIN_V1\n"
    return parent,stream,p
parent,stream,p=connect()
fixture=pathlib.Path("/var/cache/apt/archives/syspulse-session-test_0_amd64.deb")
try:
    for _ in range(2):
        fixture.write_bytes(b"disposable cache fixture")
        parent.sendall(b"apt-clean\n")
        response=json.loads(stream.readline(400002))
        assert response["code"]==0,response["code"]
        assert not fixture.exists(),"Actual cache cleanup failed"
    stream.close();parent.close()
    assert p.wait(timeout=5)==0,"Broker did not exit on EOF"
finally:
    if p.poll() is None:p.kill();p.wait()
    fixture.unlink(missing_ok=True)
for request in (b"sh\n",b"apt-clean --anything\n",b"x"*65+b"\n"):
    parent,stream,p=connect()
    try:
        parent.sendall(request)
        assert stream.readline(400002)==b""
        assert p.wait(timeout=5)==1
    finally:
        stream.close();parent.close()
        if p.poll() is None:p.kill();p.wait()
print("Two real catalog operations reused one broker; EOF revoked session; arbitrary and oversized requests rejected")
