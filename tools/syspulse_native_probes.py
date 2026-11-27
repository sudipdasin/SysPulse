# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed read-only APT solver and procps sampling adapters; never execute a plan."""
import re, statistics
APT_ARGS=("apt-get","-s","--no-download","--no-remove","-f","install")
VM_ARGS=("vmstat","-w","1","3")
SUMMARY=re.compile(r"^(\d+) upgraded, (\d+) newly installed, (\d+) to remove and (\d+) not upgraded\.$")
def apt_result(status,raw,bounded):
    result={"status":"unknown","install_or_upgrade":0,"configure":0,"remove":0,"held_back":0,"execution_authorized":False}
    if status not in ("ok","failed") or bounded:return result
    if len(raw)>262144 or "\x00" in raw:return result
    lines=raw.splitlines()
    if len(lines)>6000:return result
    actions={k:0 for k in ("Inst","Conf","Remv","Purg")}
    summaries=[]; malformed=False; warning=False; removal_disabled=False
    for raw_line in lines:
        line=raw_line.strip()
        m=SUMMARY.fullmatch(line)
        if m:
            if any(len(x)>6 for x in m.groups()):return result
            summaries.append(tuple(map(int,m.groups())))
        token=line.split(maxsplit=1)
        if token and token[0] in actions:
            actions[token[0]]+=1
            if len(token)!=2:malformed=True
        if line.startswith(("E:","W:","N:")):warning=True
        if "remove is disabled" in line.lower():removal_disabled=True
    result.update(install_or_upgrade=actions["Inst"],configure=actions["Conf"],remove=actions["Remv"]+actions["Purg"])
    result["remove"]=max([result["remove"]]+[x[2] for x in summaries])
    if removal_disabled or result["remove"]:result["status"]="removal_blocked";return result
    if status!="ok":result["status"]="solver_failed";return result
    if len(summaries)!=1 or warning or malformed:return result
    upgraded,new,_,held=summaries[0]
    if max(upgraded,new,held)>100000 or upgraded+new!=actions["Inst"]:return result
    result["held_back"]=held
    result["status"]="review" if upgraded+new or actions["Conf"] else "no_changes"
    return result

def vm_result(status,raw,bounded):
    result={"status":"unknown","samples":0}
    if status!="ok" or bounded or len(raw)>16384 or "\x00" in raw:return result
    fields=("r","b","si","so","us","sy","id","wa","st")
    header=None;rows=[]
    for line in raw.splitlines():
        cols=line.split()
        if not cols:continue
        if all(k in cols for k in fields):
            if header is not None or len(set(cols))!=len(cols):return result
            header=cols;continue
        if header is None:continue
        if len(cols)!=len(header) or any(not re.fullmatch(r"[0-9]{1,12}",x) for x in cols):return result
        row={k:int(cols[header.index(k)]) for k in fields}
        if any(row[k]>100 for k in ("us","sy","id","wa","st")):return result
        rows.append(row)
    if len(rows)!=3:return result
    # First vmstat row is a since-boot average; never count it as an interval.
    recent=rows[1:]
    result.update(status="sampled",samples=2,interval_seconds=1)
    for k in fields:result[k+"_mean"]=statistics.mean(x[k] for x in recent)
    return result

def collect_apt(command):
    return apt_result(*command(list(APT_ARGS),timeout=8,limit=262144))
def collect_vm(command):
    return vm_result(*command(list(VM_ARGS),timeout=6,limit=16384))
