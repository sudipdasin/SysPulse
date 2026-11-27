#!/usr/bin/python3
"""Isolated source-side Alpha feed publisher. No build execution in signer."""
import argparse, base64, datetime as dt, email.utils, fcntl, hashlib, json, os
import pathlib, re, shutil, stat, subprocess, tempfile, unittest, urllib.request
from contextlib import contextmanager
P=pathlib.Path
KEY="DA269C59E903F28BD46DA217F88380E446488691"
PIN="1d16b08d5bf50ba03cd8f294a94ef7f5259f9c5f4795a39134e9fde4b5968dc6"
REPO="https://github.com/sudipdasin/SysPulse.git"
BRANCH="updates-alpha"
CATALOG="release-candidates"
ANCHOR=P(__file__).resolve().parents[2]/"assets/update-trust/alpha-public.gpg.b64"
MAX_DEB=16*1024*1024
MIN_ALPHA=32
MIN_DIGEST="4ad1349a6c369aa211d80b873074082f5d50bd29934e07f1c55221e34d54babc"
UTC=dt.timezone.utc
HEX40=r"[0-9a-f]{40}"
HEX64=r"[0-9a-f]{64}"
def require(ok,msg):
    if not ok: raise ValueError(msg)
def digest(data): return hashlib.sha256(data).hexdigest()
def hashfile(p):
    with p.open("rb") as f:
        h=hashlib.sha256()
        for b in iter(lambda:f.read(1048576),b""):h.update(b)
    return h.hexdigest()
def version(n): return f"3.0.0~alpha{n}~render1"
def number(v):
    m=re.fullmatch(r"3\.0\.0~alpha([1-9][0-9]{0,5})~render1",v)
    require(m is not None,"Unsupported package version")
    return int(m[1])
