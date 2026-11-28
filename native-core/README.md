# SysPulse native functional parity development

Framework-free C++17 native Linux development source. This is not a new preview release or complete replacement. Read PARITY-STATUS.md for evidence and outstanding gates.

Build and test (Linux X11 development headers, json-c, OpenSSL, CMake, C++ compiler, Xvfb):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/syspulse-dashboard-x11
```

The native GUI smoke suite must actually be present. Run the action, storage, backup and desktop report tests separately under a disposable ordinary account to exercise mutation tests; root-only CTest cannot verify those paths. Keep production Alpha39 installed. Do not package or publish this source as a finished release. Packaging requires an explicit newer reviewed version and fresh output filename.
