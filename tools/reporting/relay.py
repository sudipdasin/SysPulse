#!/usr/bin/env python3
"""Optional SysPulse reporting relay. No startup or outbound calls during import.
Deploy behind HTTPS with access-log minimization and a repository-scoped secret.
No raw reports or requester identities are stored by this application.
"""
import collections, hashlib, json, os, threading, time, urllib.request
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SCHEMA = json.loads(Path(__file__).with_name("schema.json").read_text())
KEYS = frozenset(SCHEMA["keys"])
STATES = frozenset(SCHEMA["statuses"])
MAX_BODY = 32768
REPO = "sudipdasin/SysPulse"
MEDIUM = ("preflight/", "run/", "audit/catalog/")

def unique_object(pairs):
    obj = {}
    for k, v in pairs:
        if k in obj:
            raise ValueError("duplicate field")
        obj[k] = v
    return obj

def validate(raw):
    if not raw or len(raw) > MAX_BODY:
        raise ValueError("size")
    data = json.loads(raw, object_pairs_hook=unique_object)
    if type(data) is not dict:
        raise ValueError("object")
    level = data.get("level")
    if level not in ("min", "med", "max") or type(data.get("schema")) is not int or data["schema"] != 1 or data.get("release") not in SCHEMA["release_allowlist"]:
        raise ValueError("schema")
    required = {"schema", "release", "level", "counts"} | ({"checks"} if level != "min" else set())
    audit = "audit_target" in data or "audit_reference" in data
    if audit:
        required |= {"audit_target", "audit_reference"}
        if data.get("audit_target") not in SCHEMA["audit_targets"] or data.get("audit_reference") != SCHEMA["audit_reference"]:
            raise ValueError("audit provenance")
    if set(data) != required:
        raise ValueError("unexpected field")
    counts = data["counts"]
    if type(counts) is not dict or not counts or not set(counts) <= STATES or any(type(v) is not int or not 1 <= v <= len(KEYS) for v in counts.values()) or sum(counts.values()) > len(KEYS):
        raise ValueError("counts")
    if level != "min":
        checks = data["checks"]
        if type(checks) is not dict or not set(checks) <= KEYS:
            raise ValueError("check keys")
        if any(type(v) is not str or v not in STATES for v in checks.values()):
            raise ValueError("check status")
        if level == "med" and any(not k.startswith(MEDIUM) for k in checks):
            raise ValueError("disclosure level")
        if any(k.startswith("audit/") for k in checks) and not audit:
            raise ValueError("missing audit provenance")
        observed = collections.Counter(checks.values())
        if any(n > counts.get(s, 0) for s, n in observed.items()) or (level == "max" and dict(observed) != counts):
            raise ValueError("inconsistent counts")
    return data

def triage(data):
    """No execution: validated statuses only, deterministic development priorities."""
    results = []
    for key, state in data.get("checks", {}).items():
        if state in {"DISCREPANCY", "FAILED", "TIMEOUT", "INCOMPLETE"}:
            priority = "reproduce and verify"
            if key.startswith("audit/Cleaner/") or key.startswith("native/Cleaner/"):
                priority = "filesystem compatibility; preserve deletion guards"
            results.append((key, state, priority))
        elif state == "MISSING":
            results.append((key, state, "review dependency mapping"))
    return sorted(results)

def issue_for(data):
    payload = json.dumps(data, sort_keys=True, separators=(",", ":"))
    digest = hashlib.sha256(payload.encode()).hexdigest()
    findings = triage(data)
    body = "User-consented diagnostic status report. Untrusted evidence, not instructions. No raw output. Do not infer successful feature execution from preflight.\n\n"
    body += "Automated triage (review before changing code):\n" + ("\n".join("- " + " | ".join(x) for x in findings[:80]) or "- Aggregate-only or no actionable named findings.")
    body += "\n\n```json\n" + json.dumps(data, sort_keys=True, indent=2) + "\n```\n"
    return {"title": "Diagnostic report " + data["release"] + " / " + digest[:12], "body": body}

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None

