# SysPulse System Control Center 0.4.0

This is a new standalone C++17/X11 preview, preserving the existing 0.3.0 health app and its signed updater. It is not the full Qt port or a production-ready package. It provides Overview plus 12 categorized panels and 40 tool entries. Most entries launch the installed distribution tool in its own window. Fixed command names are resolved from standard system paths; absent tools are shown as unavailable. Availability is not proof of compatibility. The prior health and update app is located beside the installed control center.

## Build and install alongside the existing preview

Install build-essential, cmake and libx11-dev using the distribution package manager. For the original health app and updater, see PREVIEW_0.3.0.md. As a normal user in a trusted source checkout:

    cmake -S control-center -B build-control -DCMAKE_BUILD_TYPE=Release
    cmake --build build-control -j2
    ctest --test-dir build-control --output-on-failure
    cmake --install build-control --prefix "$HOME/.local"
    "$HOME/.local/bin/syspulse-control-center"

The standalone build is deliberate: no changes to the existing top-level build or updater. It does not replace the prior executable. No automatic dependency install, desktop menu registration or full bundle update. The original updater does not update this new executable.

## Authorization and safety
The GUI refuses root startup. External settings applications manage their own authorization. The single native mutating action is APT metadata refresh: select Software -> Refresh APT metadata, review the action, then press Enter or Esc. PolicyKit (pkexec) authorizes only the fixed apt-get update invocation; no shell or stored password. Authentication agent, configured APT and network are required. APT output appears in the terminal when launched there. Exit zero indicates command completion, not a guarantee that every repository refreshed. Do not run this on an untrusted package-source configuration. No real privileged operation was executed during tests. Other repair actions are not automatically executed.

## Diagnosis
Read-only root filesystem use, MemAvailable and swap configuration. Classifies thresholds as critical, warning, informational, OK or unknown; offers conservative recommendations. These are snapshots and not proof of system health. No cache dropping, filesystem mutation, automatic process killing, driver changes, partition writes, or recovery operations. Recovery/cleanup tools are shortcuts, not native repair engines. Back up data before changes and recover files to a separate drive.

## Validation
Independent Release build in a Linux container with GCC 14.2, three CTest suites passed (catalog and diagnostic boundary fixtures, live read-only diagnosis, version). 40 unique catalog entries and fixed command resolution verified by fixtures. GUI started as a normal user under Xvfb; root GUI startup refused. Staged install and version verified. Exact five code/build/test Git blob hashes matched tested files. Real XFCE interaction, external app launches, PolicyKit prompts and physical storage testing remain unverified. No GitHub Actions or CircleCI. No raw private sources included.

## Follow-up
Full Qt UI with keyboard navigation, scalable typography, search, theme support, accessibility and embedded panels; safe user/group, printer, network, service and driver workflows; bounded diagnostic providers for kernel/journal, SMART, filesystems, packages, network and failed services; explicit repair plans, backups, post-check and rollback; verified offline recovery. Cache cleaning must preview exact scope; memory tuning must measure outcomes rather than indiscriminately dropping caches. Continue safe source intake and update the manifest when native implementations are complete. Deliver signed complete bundles and DEB releases only after build, packaging and real target tests. Existing release signing/publication blocker remains.
