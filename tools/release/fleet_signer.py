#!/usr/bin/python3
"""One project per isolated signer process; no build/package execution."""
import argparse, datetime as dt, email.utils, hashlib, io, json, os, pathlib, re, resource, shutil, stat, subprocess, tarfile, tempfile, urllib.request
from fleet_policy import *
from source_signer import private_dir, read_protected, secret_fd, locked, plain, syncdir, journal, reconcile, fields, digest, hashfile, env
P=pathlib.Path
UTC=dt.timezone.utc
MAX=64*1024*1024
def atomic(path,data):
    fd,tmp=tempfile.mkstemp(dir=path.parent,prefix=".state-")
    try:
        with os.fdopen(fd,"wb") as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(tmp,path);syncdir(path.parent)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)
def run(home,*args,cwd=None,limit=1048576,timeout=120,pass_fds=(),extra=None):
    # Disk-backed output and kernel file-size cap, not unbounded PIPE buffering.
    def cap():
        resource.setrlimit(resource.RLIMIT_FSIZE,(MAX*2,MAX*2))
    e=env(home);e.update(extra or {})
    with tempfile.TemporaryFile(dir=home) as out,tempfile.TemporaryFile(dir=home) as err:
        p=subprocess.run([str(x) for x in args],cwd=cwd,env=e,stdin=subprocess.DEVNULL,stdout=out,stderr=err,timeout=timeout,pass_fds=pass_fds,preexec_fn=cap)
        require(p.returncode==0,P(args[0]).name+" failed")
        require(out.tell()<=limit,"Command output exceeds policy")
        out.seek(0);return out.read(limit+1)
def git(home,*args,cwd=None,extra=None,limit=1048576):
    return run(home,"git","-c","core.hooksPath=/dev/null","-c","credential.helper=","-c","http.sslVerify=true","-c","http.followRedirects=false","-c","protocol.file.allow=never",*args,cwd=cwd,extra=extra,limit=limit)
def clone(home,p,branch,path):
    git(home,"clone","--quiet","--depth","1","--single-branch","--branch",branch,p["repository"],path)
    require(git(home,"remote","get-url","origin",cwd=path).decode().strip()==p["repository"],"Remote mismatch")
    return git(home,"rev-parse","HEAD",cwd=path).decode().strip()
def identity(home,p,deb,v):
    for key,expected in (("Package",p["package"]),("Version",v),("Architecture",p["architecture"])):
        require(run(home,"dpkg-deb","-f",deb,key).decode().strip()==expected,"Package identity mismatch: "+key)
def anchor(home,p):
    data=trusted_file(p["anchor_path"])
    require(digest(data)==p["anchor_sha256"],"Public trust pin mismatch")
    path=home/"owner.gpg";path.write_bytes(data)
    # Reject a multi-primary keyring, even if a misconfigured policy pins it.
    raw=run(home,"gpg","--no-options","--batch","--with-colons","--show-keys",path)
    prim=[];pending=False
    for line in raw.decode().splitlines():
        cols=line.split(":")
        if cols[0]=="pub":pending=True
        elif cols[0]=="sub":pending=False
        elif cols[0]=="fpr" and pending:prim.append(cols[9]);pending=False
    require(prim==[p["fingerprint"]],"Public key fingerprint mismatch")
    return path
def package_path(p,entry):
    v=safe_version(entry["Version"])
    canonical=filename(p,v)
    # Preserve the legacy SysPulse Alpha naming on existing clients only.
    if p["project"]=="syspulse" and p["package"]=="syspulse" and p["channel"]=="alpha" and p["architecture"]=="amd64":
        m=re.fullmatch(r"3\.0\.0~alpha([1-9][0-9]{0,5})~render1",v)
        if m:canonical=f"pool/syspulse-3.0.0-alpha.{m[1]}_amd64.deb"
    require(entry.get("Filename")==canonical,"Noncanonical package path")
    return canonical
def new_path(p,v):
    if p["project"]=="syspulse" and p["package"]=="syspulse" and p["channel"]=="alpha" and p["architecture"]=="amd64":
        m=re.fullmatch(r"3\.0\.0~alpha([1-9][0-9]{0,5})~render1",v)
        if m:return f"pool/syspulse-3.0.0-alpha.{m[1]}_amd64.deb"
    return filename(p,v)
