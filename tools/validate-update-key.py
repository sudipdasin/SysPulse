#!/usr/bin/env python3
"""Offline release build gate; never import a private signing key."""
import hashlib, pathlib, re, subprocess, sys, tempfile
def validate(path, pin):
    path=pathlib.Path(path)
    if not re.fullmatch(r"[0-9a-f]{64}",pin) or not path.is_file() or path.is_symlink() or not 0<path.stat().st_size<=65536:
        return False
    data=path.read_bytes()
    if hashlib.sha256(data).hexdigest()!=pin or not data[0]&128:
        return False
    # Parse packet headers and reject secret material independently of GnuPG's display.
    # Public-key exports use public key/subkey, signatures, user ID/attribute and trust packets.
    i=0; primary=0
    while i<len(data):
        tag=data[i];i+=1
        if not tag&128:return False
        if tag&64:
            kind=tag&63
            if i>=len(data):return False
            size=data[i];i+=1
            if 192<=size<=223:
                if i>=len(data):return False
                size=((size-192)<<8)+data[i]+192;i+=1
            elif size==255:
                if i+4>len(data):return False
                size=int.from_bytes(data[i:i+4],"big");i+=4
            elif size>=224:return False
        else:
            kind=(tag>>2)&15
            length=tag&3
            if length==3:return False
            width=1<<length
            if i+width>len(data):return False
            size=int.from_bytes(data[i:i+width],"big");i+=width
        if kind not in (2,6,12,13,14,17) or size==0 or i+size>len(data):return False
        primary+=kind==6
        i+=size
    if primary!=1:return False
    with tempfile.TemporaryDirectory() as home:
        r=subprocess.run(["/usr/bin/gpg","--no-options","--batch","--homedir",home,"--with-colons","--import-options","show-only","--dry-run","--import",str(path.resolve())],capture_output=True,timeout=20)
        lines=r.stdout.splitlines()
        return r.returncode==0 and sum(x.startswith(b"pub:") for x in lines)==1 and not any(x.startswith((b"sec:",b"ssb:")) for x in lines)
if __name__=="__main__":
    try:
        ok=len(sys.argv)==3 and validate(*sys.argv[1:])
    except (OSError,ValueError,subprocess.SubprocessError):ok=False
    if not ok:print("Public release key validation failed.",file=sys.stderr)
    sys.exit(0 if ok else 1)
