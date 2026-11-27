FROM ubuntu:22.04 AS dependencies
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake qtbase5-dev dpkg-dev desktop-file-utils python3 openssl gnupg file git \
    fonts-dejavu-core ca-certificates xdg-utils procps iproute2 util-linux dbus-x11 \
    && rm -rf /var/lib/apt/lists/*
RUN useradd --create-home --uid 10001 builder && mkdir -p /src /work /out /upgrade /oldsrc /oldwork /oldout && chown builder:builder /src /work /out /upgrade /oldsrc /oldwork /oldout
FROM dependencies AS previous
USER builder
ENV HOME=/home/builder QT_QPA_PLATFORM=offscreen GIT_TERMINAL_PROMPT=0
# Reconstruct the prior evaluation package from its exact deployed source commit.
# This is not a claim of byte-for-byte reproducibility.
RUN git -C /oldsrc init -q \
    && git -C /oldsrc remote add origin https://github.com/sudipdasin/SysPulse.git \
    && git -C /oldsrc fetch --no-tags --depth=1 origin d7238fcba1f7d52afed971dd538fcfca55429b5f \
    && git -C /oldsrc checkout --detach FETCH_HEAD \
    && test "$(git -C /oldsrc rev-parse HEAD)" = d7238fcba1f7d52afed971dd538fcfca55429b5f \
    && cmake -S /oldsrc -B /oldwork -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DSYSPULSE_PACKAGE_TARGET=ubuntu-22.04 \
    && cmake --build /oldwork --parallel 2 \
    && cpack --config /oldwork/CPackConfig.cmake -G DEB -D CPACK_DEBIAN_PACKAGE_VERSION=3.0.0~alpha22~render1 \
    && test "$(dpkg-deb -f /oldwork/packages/*.deb Package)" = syspulse \
    && test "$(dpkg-deb -f /oldwork/packages/*.deb Architecture)" = amd64 \
    && test "$(dpkg-deb -f /oldwork/packages/*.deb Version)" = '3.0.0~alpha22~render1' \
    && cp /oldwork/packages/*.deb /oldout/previous.deb
FROM dependencies AS build
COPY --chown=builder:builder CMakeLists.txt LICENSE PRIVACY.md /src/
COPY --chown=builder:builder include/ /src/include/
COPY --chown=builder:builder src/ /src/src/
COPY --chown=builder:builder tests/ /src/tests/
COPY --chown=builder:builder assets/ /src/assets/
COPY --chown=builder:builder docs/SYSPULSE_MILESTONE.md docs/BETA_READINESS.md /src/docs/
COPY --chown=builder:builder tools/ /src/tools/
USER builder
RUN bash /src/tools/render/build-candidate.sh /src /work /out
RUN bash /src/tools/render/make-upgrade-fixture.sh /out/syspulse-3.0.0-alpha.23-x86_64-ubuntu-22.04-render-eval.deb /upgrade
FROM ubuntu:24.04 AS installcheck
COPY --from=previous /oldout/ /previous/
COPY --from=build /out/ /candidate/
COPY --from=build --chown=0:0 /upgrade/ /upgrade/
COPY --from=build /work/build/syspulse-cleaner-ui-tests /usr/local/bin/syspulse-cleaner-runtime-test
COPY --from=build /work/build/syspulse-cleaner-tests /usr/local/bin/syspulse-cleaner-core-runtime-test
COPY tools/render/test-volume-runtime.py /usr/local/bin/syspulse-volume-runtime-test.py
COPY --from=build /work/build/tux-tests /usr/local/bin/syspulse-core-runtime-test
COPY --from=build /work/build/syspulse-shell-tests /usr/local/bin/syspulse-shell-runtime-test
COPY tools/render/x11-runtime-test.sh /usr/local/bin/syspulse-x11-runtime-test.sh
COPY tools/render/test-apt-updater-runtime.py /usr/local/bin/syspulse-apt-updater-runtime-test.py
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-remove --no-install-recommends /previous/previous.deb python3 ca-certificates fonts-dejavu-core xvfb xauth dbus-x11 xinput x11-xserver-utils xfconf xfce4-settings procps iproute2 util-linux \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha22~render1' \
    && useradd --create-home --uid 10001 tester
USER tester
ENV HOME=/home/tester QT_QPA_PLATFORM=offscreen XDG_RUNTIME_DIR=/home/tester/runtime
RUN mkdir -m 700 "$XDG_RUNTIME_DIR" \
    && mkdir -p "$HOME/.config/SysPulse" \
    && printf '[updates]\nautomaticCachedCheck=true\n[General]\nupgradeFixture=preserve-user-settings\n' > "$HOME/.config/SysPulse/SysPulse.ini" \
    && sha256sum "$HOME/.config/SysPulse/SysPulse.ini" > "$HOME/preference.sha256" \
    && syspulse --version | grep -F '3.0.0-alpha.22' \
    && (rc=0; timeout 4s syspulse >/tmp/prior-launch.log 2>&1 || rc=$?; test "$rc" = 124)
USER root
ENV HOME=/root
RUN printf 'deb [arch=amd64 signed-by=/upgrade/test-only.gpg] file:/upgrade/repo ./\n' > /tmp/syspulse-upgrade.list \
    && apt-get -o Dir::Etc::sourcelist=/tmp/syspulse-upgrade.list -o Dir::Etc::sourceparts=- -o APT::Get::List-Cleanup=0 -o Acquire::Languages=none update \
    && apt-cache -o Dir::Etc::sourcelist=/tmp/syspulse-upgrade.list -o Dir::Etc::sourceparts=- policy syspulse | grep -F 'Candidate: 3.0.0~alpha23~render1' \
    && printf 'corruption-fixture\n' >> /upgrade/repo/pool/syspulse_3.0.0~alpha23~render1_amd64.deb \
    && (if DEBIAN_FRONTEND=noninteractive apt-get -o Dir::Etc::sourcelist=/tmp/syspulse-upgrade.list -o Dir::Etc::sourceparts=- install -y --no-remove --only-upgrade --no-install-recommends syspulse=3.0.0~alpha23~render1 >/tmp/tamper.log 2>&1; then echo 'Tampered package unexpectedly accepted'; exit 1; fi; (grep -Ei 'hash sum mismatch|file has unexpected size|size mismatch' /tmp/tamper.log || { cat /tmp/tamper.log; exit 1; })) \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha22~render1' \
    && cp /candidate/*.deb /upgrade/repo/pool/syspulse_3.0.0~alpha23~render1_amd64.deb \
    && dpkg-deb -x /candidate/*.deb /tmp/updater-test \
    && python3 /usr/local/bin/syspulse-apt-updater-runtime-test.py /tmp/updater-test/usr/bin/syspulse-apt-update \
    && rm -rf /tmp/updater-test \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha23~render1' \
    && test "$(stat -c '%u:%g:%a' /usr/bin/syspulse-memory-control)" = '0:0:755' \
    && test "$(stat -c '%u:%g:%a' /usr/bin/syspulse-volume-scan)" = '0:0:755' \
    && python3 /usr/local/bin/syspulse-volume-runtime-test.py
USER tester
ENV HOME=/home/tester
RUN sha256sum -c "$HOME/preference.sha256" \
    && syspulse --version | grep -F '3.0.0-alpha.23' \
    && (rc=0; timeout 4s syspulse >/tmp/upgraded-launch.log 2>&1 || rc=$?; test "$rc" = 124) \
    && sha256sum -c "$HOME/preference.sha256"
USER root
ENV HOME=/root
# Explicit downgrade only in the disposable test container, to the pinned prior package.
# This is a recovery test, not a production automatic rollback policy.
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-remove --allow-downgrades --no-install-recommends /previous/previous.deb \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha22~render1'
USER tester
ENV HOME=/home/tester
RUN sha256sum -c "$HOME/preference.sha256" \
    && syspulse --version | grep -F '3.0.0-alpha.22' \
    && (rc=0; timeout 4s syspulse >/tmp/recovered-launch.log 2>&1 || rc=$?; test "$rc" = 124)
USER root
ENV HOME=/root
RUN DEBIAN_FRONTEND=noninteractive apt-get -o Dir::Etc::sourcelist=/tmp/syspulse-upgrade.list -o Dir::Etc::sourceparts=- install -y --no-remove --only-upgrade --no-install-recommends syspulse=3.0.0~alpha23~render1 \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha23~render1'
RUN DEBIAN_FRONTEND=noninteractive apt-get remove -y syspulse \
    && test ! -e /usr/bin/syspulse && test ! -e /usr/bin/syspulse-memory-control \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-remove --no-install-recommends /candidate/*.deb \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha23~render1' \
    && test "$(stat -c '%u:%g:%a' /usr/bin/syspulse-memory-control)" = '0:0:755' \
    && rm -rf /var/lib/apt/lists/*
USER tester
ENV HOME=/home/tester
RUN sha256sum -c "$HOME/preference.sha256" \
    && syspulse --version | grep -F '3.0.0-alpha.23' \
    && tux-steward --version | grep -F '3.0.0-alpha.23' \
    && tux-task-manager --version | grep -F '3.0.0-alpha.23' \
    && tux-setup --version | grep -F '3.0.0-alpha.23' \
    && syspulse --help >/dev/null \
    && syspulse-apt-update check alpha | grep -F 'UNCONFIGURED 3.0.0~alpha23~render1' \
    && timeout 20s /usr/local/bin/syspulse-cleaner-core-runtime-test \
    && timeout 40s /usr/local/bin/syspulse-cleaner-runtime-test \
    && (rc=0; syspulse-volume-scan / standard >/tmp/volume-guard.log 2>&1 || rc=$?; test "$rc" = 1 && grep -F 'Administrator authorization required' /tmp/volume-guard.log) \
    && timeout 90s bash /usr/local/bin/syspulse-x11-runtime-test.sh \
    && (rc=0; syspulse-memory-control enable >/tmp/helper-guard.log 2>&1 || rc=$?; test "$rc" = 1 && grep -F 'Administrator authorization required' /tmp/helper-guard.log) \
    && (rc=0; timeout 4s syspulse >/tmp/reinstalled-launch.log 2>&1 || rc=$?; test "$rc" = 124) \
    && printf 'Ubuntu 24.04 amd64: prior alpha.22 rebuilt from pinned deployed source; real APT installation, tampered package rejection with prior version retained, signed local repository upgrade to alpha.23 through the actual new updater helper, explicit pinned prior-package recovery and signed re-upgrade, preference checksum preservation, remove/reinstall, all four versions, helper ownership and non-root refusal, unprivileged offscreen startup, Cleaner GUI functional regression on Ubuntu 24.04 runtime, virtual X11 GUI shell including single startup-window check, actual root read-only volume metadata scan, non-root refusal and real X11 diagnostics, and isolated XFCE compositor setting readback passed. Disposable test key only; no production update feed or real Mint XFCE VM.\n' > /home/tester/INSTALL-STATUS.txt
FROM python:3.11-slim-bookworm AS evaluation
COPY --from=installcheck /home/tester/INSTALL-STATUS.txt /srv/artifacts/INSTALL-STATUS.txt
COPY --from=build /out/ /srv/artifacts/
USER 10001:10001
EXPOSE 10000
CMD ["python3", "-m", "http.server", "10000", "--bind", "0.0.0.0", "--directory", "/srv/artifacts"]
