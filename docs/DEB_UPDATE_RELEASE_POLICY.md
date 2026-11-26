# Mandatory XFCE distribution and update policy

Owner requirement: always deliver a real .deb for the established SysPulse Qt application on Linux Mint 22.3 XFCE amd64, capable of updating the entire installed application from the canonical repository (https://github.com/sudipdasin/SysPulse). Applies to every distributable preview, beta, release candidate and stable release. Source-only development checkpoints are not releases. No substitute X11 prototype, empty package, unsigned bootstrap or shell installer presented as the required deliverable.

## Distribution
- Build on a compatible target VM, use the native CMake/CPack configuration and derive actual dependencies. Package the application, required helpers, menu entry, icon, documentation and updater components. Preserve user preferences and existing features.
- Run the complete inherited and new native tests, real desktop startup and package install/upgrade/uninstall checks. Record exact source commit, target, Debian version, SHA-256 and evidence. Sign and publish immutable assets on the canonical release channel, with a verified direct DEB download. Do not label a development checkpoint as a completed release if any gate is missing.
- No GitHub Actions or CircleCI. No paid resources or stable release without owner approval.

## Full application updates
Preferred distribution design: a signed, dedicated APT repository synchronized with canonical release artifacts. Enroll an independently authenticated public signing key using scoped Signed-By trust; verify signed repository metadata and package hashes. Never use trusted=yes, apt-key, unauthenticated packages, arbitrary root shell, or a mutable URL as sufficient trust. Keep the private signing key outside source control and test keys out of production.

Existing Qt Settings/Software panel should show installed version, latest eligible version, channel, check result, release information and update status. Enable automatic update checks with a clear preference and disable control; actual installation uses the system package manager and explicit administrator authorization, or separately configured opt-in automatic installation through the operating system. Stable channel by default for stable installations; previews require an explicit preview channel. Never overwrite a dpkg-managed executable directly. Use the full signed package, reject unauthorized downgrade or channel crossing, handle failed/interrupted updates truthfully and preserve configuration. Provide a supported recovery path using trusted retained packages and documented limitations; never promise transactional rollback if unavailable.

An authenticated signed release-package workflow may be used as an alternative to APT repository hosting only if it provides equivalent trust, immutable staging, downgrade/replay protection and package-manager integration. Existing offline verifier/staging components are a foundation, not a live updater.

## Release gate
Verify actual published asset, trust enrollment, live version discovery, tamper rejection, update from the preceding installed release, settings preservation, denied/cancelled authentication and interruption recovery. Run this on the intended XFCE target, not only with mocks. Report real test results and unresolved blockers. Do not distribute a package as compliant until automatic checks and complete-package update path actually work.

Current checkpoint: no official releases returned by the canonical release listing; local build environment has no Qt5Widgets development metadata or QEMU and no GitHub DNS resolution. No verified DEB or active signed repository is available in this session. This document records the requirement, not an implementation or completed release.
