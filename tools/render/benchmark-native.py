#!/usr/bin/python3
"""Run native measurements serially so concurrent work does not skew them."""
import json, os, pathlib, platform, statistics, subprocess, sys, tempfile, time

def run(exe, args, env, timeout):
    p=subprocess.run([str(exe),*args],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=timeout,check=True)
    if len(p.stdout)>1048576: raise ValueError("Oversized benchmark output")
    return p.stdout.decode()

def summarize(samples, keys):
    return {key:{"median":statistics.median([s[key] for s in samples]),"max":max(s[key] for s in samples),"min":min(s[key] for s in samples)} for key in keys}

def main():
    if len(sys.argv)!=5 or os.geteuid()==0: raise SystemExit("usage (non-root): benchmark-native.py IDLE_PROBE CLEANER_PROBE SOURCE_SHA OUTPUT")
    idle,cleaner,source,out=sys.argv[1:]
    report={"schema":1,"source_commit":source,"environment":{"platform":platform.platform(),"logical_cpus":os.cpu_count(),"renderer":"offscreen","fixture":"synthetic local files in container; not physical HDD/SSD"},"idle":{},"cleaner":{}}
    with tempfile.TemporaryDirectory(prefix="syspulse-perf-") as tmp:
        root=pathlib.Path(tmp)
        env=dict(os.environ,HOME=tmp,QT_QPA_PLATFORM="offscreen")
        for k in ("CONFIG","DATA","CACHE","STATE","RUNTIME"):
            d=root/k.lower();d.mkdir(mode=0o700);env["XDG_"+k+"_HOME" if k!="RUNTIME" else "XDG_RUNTIME_DIR"]=str(d)
        for mode in ("live","paused"):
            samples=[]
            for repeat in range(3):
                args=["--duration-ms","6000","--source-label",source]+(["--paused"] if mode=="paused" else [])
                sample=json.loads(run(idle,args,env,20))
                if sample["mode"]!=mode or sample["source_label"]!=source or sample["application_version"]!="3.0.0-alpha.31" or sample["elapsed_seconds"]<5.9 or sample["elapsed_seconds"]>10 or sample["rss_end_kib"]<=0:
                    raise ValueError("Invalid native idle sample")
                sample["repeat"]=repeat+1;samples.append(sample)
            summary=summarize(samples,["cpu_percent_one_core","rss_endpoint_max_kib","application_timer_events_per_second"])
            report["idle"][mode]={"samples":samples,"summary":summary}
        samples=[json.loads(line) for line in run(cleaner,[],env,180).splitlines()]
        for count in (10000,50000):
            selected=[s for s in samples if s["files"]==count]
            if len(selected)!=3 or any(not s["complete"] or s["logical_bytes"]!=16*count or s["seconds"]<=0 for s in selected):
                raise ValueError("Invalid Cleaner benchmark sample")
            report["cleaner"][str(count)]={"samples":selected,"summary":summarize(selected,["seconds","files_per_second"])}
    # Record provisional budget observations, not a promise of target hardware speed.
    live=report["idle"]["live"]["summary"];paused=report["idle"]["paused"]["summary"]
    report["provisional_budget"]={"live_cpu_median_under_5_percent":live["cpu_percent_one_core"]["median"]<5,"paused_cpu_median_under_2_percent":paused["cpu_percent_one_core"]["median"]<2,"rss_endpoint_max_under_128_mib":max(live["rss_endpoint_max_kib"]["max"],paused["rss_endpoint_max_kib"]["max"])<131072}
    report["limitations"]=["CPU is process-only fraction of one core; child processes excluded","RSS endpoints are not peak memory","scans include metadata only, not duplicate-content hashing","three samples per workload; no physical laptop or disk throughput inference","absolute budgets are provisional engineering targets, not release acceptance by the owner"]
    pathlib.Path(out).write_text(json.dumps(report,indent=2)+"\n")
    print("BENCHMARK_SUMMARY "+json.dumps({"idle":{k:v["summary"] for k,v in report["idle"].items()},"cleaner":{k:v["summary"] for k,v in report["cleaner"].items()},"budget":report["provisional_budget"]},sort_keys=True))
if __name__=="__main__":main()
