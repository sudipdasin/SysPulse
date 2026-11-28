# SysPulse native runtime R3 — 2026-11-28

Independent SysPulse development only. Base R2 91fa5e8538c4e694ea8f5bc6d7d3fa54438b8d48, exact tested implementation f39e8675cb8ad88a94619d93289340bdc6d4582c. Existing Alpha39 Qt application, its responsive Overview candidate, main and integration untouched. No changes to Master CI.

## Implementation and reuse

- Reuse R1/R2 native collectors and validated CPU/counter deltas, Linux procfs/sysfs interfaces and documented kernel Pressure Stall Information format (https://docs.kernel.org/accounting/psi.html). No third-party source copied, no new application framework. Native C++17 and standard library only in this batch.
- Monotonic timestamped CPU/memory/per-core samples; no rate for stale intervals above 30 s, counter reset, duplicate core identity, or new core without baseline. Aggregate history retains at most 600 rows and no per-core allocations. Aggregate CPU and per-core data derived from the same proc/stat read.
- PID plus process start ticks for CPU rates and read-only identity review. Missing, changed, duplicate or protected identity returns explicit state. No process mutation; this is not action authorization or PID-reuse-proof signaling. A future executor needs fresh revalidation and platform handle/pidfd where supported.
- Per-device throughput using sysfs diskseq before and after stat reads; reject different/missing identity or regressing counters; sectors converted to 512-byte units. Network ifindex/iflink checked before and after reads and across samples, best-effort only (indices can be reused). No aggregation across overlapping disks/partitions/stacked devices, no claim of atomic multi-device snapshots. Bounded 256 examined rows per category and 256 collected rows overall; cap may omit network rows after many disk rows. Unsupported/hidden devices omitted rather than zero-filled.
- Bounded PSI reader for CPU, memory and I/O some/full rows, with malformed/nonfinite/overflow/duplicate rejection. PSI was unavailable in current container; fixture tests and actual unavailable path passed. System CPU full not interpreted as actual CPU stall.
- Standalone syspulse-sample executable: one real one-second sample, per-device readings and PSI availability, no background scheduler or destructive operations.

## Course corrections

First iteration passed 87 checks. Before final integration, replaced separate aggregate/per-core baselines with same proc/stat read, limited retained history to aggregate rates only, and added device identity-aware rates rather than aggregating disk rows or matching by device name alone. Final integrated result is 100 distinct checks; do not add intermediate or sanitizer repetitions.

## Test evidence

Debian GCC 14.2.0, strict Release (-Wall -Wextra -Wpedantic -Werror), Debug UBSan (-fsanitize=undefined -fno-sanitize-recover=all). Both 4/4 CTest suites pass: 27 R1, 28 R2, 33 runtime, 12 device-rate checks. Real live Linux timed CPU/memory/core and device samples plus fixture regression. One live sample displayed CPU/memory, 5 matched cores, 26 disk rows and 2 network rows, PSI unavailable. No machine-specific raw output published. Release binary 123296 bytes, static library 150946 bytes; one observed sample process peak RSS 1928 KiB, elapsed 1.00 s including deliberate one-second sampling interval. ldd standard C/C++ runtime libraries. Not whole application benchmarks or speedup versus Qt. Release log SHA256 fce462d3dd49934db3eabe62bcbaff334eb43b07064f3332d6596e3e6170ab3c; UBSan log SHA256 01a8b16850054a3fa47069396486b5ab90ed0d5b2c45beab9a8b4485d2a1ee8d.

All 8 changed repository blobs matched local tested bytes:
- native-core/CMakeLists.txt 64aa7fea54331aac358b518add28245ff58ede11
- native-core/include/syspulse/native/runtime.hpp 5928c662cb22fc4cc973320c986efafb518590f3
- native-core/include/syspulse/native/device_rates.hpp 147759d7669c82e3cc6a9db8f190241e377e0300
- native-core/src/runtime.cpp 96984b2879287049091151adf6aa73ae3438b180
- native-core/src/device_rates.cpp f68795bc68d48de975ac95f7be140ece09f2b9c2
- native-core/src/sample_main.cpp c9ffa64a1e4bd76ee50952966f0f83a37d83646c
- native-core/tests/test_runtime.cpp a46e90de10b540f7898f71d179dd8a1a4fa57547
- native-core/tests/test_device_rates.cpp c61dd6a5f2ced282b27eedd5be598ff2c90323da

Seven inherited R1/R2 local source/test hashes match previously verified repository blobs. Build: cmake -S native-core -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j2; ctest --test-dir build --output-on-failure; ./build/syspulse-sample. Source ZIP and logs delivered separately. Direct Git network DNS unavailable; connector source writes and exact readback succeeded.

## Next engineering gates

Port actual existing diagnostics rules and learning contracts with differential fixtures; build process tree and native service/startup adapters; native GUI presentation with actual X11/Windows accessibility/DPI review (reuse mature native platform components, no resurrection of rejected previews); native privilege broker and safe plan/execute/verify; package and signed updater. Full native application, continuous scheduler, advanced diagnostic/maintenance feature parity, Windows native device/process/PSI equivalents, Windows compiler/VM, target Mint physical device, complete desktop GUI and DEB are not accepted. No main/integration merge, release, deployment, paid infrastructure, GitHub Actions or CircleCI. Keep historic Qt baseline intact and update continuity after each validated batch.
