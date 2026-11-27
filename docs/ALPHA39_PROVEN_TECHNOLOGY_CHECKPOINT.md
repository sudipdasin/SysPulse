# Alpha.39 proven-technology checkpoint — 2026-11-27

## Owner directive
Accelerate development by reusing proven open-source operating-system utilities, integrating features only where they improve measured outcomes, and course-correcting after each milestone. Keep native Qt/XFCE responsiveness, user data protection, signed update trust and free-only infrastructure. No GitHub Actions or CircleCI.

## Implemented
- APT native solver preview: fixed non-root apt-get -s --no-download --no-remove --fix-broken install. Counts only, no raw names or paths. Refuse removals, unresolved dependencies, unknown or warning-bearing output. This is a diagnostic preview, never authority to execute repairs.
- dpkg native file verification for fixed apt/dpkg/systemd package set. Separate core executable/library, configuration, documentation and other differences. Local modifications can be intentional, and dpkg's checksum comparison is not a security attestation.
- Analyze System adds two sources, integrates package evidence into existing SQLite assessment, adds native Package repair preview tab, blocks APT archive cleanup on unknown/proposed/unresolved solver results, refreshes solver before archive cleanup. Parallel read-only SMART and APT/kernel checks avoid unnecessary extra serial latency. No change to existing owner signing or arbitrary command execution policy.
- Render-independent local builder tools/build-alpha39-local.sh pins source f913e2a0506aaaf9bff3101af517f24d318bb2a2, supports Mint 22.x noble/Ubuntu 22.04/24.04 amd64, checks prerequisites, compiles and runs CTest, benchmark replay, CPack, package and embedded owner public-anchor validation. No root, auto dependency installation, install, signing or publishing. Preflight and syntax tested; native build branch not executed here.

## Evidence
- Exact repository component c2add77dcdf52eec8e238e8a17ddafe1430ecc48 and test 97bf8c6dc2c56cafa6219e7a48da9d7b1d0e7316: 12/12 local component tests passed. Initial 11 tests and real Debian 13 non-root native utility smoke passed; tightened warning handling in final revision and reran component tests. Actual utility smoke reported no proposed package changes, no core-file changes across three packages, and intentional/possible documentation differences. dpkg status and APT extended-state SHA checks unchanged. Actual broken-dependency fixture using alternate apt status database refused removal; fixture SHA unchanged. Not real Mint runtime or broken-package repair execution.
- Local builder exact blob 5011fde5597165c1c55dc71a7622a5d7b5155bb6; bash -n passed, preflight correctly rejected Debian 13 root environment with absent Qt development files.
- Prior Alpha.38 cloud deployment dep-dc4n8vuc6qcc73e8k0sg canceled before build because free Render build minutes exhausted. No retry or purchase. Earlier isolated Alpha.38 18 tests remain valid; integrated DB, GUI, native build and Alpha.39 combined test suite not executed.
- Alpha.37 remains validated evaluation package, Alpha.32 existing signed feed. DEB Release Fleet main unchanged from reviewed baseline 6f7176b32896c471814850e1dad11c28c3dca9aa. No production enrollment, signing, release catalogue policy relaxation or key replacement.

## Next gates
Execute exact Alpha.39 full CTest + benchmarks in a compatible native build environment; correct any failures before retaining DEB. Run disposable signed Alpha.32 upgrade, tamper rejection, settings preservation, full GUI and physical Mint acceptance. Preserve prior package bytes. Add independent real-world labeled log corpus and reviewed reversible repair recipes; use native APT, dpkg, SMART, XFCE recovery and other proven utilities rather than custom solvers. Never auto-apply external suggestions. Source commits use [skip ci]; no paid build resources.
