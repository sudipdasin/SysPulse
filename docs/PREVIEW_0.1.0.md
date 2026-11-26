# Native preview 0.1.0 — 2026-11-26

This is a new C++17/X11 proof of concept, not the full Tux Steward migration or a stable release. It offers read-only RAM, swap, filesystem and logical CPU snapshots and explicit check/install signed-update controls. X11 was selected for this preview because Qt development headers were unavailable in the test environment. Qt migration, accessibility, Wayland, Windows, real XFCE testing and the remaining source-intake IDs are pending.

## Build on Linux Mint/XFCE
Install build dependencies (review the package manager prompt): sudo apt install build-essential cmake libx11-dev curl openssl util-linux
Then run, as your desktop user:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j2
    ctest --test-dir build --output-on-failure
    cmake --install build --prefix "$HOME/.local"
    "$HOME/.local/bin/syspulse"

The updater and executable must be installed together in the per-user bin directory. Never run the GUI or updater as root. The GUI reports basic operation state; detailed update output is on the launching terminal. No automatic downloads on startup.

## Release update contract
For Linux x86_64 only, the helper reads the canonical repository's latest stable release download URLs for syspulse-linux-x86_64.manifest and syspulse-linux-x86_64.manifest.sig. The manifest is exactly two lines: version=MAJOR.MINOR.PATCH and sha256=lowercase SHA-256 of the binary. The release tag must be vMAJOR.MINOR.PATCH and contain syspulse-linux-x86_64. Sign the exact manifest using an owner-controlled offline release signing key (openssl dgst -sha256 -sign PRIVATE_KEY -out MANIFEST.sig MANIFEST). Never commit a private key. Enroll the public key through a separately authenticated channel at the user's config directory syspulse/update-public.pem; no automatic trust-on-first-use. The release must be independently signed and tested on the target platform before publishing.

The updater verifies manifest signature, rejects non-newer versions, validates downloaded binary digest and reported version, locks the installation directory, backs up the installed executable, and atomically replaces it. Restart manually. It does not update its own helper, external dependencies, system-installed packages or Windows editions. Rollback is manual via the retained previous binary. No live signing key or released assets exist yet; updater intentionally refuses without enrolled trust. Local fixture signing keys were temporary and excluded from all artifacts. No stable release is authorized.

## Evidence
Local CMake Release build with GCC 14.2, three CTest suites passed as a non-root user, including temporary signed update fixture, missing key, corrupted binary, signed installation, same-version rejection and tampered manifest. Xvfb launch remained running for a two-second smoke check; no real XFCE or physical hardware validation. No GitHub Actions or CircleCI. Exact Git blob comparison performed for CMakeLists.txt, src/main.cpp, tools/syspulse-update and tests/update-test.sh. SP-001 has a partial native preview; other intake implementations are pending. Actual remote release fetch/install not verified.

## Next work
Port legacy Qt GUI and task manager in isolated reviewed batches; expand diagnostics; implement disk/input safeguards; add complete signed bundle update with release-key rotation, compatibility checks, rollback UI, proper version/status presentation and release asset publication. Do not claim production readiness.
