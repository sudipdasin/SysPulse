#!/usr/bin/python3
"""Holdout: novel synthetic log phrasings, including mixed severity and recovery.
Keep this corpus separate from the 32-template tuning benchmark.
"""
import importlib.machinery,importlib.util,json,pathlib,sys,collections,random,time,resource
tools=pathlib.Path(__file__).resolve().parents[1]/"tools";sys.path.insert(0,str(tools))
loader=importlib.machinery.SourceFileLoader("diag_holdout",str(tools/"syspulse-diagnostics"))
spec=importlib.util.spec_from_loader(loader.name,loader);d=importlib.util.module_from_spec(spec);loader.exec_module(d)
# Expected category is human-assigned; none means a nonfault control.
CASES=[
("storage_io",3,"end_request: I/O error, dev sdb, sector 9862"),
("storage_io",4,"ata4.00: failed command: READ FPDMA QUEUED"),
("storage_io",3,"nvme1: controller timeout during shutdown"),
("filesystem",3,"EXT4-fs error (device dm-0): ext4_find_entry: inode #431"),
("filesystem",4,"XFS (dm-2): metadata error in xfs_trans_read_buf"),
("filesystem",3,"Remounting filesystem read-only"),
("disk_health",4,"SMART overall-health self-assessment test result: FAILED!"),
("disk_health",4,"Drive has 12 pending sectors"),
("oom",3,"Memory cgroup out of memory: Killed process 4112"),
("oom",4,"systemd-oomd: Killed /user.slice due to memory pressure"),
("thermal",4,"CPU7: Core temperature above threshold, cpu clock throttled"),
("thermal",3,"ACPI: critical temperature reached"),
("kernel_fault",3,"watchdog: BUG: soft lockup - CPU#2 stuck"),
("kernel_fault",2,"Machine check: Hardware Error"),
("gpu",3,"amdgpu 0000:03:00.0: ring gfx timeout"),
("gpu",4,"i915: GPU HANG: ecode 12:1:ae89"),
("memory_pressure",4,"systemd: memory pressure detected in user.slice"),
("memory_pressure",4,"page allocation failure: order:4"),
("service_failure",3,"cups.service: Failed with result 'exit-code'."),
("service_failure",4,"Dependency failed for fixture service"),
("package_failure",3,"E: Unmet dependencies. Try apt --fix-broken install."),
("package_failure",3,"dpkg: error: requested operation requires superuser privilege"),
("network",4,"DHCP transaction timeout on interface"),
("network",4,"Network is unreachable"),
("authentication",4,"pam_unix(login:auth): authentication failure"),
("authentication",4,"Invalid user demo from 192.0.2.2"),
("application_crash",3,"app[21]: segfault at 0 ip 0012"),
("application_crash",4,"Process terminated: core dumped"),
("other_error",3,"Unexpected condition 42"),
("other_error",4,"I/O subsystem reported an unspecified error"),
(None,6,"Reached target Graphical Interface."),
(None,6,"EXT4-fs (dm-0): mounted filesystem with ordered data mode"),
(None,6,"SMART overall-health self-assessment test result: PASSED"),
(None,6,"NetworkManager: device state change activated"),
(None,6,"systemd: Started session scope"),
(None,6,"apt: package successfully configured"),
(None,6,"Accepted publickey for user"),
(None,6,"GPU driver initialized"),
(None,6,"CPU temperature 46 C"),
(None,6,"No problems found"),
# Harder controls; possible false positives are measured, not hidden.
(None,6,"Memory pressure returned to normal"),
(None,6,"Previous package error was resolved during recovery"),
(None,6,"Filesystem check complete: 0 errors"),
(None,6,"SMART: 0 pending sectors, 0 offline uncorrectable sectors"),
(None,6,"Connection timed out earlier; link restored"),
(None,6,"GPU reset completed successfully; display restored"),
("storage_io",4,"GPU reset completed successfully; Buffer I/O error on device sda"),
("disk_health",4,"SMART health passed; 7 pending sectors"),
("kernel_fault",3,"No errors detected in memory test; general protection fault"),
("other_error",2,"No errors detected in memory test"),
]
CRITICAL={"storage_io","filesystem","disk_health","oom","thermal","kernel_fault"}
def run(repeats=1000):
    rows=list(CASES)*repeats;random.Random(71037).shuffle(rows)
    confusion=collections.Counter();start=time.perf_counter()
    for expected,priority,msg in rows:confusion[(expected,d.classify(msg,priority))]+=1
    elapsed=time.perf_counter()-start
    tp=sum(n for (e,a),n in confusion.items() if e is not None and a is not None)
    fp=sum(n for (e,a),n in confusion.items() if e is None and a is not None)
    fn=sum(n for (e,a),n in confusion.items() if e is not None and a is None)
    critical_total=sum(n for (e,a),n in confusion.items() if e in CRITICAL)
    critical_mismatch=sum(n for (e,a),n in confusion.items() if e in CRITICAL and a!=e)
    return {"schema":1,"corpus":"synthetic holdout, not external production logs","templates":len(CASES),"samples":len(rows),"precision":tp/max(1,tp+fp),"recall":tp/max(1,tp+fn),"exact_accuracy":sum(n for (e,a),n in confusion.items() if e==a)/len(rows),"critical_samples":critical_total,"critical_category_mismatches":critical_mismatch,"false_positives":fp,"false_negatives":fn,"lines_per_second":len(rows)/max(elapsed,1e-9),"peak_rss_kib":resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,"misclassified_templates":[{"expected":e,"actual":d.classify(msg,p),"priority":p,"message":msg} for e,p,msg in CASES if d.classify(msg,p)!=e]}
if __name__=="__main__":
    out=run();print(json.dumps(out,indent=2,allow_nan=False))
    # Conservative gate: never silently miss a critical category.
    if out["critical_category_mismatches"]:raise SystemExit(1)
