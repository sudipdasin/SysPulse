# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded SMART JSON evidence parser; no device access or repair authority."""
import json


def integer(value):
    return value if type(value) is int and 0 <= value < 2**64 else None


def unique_pairs(pairs):
    obj = {}
    for key, value in pairs:
        if key in obj:
            raise ValueError("Duplicate SMART JSON key")
        obj[key] = value
    return obj


def invalid_constant(_):
    raise ValueError("Nonfinite SMART JSON value")


def smart_result(raw, status, bounded):
    unknown = {"status": "partial", "health": "unknown"}
    if bounded:
        return {"status": "bounded", "health": "unknown"}
    if status not in ("ok", "failed"):
        return {"status": status if status in ("unavailable", "permission", "timeout") else "partial", "health": "unknown"}
    if not isinstance(raw, str) or len(raw) > 131072:
        return unknown
    try:
        row = json.loads(raw, object_pairs_hook=unique_pairs, parse_constant=invalid_constant)
        if not isinstance(row, dict):
            return unknown
        tool = row.get("smartctl")
        rc = integer(tool.get("exit_status")) if isinstance(tool, dict) else None
        if rc is None or rc > 255:
            return unknown
        malformed = (status == "ok") != (rc == 0)
        health_obj = row.get("smart_status", {})
        health = health_obj.get("passed") if isinstance(health_obj, dict) else None
        if "smart_status" in row and type(health) is not bool:
            malformed = True
        nv = row.get("nvme_smart_health_information_log", {})
        ata = row.get("ata_smart_attributes", {})
        if not isinstance(nv, dict) or not isinstance(ata, dict):
            malformed = True
        nv = nv if isinstance(nv, dict) else {}
        ata = ata if isinstance(ata, dict) else {}
        counters = {}
        for key in ("critical_warning", "media_errors"):
            if key in nv:
                value = integer(nv[key])
                if value is None or (key == "critical_warning" and value > 255):
                    malformed = True
                else:
                    counters[key] = value
        attrs = {}
        table = ata.get("table", [])
        if not isinstance(table, list) or len(table) > 256 or ("ata_smart_attributes" in row and "table" not in ata):
            malformed = True
        for entry in table[:256] if isinstance(table, list) else []:
            if not isinstance(entry, dict) or type(entry.get("id")) is not int:
                malformed = True
                continue
            ident = entry["id"]
            if ident not in (5, 197, 198):
                continue
            detail = entry.get("raw")
            value = integer(detail.get("value")) if isinstance(detail, dict) else None
            if value is None:
                malformed = True
                continue
            if ident in attrs:
                malformed = True
            attrs[ident] = max(value, attrs.get(ident, 0))
        # smartctl bits 3/4: current failure; bits 5/6/7: historical evidence.
        # An incomplete query never erases independently available positive risk.
        risk = health is False or bool(rc & 0x18) or bool(counters.get("critical_warning")) or bool(attrs.get(197)) or bool(attrs.get(198))
        caution = bool(rc & 0xe0) or bool(counters.get("media_errors")) or bool(attrs.get(5))
        partial = malformed or bool(rc & 7) or health is None
        result = {"status": "partial" if partial else "ok", "health": "critical" if risk else "warning" if caution else "passed" if health is True and not partial else "unknown"}
        for key, value in (("pending_sectors", attrs.get(197)), ("uncorrectable_sectors", attrs.get(198)), ("reallocated_sectors", attrs.get(5)), ("media_errors", counters.get("media_errors")), ("nvme_critical_warning", counters.get("critical_warning"))):
            if value is not None:
                result[key] = value
        return result
    except (ValueError, TypeError, RecursionError, OverflowError):
        return unknown
