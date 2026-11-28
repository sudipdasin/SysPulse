# SysPulse R7 local recovery — 2026-11-28

Application work only. R6 archive recovered and checksum manifest verified. R7 development package and source/evidence delivered in conversation. Remote native source remains R4 69fae0af9ad3af7fe0f79b8382f0a8fc3c3b4175; R5–R7 source synchronization is not complete. This is a documentation checkpoint, not a source merge.

R7 adds bounded direct C++ subprocess execution, dpkg audit, PolicyKit-reviewed APT autoclean simulation and separate confirmation for cleanup, package-list refresh, and selected user-session systemd service start/stop. No shell wrappers. Earlier startup, cache and process operations retained. APT simulation initially failed under ordinary user because archives/partial was restricted; corrected by requiring authorization for simulation and separate cleanup review. Simulation gate expires after 60 seconds, action review after 30 seconds. No privileged mutation or real user-service mutation accepted: pkexec and live desktop service manager unavailable here.

Release, UBSan and static runtime compatibility build each passed 9/9 CTest suites; ordinary user runner 17 checks, inherited R6 mutation fixtures rerun, actual read-only dpkg audit and root APT simulation passed. Exact extracted package X11 smoke and maintenance navigation/audit click rendered, payload manifest and SHA256 verified. No physical Mint, Windows or production acceptance.

Artifacts:
- SysPulse-Native-R7-Maintenance-Candidate-amd64.deb, 611128 bytes, SHA256 f839765c6a65142583bda8226d7058c2e049576212d0f9f0807b4dd6fd3082a2; package syspulse-native-preview, version 3.0.0~native.r7+preview1, installed size 3764 KiB.
- SysPulse-Native-R7-Source-and-Evidence.zip, SHA256 085fe864627e69b35bdde92822af5ec73bb9c9fc2f34ebecc78aba48e116cca9.

User explicitly requests COMPLETE replacement now, not another preview. Do not label R7 complete. Preserve Alpha39. Missing parity: full package repair/upgrades, system-wide services/memory optimization, SMART/SSD/filesystem recovery, network/security/desktop/power/gaming/schedules, complete diagnostic intelligence, professional accessible GUI, native Windows, signed updater and rollback. Priority is exact baseline feature inventory and port of working components, actual isolated execution tests, source synchronization and full feature parity before replacement. No Master CI changes, paid infrastructure, GitHub Actions/CircleCI or production release.