def json_strict(data):
    def pairs(values):
        d={}
        for k,v in values:
            require(k not in d,"Duplicate JSON field");d[k]=v
        return d
    return json.loads(data,object_pairs_hook=pairs,parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON value")))
def sha_valid(v,n=64): return isinstance(v,str) and re.fullmatch(HEX64 if n==64 else HEX40,v) is not None
def validate(row):
    keys={"alpha","source","deployment","origin","sha256","tested_from","tested_from_sha256","suites"}
    require(isinstance(row,dict) and set(row)==keys,"Invalid candidate schema")
    n,old,count=(row[k] for k in ("alpha","tested_from","suites"))
    require(type(n) is int and type(old) is int and type(count) is int and 0<old<n<=999999 and 54<=count<=100000,"Version or test-count gate failed")
    require(sha_valid(row["source"],40) and sha_valid(row["deployment"],40) and sha_valid(row["sha256"]) and sha_valid(row["tested_from_sha256"]),"Invalid candidate digest")
    require(isinstance(row["origin"],str) and re.fullmatch(r"https://syspulse-alpha"+str(n)+r"-[a-z0-9-]{1,60}\.onrender\.com/",row["origin"]),"Unexpected artifact origin")
    return row
def approved(catalog,gate,current,now):
    # Gate is supplied by a separate trusted validator via a protected local
    # handoff, not downloaded from the public repository or artifact server.
    require(isinstance(catalog,dict) and set(catalog)=={"schema","candidates"} and type(catalog["schema"]) is int and catalog["schema"]==1 and isinstance(catalog["candidates"],list) and len(catalog["candidates"])<=500,"Invalid catalogue")
    require(isinstance(gate,dict) and set(gate)=={"schema","candidate","verified_at","expires_at","native_regression","signed_upgrade","tamper_rejection","runtime","package_verified","physical_acceptance"} and type(gate["schema"]) is int and gate["schema"]==1,"Invalid trusted approval")
    for k in ("native_regression","signed_upgrade","tamper_rejection","runtime","package_verified"):
        require(gate[k] is True,"Trusted validation gate failed: "+k)
    require(type(gate["physical_acceptance"]) is bool,"Invalid physical acceptance state")
    row=validate(gate["candidate"])
    start=dt.datetime.fromisoformat(gate["verified_at"]);end=dt.datetime.fromisoformat(gate["expires_at"])
    require(start.tzinfo is not None and end.tzinfo is not None and start<=now<end and end-start<=dt.timedelta(hours=24),"Expired or invalid approval")
    matches=[r for r in catalog["candidates"] if isinstance(r,dict) and r.get("alpha")==row["alpha"]]
    require(len(matches)==1 and validate(matches[0])==row,"Catalogue disagrees with trusted approval")
    require(row["tested_from"]==number(current["Version"]) and row["tested_from_sha256"]==current["SHA256"],"Candidate not tested against exact current signed package")
    return row
def private_dir(p):
    p=P(p)
    require(p.is_absolute() and p.resolve(strict=True)==p,"Private directory must be canonical")
    s=p.stat();require(stat.S_ISDIR(s.st_mode) and s.st_uid==os.getuid() and not s.st_mode&0o077,"Private directory must be owned by signer and mode 0700")
    return p
def read_protected(path,validator_uid,limit=65536):
    # Trust boundary: signer cannot edit the validator's handoff or its ancestors.
    p=P(path);require(p.is_absolute() and p.resolve(strict=True)==p,"Approval path must be canonical")
    require(validator_uid!=os.getuid(),"Validator and signer must use different accounts")
    for parent in p.parents:
        s=parent.stat();require(s.st_uid in (0,validator_uid) and not s.st_mode&0o022,"Approval ancestor is not protected")
    fd=os.open(p,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    with os.fdopen(fd,"rb") as f:
        s=os.fstat(f.fileno());require(stat.S_ISREG(s.st_mode) and s.st_uid==validator_uid and s.st_nlink==1 and not s.st_mode&0o022 and s.st_size<=limit,"Untrusted approval file")
        data=f.read(limit+1);require(len(data)<=limit,"Approval exceeds limit");return data
def secret_fd(path):
    p=P(path);require(p.is_absolute() and p.resolve(strict=True)==p,"Secret path must be canonical")
    fd=os.open(p,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    s=os.fstat(fd)
    if not(stat.S_ISREG(s.st_mode) and s.st_uid==os.getuid() and s.st_nlink==1 and not s.st_mode&0o077 and 0<s.st_size<=4096):
        os.close(fd);raise ValueError("Unsafe credential file")
    return fd
def env(home):
    return {"PATH":"/usr/bin:/bin","HOME":str(home),"LANG":"C.UTF-8","LC_ALL":"C.UTF-8","GIT_CONFIG_NOSYSTEM":"1","GIT_CONFIG_GLOBAL":"/dev/null","GIT_TERMINAL_PROMPT":"0","GIT_ALLOW_PROTOCOL":"https","GIT_NO_REPLACE_OBJECTS":"1"}
def run(home,*args,cwd=None,timeout=180,pass_fds=(),extra=None):
    e=env(home);e.update(extra or {})
    p=subprocess.run([str(a) for a in args],cwd=cwd,env=e,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=timeout,pass_fds=pass_fds)
    # Never dump subprocess output that might contain credentials.
    require(p.returncode==0,P(args[0]).name+" failed with exit "+str(p.returncode))
    return p.stdout
def git(home,*args,cwd=None,extra=None):
    return run(home,"git","-c","core.hooksPath=/dev/null","-c","credential.helper=","-c","http.sslVerify=true","-c","http.followRedirects=false","-c","protocol.file.allow=never",*args,cwd=cwd,extra=extra)
def clone(home,branch,path):
    git(home,"clone","--quiet","--depth","1","--single-branch","--branch",branch,REPO,path)
    require(git(home,"remote","get-url","origin",cwd=path).decode().strip()==REPO,"Unexpected clone remote")
    return git(home,"rev-parse","HEAD",cwd=path).decode().strip()
def plain(p,max_bytes):
    s=p.lstat();require(stat.S_ISREG(s.st_mode) and s.st_size<=max_bytes,"Unsafe or oversized file: "+p.name);return p.read_bytes()
def fields(data):
    d={}
    for line in data.decode("utf-8").splitlines():
        if not line or line[0].isspace():continue
        k,sep,v=line.partition(":")
        require(sep and k not in d,"Malformed metadata");d[k]=v.strip()
    return d
def package_identity(home,p,v):
    for k,value in (("Package","syspulse"),("Version",v),("Architecture","amd64")):
        require(run(home,"dpkg-deb","-f",p,k).decode().strip()==value,"Package "+k+" mismatch")
def verify_feed(home,root,key,now,allow_expired=False):
    release=plain(root/"Release",8192);plain(root/"InRelease",16384);plain(root/"Release.gpg",8192)
    clear=home/"verified-release";clear.unlink(missing_ok=True)
    run(home,"gpgv","--keyring",key,"--output",clear,root/"InRelease")
    require(clear.read_bytes()==release,"Clear signature content mismatch")
    run(home,"gpgv","--keyring",key,root/"Release.gpg",root/"Release")
    m=fields(release);require(m.get("Origin")=="SysPulse Community" and m.get("Label")=="SysPulse" and m.get("Architectures")=="amd64","Feed identity mismatch")
    start=email.utils.parsedate_to_datetime(m["Date"]);end=email.utils.parsedate_to_datetime(m["Valid-Until"])
    require(start<=now+dt.timedelta(minutes=5) and dt.timedelta(0)<end-start<=dt.timedelta(days=30),"Invalid feed dates")
    require(allow_expired or now<end,"Current feed expired; renew metadata first")
    index=plain(root/"Packages",65536)
    match=re.search(rb"^SHA256:\n ([0-9a-f]{64}) ([0-9]+) Packages\n?\Z",release,re.M)
    require(match and match[1].decode()==digest(index) and int(match[2])==len(index),"Signed index hash mismatch")
    require(len(index.strip().split(b"\n\n"))==1,"Expected one package")
    e=fields(index);n=number(e["Version"])
    require(e.get("Package")=="syspulse" and e.get("Architecture")=="amd64" and e.get("Filename")==f"pool/syspulse-3.0.0-alpha.{n}_amd64.deb","Unexpected package path")
    require((root/"pool").is_dir() and not (root/"pool").is_symlink(),"Unsafe pool")
    p=root/e["Filename"];data=plain(p,MAX_DEB)
    require(len(data)==int(e["Size"]) and digest(data)==e["SHA256"],"Signed package hash mismatch")
    package_identity(home,p,e["Version"]);return e,end
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*a,**kw):return None
def download(url,limit):
    with urllib.request.build_opener(NoRedirect()).open(urllib.request.Request(url,headers={"User-Agent":"SysPulse-source-signer/1"}),timeout=45) as r:
        require(r.geturl()==url,"Redirect refused");b=r.read(limit+1)
        require(len(b)<=limit,"Download too large");return b
def fetch_candidate(home,row):
    n=row["alpha"];base=row["origin"];name=f"syspulse-3.0.0-alpha.{n}-x86_64-ubuntu-22.04-render-eval.deb"
    build=download(base+"BUILD-STATUS.txt",16384).decode()
    install=download(base+"INSTALL-STATUS.txt",32768).decode()
    for needle in (f"Source baseline: {row['source']}",f"Application version: 3.0.0-alpha.{n}",f"Debian package version: {version(n)}","package verification: passed"):
        require(needle in build,"Build report mismatch")
    for needle in (f"prior alpha.{row['tested_from']} package verified by SHA-256",f"upgrade to alpha.{n}","tampered package rejection","preference checksum preservation","readback passed"):
        require(needle in install,"Installation report mismatch")
    data=download(base+name,MAX_DEB);require(digest(data)==row["sha256"],"Candidate digest mismatch")
    p=home/"candidate.deb";p.write_bytes(data);package_identity(home,p,version(n))
    # Read trust anchor without executing or extracting arbitrary package paths.
    import tarfile,io
    tar=run(home,"dpkg-deb","--fsys-tarfile",p,timeout=60)
    require(len(tar)<=64*1024*1024,"Oversized package payload")
    with tarfile.open(fileobj=io.BytesIO(tar),mode="r:") as archive:
        matching=[m for m in archive if m.name.lstrip("./")=="usr/share/syspulse/update-keys/alpha.gpg"]
        require(len(matching)==1 and matching[0].isreg() and matching[0].size<=65536,"Missing package trust anchor")
        require(digest(archive.extractfile(matching[0]).read())==PIN,"Candidate owner trust anchor mismatch")
    return p
def write_release(root,now):
    p=root/"Packages"
    fmt=lambda x:email.utils.format_datetime(x,usegmt=True)
    text="Origin: SysPulse Community\nLabel: SysPulse\nArchitectures: amd64\nDate: "+fmt(now)+"\nValid-Until: "+fmt(now+dt.timedelta(days=7))+"\nSHA256:\n "+hashfile(p)+" "+str(p.stat().st_size)+" Packages\n"
    (root/"Release").write_text(text)
def sign(home,root,gpg_home,passphrase):
    gpg_home=private_dir(gpg_home)
    args=["gpg","--no-options","--homedir",gpg_home,"--batch","--no-tty","--local-user",KEY+"!","--digest-algo","SHA256"]
    for name,mode in (("InRelease","--clearsign"),("Release.gpg","--detach-sign")):
        (root/name).unlink()
        if passphrase:
            fd=secret_fd(passphrase)
            try:run(home,*args,"--pinentry-mode","loopback","--passphrase-fd",str(fd),"--output",root/name,mode,root/"Release",pass_fds=(fd,),timeout=90)
            finally:os.close(fd)
        else:run(home,*args,"--pinentry-mode","error","--output",root/name,mode,root/"Release",timeout=90)
def syncdir(p):
    fd=os.open(p,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)
def journal(state,obj):
    p=state/"pending.json";fd,tmp=tempfile.mkstemp(dir=state,prefix=".pending-")
    try:
        with os.fdopen(fd,"w") as f:json.dump(obj,f);f.flush();os.fsync(f.fileno())
        os.link(tmp,p)
    finally:os.unlink(tmp)
    syncdir(state)
def reconcile(state,actual,current):
    p=state/"pending.json"
    if not p.exists():return
    obj=json_strict(plain(p,4096))
    require(isinstance(obj,dict) and set(obj)=={"parent","commit","version","digest"} and sha_valid(obj["commit"],40) and sha_valid(obj["parent"],40) and sha_valid(obj["digest"]),"Invalid pending journal")
    require(actual==obj["commit"] and current["Version"]==obj["version"] and current["SHA256"]==obj["digest"],"Unresolved previous push: operator must reconcile; no retry")
    p.unlink();syncdir(state)
def checkpoint(state,current):
    # Never refresh metadata for a rolled-back but historically valid signature.
    p=state/"high-water.json";n=number(current["Version"]);d=current["SHA256"]
    require(sha_valid(d),"Invalid current digest")
    if p.exists():
        old=json_strict(plain(p,4096))
        require(isinstance(old,dict) and set(old)=={"alpha","sha256"} and type(old["alpha"]) is int and sha_valid(old["sha256"]),"Invalid high-water checkpoint")
    else:old={"alpha":MIN_ALPHA,"sha256":MIN_DIGEST}
    require(n>old["alpha"] or (n==old["alpha"] and d==old["sha256"]),"Signed feed rollback or same-version replacement detected")
    if n==old["alpha"] and p.exists():return
    fd,tmp=tempfile.mkstemp(dir=state,prefix=".checkpoint-")
    try:
        with os.fdopen(fd,"w") as f:json.dump({"alpha":n,"sha256":d},f);f.flush();os.fsync(f.fileno())
        os.replace(tmp,p);syncdir(state)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)
@contextmanager
def locked(state):
    fd=os.open(state/"sign.lock",os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK,0o600)
    try:
        s=os.fstat(fd);require(stat.S_ISREG(s.st_mode) and s.st_uid==os.getuid() and s.st_nlink==1 and not s.st_mode&0o077,"Unsafe signing lock")
        fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB);yield
    finally:os.close(fd)
