FROM ubuntu:22.04 AS build
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake qtbase5-dev dpkg-dev desktop-file-utils python3 openssl file \
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
FROM python:3.11-slim-bookworm AS evaluation
COPY --from=build /out/ /srv/artifacts/
USER 10001:10001
EXPOSE 10000
CMD ["python3", "-m", "http.server", "10000", "--bind", "0.0.0.0", "--directory", "/srv/artifacts"]
