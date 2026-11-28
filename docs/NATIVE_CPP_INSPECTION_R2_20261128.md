# Native C++ inspection milestone R2 — 2026-11-28

Independent SysPulse workstream only. Base R1 be6cdb4443e2e91a3f34c9aeb0ef5eb2c5b2e4b5, source/test checkpoint 43695bc170bfa1e691e9ce034ea322936eff18a6. R1 four local source/build blobs verified before changes. No change to existing Qt application, integration/alpha39, main, Master CI, releases or cloud providers.

## Completed implementation slices
- Framework-free Linux per-core CPU ticks, disk read/write sector and busy counters, and per-interface network RX/TX counters; counter delta validation, no overlapping disk aggregation.
- Read-only Linux process inspection with PID, parent, state, name, CPU ticks, start ticks and optional RSS pages; bounded input and enumeration, process disappearance tolerated, no signaling or privilege escalation.
- Evidence availability flags and simple memory-availability heuristic (not a fault classifier or authorization to change the system).
- Standalone native inspection executable with sanitized summary counts; no shell, GUI framework, background polling, network or system mutation. R1 Windows aggregate CPU/memory backend preserved, R2 Linux-only capabilities are unavailable on other platforms.

## Verification
Debian GCC 14.2.0, C++17, Release with -Wall -Wextra -Wpedantic -Werror, and separately Debug UBSan with -fno-sanitize-recover=all. Both builds succeeded, 2/2 CTest suites and 55 distinct checks (27 R1 + 28 new R2) passed on each; no double counting. Real Linux CPU/memory/core/network/process collectors and process cap tested; CLI manually executed. Release CLI 55216 bytes, static library 52142 bytes; one invocation peak RSS 1664 KiB, elapsed displayed 0.00 s at coarse time resolution. Standard C/C++ runtime dependencies only; not a whole-application benchmark or measured improvement over Qt.

All changed remote blobs matched tested local bytes:
- native-core/include/syspulse/native/inspection.hpp 97e242b34e61ba7e1948acec3890f162b9aa22df
- native-core/src/inspection.cpp 4019b282aae0b3231008d8900c3078ee024e9d53
- native-core/src/inspect_main.cpp c106a788e5b2ede288d1b2106717dcfeaab7f1f3
- native-core/tests/test_inspection.cpp 8070edcabedb3446a92e2743de42e6473d13b87f
- native-core/CMakeLists.txt 5d606ccbdd7e4d27a6be42e0cdcd82ce95834a9e

Reproduce: cmake -S native-core -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j2; ctest --test-dir build --output-on-failure; ./build/syspulse-inspect. Source ZIP, local test logs and SHA256 manifest delivered separately. No GitHub Actions/CircleCI, paid provider, deployment, release, installer or automatic integration merge.

## Limits and next work
Disk rows can overlap, process list is readable rows only, availability is not completeness. No full live rate sampling, cache/buffers, CPU pressure/thermal/GPU, disk capacity/SMART, Windows R2 adapters, advanced process controls, full diagnostic parity, native GUI or signed install/update yet. Thresholds are provisional read-only heuristics. Windows toolchain and physical Mint desktop not available in this run. Preserve alpha39 approved UI and full feature/safety baseline; R2 remains a development utility, not replacement.

Next batch: exact baseline differential fixtures for monitoring, monotonic sampled rate engine with device identity and reset handling, bounded mount/pressure evidence, process identity-aware safe planning, then native UI presentation and Windows backends with real platform tests. Record evidence and test before merge. Recover this topic HEAD and original native blueprint in future sessions.