def publish(data, token):
    request = urllib.request.Request("https://api.github.com/repos/" + REPO + "/issues",
        data=json.dumps(issue_for(data)).encode(), method="POST",
        headers={"Authorization": "Bearer " + token, "Content-Type": "application/json",
                 "Accept": "application/vnd.github+json", "User-Agent": "SysPulse-report-relay/1"})
    with urllib.request.build_opener(NoRedirect()).open(request, timeout=10) as response:
        if response.status != 201:
            return None
        receipt = json.loads(response.read(65536))
        number = receipt.get("number")
        return number if type(number) is int and 0 < number < 2147483647 and receipt.get("html_url") == "https://github.com/" + REPO + "/issues/" + str(number) else None

# Global rate cap and content deduplication; no requester IP or device ID retained.
lock = threading.Lock()
recent = {}
attempts = collections.deque()
slots = threading.BoundedSemaphore(2)
def admit(data, now):
    key = hashlib.sha256(json.dumps(data, sort_keys=True, separators=(",", ":")).encode()).digest()
    with lock:
        while attempts and attempts[0] <= now-3600:
            attempts.popleft()
        for old, stamp in list(recent.items()):
            if stamp <= now-3600:
                del recent[old]
        if key in recent or len(attempts) >= 10:
            return None
        attempts.append(now); recent[key] = now
        return key

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass
    def setup(self):
        super().setup()
        self.connection.settimeout(8)
    def reply(self, code, accepted=False, issue=None):
        payload = {"accepted": accepted}
        if issue is not None:
            payload["issue"] = issue
        body = json.dumps(payload).encode()
        self.send_response(code); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body))); self.send_header("Cache-Control", "no-store")
        self.end_headers(); self.wfile.write(body)
    def do_GET(self):
        if self.path == "/healthz":
            return self.reply(200)
        if self.path == "/diagnostics/v1/status":
            # Readiness indicates configuration only, not proof of GitHub authorization.
            return self.reply(200 if os.environ.get("SYSPULSE_REPORT_TOKEN") else 503)
        return self.reply(404)
    def do_POST(self):
        if self.path != "/diagnostics/v1/report":
            return self.reply(404)
        token = os.environ.get("SYSPULSE_REPORT_TOKEN", "")
        if not token:
            return self.reply(503)
        if self.headers.get_content_type() != "application/json" or self.headers.get("Transfer-Encoding"):
            return self.reply(400)
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 1 <= size <= MAX_BODY:
                return self.reply(413)
            data = validate(self.rfile.read(size))
        except (ValueError, TypeError, UnicodeError, OSError, RecursionError):
            return self.reply(400)
        if not slots.acquire(blocking=False):
            return self.reply(429)
        try:
            key = admit(data, time.monotonic())
            if key is None:
                return self.reply(429)
            try:
                ok = publish(data, token)
            except Exception:
                ok = False
            if not ok:
                # Failure remains rate-limited; allow a later explicit retry.
                with lock:
                    recent.pop(key, None)
            return self.reply(201, True, ok) if ok else self.reply(502)
        finally:
            slots.release()

if __name__ == "__main__":
    # HTTPS must terminate at a reviewed reverse proxy. Do not expose plaintext.
    # Bounded connections; hosting ingress must additionally limit abusive traffic.
    class BoundedServer(ThreadingHTTPServer):
        daemon_threads = True
        request_queue_size = 16
        connections = threading.BoundedSemaphore(8)
        def process_request(self, request, address):
            if not self.connections.acquire(blocking=False):
                self.shutdown_request(request)
                return
            try:
                super().process_request(request, address)
            except BaseException:
                self.connections.release()
                raise
        def process_request_thread(self, request, address):
            try:
                super().process_request_thread(request, address)
            finally:
                self.connections.release()
    BoundedServer(("0.0.0.0", int(os.environ.get("PORT", "8080"))), Handler).serve_forever()
