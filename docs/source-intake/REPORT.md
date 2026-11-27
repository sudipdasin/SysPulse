# SysPulse source intake report

Batch intake-001 — Linux references. Three standalone read-only Bash prototypes are included; SP-001 native integration partially implemented; remaining native integration pending. Nine other utility specifications are queued. No raw attachments, real configurations or logs included. Windows is planned, not implemented.

| ID | Capability | Intake outcome |
|---|---|---|
| SP-001 | System diagnostics | Read-only prototype |
| SP-002 | Capability detection | Read-only prototype |
| SP-003 | Browser process summary | Read-only prototype |
| SP-004 | Input device control | Specification |
| SP-005 | HDD maintenance and power management | Specification |
| SP-006 | SSD maintenance and TRIM | Specification |
| SP-007 | Filesystem repair | Specification |
| SP-008 | Low-memory/XFCE optimization | Specification |
| SP-009 | Gaming dependencies | Specification |
| SP-010 | Wine game and prefix management | Specification |
| SP-011 | Browser cache and launcher configuration | Specification |
| SP-012 | Scheduled maintenance | Specification |

Privacy: clean-room prototypes, no private identifiers or original logs. Machine-specific notes inform designs only. Known-identifier scanning is limited, not guaranteed anonymization. Testing: smoke checks on staging prototypes cover syntax and synthetic privacy omission; native SP-001 preview built and local CTest passed; physical desktop/hardware and remaining C++ integration not yet tested. No claims of Windows testing. Hourly scheduling remains blocked pending available task capacity. Keep this report current alongside manifest and queue; record real test/commit evidence on each batch. See implementation queue for acceptance criteria.

Native milestone: [0.1.0 preview](../PREVIEW_0.1.0.md), [implementation status](IMPLEMENTATION_STATUS.md). Signed-update fixture tests passed; no actual published release or enrolled key. Existing intake IDs preserved.

### Native milestone 0.3.0
SP-002 and SP-003 have native read-only GUI and CLI previews; SP-001 remains partial. Seven combined CTest suites passed, Xvfb startup and isolated non-root installation verified. Real Mint/XFCE interaction and live signed-release delivery remain pending.

### Alpha.16 diagnostic expansion (source checkpoint)
20 read-only catalog entries extend existing SP-001/SP-002 diagnostics and inspection aspects of SP-004–009/SP-012. This does not establish completion of device disabling, filesystem repair, Wine setup, browser configuration, maintenance scheduling or production updates. Exact source checkpoint: ef99e6fd7cd136c46780e63b420f4567e55f8c6e. Native/package test evidence must be recorded separately; no raw attached scripts or machine metadata incorporated.

### Alpha.17 reliability freeze
Cleaner scope and existing action availability hardening only. No new intake ID claimed complete. Real diagnostic execution and Quick-mode regression gates added; native/package verification required. Source checkpoint 6b9cd9a3f09441636d6a0f785332ec187d2b27bd.

### Alpha.18 reliability checkpoint
No intake IDs promoted. Alpha.17 verified live: 40 CTest suites passed; five portable diagnostics exit 0; block inventory exit 1 remains unresolved. Alpha.18 stale-state and diagnostic outcome corrections plus target-runtime Cleaner regression gate. Native and lifecycle validation pending. Source 833d4634f813d7f97acf6c18654e1dad04cca42f.

### Alpha.19 reliability checkpoint
Alpha.18 Render deployment confirmed live, full native tests and install/runtime gate completed per deployment; public endpoint timed out on recheck. Alpha.19 addresses existing dependency remediation and stale settings index; adds disposable virtual X11 functional gates. No new intake IDs promoted. Source 673ba79f26df3ebf28dc22539cc2359582d96d5f. Native and lifecycle tests pending.

### Alpha.20 diagnostic consent source checkpoint
Owner requested opt-in diagnostic feedback. Implements fixed schema, consent dialog, local audit import and analysis, safe export, disabled-by-default HTTPS transport. No personal reports committed. Relay and native validation pending. No intake completion claimed.

### Alpha.21 reliability checkpoint
Synthetic regression for configured relay, actual issue receipt contract, modern XFCE default-applications launcher, fixed-path APT and Lutris probe, degraded systemd classification. No user reports or machine facts committed. No safety bypass. Direct publication blocked until the operator provisions an issues-write credential. Native and target runtime gates pending.

### Alpha.22 signed-update source checkpoint
Native scoped APT updater, explicit channel preference and per-update PolicyKit authorization. No signing key generated or enrolled without owner trust. Production feed publication and physical target validation pending. No intake IDs promoted.

Alpha.22 build gate exercises the actual updater helper against the previous installed package and signed disposable repository, not merely raw APT commands. Signed production feed remains separate.

### Alpha.23 startup and Cleaner checkpoint
Fix pre-parent visibility of dependency buttons causing transient top-level windows. Add mounted local filesystem selector with explicit whole-volume budgets, default custom folder, permission vs mount exclusion counters and same-device directory alias detection. Normal-user inspection only; no elevated arbitrary deletion. Existing APT and archived-journal cleanup routes retain explicit administrator approval. Full regression and Mint desktop validation required. No intake IDs promoted.

Alpha.23 administrative scan candidate: PolicyKit-authorized fixed helper, exact local mount root and fixed budget only, no file-content reads, no delete operations, bounded metadata result. Root scan does not create deletion candidates or compare duplicates. Same-mount traversal checks Linux statx mount ID where available, with directory inode alias fallback. Not a raw disk/partition recovery scanner. Root scan and X11 startup tests required before distribution.
