#!/usr/bin/python3
import json,os,sys,subprocess,statistics,tempfile,pathlib
if len(sys.argv)!=3 or os.geteuid()==0: raise SystemExit("usage: endurance IDLE_PROBE OUTPUT")
exe,out=sys.argv[1:]
samples=[]
with tempfile.TemporaryDirectory() as home:
    env=dict(os.environ,HOME=home,QT_QPA_PLATFORM="offscreen")
    for key in ("XDG_CONFIG_HOME","XDG_DATA_HOME","XDG_CACHE_HOME","XDG_STATE_HOME","XDG_RUNTIME_DIR"):
        p=pathlib.Path(home)/key;p.mkdir(mode=0o700);env[key]=str(p)
    for rep in range(3):
        for mode in ("live","paused"):
            args=[exe,"--duration-ms","30000","--source-label","1dd50ad48ab08d4f90358690ad8c7864d7185d75"]+(["--paused"] if mode=="paused" else [])
            p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=45,check=True)
            sample=json.loads(p.stdout)
            assert sample["mode"]==mode and sample["application_version"]=="3.0.0-alpha.31"
            # Linux RSS counters can be asynchronously accounted; retain every raw
            # value instead of assuming the reported VmHWM always dominates VmRSS.
            # The budget uses the largest of the three observed values.
            assert 29.9<=sample["elapsed_seconds"]<=35, sample
            assert min(sample["rss_peak_kib"],sample["rss_start_kib"],sample["rss_end_kib"])>0, sample
            sample["rss_counter_consistent"]=sample["rss_peak_kib"]>=max(sample["rss_start_kib"],sample["rss_end_kib"])
            sample["rss_observed_max_kib"]=max(sample["rss_peak_kib"],sample["rss_start_kib"],sample["rss_end_kib"])
            sample["repeat"]=rep+1;samples.append(sample)
            print("ENDURANCE_SAMPLE "+json.dumps(sample),flush=True)
summary={}
for mode in ("live","paused"):
    group=[s for s in samples if s["mode"]==mode]
    summary[mode]={"cpu_median_percent":statistics.median(s["cpu_percent_one_core"] for s in group),"max_peak_rss_kib":max(s["rss_observed_max_kib"] for s in group),"max_timer_events_per_second":max(s["application_timer_events_per_second"] for s in group)}
gates={"live_cpu":summary["live"]["cpu_median_percent"]<5,"paused_cpu":summary["paused"]["cpu_median_percent"]<2,"peak_rss":max(v["max_peak_rss_kib"] for v in summary.values())<131072,"paused_timers":summary["paused"]["max_timer_events_per_second"]==0}
report={"source_commit":"1dd50ad48ab08d4f90358690ad8c7864d7185d75","samples":samples,"summary":summary,"gates":gates,"scope":"offscreen container, process-only CPU, largest of VmHWM and VmRSS endpoints (kernel counters may disagree), 30 seconds per repetition"}
pathlib.Path(out).write_text(json.dumps(report,indent=2)+"\n")
print("ENDURANCE_SUMMARY "+json.dumps({"summary":summary,"gates":gates}),flush=True)
