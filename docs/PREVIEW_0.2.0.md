# SysPulse 0.2.0 source preview

Development checkpoint, not a production release. Retains the native X11 diagnostics preview and corrects RAM availability and swap usage from Linux /proc/meminfo. Missing and invalid counters are reported as unavailable. Updater rejects writable or symlinked key directories, unsafe installation parents, empty and oversized release assets. Existing signed manifest, checksum, version verification and atomic executable replacement retained.

Build and install as normal user, following [0.1.0 build and update instructions](PREVIEW_0.1.0.md). The updater currently replaces only the executable; install the new helper manually with this source preview. No official signing key or published release assets have been configured, so live updates are not ready. Never bypass verification. No GitHub Actions or CircleCI used. 

Local evidence: CMake Release build succeeded on Debian with GCC 14.2; four CTest suites passed as a non-root user (version, diagnostics, updater, memory parser). Synthetic cases include missing/negative or inconsistent memory counters, malformed units, writable trust directory, oversized manifest, missing key, corrupted binary, successful signed update, same version, and tampering. Xvfb GUI startup remained running for two seconds; real desktop interaction and Mint hardware compatibility remain unverified. All six changed code/test/build blobs matched local Git hashes before merge. No personal attachments or production credentials included.

SP-001 remains partial; the legacy Qt migration and remaining intake IDs are pending. Keep using the source-intake report and manifest. No stable release or signed release asset is published.