def transaction(a):
    require(os.geteuid()!=0,"Run as dedicated unprivileged signer")
    state=private_dir(a.state)
    with locked(state),tempfile.TemporaryDirectory(dir=state,prefix="job-") as temp:
        home=P(temp);now=dt.datetime.now(UTC)
        key=home/"owner.gpg";anchor=base64.b64decode(ANCHOR.read_text().strip(),validate=True)
        require(digest(anchor)==PIN,"Pinned owner key mismatch");key.write_bytes(anchor)
        root=home/"feed";parent=clone(home,BRANCH,root)
        current,end=verify_feed(home,root,key,now,allow_expired=True);checkpoint(state,current);reconcile(state,parent,current)
        print("Verified signed feed",current["Version"],parent,flush=True)
        if a.mode=="verify":return
        if a.mode=="renew":
            if end-now>dt.timedelta(days=3):print("Renewal not due");return
        else:
            require(now<end,"Renew expired signed feed before publishing")
            require(a.approval and a.validator_uid is not None,"Protected independent validator approval required")
            catroot=home/"catalog";catalog_commit=clone(home,CATALOG,catroot)
            catalog=json_strict(plain(catroot/"release-candidates.json",262144))
            gate=json_strict(read_protected(a.approval,a.validator_uid))
            if isinstance(gate,dict) and isinstance(gate.get("candidate"),dict):
                proposed=validate(gate["candidate"])
                if version(proposed["alpha"])==current["Version"] and proposed["sha256"]==current["SHA256"]:
                    print("Trusted candidate already published; no action");return
            row=approved(catalog,gate,current,now)
            print("Eligible approved candidate",version(row["alpha"]),"catalogue",catalog_commit,flush=True)
            pkg=fetch_candidate(home,row)
            filename=f"pool/syspulse-3.0.0-alpha.{row['alpha']}_amd64.deb"
            require(not (root/filename).exists(),"Version path already exists")
            shutil.copyfile(pkg,root/filename)
            # dpkg-scanpackages accepts an explicit package file, not arbitrary scripts.
            index=run(home,"dpkg-scanpackages",filename,"/dev/null",cwd=root)
            (root/"Packages").write_bytes(index)
        if not a.apply:print("Dry run: no signing or push");return
        require(a.gpg_home and a.git_token,"Signing home and protected Git credential required")
        # Validate credential file before any signing. No secret in argv, git URL,
        # logs, source checkout, or build environment.
        fd=secret_fd(a.git_token);os.close(fd)
        write_release(root,now);sign(home,root,a.gpg_home,a.passphrase)
        entry,_=verify_feed(home,root,key,now)
        if a.mode=="renew":require(entry==current,"Renewal changed package index")
        else:require(entry["Version"]==version(row["alpha"]) and entry["SHA256"]==row["sha256"],"Signed candidate changed")
        for file in ("Release","InRelease","Release.gpg","Packages",entry["Filename"]):git(home,"add","--",file,cwd=root)
        git(home,"-c","user.name=SysPulse Source Signer","-c","user.email=release@syspulse.invalid","commit","-m","Publish verified "+entry["Version"]+" [skip ci]",cwd=root)
        commit=git(home,"rev-parse","HEAD",cwd=root).decode().strip()
        require(git(home,"rev-parse","HEAD^",cwd=root).decode().strip()==parent,"Unexpected commit parent")
        for file in ("Release","InRelease","Release.gpg","Packages",entry["Filename"]):
            require(git(home,"show",commit+":"+file,cwd=root)==(root/file).read_bytes(),"Committed file mismatch")
        remote=git(home,"ls-remote","origin","refs/heads/"+BRANCH,cwd=root).decode().split()
        require(remote==[parent,"refs/heads/"+BRANCH],"Feed changed; retry validation")
        ask=home/"askpass";ask.write_text("#!/usr/bin/python3\nimport os,sys\nif 'https://github.com' in sys.argv[1] and 'Username' in sys.argv[1]: print('x-access-token')\nelif 'https://x-access-token@github.com' in sys.argv[1] and 'Password' in sys.argv[1]: print(open(os.environ['SYSPULSE_TOKEN_FILE']).read().strip())\nelse: sys.exit(1)\n");ask.chmod(0o700)
        journal(state,{"parent":parent,"commit":commit,"version":entry["Version"],"digest":entry["SHA256"]})
        git(home,"push","origin","HEAD:refs/heads/"+BRANCH,cwd=root,extra={"GIT_ASKPASS":str(ask),"SYSPULSE_TOKEN_FILE":str(a.git_token)})
        check=home/"remote";actual=clone(home,BRANCH,check);verified,_=verify_feed(home,check,key,dt.datetime.now(UTC))
        require(actual==commit and verified==entry,"Remote signed readback mismatch")
        checkpoint(state,verified);reconcile(state,actual,verified);print("Published and verified",entry["Version"],commit)
