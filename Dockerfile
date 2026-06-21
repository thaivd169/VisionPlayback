# syntax=docker/dockerfile:1
#
# Multi-stage build for the VisionPlayback headless DASH daemon.
#
#   docker build --build-context hcnetsdk=../3rdparty/HCNetSDK -t visionplayback:latest .
#
# The HCNetSDK is brought in as a BuildKit *named build context* (it lives
# outside the repo at ../3rdparty/HCNetSDK); only incEn/ + lib/ are copied, into
# the /src/3rdparty/HCNetSDK path that CMake's ${CMAKE_SOURCE_DIR}/../3rdparty
# expects. The final image carries only the relocatable install tree + ffmpeg.

##############################
# Stage 1 — build environment
##############################
FROM ubuntu:24.04 AS builder

ARG QT_VERSION=6.11.0
ARG DEBIAN_FRONTEND=noninteractive

# Toolchain + the tools the Qt deploy step uses (objdump via binutils, patchelf),
# a python venv for aqtinstall (PEP 668 forbids global pip on 24.04), and glib —
# Qt 6.11's libQt6Core has a DT_NEEDED on libglib-2.0.so.0 (built with glib event
# loop integration), so it's required to link the executable.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    python3 python3-venv patchelf \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Qt 6.11: Core/Network come from qtbase; HttpServer + its WebSockets dep added.
RUN python3 -m venv /opt/aqt && /opt/aqt/bin/pip install --no-cache-dir aqtinstall \
    && /opt/aqt/bin/aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 \
    -m qthttpserver qtwebsockets --outputdir /opt/Qt

# Sources: repo + the HCNetSDK subset placed to satisfy CMake's ../3rdparty path.
WORKDIR /src
COPY . /src/VisionPlayback
COPY --from=hcnetsdk incEn /src/3rdparty/HCNetSDK/incEn
COPY --from=hcnetsdk lib   /src/3rdparty/HCNetSDK/lib

# Configure -> build -> install the relocatable tree into /opt/visionplayback.
# (Bypass CMakePresets: they hard-code the dev machine's HOME/Ninja paths.)
RUN cmake -S /src/VisionPlayback -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/Qt/${QT_VERSION}/gcc_64 \
    -DCMAKE_INSTALL_PREFIX=/opt/visionplayback \
    && cmake --build /build \
    && cmake --install /build

################################
# Stage 2 — runtime environment
################################
FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive

# ffmpeg = the only external runtime dep (launched via QProcess on PATH).
# libglib2.0-0 = Qt6Core's DT_NEEDED glib (a system lib, so Qt's deploy does not
# bundle it). No system OpenSSL needed: the app serves plain HTTP (Qt's TLS plugin
# is never loaded) and the camera SDK uses its own bundled libssl/libcrypto 1.1.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg ca-certificates libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Qt requires a UTF-8 locale (otherwise it warns and falls back to C.UTF-8).
ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

# The self-contained install tree (binary + bundled Qt + HCNetSDK + plugins).
COPY --from=builder /opt/visionplayback /opt/visionplayback

# Runs as root (the image default — no USER set). Root can write into any
# bind-mounted host directory regardless of its ownership, so the downloads
# volume "just works" with no uid matching. The tradeoff: downloaded files are
# owned by root on the host (use sudo to delete/move them). WORKDIR holds the
# SDK's ./sdkLog/; downloads is a volume.
RUN mkdir -p /var/lib/visionplayback/downloads /var/lib/visionplayback/sdkLog
WORKDIR /var/lib/visionplayback

EXPOSE 18068
VOLUME ["/var/lib/visionplayback/downloads"]

# Stable flags live in ENTRYPOINT so `docker run <img> --api-key=... [--port=...]`
# simply APPENDS args (QCommandLineParser takes the last value, so overrides work).
# NOTE: the built-in default API key is an insecure dev placeholder — always pass
# --api-key=<secret> at run time.
ENTRYPOINT ["/opt/visionplayback/bin/VisionPlayback", \
    "--port=18068", \
    "--downloads-dir=/var/lib/visionplayback/downloads"]
