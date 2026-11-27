#!/usr/bin/python3
"""Deterministic synthetic fault benchmark; not real-world accuracy evidence."""
import argparse,collections,importlib.machinery,importlib.util,json,pathlib,random,resource,sys,time
tools=pathlib.Path(__file__).resolve().parents[1]/"tools";sys.path.insert(0,str(tools))
loader=importlib.machinery.SourceFileLoader("diag",str(tools/"syspulse-diagnostics"))
spec=importlib.util.spec_from_loader(loader.name,loader);d=importlib.util.module_from_spec(spec);loader.exec_module(d)
import syspulse_hybrid as h
# Independently specified expected categories. Deliberate negative/control
# messages include recovery, historical counters and misleading substrings.
FIXTURES=[
("storage_io","Buffer I/O error on device sd{letter}, logical block {n}"),
("storage_io","nvme{n}: I/O timeout, reset controller"),
("filesystem","EXT4-fs error (device sd{letter}1): corrupted inode {n}"),
("filesystem","BTRFS error: corrupt leaf at {n}"),
("disk_health","SMART overall-health self-assessment test result: FAILED"),
("disk_health","{n} pending sectors detected"),
("oom","Out of memory: Killed process {n} (privateuser)"),
("oom","oom-kill:constraint=CONSTRAINT_NONE,pid={n}"),
("gpu","i915 GPU HANG: ecode {n}"),
("gpu","amdgpu: GPU reset begin"),
("thermal","CPU{n}: temperature above threshold, cpu clock throttled"),
("thermal","critical temperature reached ({n} C)"),
("kernel_fault","BUG: soft lockup - CPU#{n} stuck for 22s"),
("kernel_fault","general protection fault: {n}"),
("service_failure","Failed to start fixture.service."),
("service_failure","fixture.service: Main process exited, code=exited, status=1/FAILURE"),
("package_failure","dpkg: error processing package fixture-package (--configure)"),
("package_failure","E: Sub-process /usr/bin/dpkg returned an error code ({n})"),
("network","DNS resolution timeout for private.example"),
("network","link is down on eth{n}"),
("authentication","Failed password for invalid user privateuser from 192.0.2.1"),
("application_crash","fixture[{n}]: segfault at 0 ip 00007f"),
(None,"Started fixture.service."),
(None,"EXT4-fs (sd{letter}1): mounted filesystem with ordered data mode"),
(None,"SMART health passed; 0 pending sectors"),
(None,"Network connection restored after timeout"),
(None,"No error detected in memory test"),
(None,"Previous error resolved; service started successfully"),
(None,"Finished system update; no failures"),
(None,"GPU reset completed successfully"),
(None,"Package installation completed successfully"),
(None,"authentication succeeded for privateuser")
]
def run(repeats):
    rng=random.Random(60236);rows=[]
    for expected,template in FIXTURES:
        for i in range(repeats):
            row=template.format(n=rng.randrange(1,9999),letter=rng.choice("abcdefgh"));rows.append((expected,row))
    rng.shuffle(rows);conf=collections.Counter();miner=h.Miner()
    start=time.perf_counter()
    for expected,msg in rows:
        actual=d.classify(msg)
        conf[(expected,actual)]+=1
        if actual:miner.add(actual,msg)
    elapsed=time.perf_counter()-start
    tp=sum(n for (e,a),n in conf.items() if e is not None and a is not None)
    fp=sum(n for (e,a),n in conf.items() if e is None and a is not None)
    fn=sum(n for (e,a),n in conf.items() if e is not None and a is None)
    tn=sum(n for (e,a),n in conf.items() if e is None and a is None)
    per={}
    for expected,_ in FIXTURES:
        if expected is None or expected in per:continue
        total=sum(n for (e,a),n in conf.items() if e==expected);match=conf[(expected,expected)]
        per[expected]={"samples":total,"correct":match,"recall":match/total}
    return {"schema":1,"corpus":"synthetic deterministic templates; not independent real-system logs","templates":len(FIXTURES),"samples":len(rows),"repeats_per_template":repeats,"true_positive":tp,"false_positive":fp,"false_negative":fn,"true_negative":tn,"fault_detection_precision":tp/max(1,tp+fp),"fault_detection_recall":tp/max(1,tp+fn),"exact_category_accuracy":sum(n for (e,a),n in conf.items() if e==a)/len(rows),"per_category":per,"elapsed_seconds":elapsed,"lines_per_second":len(rows)/max(elapsed,1e-9),"peak_process_rss_kib":resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,"masked_signatures_retained":len(miner.result()),"confusion":[{"expected":e,"actual":a,"count":n} for (e,a),n in conf.items()],"limits":"Synthetic repetition measures rule behavior and throughput, not generalization or unique incidents. RSS includes fixture corpus and interpreter."}
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--repeats",type=int,default=1000);a=p.parse_args()
    if not 1<=a.repeats<=5000:p.error("repeats must be 1..5000")
    print(json.dumps(run(a.repeats),indent=2,allow_nan=False))
