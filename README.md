# SysPulse

Open-source, lightweight system utilities for Linux and Windows: diagnose, maintain, repair, optimize and administer computers. Community development initially targets Linux Mint/XFCE, with native Windows support planned.

**Status:** native Linux 0.2.0 source preview and source-intake foundation; no published release or verified production package. The previous Tux Steward C++/Qt project is a candidate for reviewed feature migration, not a dependency or a completed port.

- [Build and install native preview](docs/PREVIEW_0.2.0.md)
- [Previous preview](docs/PREVIEW_0.1.0.md)
- [Developer handoff](docs/PROJECT_HANDOFF.md)
- [Source intake report](docs/source-intake/REPORT.md)
- [Implementation queue](docs/source-intake/IMPLEMENTATION_QUEUE.md)
- [Privacy policy](docs/source-intake/PRIVACY_POLICY.md)
- [Utility manifest](docs/source-intake/manifest.json)
- [Intake changelog](docs/source-intake/CHANGELOG.md)

Linux read-only reference prototypes can be invoked using Bash: `bash source-intake/linux/capabilities.sh`, `bash source-intake/linux/diagnostics.sh --summary`, `bash source-intake/linux/browser-process-summary.sh`. No privileges or installation are required. Review any detailed local diagnostic output before sharing. Query success does not imply health.

GPL v3; see LICENSE. Future Pro development belongs in a separate repository and must comply with all upstream licenses. No Pro code is included here.
