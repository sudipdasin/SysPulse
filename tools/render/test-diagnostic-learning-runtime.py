#!/usr/bin/python3
"""Live non-root read-only diagnostic/database smoke; no maintenance mutation."""
import json,os,pathlib,sqlite3,stat,subprocess,sys,tempfile
helper=pathlib.Path(sys.argv[1]).resolve()
assert os.geteuid()!=0
with tempfile.TemporaryDirectory(prefix="syspulse-diagnostics-smoke-") as home:
    env=dict(os.environ,HOME=home)
    def call(*args,ok=True):
        r=subprocess.run(["/usr/bin/python3",str(helper),*args],env=env,capture_output=True,timeout=145)
        assert (r.returncode==0)==ok,(args,r.returncode,r.stdout[:500],r.stderr[:500])
        return json.loads(r.stdout)
    a=call("analyze")
    assert a["state"]=="ready" and len(a["sources"])>=35 and a["coverage"]["sources"]==len(a["sources"])
    assert a["metrics"]["memory_total_kib"]>0 and a["metrics"]["root_total_bytes"]>0
    ident=a["id"]
    db=pathlib.Path(home)/".local/state/syspulse/diagnostics/history.sqlite3"
    assert db.is_file() and db.stat().st_mode&0o077==0 and db.parent.stat().st_mode&0o077==0
    p=call("plan",ident);assert p==a
    assert call("reserve",ident,'["own_memory"]')["state"]=="reserved"
    assert call("reserve",ident,'["own_memory"]',ok=False)["error"]
    # Receipt exercises the learning transaction only; no memory operation is claimed.
    assert call("finish",ident,'{"own_memory":{"status":"no_effect","bytes":0}}')["state"]=="finished"
    h=call("history");assert h["analyses"][0]["state"]=="finished"
    assert h["learning"][0]["attempts"]==1 and h["learning"][0]["no_effect"]==1
    assert call("plan",ident,ok=False)["error"]
    with sqlite3.connect(db) as c:
        assert c.execute("PRAGMA integrity_check").fetchone()[0]=="ok"
        assert c.execute("SELECT count(*) FROM boost").fetchone()[0]==1
    assert call("clear")["state"]=="cleared"
    h=call("history");assert h["analyses"]==[] and h["learning"]==[]
    print("Actual non-root diagnostics, "+str(len(a["sources"]))+" source coverage, private SQLite persistence, same-caller one-use reservation, simulated receipt, history and clear passed. No maintenance mutation.")
