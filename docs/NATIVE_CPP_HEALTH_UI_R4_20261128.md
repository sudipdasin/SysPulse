# Native C++ health and UI R4 — 2026-11-28

Scope: SysPulse only. R3 base ceb6f3e6b537a1c6ee4f78add5fbe18db9d55a97. Exact tested source commit 95bb11275660cc1aa4099a170e75c972f9ef5b94. Existing Alpha39, approved overview layout branch, Master CI and production release paths are unchanged.

## Reuse and implementation
- Reused the existing GPL-3.0-or-later health_triage.hpp byte-for-byte (blob 23f0e1f3c6f80d95bf6713401d3eb3858bc7be04). Native bridge collects memory and root filesystem space; existing thresholds and explicit unknown semantics preserved. Review plan lists unknown/warning/critical checks only; no authorization or execution.
- Bounded process forest (4096 rows), duplicate/zero PID and cyclic/self-parent rejection; missing parents become roots. Partial snapshot does not prove live parent identity, and no process mutation is implemented.
- Experimental native Xlib dashboard: four evidence cards, wide/narrow reflow, three health findings, R refresh/Q close, live one-second CPU/memory collection. Root storage/process count update on manual refresh, not each tick. Event loop blocks while unmapped, resets CPU baseline on remap. No Qt, Python or shell runtime in prototype. X11 is an optional system dependency. This is a proof of native rendering, not revival of the rejected historical UI as the replacement. Bitmap-font quality, DPI, accessibility, clipping of longer advisory text, full navigation and approved Precision layout remain unresolved. The GUI does not expose Boost or privileged operations; Linux X11 only.
- Existing R1–R3 code retained unchanged.

## Executed evidence
Debian GCC 14.2.0, Release -Wall -Wextra -Wpedantic -Werror; separate Debug UBSan -fsanitize=undefined -fno-sanitize-recover=all. Both 6/6 CTest suites passed: 100 inherited checks + 24 new core checks, plus one Xvfb graphical smoke suite. These are repeated across build configurations, not additive. Actual Linux memory/root capacity and real CPU sampling tested. Xvfb test rendered wide 1100x600 and narrow 640x540, verified accent and wrapped-card pixels; no claim of comprehensive visual or interaction acceptance. Actual Xvfb screenshot captured and inspected; default bitmap fonts are small, design remains prototype-level. ImageMagick import lacked X11 support; captured using Python Xlib tooling outside the application, not a runtime dependency. Release GUI executable 118176 bytes, linked native X11 plus standard runtime and transitive platform libraries. No GUI RSS measurement or complete app performance claim.
Release CTest log SHA256 968fcc73611b762c20cf4afc31284c00859f1ae5efd1a888e99a8b2bd1bf8a33. UBSan log SHA256 581cf592cdd6a5f5f500541c7c63a9211ab472e2e62950c333daf16bb78d8d04.

All six changed blobs matched local tested bytes:
- native-core/CMakeLists.txt 6c86f3ae537d7ee46cf06c7e5b5efd749b626bee
- native-core/include/syspulse/health_triage.hpp 23f0e1f3c6f80d95bf6713401d3eb3858bc7be04
- native-core/include/syspulse/native/health.hpp 1874129942dd73a2bae4bbd83401e61422a3cafc
- native-core/src/health.cpp 28fd62a2b46dcfc916976c9f7d83d3ca4c48f6df
- native-core/tests/test_health.cpp 3c8551d7172c14bb3e7e5f3b95036d373bc604a5
- native-core/src/dashboard_x11.cpp 0c5888f869a2c00145bb214e547af682535b972f

## Course correction and next gates
First native GUI smoke passed, then removed fixed 100ms normal-loop wakeups, paused sampling while unmapped, reset baseline on remap and added narrow geometry pixel verification before rerunning both configurations. Next: use mature native text/rendering (e.g. Xft/fontconfig where available), establish shared presentation model with testable responsive geometry and proper keyboard/accessibility, develop Win32 backend with actual Windows toolchain; port remaining existing diagnostics/classification/learning contracts and native service/startup inspection. Process tree needs identity-aware parent linkage before action controls. Native maintenance executor needs plan identity, freshness, permissions, authorization, rollback and real isolated tests. Full app parity, approved UI, target Mint desktop, Windows runtime and signed installer remain unaccepted. No DEB/EXE replacement, production merge, release, provider submission, paid infrastructure, GitHub Actions or CircleCI. Source review bundle and screenshot are evaluation-only. Build: cmake -S native-core -B build -DCMAKE_BUILD_TYPE=Release; cmake --build build -j2; ctest --test-dir build --output-on-failure; ./build/syspulse-dashboard-x11. X11 development library and Xvfb required for native GUI test; if unavailable CMake omits optional target/test; do not claim six suites without verifying.
