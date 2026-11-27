# Alpha.39 integration recovery — 2026-11-28

## Canonical working line
Integration branch: integration/alpha39. Resolve its actual remote head before each batch; never infer head from this document or an old chat. Starting reviewed application source: 4007b5b21a55e46385e7ddb611b6a5aa968a30fe (dev/alpha39-environment-preparation). Main: 51dfc5e57db52a34d31178157069f6f855ceef8d. Alternate triage: 5a2853b6ba1d3b9b2015eaeda2238837e644398d.

Live comparisons: main diverged 413 development commits / 1 main-only commit (README correction). Alternate triage diverged 87 development commits / 11 alternate commits from Alpha.38 803ba8600963f4d520a02775e7d29b3b4f49efd8. All fourteen other Alpha.39 topic heads are ancestors of the reviewed application line. No destructive rebase, force push, deletion, or production promotion.

## Explicit merge resolution
The integration merge retains the reviewed application tree. Reconciles README with main's established native Qt migration, Linux Mint first, distribution policy, privacy and historic-preview warnings; supersedes stale Alpha.9/25/27 and mutable live-download claims. Main is not changed.
Alternate triage's seven changed paths reviewed by role: tools/syspulse_package_triage.py and tests/test_package_triage.py are historical competing implementations; their functionality is superseded by native_probes, integrity and strict package_evidence modules and corresponding tests. Keep their original commits reachable as merge parent, do not install a second parser or register obsolete schemas. CMakeLists.txt, tools/syspulse-diagnostics and src/syspulse_window.cpp retain newer implementation (read-only APT preview and core integrity tabs, mandatory initial/fresh gates); tools/build-alpha39-local.sh retains newer pinned recovery/observability builder. Preserve alternate's original evidence doc verbatim. This is a deliberate resolution of overlapping changes, not a blind union; previous evidence does not validate native GUI compilation. No runtime code changed by integration merge.

## Recovery protocol
1. Fetch exact integration head and compare against previous checkpoint. Work in separate topic branches from that head; use small batches and test before integration.
2. Maintain actual commit, branch, source/artifact digests, tests, skips, blockers and next gate in durable docs. Use explicit source-only commits with [skip ci]. Never run GitHub Actions/CircleCI, no paid services.
3. When direct workspace access fails, use connector read/write for source and local disposable fixtures, never claim a partial source subset is a full checkout. Do not discard dirty work, reset existing branches, or overwrite user workspace.
4. Merge reviewed branches into integration only after applicable checks; keep stable main and release feeds unchanged until full native/installed acceptance and authorization. Preserve alternate histories, existing UI/features and trust.
5. Owner device previously fetched pinned source and passed 286/287 offline checks (one skip); updated source not yet retested on owner device. Sandbox apt/DNS and Qt development blockers remain; Render free quota unconfirmed. Use owner-supported Mint/Ubuntu or an independently verified free native environment.

## Required gates
Run full offline diagnostics and native Qt CTest; benchmark replay, real DEB payload verification, disposable signed installed upgrade/tamper/recovery, physical Mint XFCE and SMART acceptance. No production release or version increment before these. Current preparation launcher intentionally pins earlier reviewed application tree; documentation-only merge does not change runtime.