def verify(home,p,root,key,now,allow_expired=False):
    release=plain(root/"Release",8192);plain(root/"InRelease",16384);plain(root/"Release.gpg",8192)
    clear=home/"verified-release";clear.unlink(missing_ok=True)
    run(home,"gpgv","--keyring",key,"--output",clear,root/"InRelease")
    require(clear.read_bytes()==release,"Clear signature mismatch")
    run(home,"gpgv","--keyring",key,root/"Release.gpg",root/"Release")
    meta=fields(release)
    require(meta.get("Origin")==p["origin"] and meta.get("Label")==p["label"] and meta.get("Architectures")==p["architecture"],"Wrong signed feed identity")
    start=email.utils.parsedate_to_datetime(meta["Date"]);end=email.utils.parsedate_to_datetime(meta["Valid-Until"])
    require(start<=now+dt.timedelta(minutes=5) and dt.timedelta(0)<end-start<=dt.timedelta(days=30),"Invalid signed dates")
    require(allow_expired or now<end,"Feed expired")
    index=plain(root/"Packages",65536)
    match=re.search(rb"^SHA256:\n ([0-9a-f]{64}) ([0-9]+) Packages\n?\Z",release,re.M)
    require(match and match[1].decode()==digest(index) and int(match[2])==len(index),"Index hash mismatch")
    require(len(index.strip().split(b"\n\n"))==1,"Only single-package flat feeds supported")
    entry=fields(index)
    require(entry.get("Package")==p["package"] and entry.get("Architecture")==p["architecture"],"Wrong package or architecture")
    path=package_path(p,entry)
    require((root/"pool").is_dir() and not (root/"pool").is_symlink(),"Unsafe pool")
    data=plain(root/path,MAX)
    require(len(data)==int(entry["Size"]) and digest(data)==entry["SHA256"],"Package hash mismatch")
    identity(home,p,root/path,entry["Version"])
    return entry,start,end
def highwater(state,p,entry,start):
    path=state/"fleet-water.json"
    v=entry["Version"];sha=entry["SHA256"];ts=start.isoformat()
    old=strict(plain(path,4096)) if path.exists() else {"policy":policy_digest(p),"version":p["minimum_version"],"sha256":p["minimum_sha256"],"date":None}
    exact(old,"policy version sha256 date","Invalid high-water state")
    require(old["policy"]==policy_digest(p),"State belongs to different project policy")
    require(newer(v,old["version"]) or (v==old["version"] and sha==old["sha256"]),"Rollback or same-version replacement")
    require(old["date"] is None or start>=dt.datetime.fromisoformat(old["date"]),"Signed metadata replay")
    atomic(path,json.dumps({"policy":policy_digest(p),"version":v,"sha256":sha,"date":ts}).encode())
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kw):return None
def download(url,limit):
    # Disable process-environment proxy injection; trusted host egress policy is still required.
    op=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())
    with op.open(urllib.request.Request(url,headers={"User-Agent":"DebReleaseFleet/2"}),timeout=30) as r:
        require(r.geturl()==url,"Redirect rejected")
        data=r.read(limit+1);require(len(data)<=limit,"Oversized download");return data
def check_embedded(home,p,deb):
    data=run(home,"dpkg-deb","--fsys-tarfile",deb,limit=MAX,timeout=60)
    with tarfile.open(fileobj=io.BytesIO(data),mode="r:") as tar:
        members=[]
        for i,m in enumerate(tar):
            require(i<100000,"Excessive tar members")
            name=m.name[2:] if m.name.startswith("./") else m.name
            if name==p["embedded_anchor"]:members.append(m)
        require(len(members)==1 and members[0].isreg() and members[0].size<=65536,"Embedded key missing/ambiguous")
        require(digest(tar.extractfile(members[0]).read())==p["anchor_sha256"],"Embedded trust mismatch")
def fetch(home,p,c):
    base=c["artifact_url"].rsplit("/",1)[0]+"/"
    for name,k in (("BUILD-STATUS.txt","build_report_sha256"),("INSTALL-STATUS.txt","install_report_sha256")):
        require(digest(download(base+name,65536))==c[k],"Validated report digest changed")
    data=download(c["artifact_url"],c["size"])
    require(len(data)==c["size"] and digest(data)==c["sha256"],"Artifact digest/size mismatch")
    deb=home/"candidate.deb";deb.write_bytes(data);identity(home,p,deb,c["version"]);check_embedded(home,p,deb);return deb
def write_release(p,root,now):
    index=root/"Packages";fmt=lambda x:email.utils.format_datetime(x,usegmt=True)
    text=f"Origin: {p['origin']}\nLabel: {p['label']}\nArchitectures: {p['architecture']}\nDate: {fmt(now)}\nValid-Until: {fmt(now+dt.timedelta(days=7))}\nSHA256:\n {hashfile(index)} {index.stat().st_size} Packages\n"
    (root/"Release").write_text(text)
def sign(home,p,root,gpg_home,passphrase):
    private_dir(gpg_home)
    args=["gpg","--no-options","--homedir",gpg_home,"--batch","--no-tty","--local-user",p["fingerprint"]+"!","--digest-algo","SHA256"]
    for name,mode in (("InRelease","--clearsign"),("Release.gpg","--detach-sign")):
        (root/name).unlink(missing_ok=True)
        if passphrase:
            fd=secret_fd(passphrase)
            try:run(home,*args,"--pinentry-mode","loopback","--passphrase-fd",str(fd),"--output",root/name,mode,root/"Release",pass_fds=(fd,),timeout=90)
            finally:os.close(fd)
        else:run(home,*args,"--pinentry-mode","error","--output",root/name,mode,root/"Release",timeout=90)
def exact_lease(parent,branch):
    require(hexstr(parent,40) and re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_-]{1,63}",branch),"Invalid expected parent")
    return "--force-with-lease=refs/heads/"+branch+":"+parent
