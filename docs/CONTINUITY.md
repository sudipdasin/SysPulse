# SysPulse continuity and migration correction

The established SysPulse 3.0.0-alpha.2 Qt code lives at https://github.com/sudipdasin/tux-steward/tree/0105b57667f79129765c56e3f1654cd91c42cf3a . The separate X11 previews in this repository are historical experiments, not the approved product. Stop their development. Preserve their history without making them the release path. The canonical repository remains SysPulse. Existing Tux Steward repository is read-only for this migration.

## Preserve
Precision white/crimson style, Observatory performance tabs, Forge startup/service tables, persistent sidebar, global search, overview, storage, network, desktop, security, schedules, recovery, reports, experimental and settings pages. Use the existing Qt action catalog, bounded runner, reviewed PolicyKit actions, health/report dialogs and process controls. Preserve all inherited capabilities and tests. No arbitrary root shell or stored password.

## Implement within existing panels
- Desktop: settings, appearance, input, power, login/display managers.
- Storage/recovery: disk and partition management, SMART, offline filesystem repair and data recovery with separate recovery target; Timeshift and personal backups.
- Management: users/groups, printers, drivers, services and startup; platform-specific capabilities.
- Performance: measured memory/zRAM optimization, reversible changes, cache preview; no blind cache dropping.
- Software/security/network: package and OS updates, package repair, software manager, network and firewall; least privilege, explicit authorization.
- Diagnostics: classify actual evidence, unknown state on missing probes; plan/preview, backup, execute, verify and rollback where supported; do not claim repair on command exit alone.
- Update: signed complete bundle or distribution package via authenticated release channel, not only one binary; preserve installed app and verify rollback. Real target tests required.

## Gates
Import source and tests from pinned revision with required license/attribution, excluding raw attachments, secrets, private machine data, backups and CI configs. Use small isolated branches; no GitHub Actions/CircleCI, no paid resources. Run full native Qt build, existing CTest regression suite, offscreen and XFCE VM tests on target compatible distro; verify packaging and in-app update before release. Do not merge incomplete or untested migration into main. No new UI direction. Previous X11 previews are not a substitute. Update intake report and manifest after verified integration.
