#!/usr/bin/python3
"""Live no-secret end-to-end research/SQLite/follow-up smoke with synthetic issue."""
import importlib.machinery, importlib.util, json, os, pathlib, sys, tempfile, time
helper=pathlib.Path(sys.argv[1]).resolve();sys.path.insert(0,str(helper.parent))
loader=importlib.machinery.SourceFileLoader("diagnostics_runtime",str(helper));spec=importlib.util.spec_from_loader(loader.name,loader);d=importlib.util.module_from_spec(spec);loader.exec_module(d)
assert os.geteuid()!=0
with tempfile.TemporaryDirectory() as tmp:
    os.environ["HOME"]=tmp;os.umask(0o077)
    with d.connect(d.secure_db()) as c:
        m=d.numeric_snapshot()
        # Synthetic generic package issue; no real local logs are sent.
        sources=[d.source("fixture","ok",{"package_failure":1},1)]
        b=d.boot_id();a=d.record_analysis(c,m,sources,b,d.clock(),int(time.time()))
        r=d.investigate(c,a["id"],["web","groq","gemini"],b,d.clock(),int(time.time()),lookup=lambda _:None)
        assert r["status"]=="complete"
        assert all(x["status"]=="not_configured" for x in r["results"] if x["provider"] in ("groq","gemini"))
        q=d.refine(c,a["id"],b,d.clock(),int(time.time()))
        assert q["followup_state"]=="complete" and 0<len(q["followup"])<=4
        revised=d.read_analysis(c,a["id"],b,d.clock(),int(time.time()))
        assert len(revised["sources"])==len(sources)+len(q["followup"])
        assert not revised["recommendations"]["archives"]["allowed"]
        assert d.show_history(c)["research"][0]["followup_state"]=="complete"
        assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
        print(json.dumps({"web_status":next(x["status"] for x in r["results"] if x["provider"]=="web"),"references":sum(len(x.get("references",[])) for x in r["results"]),"local_followup_probes":len(q["followup"]),"retained":True,"safety_preserved":True,"ai_credentials":"not_configured","mutation":False}))
