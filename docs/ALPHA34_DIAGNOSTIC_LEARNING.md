# Alpha.34 intelligent diagnostics and adaptive Boost

Owner directive 2026-11-27: Analyze System is the mandatory precursor to Boost. Local diagnostic evidence and historical action outcomes inform recommendations; fixed safety policy always takes precedence.

## Implementation

- tools/syspulse-diagnostics: non-root, read-only bounded probes of 37 fixed sources (journal system/user/kernel, dmesg, failed service state, dpkg audit, core dumps, hardware/network/mount inventory, current/rotated/compressed syslog/kernel/auth/APT/dpkg/Xorg/session logs). No claim of exhaustive logs: 1 MiB and 6000-line budgets per source, recent journal windows, explicit coverage states and permissions. Raw messages discarded; per-source categorized observations may overlap, not unique incidents.
- Metrics: RAM/swap, VM counters, load, PSI, storage free/total/inodes/read-only/device identities, temperature and aggregate hardware/process inventory when readable.
- Private SQLite database at ~/.local/state/syspulse/diagnostics/history.sqlite3 (180 sessions); schema version 1, foreign keys, immediate transactions, no startup collection, no telemetry, no raw logs. Retain analysis, findings/coverage, current/previous measurements, action-specific receipts and recurrence over up to ten analyses. Clear history through explicit GUI confirmation.
- Rule model: fixed diagnostic classifiers, context-specific action outcomes, demote defaults after >=3 attempts and at least half failure/no-effect. No autonomous command generation, privilege escalation, kernel parameter changes or self-modifying code.
- Boost: require latest unconsumed analysis, same boot, <=15 minutes by wall and boot time, approved action IDs, live caller PID/start identity and fresh kernel/package/storage checks for cache removal. Existing native bounded thumbnail/archives safety checks remain. Critical storage/kernel/thermal conditions and unknown kernel visibility prevent cleanup. Reserve one-use plan atomically; record actual action outcome and before/after measurements. Interrupted reservations are not counted as success. User review remains mandatory.
- GUI: analysis review, findings/coverage/measurements/recommendations tabs, recurrence counts, history and clearing, Boost buttons locked until analysis, advanced legacy diagnostic checklist still accessible separately.

## Evidence and open gates

Native helper 28/28 tests and actual non-root CLI collection/database smoke passed at no-secret reusable validation service deployment dep-dc4m22mi0phs73aoue9g (37 sources, real SQLite integrity and permissions, same-caller reservation and simulated no-effect receipt; no maintenance action in smoke). Tested helper blob f1fb8e5bcc2a770898c15f454ab68fd7e9939c27. Native C++/Qt build and integrated GUI/installation tests separate; no claim yet. Physical Mint acceptance, privileged log/SMART coverage, expanded issue remediation and adaptive outcome validation over real workloads pending. Automatic repairs are not permitted from log strings. No new production signer, key, feed changes, CI or paid resources.
