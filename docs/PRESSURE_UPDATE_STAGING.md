# Resource pressure and update staging checkpoint

This isolated integration branch combines the established Qt design and tool-discovery candidate, two locally tested new components, and the already verified website source from main. No replacement application or stylesheet change. No GitHub Actions or CircleCI. No live release or main promotion.

## Resource pressure
Linux PSI (CPU, memory, I/O) reader returns optional some/full averages over 10, 60 and 300 seconds and cumulative stall microseconds. Missing, malformed, duplicate or out-of-range data is unavailable; no invented zeroes. Read-only, no automatic tuning, no severity thresholds. Kernel support and permissions may vary. The live check verifies output shape, not hardware accuracy. Not yet connected to Observatory Qt graphs.

## Update staging
Combines the existing signed-package verifier and version/channel admission policy in a private temporary snapshot. Rejects nonregular inputs, symlinks, excessive sizes, tampering, untrusted key and rejected release versions. Cleans the snapshot on normal/exceptional context exit. The caller must independently authenticate trust fingerprint, release selection and installed version. This is not a privileged-safe installation handoff; same-UID hostile processes and hostile parent-directory races are outside its guarantees. No download, install, key provisioning or persistent replay protection. Do not pass user-writable files to root; future privileged installer must securely copy and independently reverify authenticated content in root-owned staging. Fixtures are synthetic, not actual DEBs.

## Actual local checks
Run `bash tools/test-offline-components.sh` as a normal user in a trusted checkout with GCC C++17, UBSan, Python 3 and OpenSSL. Executed in a temporary directory as nobody, all six suites passed: triage 8 checks, measurement 13, pressure 14, signed package 11 unittest methods, release policy 7, staging 19 (11 inherited fixture methods plus eight new staging methods). 72 reported check/method executions, 61 distinct cases/methods, including subcases grouped under methods. All eight inherited component file hashes matched the existing candidate before tests; four new source/test hashes and test harness compared to local Git hashes. No target Qt GUI compilation or installation. Qt 5/6 development files and QEMU not found; direct git fetch failed DNS. No network or system mutation in component tests. Full native Qt build, inherited CTest, actual Linux Mint/XFCE and package gates are required before main promotion. Existing advanced tool integrations and visual refinement remain unvalidated.

## Remaining priority
Secure a target-compatible Qt build environment; wire PSI into existing monitoring, add signed download/reverification and explicit package-manager approval/rollback, validate complete app and existing UI, update source-intake implementation status only when functionality is integrated and verified.
