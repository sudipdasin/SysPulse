# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded, opt-in external investigation. No arbitrary URLs, commands or model tools."""
import concurrent.futures, gzip, hashlib, html, http.client, io, json, re, time, urllib.error, urllib.parse, urllib.request
from html.parser import HTMLParser

PROVIDERS=("web","groq","gemini")
ACTIONS=("thumbnails","archives","own_memory","pause_monitoring")
QUERIES={
 "storage_io":("Linux disk I/O errors",("kernel","devices")),
 "filesystem":("Linux ext4 filesystem errors read only",("filesystem-config","devices")),
 "disk_health":("Linux SMART disk health errors",("devices","kernel")),
 "oom":("Linux out of memory OOM swap",("memory","kernel")),
 "memory_pressure":("Linux memory pressure swap",("memory",)),
 "gpu":("Linux graphics GPU hang driver",("kernel",)),
 "thermal":("Linux thermal throttling temperature",("kernel",)),
 "kernel_fault":("Linux kernel hardware error watchdog",("kernel","journal")),
 "service_failure":("Ubuntu systemd failed service diagnosis",("failed","systemd-state")),
 "package_failure":("Ubuntu dpkg broken packages audit",("package-audit","package-origins")),
 "network":("Linux Mint network DNS connection troubleshooting",("network-links","network-routes","network-manager-state")),
 "authentication":("Linux PAM authentication failure troubleshooting",("journal",)),
 "application_crash":("Linux application crash coredump diagnosis",("journal",)),
 "other_error":("Linux Mint systemd journal troubleshooting",("journal",)),
 "low_available_memory":("Linux Mint low available memory swap",("memory",)),
 "low_disk_space":("Linux Mint disk space cache cleanup",("storage","journal-size")),
 "low_inodes":("Linux filesystem inode exhaustion",("storage","filesystem-config")),
 "hardware_or_kernel_review":("Linux kernel hardware fault diagnosis",("kernel","devices")),
 "storage_review_required":("Linux filesystem storage error backup diagnosis",("devices","kernel","filesystem-config"))
}
ROUTES=tuple(sorted({r for _,rs in QUERIES.values() for r in rs}))
# Only these fixed strings are sent externally. No raw messages, machine IDs,
# boot identifiers, paths, user text, package names or exact hardware metrics.
def profile(analysis):
    codes=sorted({f.get("code") for f in analysis.get("findings",[]) if isinstance(f,dict) and isinstance(f.get("code"),str) and f["code"] in QUERIES})
    context=analysis.get("context")
    return {"schema":1,"platform":"Linux Mint XFCE / Ubuntu family","codes":codes,"context":context if context in ("disk","memory","normal") else "normal"}
def fingerprint(p):
    return hashlib.sha256(json.dumps(p,sort_keys=True,separators=(",",":")).encode()).hexdigest()
def routes(p):return sorted({r for code in p["codes"] for r in QUERIES[code][1]})
def strict(raw):
    def pairs(values):
        out={}
        for k,v in values:
            if k in out:raise ValueError("duplicate key")
            out[k]=v
        return out
    return json.loads(raw,object_pairs_hook=pairs,parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite")))
def advice(raw,p):
    if not isinstance(raw,str) or len(raw)>8192:raise ValueError("invalid advice length")
    v=strict(raw)
    if not isinstance(v,dict) or set(v)!={"investigate","caution","prioritize"}:raise ValueError("invalid advice fields")
    for k,allowed in (("investigate",routes(p)),("caution",ACTIONS),("prioritize",ACTIONS)):
        a=v[k]
        if not isinstance(a,list) or len(a)>len(allowed) or any(not isinstance(x,str) or x not in allowed for x in a) or len(set(a))!=len(a):raise ValueError("unrecognized advice ID")
    if set(v["caution"])&set(v["prioritize"]):raise ValueError("conflicting advice")
    return {k:sorted(v[k]) for k in v}
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*a,**kw):return None
HOSTS={"api.stackexchange.com","api.groq.com","generativelanguage.googleapis.com"}
def request(url,payload=None,headers=None,limit=262144):
    u=urllib.parse.urlsplit(url)
    if u.scheme!="https" or u.netloc not in HOSTS or u.fragment:raise ValueError("unapproved endpoint")
    op=urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect())
    hdr={"User-Agent":"SysPulse/3 diagnostic research","Accept":"application/json","Accept-Encoding":"identity"}
    hdr.update(headers or {})
    data=None if payload is None else json.dumps(payload,separators=(",",":")).encode()
    if data is not None:hdr["Content-Type"]="application/json"
    deadline=time.monotonic()+12
    with op.open(urllib.request.Request(url,data=data,headers=hdr),timeout=8) as r:
        if r.geturl()!=url:raise ValueError("redirect refused")
        out=bytearray()
        while True:
            if time.monotonic()>deadline:raise TimeoutError("response deadline")
            b=r.read1(min(16384,limit+1-len(out)))
            if not b:break
            out.extend(b)
            if len(out)>limit:raise ValueError("response too large")
        encoding=r.headers.get("Content-Encoding","").lower()
        if encoding=="gzip":
            with gzip.GzipFile(fileobj=io.BytesIO(out)) as f:out=f.read(limit+1)
            if len(out)>limit:raise ValueError("decompressed response too large")
        elif encoding not in ("","identity"):raise ValueError("unsupported encoding")
        return strict(out)
