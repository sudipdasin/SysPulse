# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only package triage using APT's solver and dpkg's native verifier."""
import re

SOLVER = ("apt-get", "-s", "--no-download", "--no-remove", "--fix-broken",
          "-o", "Debug::NoLocking=true", "-o", "APT::Get::Show-User-Simulation-Note=false", "install")
CORE = ("dpkg", "apt", "systemd")
SUMMARY = re.compile(r"^(\d+) upgraded, (\d+) newly installed, (\d+) to remove and (\d+) not upgraded\.$")

def solver_result(status, raw, bounded):
    result = {"status": "unknown", "upgrades": 0, "installs": 0, "configurations": 0, "removals": 0, "held_back": 0}
    if bounded or status in ("unavailable", "permission", "timeout", "bounded"):
        return result
    lines = raw.splitlines()
    if len(lines) > 4000:
        return result
    if any(re.match(r"^Remv\s", x) for x in lines) or any("remove is disabled" in x.lower() for x in lines):
        result["status"] = "blocked_removal"
        return result
    if status != "ok" or any(x.startswith("E:") for x in lines):
        if any(re.search(r"unmet dependencies|held broken packages|unable to correct problems", x, re.I) for x in lines):
            result["status"] = "unresolved"
        return result
    if any(x.startswith(("W:", "N:")) for x in lines):
        return result
    summaries = [SUMMARY.fullmatch(x) for x in lines if SUMMARY.fullmatch(x)]
    if len(summaries) != 1:
        return result
    u, i, r, h = map(int, summaries[0].groups())
    if max(u, i, r, h) > 100000:
        return result
    result.update(upgrades=u, installs=i, removals=r, held_back=h)
    if r:
        result["status"] = "blocked_removal"
        return result
    installs = sum(bool(re.match(r"^Inst\s", x)) for x in lines)
    configs = sum(bool(re.match(r"^Conf\s", x)) for x in lines)
    if installs != u+i or configs > 100000:
        return result
    result["configurations"] = configs
    result["status"] = "changes_proposed" if u or i or configs else "no_changes_proposed"
    return result

def integrity_result(status, raw, bounded):
    out = {"status": "unknown", "core_file_changes": 0, "configuration_changes": 0, "documentation_changes": 0, "other_changes": 0}
    if status != "ok" or bounded:
        return out
    lines = raw.splitlines()
    if len(lines) > 4000:
        return out
    for line in lines:
        if not line.strip():
            continue
        m = re.fullmatch(r"(?:missing\s+|[?\.A-Z0-9]{9}\s+)(?:(c)\s+)?(/[^\r\n]+)", line.strip())
        if not m:
            return dict(out, status="unknown")
        config, path = m.groups()
        if config or path.startswith("/etc/"):
            key = "configuration_changes"
        elif path.startswith(("/usr/share/doc/", "/usr/share/man/", "/usr/share/locale/", "/usr/share/lintian/")):
            key = "documentation_changes"
        elif path.startswith(("/usr/bin/", "/usr/sbin/", "/bin/", "/sbin/", "/usr/lib/", "/lib/")):
            key = "core_file_changes"
        else:
            key = "other_changes"
        out[key] += 1
    out["status"] = "differences" if any(out[k] for k in out if k != "status") else "no_reported_differences"
    return out

def solver(command):
    status, raw, bounded = command(list(SOLVER), timeout=8, limit=131072)
    return solver_result(status, raw, bounded)

def integrity(command):
    rows = []
    for package in CORE:
        status, raw, bounded = command(["dpkg-query", "-W", "-f=${db:Status-Status}", package], timeout=3, limit=1024)
        if status != "ok" or bounded or raw.strip() != "installed":
            rows.append({"package": package, "status": "unknown"})
            continue
        status, raw, bounded = command(["dpkg", "--verify", package], timeout=5, limit=131072)
        rows.append(dict(integrity_result(status, raw, bounded), package=package))
    return rows
