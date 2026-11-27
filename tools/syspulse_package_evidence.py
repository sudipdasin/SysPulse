# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail-closed package evidence policy; never execute maintenance commands."""
CORE = {"apt", "dpkg", "systemd"}


def package_evidence_ok(sources):
    """Require one complete, internally consistent record for each native probe."""
    try:
        audit, solver, integrity = (
            [s for s in sources if s.get("name") == name]
            for name in ("dpkg-audit", "apt-repair-simulation", "dpkg-core-integrity")
        )
        if any(len(rows) != 1 for rows in (audit, solver, integrity)):
            return False
        a, s, i = audit[0], solver[0], integrity[0]
        if any(row.get("bounded") is not False or row.get("events") != {} for row in (a, s, i)):
            return False
        if a.get("status") not in ("ok", "empty") or type(a.get("lines")) is not int or a["lines"] != 0:
            return False
        if s.get("status") != "ok" or i.get("status") != "ok":
            return False
        plan = s["apt_simulation"]
        if plan.get("status") != "no_changes" or plan.get("execution_authorized") is not False:
            return False
        if any(type(plan.get(k)) is not int or plan[k] != 0 for k in ("install_or_upgrade", "configure", "remove")):
            return False
        rows = i["integrity"]
        if not isinstance(rows, list) or len(rows) != len(CORE) or {r["package"] for r in rows} != CORE:
            return False
        return all(r.get("status") in ("no_reported_differences", "differences") and type(r.get("core_file_changes")) is int and r["core_file_changes"] == 0 for r in rows)
    except (TypeError, KeyError, AttributeError, ValueError):
        return False
