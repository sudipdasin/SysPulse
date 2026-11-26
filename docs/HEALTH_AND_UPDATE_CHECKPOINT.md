# Health and update component checkpoint

Base: established Qt integration candidate d68cc0516eb6d23f339ad41338832650c98a0e31. No changes to approved stylesheet, GUI, existing action catalog or release version. These are independently tested components staged together, not a verified full Qt application or release.

## Diagnostic policy
include/syspulse/health_triage.hpp accepts measured available/total RAM and root storage counters and optional swap capacity. Missing/invalid values are unknown. Conservative policy defaults: RAM below 5% critical, below 15% warning; available root storage at or below 2% critical, at or below 10% warning. Swap presence is informational, not evidence of health. Integrate later using existing measured Qt metrics, and show findings in the established health/overview panels. No system change or automatic repair is performed. Preserve existing reviewed diagnostic workflows.

## Signed package verification
tools/verify-signed-package.py is an offline development admission check, not an installed updater. It requires an independently trusted PEM fingerprint and exact expected version, platform and source commit, verifies detached OpenSSL signature over manifest bytes, rejects duplicate/unknown fields, unsafe filenames, oversized inputs, mismatched identity, nonregular or symlinked payloads and incorrect package size/digest. The schema is a JSON object with schema=1, version, target, commit, filename, size and sha256. Supports named Mint/Ubuntu amd64 target labels, not proof of ABI compatibility. Use only in an isolated trusted staging directory; concurrent hostile filesystem changes are outside this offline verifier's threat model. Fixture payload is not a real DEB. No installation, extraction, version rollback protection, live retrieval or key provisioning. Future Qt update integration must authenticate release selection, prevent downgrade/replay, stage immutable verified assets, use reviewed OS package manager authorization and validate complete-package upgrade/rollback. No official key or live signed release yet. Never ship test keys.

## Executed local evidence
From a temporary isolated source tree:
- g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined -Iinclude tests/test_health_triage.cpp -o triage-test; ./triage-test: 8 fixture cases passed.
- python3 tests/test_signed_package.py: 11 tests passed, using temporary RSA signing keys and synthetic payload.
- Combined rerun: all 19 fixture cases passed.
- Four remote source/test blob hashes matched tested local files.
- Qt 5.15 Core/Widgets/Network CMake dependency probe failed (Qt5Config.cmake unavailable); local direct GitHub access failed DNS. No full application CTest/VM/physical hardware or privilege tests performed.
- Both independent source branches combined in this validation candidate only. Main unchanged; no GitHub Actions/CircleCI; no release published.

Keep the full established Qt application, UI and inherited tests. Wire these components into the existing GUI/build only with native test availability, and update the central intake report after verified native integration. No X11 replacement application. No original private sources or machine details included.
