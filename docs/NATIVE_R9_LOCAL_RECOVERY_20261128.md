# R9 native Linux administration recovery — 2026-11-28

Owner explicitly requested complete replacement, not more previews. Full replacement gate FAILED. R9 is an unsigned co-installable evaluation only. No Master CI changes. No GitHub Actions, CircleCI, paid services or production release. Remote native implementation remains R4; R5–R9 local source synchronization outstanding; this commit documents rather than merges source.

R9 local base d1494f6545ed991f4d8230bd4b645271da0c40fa; local commit dae438495fedab026d8600669486eb3a39f6eea5, tree 19433ceeadf075a49f8504f5015027ec9b58422e. Native C++17 X11, OpenSSL, json-c. Adds scoped user/system service start/stop/restart, Wi-Fi controls, power profiles, network evidence, journal errors, kernel warnings, firewall status and system timer inspection. Ports existing Alpha39 UDisks offline ext filesystem check/repair workflow: unmounted, writable, unique UUID/path/device number, exact revalidation, UDisks Block identity verification, failed consistency check before separately reviewed repair, backup warning and no forced unmount. Residual device identity race; selected critical service protection is not complete dependency/alias safety analysis. Prior R8 operations retained. No automatic maintenance.

Release, UBSan and compatibility package builds: 14/14 suites each, 52 administration checks, real read-only IP/route/socket/block inventory, ordinary-user disposable cache deletion, startup disable/restore, child SIGTERM. Extracted package payload verified, X11 wide/narrow smoke passed, ordinary-user GUI actual network query, Wi-Fi and power review/cancel, no-selection service restart and offline repair refusal, inherited package/TRIM review gates passed. No actual new privileged service/filesystem mutations, Wi-Fi/power changes or target Mint acceptance; missing desktop tools/services. Windows and signed updates not accepted. No full diagnostics/learning, SMART, desktop/input/gaming/scheduling/backup/security or professional native UI parity.

Artifacts delivered:
- SysPulse-Native-R9-Administration-amd64.deb, 868088 bytes, SHA256 4db879fe5b9a890f816cbad1034bea5883c24545fd67cc4f50e0d9c3a4c3bc4b, version 3.0.0~native.r9+preview1, installed size 6280 KiB.
- SysPulse-Native-R9-Source-and-Evidence.zip, SHA256 7f26bc025a7298a914454eff09591bfbc4ae69a3c041e7c35a34ef58a4f91256.
- SysPulse-R9-local-source.bundle, SHA256 abad3359112b42c2548b3857080737320ca5aa37ddb835e6825e65227c1a69bf.
- SysPulse-R9-from-R8.patch, SHA256 3d924024e15e639bfa891bae3645e06a255870c911aa5d003c1eeda58c1b157b.
- Full replacement parity gate report: SysPulse-R9-Full-Replacement-Status.md.

Direct Git DNS unavailable; source history locally committed and bundled. Keep Alpha39. Do not rename evaluations as production replacement. Actual target-system tests and migration of remaining existing engines are mandatory; do not count fixture tests as functional parity.