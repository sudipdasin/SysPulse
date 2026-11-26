# SysPulse Community — established Qt migration candidate

This branch restores the established SysPulse 3.0.0-alpha.2 application from the original [Tux Steward source checkpoint](https://github.com/sudipdasin/tux-steward/tree/0105b57667f79129765c56e3f1654cd91c42cf3a). The canonical repository is SysPulse. Preserve the approved Precision white/crimson theme, Observatory monitoring, Forge management, existing navigation and working features.

**Alpha.15 graphical menu evaluation candidate.** Converts the eight XFCE Desktop launchers into three-column graphic tiles, upgrades 63 other native launchers, and adds icon and hover coverage for in-app menus, tabs and dialog actions. Native and package lifecycle gates apply.

- [Continuity policy](docs/CONTINUITY.md)
- [Migration status](docs/QT_MIGRATION_STATUS.md)
- [Approved UI](docs/SYSPULSE_UI_SPECIFICATION.md)
- [Original milestone scope (historical)](docs/SYSPULSE_MILESTONE.md)
- [Privacy](PRIVACY.md)
- [Source intake report](docs/source-intake/REPORT.md)
- [Offline VM verification](tools/vm/README.md)

On a compatible disposable Linux Mint/Ubuntu XFCE VM with CMake, compiler and Qt 5.15 Core/Widgets/Network development files:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/syspulse --version
```

Only run the GUI as a normal user. Validate package dependencies and provenance on the actual target platform. Do not use the historical X11 installer or updater; the mature application does not yet have verified signed full-bundle updates. No GitHub Actions or CircleCI, no hosted CI release publication. See the current gates before merging or publishing.

GNU GPL v3; see LICENSE and upstream source attribution.
