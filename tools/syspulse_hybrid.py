# SPDX-License-Identifier: GPL-3.0-or-later
"""Privacy-preserving log signatures, disk health and reviewed knowledge.
Independent implementation inspired by streaming template mining, not Drain3.
No downloaded code, no privileged operations or external commands from data.
"""
import collections,json,os,re,stat,pathlib,xml.etree.ElementTree as ET
P=pathlib.Path
# Only literal fixed vocabulary survives. All other words and values are masked,
# including hostnames, paths, account names, identifiers and arbitrary log text.
WORDS=frozenset("error errors failed failure critical warning panic kernel memory pressure swap oom kill killed process gpu hang reset thermal temperature throttling disk storage filesystem corrupt corruption readonly read only remount sector sectors buffer i o timeout timed out nvme ata ext4 ext3 ext2 btrfs xfs smart pending uncorrectable reallocated service unit start dependency authentication password invalid network dns dhcp link down connection unreachable dpkg apt package packages broken unmet dependencies segmentation fault segfault coredump crash watchdog lockup hardware machine check trace call permission denied allocation full exhausted no space left device block driver journal systemd mount input output".split())
TOKEN=re.compile(r"[A-Za-z]+|[0-9]+|[^\sA-Za-z0-9]+")
MAX_PATTERNS=6
def signature(msg):
    # Collapse runs of masked tokens, keep bounded sequence of recognized words.
    words=[]
    for token in TOKEN.findall(msg[:4096])[:96]:
        word=token.lower() if token.lower() in WORDS else "*"
        if word=="*" and words and words[-1]=="*":continue
        words.append(word)
        if len(words)>=12:break
    if not any(w!="*" for w in words):return "unclassified error"
    return " ".join(words)[:120]
class Miner:
    def __init__(self):self.counts={};self.overflow=0
    def add(self,category,msg):
        key=(category,signature(msg))
        if key in self.counts:self.counts[key]+=1
        elif len(self.counts)<128:self.counts[key]=1
        else:self.overflow+=1
    def result(self):
        return [{"category":k[0],"template":k[1],"count":v} for k,v in sorted(self.counts.items(),key=lambda kv:(-kv[1],kv[0]))[:MAX_PATTERNS]]
def patterns(sources,previous):
    counts=collections.Counter()
    for s in sources:
        for p in s.get("patterns",[]):counts[(p["category"],p["template"])]+=p["count"]
    prior=collections.Counter()
    for a in previous:
        for p in a.get("patterns",[]):prior[(p["category"],p["template"])]+=1
    return [{"category":k[0],"template":k[1],"observations":v,"previous_analyses":prior[k],"new_to_recent_history":prior[k]==0} for k,v in sorted(counts.items(),key=lambda kv:(-kv[1],kv[0]))[:24]]
# Fixed reviewed advice, not a repair command. Sources are documentation links,
# never automatically fetched or run. Native tool IDs are read-only.
RECIPES={
 "storage_io":("critical","Protect data first. Review disk health and backups before writing to the affected filesystem.",["devices","kernel"],"https://www.smartmontools.org/"),
 "filesystem":("critical","Review mount state and backups; filesystem repair belongs in a separately authorized offline workflow.",["filesystem-config","devices"],"https://manpages.ubuntu.com/manpages/noble/man8/fsck.8.html"),
 "disk_health":("critical","A disk health warning warrants backup and hardware investigation, not cache cleanup.",["devices","kernel"],"https://www.smartmontools.org/"),
 "oom":("warning","Inspect memory and swap pressure; identify resource-heavy applications before changing settings.",["memory","kernel"],"https://docs.kernel.org/admin-guide/mm/index.html"),
 "low_available_memory":("warning","Compare RAM, swap and pressure; reduce unnecessary activity first.",["memory"],"https://docs.kernel.org/accounting/psi.html"),
 "package_failure":("warning","Inspect incomplete packages and trusted package origins. Review a separate package repair plan.",["package-audit","package-origins"],"https://manpages.ubuntu.com/manpages/noble/man1/dpkg.1.html"),
 "service_failure":("warning","Inspect failed units and dependencies before deciding on any restart.",["failed","systemd-state"],"https://www.freedesktop.org/software/systemd/man/latest/systemctl.html"),
 "gpu":("warning","Inspect kernel graphics errors before changing drivers.",["kernel"],"https://docs.kernel.org/gpu/index.html"),
 "network":("warning","Inspect links, routes and manager state without modifying network configuration.",["network-links","network-routes","network-manager-state"],"https://networkmanager.dev/docs/"),
 "thermal":("critical","Review temperatures and cooling; do not increase performance limits.",["kernel"],"https://docs.kernel.org/driver-api/thermal/index.html"),
 "kernel_fault":("critical","Review hardware and kernel warnings; preserve diagnostic evidence.",["kernel","journal"],"https://docs.kernel.org/admin-guide/index.html"),
 "low_disk_space":("warning","Review available space and generated-cache candidates; preserve personal files.",["storage","journal-size"],"https://docs.bleachbit.org/doc/command-line-interface/index.html"),
 "low_inodes":("warning","Inspect filesystem inode consumption before selecting cleanup.",["storage","filesystem-config"],"https://manpages.ubuntu.com/manpages/noble/man1/df.1.html"),
 "authentication":("warning","Review authentication logs; do not change security policy automatically.",["journal"],"https://www.freedesktop.org/software/systemd/man/latest/journalctl.html"),
 "application_crash":("warning","Review crash history and identify affected software before repairing.",["journal"],"https://www.freedesktop.org/software/systemd/man/latest/coredumpctl.html")
}
def recipes(findings):
    out=[]
    for f in findings:
        code=f["code"]
        if code in RECIPES:
            severity,summary,tools,url=RECIPES[code]
            out.append({"code":code,"severity":severity,"guidance":summary,"tools":tools,"reference":url,"automatic_repair":False})
    return out[:15]
