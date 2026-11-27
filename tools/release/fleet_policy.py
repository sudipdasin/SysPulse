#!/usr/bin/python3
"""Strict per-project policy and independent v2 release attestation."""
import datetime as dt, hashlib, json, os, re, stat, subprocess
from pathlib import Path
from urllib.parse import urlsplit
def require(ok,msg):
    if not ok: raise ValueError(msg)
def hexstr(v,n=64):return isinstance(v,str) and re.fullmatch("[0-9a-f]{"+str(n)+"}",v) is not None
def exact(obj,keys,msg):
    require(isinstance(obj,dict) and set(obj)==set(keys.split()),msg)
def safe_version(v):
    require(isinstance(v,str) and len(v)<=96 and re.fullmatch(r"[0-9][A-Za-z0-9.+~\-]*",v),"Unsupported Debian version (epochs deliberately excluded)")
    return v
def newer(a,b):
    safe_version(a);safe_version(b)
    p=subprocess.run(["/usr/bin/dpkg","--compare-versions",a,"gt",b],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
    require(p.returncode in (0,1),"Debian version comparison failed")
    return p.returncode==0
def trusted_file(path,limit=65536):
    p=Path(path);require(p.is_absolute() and p.resolve(strict=True)==p,"Noncanonical policy path")
    for parent in p.parents:
        s=parent.stat();require(s.st_uid==0 and not s.st_mode&0o022,"Policy ancestor not root-protected")
    fd=os.open(p,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    with os.fdopen(fd,"rb") as f:
        s=os.fstat(f.fileno());require(stat.S_ISREG(s.st_mode) and s.st_uid==0 and s.st_nlink==1 and not s.st_mode&0o022 and s.st_size<=limit,"Untrusted policy file")
        data=f.read(limit+1);require(len(data)<=limit,"Policy file too large");return data
def strict(data):
    def unique(pairs):
        out={}
        for k,v in pairs:
            require(k not in out,"Duplicate JSON field");out[k]=v
        return out
    return json.loads(data,object_pairs_hook=unique,parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Invalid JSON constant")))
def policy(p):
    exact(p,"schema project channel repository feed_branch catalogue_branch package architecture origin label fingerprint anchor_path anchor_sha256 embedded_anchor minimum_version minimum_sha256 artifact_hosts minimum_suites require_physical","Invalid project policy")
    require(type(p["schema"]) is int and p["schema"]==2,"Unsupported policy schema")
    for k in ("project","package","channel"):
        require(isinstance(p[k],str) and re.fullmatch(r"[a-z0-9][a-z0-9+.-]{1,63}",p[k]),"Invalid project namespace")
    require(p["architecture"] in ("amd64","arm64"),"Unsupported architecture")
    require(isinstance(p["repository"],str) and re.fullmatch(r"https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\.git",p["repository"]),"Invalid repository")
    for k in ("feed_branch","catalogue_branch"):
        require(isinstance(p[k],str) and re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9_-]{1,63}",p[k]),"Unsafe branch")
    require(p["feed_branch"]!=p["catalogue_branch"] and p["feed_branch"] not in ("main","master"),"Feed branch must be separate")
    for k in ("origin","label"):
        require(isinstance(p[k],str) and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9 ._-]{0,79}",p[k]),"Invalid feed identity")
    require(isinstance(p["fingerprint"],str) and re.fullmatch(r"[A-F0-9]{40}",p["fingerprint"]),"Invalid owner fingerprint")
    require(hexstr(p["anchor_sha256"]) and hexstr(p["minimum_sha256"]),"Invalid pin")
    require(isinstance(p["anchor_path"],str) and p["anchor_path"].startswith("/etc/deb-release/"),"Anchor outside protected config directory")
    require(isinstance(p["embedded_anchor"],str) and re.fullmatch(r"usr/share/[a-z0-9+.-]+/update-keys/[a-z0-9.-]+\.gpg",p["embedded_anchor"]),"Unsafe embedded anchor path")
    safe_version(p["minimum_version"])
    require(isinstance(p["artifact_hosts"],list) and 0<len(p["artifact_hosts"])<=16 and all(isinstance(h,str) and re.fullmatch(r"[a-z0-9][a-z0-9.-]{2,127}",h) and "." in h and not h.endswith(".") for h in p["artifact_hosts"]) and len(set(p["artifact_hosts"]))==len(p["artifact_hosts"]),"Invalid host allowlist")
    require(type(p["minimum_suites"]) is int and 1<=p["minimum_suites"]<=100000 and type(p["require_physical"]) is bool,"Invalid test policy")
    return p
def filename(p,v):return "pool/"+p["package"]+"_"+safe_version(v)+"_"+p["architecture"]+".deb"
def candidate(p,c):
    exact(c,"project channel package architecture version source deployment artifact_url sha256 size prior_version prior_sha256 suites build_report_sha256 install_report_sha256","Invalid candidate")
    for k in ("project","channel","package","architecture"):require(c[k]==p[k],"Cross-project/channel/architecture candidate")
    for k in ("source","deployment"):require(hexstr(c[k],40),"Invalid source identity")
    for k in ("sha256","prior_sha256","build_report_sha256","install_report_sha256"):require(hexstr(c[k]),"Invalid artifact/report digest")
    require(newer(c["version"],c["prior_version"]),"Candidate downgrade or same version")
    require(type(c["size"]) is int and 1<=c["size"]<=64*1024*1024,"Artifact size out of range")
    require(type(c["suites"]) is int and p["minimum_suites"]<=c["suites"]<=100000,"Regression count too low")
    u=urlsplit(c["artifact_url"])
    require(u.scheme=="https" and u.netloc in p["artifact_hosts"] and not u.query and not u.fragment and re.fullmatch(r"/[A-Za-z0-9._~+/-]+\.deb",u.path) and all(x not in ("",".","..") for x in u.path[1:].split("/")),"Unapproved artifact URL")
    return c
def approval(p,cat,gate,current,now):
    exact(cat,"schema candidates","Invalid catalogue");require(type(cat["schema"]) is int and cat["schema"]==2 and isinstance(cat["candidates"],list) and len(cat["candidates"])<=500,"Invalid catalogue schema")
    exact(gate,"schema candidate verified_at expires_at native_regression signed_upgrade tamper_rejection runtime package_verified physical_acceptance","Invalid independent attestation")
    require(type(gate["schema"]) is int and gate["schema"]==2,"Invalid approval schema")
    for k in ("native_regression","signed_upgrade","tamper_rejection","runtime","package_verified"):require(gate[k] is True,"Failed release gate: "+k)
    require(type(gate["physical_acceptance"]) is bool and (gate["physical_acceptance"] or not p["require_physical"]),"Physical acceptance required")
    c=candidate(p,gate["candidate"])
    a=dt.datetime.fromisoformat(gate["verified_at"]);b=dt.datetime.fromisoformat(gate["expires_at"])
    require(a.tzinfo is not None and b.tzinfo is not None and a<=now<b and b-a<=dt.timedelta(hours=24),"Expired or invalid approval")
    matches=[x for x in cat["candidates"] if isinstance(x,dict) and x.get("version")==c["version"] and x.get("project")==p["project"] and x.get("channel")==p["channel"] and x.get("architecture")==p["architecture"]]
    require(len(matches)==1 and candidate(p,matches[0])==c,"Catalogue mismatch or duplicate")
    require(c["prior_version"]==current["Version"] and c["prior_sha256"]==current["SHA256"],"Wrong exact tested prior package")
    return c
def namespace(p):
    return p["project"]+"/"+p["channel"]+"/"+p["architecture"]
def policy_digest(p):
    return hashlib.sha256(json.dumps(p,sort_keys=True,separators=(",",":")).encode()).hexdigest()
