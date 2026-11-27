# SysPulse Community

Open-source native system maintenance, diagnostics and administration. Linux Mint XFCE (AMD64) first; other Linux distributions and native Windows 10/11 require independent validation. Canonical source repository: SysPulse. Preserve the established C++/Qt Precision white/crimson UI, Observatory and Forge; the original Tux Steward source is read-only.

## Current development

**Alpha.39 integration candidate — not a production release.** Continue development from [integration/alpha39](https://github.com/sudipdasin/SysPulse/tree/integration/alpha39). Do not use the old main branch or historical preview branches as a development baseline. Read [integration recovery and release gates](docs/ALPHA39_INTEGRATION_RECOVERY.md) before modifying source.

Owner-supplied laptop transcript at exact commit b6a7cb1e9ec77271fb8c103574eca640e78da701 records 286 passing distinct checks, one expected skip, zero failures, and repeated native UBSan checks. Subsequent updater and builder changes have focused fixture evidence only. Full native Qt compilation, actual Alpha.39 DEB, installed signed upgrade, physical Mint acceptance and release approval remain outstanding.

## Build and distribution

- [Canonical releases](https://github.com/sudipdasin/SysPulse/releases): do not assume a published Alpha.39 installer exists.
- [Mandatory package, update and parity policy](docs/DEB_UPDATE_RELEASE_POLICY.md)
- [Environment recovery and pinned builder](docs/ALPHA39_ENVIRONMENT_PREPARATION_CHECKPOINT.md)
- [Owner diagnostic acceptance](docs/ALPHA39_OWNER_DEVICE_DIAGNOSTIC_ACCEPTANCE.md)

On supported Mint 22.x/Ubuntu amd64, with build prerequisites installed, run the local builder as a normal user with an exact reviewed commit:

```bash
bash tools/build-alpha39-local.sh --check
SYSPULSE_BUILD_JOBS=1 bash tools/build-alpha39-local.sh --build FULL_REVIEWED_COMMIT
```

No hosted CI or paid infrastructure. No unverified release publication. Preserve public trust anchors; no arbitrary root shell or stored passwords.

## Project documentation

- [Continuity policy](docs/CONTINUITY.md)
- [Developer handoff](docs/PROJECT_HANDOFF.md)
- [Migration status](docs/QT_MIGRATION_STATUS.md)
- [Approved UI](docs/SYSPULSE_UI_SPECIFICATION.md)
- [Privacy](PRIVACY.md)
- [Source intake report](docs/source-intake/REPORT.md)
- [Offline VM verification](tools/vm/README.md)

The 0.x X11 previews, source-intake prototypes and earlier mutable evaluation services are historical experiments or checkpoints, not current installers. Do not distribute them as the established application. Preserve historical source and releases.

GNU GPL v3; see LICENSE and upstream source attribution. Pro work belongs in a separate repository under applicable upstream licensing.
