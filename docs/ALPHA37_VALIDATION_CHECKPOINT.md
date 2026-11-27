# Alpha.37 development checkpoint — 2026-11-27

## Verified evidence
- Alpha.36 hybrid engine: 19/19 focused tests; existing diagnostics and research: 40/40 and 16/16. Non-root container analysis completed with 38 source checks, SMART coverage partial.
- Alpha.37 cleaning branch: non-root compiled C++ fixture passed generated-cache scope, age, redirected path, cancellation, identity, combined batch bounds and distinct failed-marker opt-in. 41 diagnostics and 16 research tests passed before classifier integration.
- Synthetic benchmark on 32,000 messages (32 templates × 1,000): TP 22,000, FP 4,000, FN 0, TN 6,000; precision 84.615%, recall 100%, exact category accuracy 87.5%; throughput 56,141 lines/s in the cloud test. This is not a real-world labeled log corpus.
- Isolated local transcription comparison of the committed classifier rules on the same fixtures: corrected TP 22,000, FP 0, FN 0, TN 10,000, exact accuracy 100%; 88 additional safety assertions passed. This is a tuned synthetic comparison, not an integrated-source execution or proof of generalization.
- Production feed remains separate. No new signing keys, production feed modification, GitHub Actions, CircleCI, or paid service used.

## Integrated candidate
Branches dev/alpha37-safe-cleaning (e2c5933c2732d04293bd1d69a7a26d35ff2728c6) and dev/alpha36-detection-benchmark (00f7c12fe244c634e672b0f94cefab772f5245df) combined in dev/alpha37-evaluation. CMake version 37. Reproducible benchmark source retained. Not a release or merge to main.

## Next gates (mandatory)
1. Run the corrected classifier's actual repository tests and benchmark; use separate holdout and licensed real-system corpus for generalization.
2. Build exact combined Alpha.37 native package; verify installed GUI analysis and both opt-in cleanups, signed prior-version upgrade, recovery, tamper rejection and runtime regressions.
3. Confirm physical Linux Mint XFCE and SMART coverage. Never turn unknown hardware state into a healthy verdict.
4. Preserve the last working Alpha.35 candidate; do not admit the new package to the signed feed until verified.
5. Extend safe cleaning only through reviewed native recipes; add reversible repair actions with explicit permission and tested recovery; retain evidence and outcomes.