class Tests(unittest.TestCase):
    def setUp(self):
        self.now=dt.datetime.now(UTC);self.row={"alpha":34,"source":"a"*40,"deployment":"b"*40,"origin":"https://syspulse-alpha34-eval.onrender.com/","sha256":"c"*64,"tested_from":32,"tested_from_sha256":"d"*64,"suites":54}
        self.cat={"schema":1,"candidates":[self.row.copy()]};self.current={"Version":version(32),"SHA256":"d"*64}
        self.gate={"schema":1,"candidate":self.row.copy(),"verified_at":(self.now-dt.timedelta(minutes=1)).isoformat(),"expires_at":(self.now+dt.timedelta(hours=1)).isoformat(),"native_regression":True,"signed_upgrade":True,"tamper_rejection":True,"runtime":True,"package_verified":True,"physical_acceptance":False}
    def go(self):return approved(self.cat,self.gate,self.current,self.now)
    def test_valid(self):self.assertEqual(self.go(),self.row)
    def test_wrong_prior(self):
        self.current["Version"]=version(33);self.assertRaises(ValueError,self.go)
    def test_wrong_digest(self):
        self.current["SHA256"]="e"*64;self.assertRaises(ValueError,self.go)
    def test_expired(self):
        self.gate["expires_at"]=self.now.isoformat();self.assertRaises(ValueError,self.go)
    def test_future(self):
        self.gate["verified_at"]=(self.now+dt.timedelta(minutes=1)).isoformat();self.assertRaises(ValueError,self.go)
    def test_long_approval(self):
        self.gate["expires_at"]=(self.now+dt.timedelta(days=2)).isoformat();self.assertRaises(ValueError,self.go)
    def test_no_approval(self):
        self.gate={};self.assertRaises(ValueError,self.go)
    def test_failed_gate(self):
        for k in ("native_regression","signed_upgrade","tamper_rejection","runtime","package_verified"):
            self.gate[k]=False;self.assertRaises(ValueError,self.go);self.gate[k]=True
    def test_catalogue_tamper(self):
        self.cat["candidates"][0]["sha256"]="e"*64;self.assertRaises(ValueError,self.go)
    def test_duplicate(self):
        self.cat["candidates"].append(self.row);self.assertRaises(ValueError,self.go)
    def test_origin(self):
        for origin in ("http://syspulse-alpha34-eval.onrender.com/","https://evil.example/","https://syspulse-alpha34-eval.onrender.com@evil.example/"):
            self.gate["candidate"]["origin"]=origin;self.assertRaises(ValueError,self.go)
    def test_type(self):
        self.gate["candidate"]["suites"]=True;self.assertRaises(ValueError,self.go)
    def test_lower_version(self):
        self.gate["candidate"]["alpha"]=32;self.assertRaises(ValueError,self.go)
    def test_duplicate_json(self):self.assertRaises(ValueError,json_strict,b'{"x":1,"x":2}')
    def test_pending(self):
        with tempfile.TemporaryDirectory() as t:
            state=P(t);obj={"parent":"a"*40,"commit":"b"*40,"version":version(34),"digest":"c"*64}
            journal(state,obj);self.assertRaises(FileExistsError,journal,state,obj)
            self.assertRaises(ValueError,reconcile,state,"a"*40,self.current)
            reconcile(state,"b"*40,{"Version":version(34),"SHA256":"c"*64});self.assertFalse((state/"pending.json").exists())
    def test_same_identity_refused(self):
        self.assertRaises(ValueError,read_protected,P("/etc/passwd"),os.getuid())
    def test_secret_permissions(self):
        with tempfile.TemporaryDirectory() as t:
            p=P(t)/"secret";p.write_text("dummy");p.chmod(0o644);self.assertRaises(ValueError,secret_fd,p);p.chmod(0o600);os.close(secret_fd(p))
    def test_high_water(self):
        with tempfile.TemporaryDirectory() as t:
            p=P(t);checkpoint(p,{"Version":version(32),"SHA256":MIN_DIGEST})
            self.assertRaises(ValueError,checkpoint,p,{"Version":version(31),"SHA256":"a"*64})
            self.assertRaises(ValueError,checkpoint,p,{"Version":version(32),"SHA256":"a"*64})
            checkpoint(p,{"Version":version(33),"SHA256":"a"*64})
            self.assertRaises(ValueError,checkpoint,p,{"Version":version(32),"SHA256":MIN_DIGEST})
    def test_no_secret_env(self):
        e=env("/tmp");self.assertNotIn("GITHUB_TOKEN",e);self.assertNotIn("GNUPGHOME",e)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode",choices=("verify","publish","renew","self-test"))
    parser.add_argument("--state",type=P);parser.add_argument("--approval",type=P);parser.add_argument("--validator-uid",type=int)
    parser.add_argument("--gpg-home",type=P);parser.add_argument("--passphrase",type=P);parser.add_argument("--git-token",type=P)
    parser.add_argument("--apply",action="store_true",help="Explicitly enable signing and publication; default is dry run")
    a=parser.parse_args()
    if a.mode=="self-test":return 0 if unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tests)).wasSuccessful() else 1
    os.umask(0o077)
    try:require(a.state is not None,"Private persistent --state required");transaction(a);return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:
        # Paths and error details only; no secret contents or subprocess stderr.
        print("STOP:",type(e).__name__,str(e));return 1
if __name__=="__main__":raise SystemExit(main())
