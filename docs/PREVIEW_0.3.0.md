# SysPulse 0.3.0 source preview

Native C++17 X11 read-only preview. Adds capability inventory (20 command checks in standard system paths only; presence does not prove functionality), aggregate Brave process count and RSS (shared pages may be counted twice), GUI page selection and CLI entry points. Browser matching uses process comm names and can miss renamed or inaccessible processes. Process scan is capped at 8192 directory entries and checks a 250 ms soft deadline between entries. No personal process identities or command lines are reported.

## Build and install
Install build-essential, cmake, libx11-dev, curl, openssl and util-linux with the distribution package manager. As a normal user, from a trusted source checkout run:

    bash tools/install-user
    "$HOME/.local/bin/syspulse"

The installer builds in a temporary directory, runs CTest, checks per-user target directories, refuses root, locks against concurrent updater operations and installs executable plus helper. It creates a desktop entry for supported home paths and default XDG data location; custom data paths skip menu registration. Install is not transactional: stop any running SysPulse first, and retain a backup if replacing an existing installation. No administrator tasks or dependency installation occur automatically. CLI: --diagnostics, --capabilities, --browser-summary, --version.

## Evidence
Debian GLIBC 2.41 test environment, C++ Release build; seven CTest suites passed as nobody, including memory parser, signed updater fixtures, inventory parser and both new CLI modes. Xvfb two-second startup smoke check returned expected timeout; no real desktop interaction test. Installer rejected root and successfully built, tested and installed into an isolated user home, including executable, helper and desktop entry. No Mint-compatible prebuilt binary or official release is asserted. Native source and test files compared to Git blob hashes before branch commits. No GitHub Actions or CircleCI.

## Update and delivery
Signed update helper still requires a separately authenticated owner public key and actual release assets; live installation not verified. Helper itself is not automatically updated. See PREVIEW_0.1.0.md for release protocol and PREVIEW_0.2.0.md for hardening. Source preview only; full Qt migration, actual Mint validation, Windows, and remaining utilities pending. Do not publish original private references.