def direct_child(home,root,parent):
    commit=git(home,"rev-parse","HEAD",cwd=root).decode().strip()
    parents=git(home,"rev-list","--parents","-n","1",commit,cwd=root).decode().split()
    require(parents==[commit,parent],"Not a single direct successor; refuse push")
    return commit
def transaction(a,p):
    require(os.geteuid()!=0,"Dedicated non-root signer required")
    state=private_dir(a.state)
    require(state.name==p["architecture"] and state.parent.name==p["channel"] and state.parent.parent.name==p["project"],"State path must end in project/channel/architecture")
    with locked(state),tempfile.TemporaryDirectory(dir=state,prefix="job-") as tmp:
        home=P(tmp);key=anchor(home,p);root=home/"feed";parent=clone(home,p,p["feed_branch"],root)
        now=dt.datetime.now(UTC);current,start,end=verify(home,p,root,key,now,True);highwater(state,p,current,start);reconcile(state,parent,current)
        print("Verified",namespace(p),current["Version"],parent,flush=True)
        if a.mode=="verify":return
        c=None
        if a.mode=="renew":
            if end-now>dt.timedelta(days=3):print("Renewal not due");return
        else:
            require(now<end and a.approval and a.validator_uid is not None,"Fresh feed and independent approval required")
            cat=home/"catalog";clone(home,p,p["catalogue_branch"],cat)
            catalogue=strict(plain(cat/"release-candidates-v2.json",262144))
            gate=strict(read_protected(a.approval,a.validator_uid))
            c=approval(p,catalogue,gate,current,now);deb=fetch(home,p,c)
            name=new_path(p,c["version"]);require(not os.path.lexists(root/name),"Version path exists")
            shutil.copyfile(deb,root/name);(root/"Packages").write_bytes(run(home,"dpkg-scanpackages",name,"/dev/null",cwd=root))
        if not a.apply:print("Dry run; no signing or push");return
        require(a.gpg_home and a.git_token,"Protected signing credentials required")
        fd=secret_fd(a.git_token);os.close(fd)
        if c:
            # Read fresh protected approval and re-check expiration immediately before signing.
            require(approval(p,catalogue,strict(read_protected(a.approval,a.validator_uid)),current,dt.datetime.now(UTC))==c,"Approval changed during preparation")
        now=dt.datetime.now(UTC);write_release(p,root,now);sign(home,p,root,a.gpg_home,a.passphrase)
        entry,_,_=verify(home,p,root,key,now)
        require(entry==current if not c else entry["Version"]==c["version"] and entry["SHA256"]==c["sha256"],"Signed output mismatch")
        for file in ("Release","InRelease","Release.gpg","Packages",entry["Filename"]):git(home,"add","--",file,cwd=root)
        git(home,"-c","user.name=DEB Release Signer","-c","user.email=release@invalid.example","commit","-m","Publish verified "+entry["Version"]+" [skip ci]",cwd=root)
        commit=direct_child(home,root,parent)
        for file in ("Release","InRelease","Release.gpg","Packages",entry["Filename"]):
            require(git(home,"show",commit+":"+file,cwd=root,limit=MAX)==(root/file).read_bytes(),"Committed bytes changed")
        if c:require(approval(p,catalogue,strict(read_protected(a.approval,a.validator_uid)),current,dt.datetime.now(UTC))==c,"Approval changed before publication")
        # Exact lease provides atomic expected-head CAS. Direct-child proof above
        # prevents history rewrites; do not use an unconditional force push.
        branch=p["feed_branch"];ask=home/"askpass"
        ask.write_text("#!/usr/bin/python3\nimport os,sys\ns=sys.argv[1]\nif 'https://github.com' in s and 'Username' in s:print('x-access-token')\nelif 'https://x-access-token@github.com' in s and 'Password' in s:print(open(os.environ['TOKEN_PATH']).read().strip())\nelse:sys.exit(1)\n");ask.chmod(0o700)
        journal(state,{"parent":parent,"commit":commit,"version":entry["Version"],"digest":entry["SHA256"]})
        git(home,"push",exact_lease(parent,branch),"origin","HEAD:refs/heads/"+branch,cwd=root,extra={"GIT_ASKPASS":str(ask),"TOKEN_PATH":str(a.git_token)})
        remote=home/"remote";actual=clone(home,p,branch,remote);confirmed,stamp,_=verify(home,p,remote,key,dt.datetime.now(UTC))
        require(actual==commit and confirmed==entry,"Remote readback mismatch")
        highwater(state,p,confirmed,stamp);reconcile(state,actual,confirmed);print("Published and verified",namespace(p),commit)
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("mode",choices=("verify","renew","publish"));ap.add_argument("--policy",type=P,required=True);ap.add_argument("--state",type=P,required=True)
    for x in ("approval","gpg-home","passphrase","git-token"):ap.add_argument("--"+x,type=P)
    ap.add_argument("--validator-uid",type=int);ap.add_argument("--apply",action="store_true");a=ap.parse_args();os.umask(0o077)
    try:transaction(a,policy(strict(trusted_file(a.policy))));return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:print("STOP:",type(e).__name__,str(e));return 1
if __name__=="__main__":raise SystemExit(main())