def failure(provider,status):return {"provider":provider,"status":status}
def status(exc):
    if isinstance(exc,urllib.error.HTTPError):
        return "quota" if exc.code==429 else "credential_or_access" if exc.code in (401,403) else "billing_required" if exc.code==402 else "service_error"
    if isinstance(exc,(TimeoutError,urllib.error.URLError,OSError)):return "offline_or_timeout"
    return "invalid_response"
def clean(s,n):
    if not isinstance(s,str):return ""
    return " ".join("".join(c for c in html.unescape(s) if c.isprintable()).split())[:n]
class Text(HTMLParser):
    def __init__(self):super().__init__(convert_charrefs=True);self.parts=[]
    def handle_data(self,s):self.parts.append(s)
def concepts(body):
    # Only fixed recognized concepts from an accepted public answer, not executable
    # strings. Never pass the page content to a model or shell.
    if not isinstance(body,str):return []
    parser=Text();parser.feed(body[:24000]);s=" ".join(parser.parts).lower()
    rules={"backup":r"\bbackup\b","disk_health":r"\bsmart(?:ctl)?\b","filesystem_review":r"\b(?:fsck|filesystem)\b","memory_review":r"\b(?:swap|oom|memory)\b","package_review":r"\b(?:dpkg|apt)\b","service_review":r"\b(?:systemd|systemctl|journalctl)\b","network_review":r"\b(?:dns|networkmanager|dhcp)\b","kernel_review":r"\b(?:kernel|dmesg)\b"}
    return sorted(k for k,v in rules.items() if re.search(v,s))
def web(p,fetch=request):
    # A single bounded query on Ask Ubuntu; accepted answers optional second call.
    # Query is selected from static taxonomy, never generated from private data.
    priority=("storage_io","filesystem","disk_health","kernel_fault","thermal","oom","package_failure","gpu","service_failure","network","authentication","application_crash","other_error")
    code=next((x for x in priority if x in p["codes"]),p["codes"][0])
    q=urllib.parse.urlencode({"site":"askubuntu","q":QUERIES[code][0],"pagesize":5,"order":"desc","sort":"relevance","accepted":"true"})
    base="https://api.stackexchange.com/2.3/"
    row=fetch(base+"search/advanced?"+q)
    if not isinstance(row,dict) or not isinstance(row.get("items"),list) or "error_id" in row:raise ValueError("invalid search response")
    refs=[];ids=[]
    for item in row["items"][:5]:
        if not isinstance(item,dict):continue
        qid=item.get("question_id");aid=item.get("accepted_answer_id")
        if type(qid) is not int or not 0<qid<10**12 or type(aid) is not int or not 0<aid<10**12 or aid in ids:continue
        ids.append(aid)
        refs.append({"id":qid,"answer_id":aid,"title":clean(item.get("title"),180),"url":"https://askubuntu.com/questions/"+str(qid),"answer_url":"https://askubuntu.com/a/"+str(aid),"concepts":[]})
    backoff=row.get("backoff",0)
    if type(backoff) is not int or backoff<0:raise ValueError("invalid backoff")
    quota=row.get("quota_remaining")
    if ids and not backoff and type(quota) is int and quota>0:
        try:
            ans=fetch(base+"answers/"+";".join(map(str,ids))+"?"+urllib.parse.urlencode({"site":"askubuntu","filter":"withbody","pagesize":5}))
            if not isinstance(ans,dict) or not isinstance(ans.get("items"),list):raise ValueError("invalid answer response")
            if type(ans.get("backoff")) is int:backoff=max(backoff,ans["backoff"])
            for item in ans["items"][:5]:
                if not isinstance(item,dict):continue
                for r in refs:
                    if item.get("answer_id")==r["answer_id"]:
                        r["concepts"]=concepts(item.get("body"))
                        owner=item.get("owner",{})
                        r["author"]=clean(owner.get("display_name"),80) if isinstance(owner,dict) else ""
                        r["license"]=clean(item.get("content_license"),40)
        except (OSError,ValueError,TypeError,RecursionError,http.client.HTTPException,urllib.error.URLError):pass
    return {"provider":"web","status":"ok" if refs else "no_results","topic":code,"references":refs,"backoff":min(backoff,86400*30)}
