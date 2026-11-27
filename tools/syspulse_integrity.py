# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded read-only dpkg verification; differences are not proof of compromise."""
import re
CORE = ("dpkg", "apt", "systemd")

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
