# SysPulse

> **Current build and recovery (28 November 2026):** Active development is on [integration/alpha39](https://github.com/sudipdasin/SysPulse/tree/integration/alpha39), not this historical source branch. The first free AppVeyor hosted Ubuntu 24.04 amd64 build succeeded at `df76ac5ca220b2378f1b93c9059b70f990f48695` with 74/74 CTest suites and evaluation DEB/evidence upload. [Setup, manual build settings, artifacts and recovery](https://github.com/sudipdasin/SysPulse/blob/integration/alpha39/docs/ALPHA39_APPVEYOR_BUILD_SETUP.md). Installed signed upgrade, physical Mint acceptance and production release remain pending. No GitHub Actions, CircleCI or paid infrastructure.


Open-source native system maintenance, diagnostics and administration tools. Linux Mint XFCE (AMD64) first; other Linux distributions and native Windows 10/11 are roadmap targets requiring their own validation.

## Download the established native application

- [Live native DEB evaluation download directory](https://syspulse-qt-alpha3-build-eval.onrender.com/) — current candidate, checksum and build/installation reports. This is a mutable evaluation service, not an immutable official release repository.
- [Canonical GitHub Releases](https://github.com/sudipdasin/SysPulse/releases) — official distribution destination; no published assets at this checkpoint.
- [Established Qt evaluation source](https://github.com/sudipdasin/SysPulse/tree/build/render-qt-alpha3-evaluation) — native application and build configuration. Actual deployed commit must be checked against the deployment record, not assumed from branch head.
- [Mandatory package, update and parity policy](docs/DEB_UPDATE_RELEASE_POLICY.md).

Alpha.8 passed native compilation, 36/36 regression suites, 56 additional build checks and Ubuntu 24.04 container installation, then deployed live. Alpha.9 package lifecycle evaluation is in development. The Qt application preserves the established Precision interface, resource monitoring, process/service/startup controls, System Care, maintenance and reviewed system memory compression. Real Mint XFCE VM validation, production signing/updates and official Release asset publication remain outstanding. Do not install legacy X11 prototypes as the current product.

## Project documentation

- [Continuity policy](docs/CONTINUITY.md)
- [Developer handoff](docs/PROJECT_HANDOFF.md)
- [Source intake report](docs/source-intake/REPORT.md)
- [Implementation queue](docs/source-intake/IMPLEMENTATION_QUEUE.md)
- [Privacy policy](docs/source-intake/PRIVACY_POLICY.md)
- [Utility manifest](docs/source-intake/manifest.json)

GPL v3; see LICENSE. Future Pro development belongs in a separate repository and must comply with upstream licenses. No Pro code is included here.

## Historical experiments — not current installers

The 0.x X11 previews and source-intake prototypes remain in history. [Historical 0.3.0 preview](docs/PREVIEW_0.3.0.md), [0.1.0 preview](docs/PREVIEW_0.1.0.md), [0.4.0 control center experiment](docs/CONTROL_CENTER_0.4.0.md). They are not the established Qt application and must not be distributed as the current product. Do not use the older install-user script as an alternative to a real DEB.
