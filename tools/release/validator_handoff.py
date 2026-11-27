#!/usr/bin/python3
"""Trusted validator handoff: verify independent test attestation, never sign."""
import argparse,datetime as dt,json,os,pathlib,stat,tempfile
from source_signer import (P,UTC,ANCHOR,PIN,require,digest,clone,verify_feed,
    json_strict,plain,read_protected,approved,fetch_candidate,syncdir,BRANCH,CATALOG)
import base64
def emit(a):
    require(os.geteuid()!=0,"Run as the separate validator account")
    target=a.output
    require(target.is_absolute() and target.parent.resolve(strict=True)==target.parent and target.name=="approval.json","Unexpected approval destination")
    s=target.parent.stat()
    require(stat.S_ISDIR(s.st_mode) and s.st_uid==os.getuid() and not s.st_mode&0o027,"Unsafe validator directory")
    gate=json_strict(read_protected(a.attestation,a.orchestrator_uid))
    with tempfile.TemporaryDirectory(prefix="validate-") as t:
        home=P(t);now=dt.datetime.now(UTC)
        anchor=base64.b64decode(ANCHOR.read_text().strip(),validate=True)
        require(digest(anchor)==PIN,"Pinned public anchor mismatch")
        key=home/"owner.gpg";key.write_bytes(anchor)
        feed=home/"feed";clone(home,BRANCH,feed)
        current,_=verify_feed(home,feed,key,now)
        cat=home/"catalog";clone(home,CATALOG,cat)
        row=approved(json_strict(plain(cat/"release-candidates.json",262144)),gate,current,now)
        fetch_candidate(home,row)
    # A protected local orchestrator must have supplied the real test attestation.
    # Public reports are corroborating evidence, not permission to sign.
    fd,tmp=tempfile.mkstemp(prefix=".approval-",dir=target.parent)
    try:
        with os.fdopen(fd,"w") as f:
            json.dump(gate,f,sort_keys=True);f.write("\n");f.flush();os.fsync(f.fileno());os.fchmod(f.fileno(),0o640)
        os.replace(tmp,target);syncdir(target.parent)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)
    print("Validated handoff emitted for Alpha."+str(row["alpha"]),flush=True)
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--attestation",type=P,required=True)
    p.add_argument("--orchestrator-uid",type=int,required=True)
    p.add_argument("--output",type=P,required=True)
    a=p.parse_args();os.umask(0o077)
    try:emit(a)
    except Exception as exc:
        print("STOP:",type(exc).__name__,str(exc));raise SystemExit(1)
