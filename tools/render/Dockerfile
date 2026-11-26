FROM ubuntu:22.04 AS build
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake qtbase5-dev dpkg-dev desktop-file-utils python3 openssl gnupg file \
    fonts-dejavu-core ca-certificates xdg-utils procps iproute2 util-linux dbus-x11 \
    && rm -rf /var/lib/apt/lists/*
RUN useradd --create-home --uid 10001 builder && mkdir -p /src /work /out && chown builder:builder /src /work /out
COPY --chown=builder:builder CMakeLists.txt LICENSE PRIVACY.md /src/
COPY --chown=builder:builder include/ /src/include/
COPY --chown=builder:builder src/ /src/src/
COPY --chown=builder:builder tests/ /src/tests/
COPY --chown=builder:builder assets/ /src/assets/
COPY --chown=builder:builder docs/SYSPULSE_MILESTONE.md docs/BETA_READINESS.md /src/docs/
COPY --chown=builder:builder tools/ /src/tools/
USER builder
RUN bash /src/tools/render/build-candidate.sh /src /work /out
FROM ubuntu:24.04 AS installcheck
COPY --from=build /out/ /candidate/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends /candidate/*.deb fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/* \
    && test "$(dpkg-query -W -f='${Version}' syspulse)" = '3.0.0~alpha8~render1' \
    && test "$(stat -c '%u:%g:%a' /usr/bin/syspulse-memory-control)" = '0:0:755' \
    && useradd --create-home --uid 10001 tester
USER tester
ENV HOME=/home/tester QT_QPA_PLATFORM=offscreen XDG_RUNTIME_DIR=/home/tester/runtime
RUN mkdir -m 700 "$XDG_RUNTIME_DIR" \
    && syspulse --version | grep -F '3.0.0-alpha.8' \
    && tux-steward --version | grep -F '3.0.0-alpha.8' \
    && tux-task-manager --version | grep -F '3.0.0-alpha.8' \
    && tux-setup --version | grep -F '3.0.0-alpha.8' \
    && syspulse --help >/dev/null \
    && (rc=0; syspulse-memory-control enable >/tmp/helper-guard.log 2>&1 || rc=$?; test "$rc" = 1 && grep -F 'Administrator authorization required' /tmp/helper-guard.log) \
    && (rc=0; timeout 4s syspulse >/tmp/syspulse-launch.log 2>&1 || rc=$?; test "$rc" = 124) \
    && printf 'Ubuntu 24.04 amd64: actual local DEB installation, four installed executable versions, root-owned non-setuid memory-helper permissions, unprivileged memory-helper rejection, and unprivileged offscreen GUI startup passed. No XFCE VM or real desktop integration test.\n' > /home/tester/INSTALL-STATUS.txt
FROM python:3.11-slim-bookworm AS evaluation
COPY --from=installcheck /home/tester/INSTALL-STATUS.txt /srv/artifacts/INSTALL-STATUS.txt
COPY --from=build /out/ /srv/artifacts/
USER 10001:10001
EXPOSE 10000
CMD ["python3", "-m", "http.server", "10000", "--bind", "0.0.0.0", "--directory", "/srv/artifacts"]
