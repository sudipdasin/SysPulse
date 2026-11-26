# Health measurements and release admission checkpoint

Established Qt base: integration/qt-health-update-checkpoint at 16b5db8cddb5ae19394592fa9ddc7da4043b5694. This is an isolated component integration, not a native Qt release.

- health_probe.hpp reads Linux /proc/meminfo and root statvfs, validates missing, malformed, duplicate or overflowing counters, excludes reserved filesystem blocks from usable capacity and supplies the existing triage policy. No host identifiers, device names, command lines or raw logs are emitted by the component. Linux-only, read-only, snapshot; does not cover all errors.
- release-policy.py checks canonical numeric versions including alpha/beta sequence numbers, rejects equal/older versions and requires explicit preview-channel selection. It must be called only with authenticated manifest and installed-version inputs. It does not provide persistent replay protection, key enrollment, retrieval or installation. The signed package verifier remains separate; live in-app updates are not implemented.

Executed local verification in an isolated temporary source tree:
- Health probe: g++ C++17, strict warnings and UBSan; 13 checks passed, including live read-only shape check.
- Existing triage: 8 fixture cases passed, same compiler flags.
- Signed package verifier: 11 Python unittest cases passed, temporary synthetic payload/key.
- Release admission: 7 Python unittest methods passed, including malformed-input subcases.
- Total 39 reported checks/test methods passed; four new remote blob hashes exactly matched tested local files.
- A fresh CMake Qt 5.15 dependency probe failed: Qt5Config.cmake missing. No complete Qt build, desktop test, packaging or administrator-operation test performed.
- No hosted CI, no raw private attachments, no production keys, no release. Keep main unchanged until full native validation.

The two component branches are combined here after actual local tests. GUI hookup, build test registration and end-to-end updater remain separate validation work. Preserve existing Precision UI, navigation, Observatory and Forge. No X11 replacement.
