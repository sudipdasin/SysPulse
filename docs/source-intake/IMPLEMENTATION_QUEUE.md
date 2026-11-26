# Implementation queue

| Priority | Utility IDs | Acceptance criteria |
|---|---|---|
| 1 | SP-001–003 | Compare legacy catalog; port missing probes with argument-array runner, timeouts and cancellation; omit raw output by default; distinguish unavailable, failed and completed queries. |
| 2 | SP-004 | Exact device selection; X11 support, external input verification, timed recovery; no last-input disable or broad ignore rules. |
| 2 | SP-005–007 | Detect target and children; refuse mounted/in-use targets; distinguish standby/sleep; supported TRIM; offline repair only, dry-run, backup, disposable-image tests. |
| 3 | SP-008, SP-011 | Detect current settings; benchmark; preserve security updates; preview, scoped config backup, verification and undo. No universal tuning guarantees. |
| 3 | SP-009–010 | Match distribution/release and trusted packages; isolated Wine prefixes on compatible filesystems; no global process kills; atomic registry, safe scoped removal; protect game files; truthful failure reporting. |
| 4 | SP-012 | Individual action preview; no cross-user deletion; preserve current/fallback kernels; deduplicate timers, undo and verify schedule. |

All IDs remain pending application integration. Use separate branches and focused local tests. Do not trigger GitHub Actions/CircleCI. Windows backends require Windows testing.
