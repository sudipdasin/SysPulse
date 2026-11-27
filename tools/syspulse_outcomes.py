# SPDX-License-Identifier: GPL-3.0-or-later
"""Post-maintenance evidence comparison; no commands, privileges or causal claims."""
import math
CRITICAL=frozenset(("storage_io","filesystem","disk_health","oom","thermal","kernel_fault"))
STORAGE_ACTIONS=frozenset(("thumbnails","failed_thumbnails","archives"))
def number(x):
    return type(x) in (int,float) and math.isfinite(x)
def verification(before,after,sources,outcomes,prior_sources=()):
    deltas={}
    for key in ("memory_available_kib","swap_free_kib","load1","cpu_psi_some","memory_psi_some","io_psi_some"):
        a,b=before.get(key),after.get(key)
        if number(a) and number(b):deltas[key]={"before":a,"after":b,"change":round(b-a,4)}
    changed=[]
    for area in ("root","home","apt"):
        dev,total=area+"_device",area+"_total_bytes"
        free=area+"_free_bytes"
        if (not number(before.get(dev)) or not number(after.get(dev)) or
            before[dev]!=after[dev] or not number(before.get(total)) or
            before[total]!=after.get(total) or before[total]<=0):
            changed.append(area);continue
        a,b=before.get(free),after.get(free)
        if number(a) and number(b):deltas[free]={"before":a,"after":b,"change":b-a}
    counts={}
    for s in sources:
        for key,n in s.get("events",{}).items():
            if type(n) is int and n>0:counts[key]=counts.get(key,0)+n
    kernel=any(s["name"] in ("kernel-ring","kernel-journal-current-boot") and s["status"] in ("ok","empty") and not s.get("bounded") for s in sources)
    package=any(s["name"]=="dpkg-audit" and s["status"] in ("ok","empty") and not s.get("bounded") for s in sources)
    smart=next((s.get("smart",{}) for s in sources if s["name"]=="smart-disk-health"),{})
    prior_smart=next((s.get("smart",{}) for s in prior_sources if s["name"]=="smart-disk-health"),{})
    storage=bool(STORAGE_ACTIONS.intersection(outcomes))
    risks=sorted(k for k in counts if k in CRITICAL or k=="package_failure")
    if any(after.get(a+"_readonly") is True for a in ("root","home","apt")):risks.append("readonly_filesystem")
    if number(after.get("max_temperature_c")) and after["max_temperature_c"]>=90:risks.append("high_temperature")
    if any(v["status"]=="failure" for v in outcomes.values()):risks.append("action_failure")
    limited=not kernel or not package or (storage and (bool(changed) or smart.get("status")!="ok")) or (prior_smart.get("status")=="ok" and smart.get("status")!="ok")
    state="review" if risks else "limited" if limited else "observed"
    return {"schema":1,"status":state,"risks":risks,"limited":limited,"coverage":{"kernel":kernel,"package":package,"smart":smart.get("status","not_verified")},
            "deltas":deltas,"uncomparable_storage":changed,"observations":counts,
            "sources":[{"name":s["name"],"status":s["status"],"bounded":bool(s.get("bounded"))} for s in sources],
            "interpretation":"Measurements are before/after observations, not proof of causal speed gain. Existing or repeated log findings may overlap. No repair or retry was authorized."}