def prompt(p):
    return ("You are a Linux maintenance triage classifier. This input contains ONLY public taxonomy labels. "
      "Return one JSON object with exactly three keys investigate, caution, prioritize. Values are arrays of unique IDs. "
      "No commands, shell, URLs, prose, code, tools, downloads, or changes. Choose cautious diagnostics, not repairs. "
      "Investigate IDs: "+json.dumps(routes(p))+". Action IDs: "+json.dumps(ACTIONS)+
      ". Caution means avoid default selecting action; prioritize means consider existing safe action. "
      "Never presume hardware health or permission. Unknown means empty arrays. Input: "+json.dumps(p,sort_keys=True))
def ai(provider,p,key,fetch=request):
    if not isinstance(key,str) or not re.fullmatch(r"[A-Za-z0-9_.-]{16,256}",key):return failure(provider,"not_configured")
    msg=prompt(p)
    if provider=="groq":
        model="llama-3.1-8b-instant"
        r=fetch("https://api.groq.com/openai/v1/chat/completions",{"model":model,"messages":[{"role":"user","content":msg}],"temperature":0,"max_completion_tokens":384,"response_format":{"type":"json_object"}},{"Authorization":"Bearer "+key})
        raw=r["choices"][0]["message"]["content"]
    elif provider=="gemini":
        model="gemini-3.1-flash-lite"
        r=fetch("https://generativelanguage.googleapis.com/v1beta/models/"+model+":generateContent",{"contents":[{"parts":[{"text":msg}]}],"generationConfig":{"temperature":0,"maxOutputTokens":512,"responseMimeType":"application/json"}},{"x-goog-api-key":key})
        raw=r["candidates"][0]["content"]["parts"][0]["text"]
    else:raise ValueError("unsupported provider")
    return {"provider":provider,"status":"ok","model":model,"advice":advice(raw,p)}
def run(p,providers,key_lookup,fetch=request):
    def task(provider):
        try:return web(p,fetch) if provider=="web" else ai(provider,p,key_lookup(provider),fetch)
        except (OSError,ValueError,KeyError,IndexError,TypeError,AttributeError,RecursionError,http.client.HTTPException,urllib.error.URLError) as e:return failure(provider,status(e))
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        return list(pool.map(task,providers))
def combine(p,results):
    models=[r["advice"] for r in results if r["provider"] in ("groq","gemini") and r["status"]=="ok"]
    caution=sorted(set.intersection(*(set(m["caution"]) for m in models))) if len(models)>=2 else []
    priorities=sorted(set.intersection(*(set(m["prioritize"]) for m in models))) if len(models)>=2 else []
    consensus=set.intersection(*(set(m["investigate"]) for m in models)) if len(models)>=2 else set()
    concepts_seen={x for r in results if r["provider"]=="web" and r["status"]=="ok" for ref in r["references"] for x in ref["concepts"]}
    concept_routes={"disk_health":{"devices","kernel"},"filesystem_review":{"filesystem-config","devices"},"memory_review":{"memory"},"package_review":{"package-audit","package-origins"},"service_review":{"failed","systemd-state","journal"},"network_review":{"network-links","network-routes","network-manager-state"},"kernel_review":{"kernel"},"backup":{"devices","storage"}}
    web_routes={x for k in concepts_seen for x in concept_routes.get(k,set())}&set(routes(p))
    # Independent evidence ranks known diagnostic routes only. It never invents a command.
    investigation=sorted(routes(p),key=lambda x:(-(int(x in consensus)+int(x in web_routes)),x))
    return {"investigate":investigation,"consensus_caution":caution,"consensus_priority":priorities,"corroborated_routes":sorted(consensus&web_routes),"models_agree":len(models)>=2,"model_count":len(models)}
def apply_research(analysis,record):
    # May suppress default preselection, never grant an action, never override
    # local learned demotion or recommend an otherwise non-recommended action.
    if not record:return analysis
    analysis["research"]=record
    for a in record["guidance"]["consensus_caution"]:
        if a not in ACTIONS:continue
        row=analysis["recommendations"].get(a)
        if row and row["recommended"]:
            row["recommended"]=False;row["reason"]+=" Independent AI providers advise caution; optional reviewed action only."
    return analysis
