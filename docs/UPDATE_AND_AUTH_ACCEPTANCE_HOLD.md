# Owner acceptance hold — 2026-11-27

The owner will not install another SysPulse candidate until both signed in-app updates and session-only administrator authorization are fully functional. This overrides routine requests to supply intermediate installers for local testing; keep internal evaluation artifacts available for engineering, but do not recommend or request installation of another candidate before these gates pass.

## Gate A — real online application update
- Release operator provisions a protected production signing key, package-managed verified public trust anchor, canonical immutable release and live signed channel metadata.
- On an ordinary user session, actual application detects a newer real online release automatically after opt-in, authenticates signed metadata, downloads and validates the full DEB, obtains explicit approval, installs via the authorized session and verifies the installed version after relaunch. No manual key handling or repeated DEB download for provisioned installations.
- Reject tampered or expired metadata, wrong signatures, modified package, downgrade, wrong channel, unavailable feed and interrupted transfers; retain preferences and verify recovery. Never use disposable signing keys as production trust or bypass signatures.
- Verify an authenticated one-time migration route from legacy builds, accurately disclosing if the installed version cannot bootstrap itself. No claim that a cached-only updater can install new code.

## Gate B — session administrator authorization
- Normal-user GUI, optional native PolicyKit authentication once, no password stored or read by SysPulse; reuse restricted authorization for all supported internal privileged operations, including updater, protected volume scan and service management in addition to the maintenance catalog. Preserve explicit operation review.
- Verify real desktop authentication, multiple distinct authorized operations and an approved update without a second authentication prompt; explicit lock and app closure revoke access, a new session starts locked. Test denied authentication, interrupted transport, invalid arguments and unsupported commands, no replay of uncertain mutations, and fail closed.
- Third-party tools retain their independent OS authorization policies; do not imply SysPulse can suppress their prompts.
- No unrestricted root GUI, shell, global sudo caching or relaxed system permissions.

Both gates must pass together on Linux Mint 22.3 XFCE or a demonstrably equivalent target desktop with the actual production feed and authentication agent. Fixture-only test results are not completion. Continue reliability work internally; no new feature expansion until these gates pass. No GitHub Actions, CircleCI or paid infrastructure. Stable publication still requires owner approval.

## Evidence at hold
Alpha.25 commit 6496b06c0cf69692ccdf5f3cbaa226c64196a03f is live as an evaluation deployment. 45/45 native suites passed; disposable runtime verified two real catalog operations over one broker, EOF revocation and invalid request rejection. No canonical releases or update channel branches were found. Production online update and complete authorization reuse remain unverified. Do not recommend alpha.25 installation as satisfying the hold.


Owner supplied the verified Alpha public anchor. The key bootstrap input is complete; package integration, owner-side release signing, canonical feed publication and real desktop session acceptance remain required.

Alpha.26 integrates the remaining typed internal routes and enables the owner Alpha public anchor in the evaluation build. The build is a signing candidate only, not a qualifying release. The actual private-key signature and real desktop acceptance remain independent requirements.

Owner completed publication: canonical v3.0.0-alpha.26 release at ff216acf7481f2d1e79e4dc9cdfd4dcb9ea9ff36, signed feed branch commit 521e2ab2c65c5dddd745a5d7c9ae51ed60a6097b. Public InRelease independently verified with original uploaded public key; signed Packages digest and canonical asset digest match. Metadata expires 2026-12-04 02:19:32 UTC. Added disposable real HTTPS upgrade test via installed broker; no target desktop or successive real release acceptance implied. No user installation recommendation.

First live test: canonical package download and exact checksum verified, but installed unprivileged check exited 1; no successful online upgrade claimed. Focused diagnostic build records actual APT read-only error and uses only canonical release bytes. Earlier full evaluation evidence remains attached to the prior deployed commit.

Focused diagnosis established HTTP 307 from raw GitHub for APT's /./InRelease path, with redirects intentionally disabled. Use exact flat suite / (verified apt-get --print-uris) and isolated early APT_CONFIG to prevent distribution hooks in ordinary-user discovery. Increase maximum authenticated metadata age to 30 days for new client releases; Alpha.26 retains seven-day cap. Focused diagnostic uses corrected source in disposable baseline only, not a released fix. Production key remains protected, no signing bypass.

Canonical flat suite now successfully acquires signed InRelease and Packages. Previous focused test used a logically newer synthetic baseline: Debian's tilde sorts before end-of-string. Corrected to a strictly lower Alpha.25 Debian version for the diagnostic-only repack and assert ordering with dpkg before starting. No claim of real earlier release or unpatched Alpha.26 support.

Live signed feed diagnostic PASSED at 07a135aa00670bd66591a3d47860cfa02346ad0c: real owner-signed Alpha.26 discovered and installed through the same root broker after APT cache cleanup, then protected volume scan, preference preservation, current-version check and EOF revocation. The baseline was a disposable repack with the proposed corrected helper (not a real released older version); the final current-version test reapplied that helper. Alpha.27 packages the fix, 30-day age cap and tests upgrade from the exact canonical Alpha.26 package. No user installer recommendation until genuine successive online upgrade and target desktop authentication gates pass.
