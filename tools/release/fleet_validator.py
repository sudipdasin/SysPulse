#!/usr/bin/python3
"""Fleet validator handoff. Never execute DEBs or access signing credentials."""
import argparse,datetime as dt,os,stat,tempfile,json
from pathlib import Path
from fleet_policy import policy,strict,trusted_file,approval,require,namespace
from fleet_signer import anchor,clone,verify,fetch,atomic
from source_signer import read_protected,plain
def handoff(a):
    require(os.geteuid()!=0 and os.getuid()!=a.signer_uid and a.signer_uid!=a.orchestrator_uid,"Separate orchestrator, validator and signer identities required")
    p=policy(strict(trusted_file(a.policy)));target=a.output
    require(target.is_absolute() and target.name=="approval.json" and target.parent.resolve(strict=True)==target.parent,"Noncanonical approval destination")
    parent=target.parent;s=parent.stat()
    require(stat.S_ISDIR(s.st_mode) and s.st_uid==os.getuid() and not s.st_mode&0o027,"Validator directory must be protected")
    require(parent.name==p["architecture"] and parent.parent.name==p["channel"] and parent.parent.parent.name==p["project"],"Wrong project handoff namespace")
    gate=strict(read_protected(a.attestation,a.orchestrator_uid))
    with tempfile.TemporaryDirectory(prefix="fleet-validator-") as tmp:
        home=Path(tmp);key=anchor(home,p);root=home/"feed";clone(home,p,p["feed_branch"],root)
        current,_,_=verify(home,p,root,key,dt.datetime.now(dt.timezone.utc))
        cat=home/"catalog";clone(home,p,p["catalogue_branch"],cat)
        catalogue=strict(plain(cat/"release-candidates-v2.json",262144))
        c=approval(p,catalogue,gate,current,dt.datetime.now(dt.timezone.utc));fetch(home,p,c)
        require(strict(read_protected(a.attestation,a.orchestrator_uid))==gate,"Independent attestation changed")
        require(approval(p,catalogue,gate,current,dt.datetime.now(dt.timezone.utc))==c,"Approval changed or expired")
    fd,name=tempfile.mkstemp(dir=parent,prefix=".approval-")
    try:
        with os.fdopen(fd,"wb") as out:
            out.write(json.dumps(gate,sort_keys=True).encode()+b"\n");out.flush();os.fchmod(out.fileno(),0o640);os.fsync(out.fileno())
        os.replace(name,target)
        from source_signer import syncdir
        syncdir(parent)
    finally:
        if os.path.exists(name):os.unlink(name)
    print("Independent handoff emitted",namespace(p),c["version"])
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ("policy","attestation","output"):ap.add_argument("--"+name,type=Path,required=True)
    for name in ("orchestrator-uid","signer-uid"):ap.add_argument("--"+name,type=int,required=True)
    a=ap.parse_args();os.umask(0o077)
    try:handoff(a);return 0
    except Exception as exc:print("STOP:",type(exc).__name__,str(exc));return 1
if __name__=="__main__":raise SystemExit(main())
