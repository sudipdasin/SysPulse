# SPDX-License-Identifier: GPL-3.0-or-later
"""Conservative informational recovery recognition.
These observations are not proof that the underlying fault is repaired.
"""
import re
RULES=tuple((key,re.compile(pattern,re.I)) for key,pattern in (
    ("storage_health_report",r"SMART\s+health\s+passed[;,. ]+\s*0\s+pending\s+sectors[.]?"),
    ("storage_health_report",r"SMART\s*:\s*0\s+pending\s+sectors\s*,\s*0\s+offline\s+uncorrectable\s+sectors[.]?"),
    ("memory_check_clear",r"No\s+errors?\s+detected\s+in\s+(?:the\s+)?(?:memory|RAM)\s+test[.]?"),
    ("memory_recovery",r"Memory\s+pressure\s+(?:has\s+)?returned\s+to\s+normal[.]?"),
    ("service_recovery",r"Previous\s+error\s+resolved[;,. ]+\s*service\s+started\s+successfully[.]?"),
    ("package_recovery",r"Previous\s+package\s+error\s+(?:was\s+)?resolved\s+during\s+recovery[.]?"),
    ("network_recovery",r"Connection\s+timed\s+out\s+earlier[;,. ]+\s*link\s+restored[.]?"),
    ("gpu_recovery",r"GPU\s+reset\s+completed\s+successfully(?:[;,. ]+\s*display\s+restored)?[.]?"),
))
def observation(line,priority=None):
    if priority is not None and (type(priority) is not int or priority<=3):return None
    if not isinstance(line,str) or len(line)>8192:return None
    for key,rx in RULES:
        if rx.fullmatch(line.strip()):return key
    return None
