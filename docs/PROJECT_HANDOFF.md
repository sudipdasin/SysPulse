# Canonical development handoff — 2026-11-26

## Product
SysPulse Community is a collection of small standalone utilities and an optional unified launcher. Basic mode guides nontechnical users; Advanced mode exposes precise controls and diagnostics. Goal: lightweight native software, including low-memory PCs. Initial target Linux Mint XFCE (X11); add other Linux distributions and Windows 10/11 native backends with actual platform tests. No Wine dependency for the Windows edition.

## Existing foundation and migration
Tux Steward legacy repository: native C++17/Qt Widgets, documented 2.4.0-beta.1 source checkpoint; earlier verified binary release 2.3.0-beta.1 per legacy README. Existing catalog: diagnostics, storage, maintenance, performance, input inspection, gaming checks, schedules, experimental tools. Standalone task manager provides processes/tree/details, CPU/RAM/disk/network graphs, startup apps, services; bounded subprocess runner, explicit authorization, local export. Legacy README reports nineteen test suites, not independently rerun here. Do not claim these features are already ported. Inventory exact legacy commit, verify license and attribution, review every file including history-sensitive data, then migrate compatible code in isolated batches without copying raw history or CI workflows. Preserve legacy repo. Rebuild and test in new repo.

## Architecture
Shared C++ core: capability registry, typed actions, platform adapters, cancellation/deadlines, risk/permission model, audit and local export. Qt Widgets launcher and individual tools; normal-user GUI, OS-native authorization for narrowly scoped elevated work. Linux systemd/PolicyKit and distro-specific commands; Windows native APIs and narrowly scoped PowerShell where appropriate. No shell interpolation of untrusted input. No automatic mutation at startup. Prefer native platform facilities; fail closed on unsupported hardware/session. Installer packages, portable distribution and an optional signed update mechanism with explicit user control, verified manifests and rollback; never run unsigned downloaded code.

## Scope and milestones
M1: migrate verified legacy diagnostics and task manager, build CMake locally, native desktop smoke checks, portable standalone tools. M2: intake SP-001–012 in priority order, implement safe input, disk, memory, gaming, browser and schedule workflows. M3: Windows diagnostics, services, startup, storage and repair adapters; Windows VM tests. M4: accessibility, resource profiling, signed packages and safe update flow. Future Pro separate, no proprietary code in public repo and no assumption that GPL-derived code can be closed-source.

## Quality and delivery
Read source-intake report/manifest/queue every cycle; accept only reviewed IDs, track implementation commit/test evidence, and keep changelog. Semantic versioning, no fabricated tests, no final stable release without explicit approval. Use local/offline builds, disposable desktop VMs/storage images and human validation when necessary. No GitHub Actions or CircleCI. No automatic paid resources. Small 2–5 operation batches, optimistic ref check and read-back, isolate changes; no force-push. Keep all machine identifiers, secrets and raw diagnostics out of public repo.

## Intake routine
Computer Maintenance project Sources -> private inspection -> remove PII and secret material -> portable rewrite -> scan/test -> commit reviewed artifacts -> update report/manifest/queue -> development consumes accepted IDs and records verification. Hourly scheduling requested; activation must be confirmed separately. No raw attachments, real configurations, logs, personal paths, IDs, game records or private fingerprints. Exclude inaccessible or unreviewed data. No empty commits on unchanged inputs.