from syspulse_smart_evidence import smart_result
def smart_scan(command,device_ok=None):
    # Only fixed internal SATA/NVMe whole disks; skip bridges, virtual devices,
    # removable media and unknown transports. At most four devices.
    if device_ok is None:
        def device_ok(name):
            try:return stat.S_ISBLK(P("/dev",name).lstat().st_mode)
            except OSError:return False
    status,raw,bounded=command(["lsblk","-J","-d","-o","NAME,TYPE,TRAN,ROTA,RM"],timeout=4,limit=32768)
    if status!="ok" or bounded:return {"status":"partial","disks":[],"excluded":0}
    try:items=json.loads(raw)["blockdevices"];assert isinstance(items,list)
    except (ValueError,TypeError,KeyError,AssertionError,RecursionError):return {"status":"partial","disks":[],"excluded":0}
    names=[];excluded=0
    for x in items[:128]:
        if not isinstance(x,dict) or x.get("type")!="disk":continue
        name=x.get("name");tran=x.get("tran")
        if not isinstance(name,str) or x.get("rm") not in (False,0,"0") or not ((tran=="sata" and re.fullmatch(r"sd[a-z]{1,3}",name)) or (tran=="nvme" and re.fullmatch(r"nvme[0-9]{1,4}n[0-9]{1,4}",name))) or not device_ok(name):
            excluded+=1;continue
        if name not in names:names.append(name)
    disks=[]
    for name in sorted(names)[:4]:
        args=["smartctl","-j","-H","-A"]
        if name.startswith("sd"):args+=["-n","standby,3"]
        args+=["/dev/"+name]
        st,raw,bounded=command(args,timeout=4,limit=131072)
        disks.append(smart_result(raw,st,bounded))
    excluded+=max(0,len(names)-4)+max(0,len(items)-128)
    return {"status":"ok" if disks and not excluded and all(d["status"]=="ok" and d["health"]!="unknown" for d in disks) else "partial","disks":disks,"excluded":excluded}
# Optional integration: discover only system-installed BleachBit XML metadata.
# No BleachBit process, cleaner action, path definition or custom script is run.
def cleaner_catalog(root=P("/usr/share/bleachbit/cleaners")):
    try:
        for p in (root,*root.parents):
            s=p.lstat()
            if not stat.S_ISDIR(s.st_mode) or s.st_uid!=0 or s.st_mode&0o022:return {"status":"untrusted","options":[]}
        options=[]
        for p in sorted(root.glob("*.xml"))[:128]:
            s=p.lstat()
            if not stat.S_ISREG(s.st_mode) or s.st_uid!=0 or s.st_mode&0o022 or s.st_size>131072:continue
            with p.open("rb") as f:raw=f.read(131073)
            if len(raw)>131072 or re.search(rb"<!\s*(?:DOCTYPE|ENTITY)",raw,re.I):continue
            try:r=ET.fromstring(raw)
            except ET.ParseError:continue
            if r.tag!="cleaner" or not re.fullmatch(r"[a-z0-9_]{1,50}",r.get("id","")):continue
            for opt in r.findall("option")[:64]:
                ident=opt.get("id","")
                if ident in ("cache","thumbnail_cache","thumbnails") and len(options)<40:
                    options.append(r.get("id")+"."+ident)
        return {"status":"metadata_only","options":sorted(set(options))}
    except FileNotFoundError:return {"status":"not_installed","options":[]}
    except OSError:return {"status":"unavailable","options":[]}
