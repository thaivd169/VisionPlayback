# VisionPlayback — Agent Guide

Qt6/C++ **headless console daemon** that downloads surveillance video from
Hikvision cameras via HCNetSDK, packages it as MPEG-DASH with ffmpeg, and
serves both a control + polling JSON API and static DASH segments over an
embedded HTTP server. No GUI. Clients consume the served MPD URLs in any DASH
player (VLC, browser).

---

## Build

Prerequisites: Qt 6.11, CMake 3.16+, Ninja, ffmpeg on PATH, HCNetSDK at `../3rdparty/HCNetSDK/`.

```bash
# Configure (debug)
cmake --preset amd64-debug

# Build
cmake --build --preset amd64-debug

# Run
./out/build/amd64-debug/main/VisionPlayback
```

For release swap `amd64-debug` → `amd64-release`. There is also a workflow
preset that chains configure + build in one shot:

```bash
cmake --workflow --preset amd64-debug
```

**CMake auto-features** (`AUTOUIC`, `AUTOMOC`, `AUTORCC`) are ON — do not run
`moc`/`uic` manually or add generated files to the source list. `Q_OBJECT`
classes inside static libraries are picked up because each layer uses
`qt_add_library(... STATIC ...)` (not plain `add_library`).

---

## Install / Deploy

Produces a **self-contained, relocatable** tree under `CMAKE_INSTALL_PREFIX`
(`out/install/amd64-release` by preset) that bundles the executable, the Qt 6
runtime (via Qt's own `qt_generate_deploy_app_script`), and the full HCNetSDK
runtime — copy the folder to another x86-64 Linux host and run it directly, no
`LD_LIBRARY_PATH` needed.

```bash
cmake --build --preset amd64-release           # build first
cmake --install out/build/amd64-release        # → out/install/amd64-release
# or, in one shot, the install build preset:
cmake --build --preset amd64-release-install
```

Layout:

```
<prefix>/bin/VisionPlayback        # INSTALL_RPATH = $ORIGIN/../lib
<prefix>/bin/qt.conf               # generated; points Qt at ../plugins
<prefix>/lib/libQt6*.so.6          # Qt runtime (deploy script)
<prefix>/lib/libhcnetsdk.so + libHC*/libcrypto.so.1.1/...   # HCNetSDK
<prefix>/lib/HCNetSDKCom/*.so      # HCNetSDK dlopen plugins (kept beside core libs)
<prefix>/plugins/...               # Qt plugins
```

The whole HCNetSDK `lib/` dir (~26 MB) is copied wholesale: the SDK `dlopen`s
most components lazily by name, so the dependency graph is undocumented and
pruning risks cryptic runtime errors. **Runtime prerequisite:** `ffmpeg` must be
on `PATH` (`apt install ffmpeg`) — it is invoked as a child process and is
intentionally **not** bundled.

### Docker

A multi-stage [Dockerfile](Dockerfile) (both stages `ubuntu:24.04`) builds the
app in a Qt-6.11 + toolchain stage and copies only the relocatable install tree
+ `ffmpeg` into the runtime image. HCNetSDK enters as a **BuildKit named build
context** (it lives outside the repo at `../3rdparty/HCNetSDK`):

```bash
# from the repo root
docker build --build-context hcnetsdk=../3rdparty/HCNetSDK -t visionplayback:latest .

# run — ALWAYS override the insecure default API key; downloads persist in a volume
docker run --rm -p 8080:8080 \
  -v vp-downloads:/var/lib/visionplayback/downloads \
  visionplayback:latest --api-key=my-secret-key
```

[scripts/run-docker.sh](scripts/run-docker.sh) is a convenience wrapper that
bind-mounts a host folder (`$HOME/Videos/playbacks`) instead of a named volume.

`--port`/`--downloads-dir` are baked into the `ENTRYPOINT`; any flags after the
image name are appended (`QCommandLineParser` takes the last value, so they also
override the defaults). `WORKDIR` (`/var/lib/visionplayback`) holds the SDK's
`./sdkLog/`.

The container runs as **root** (the image default — no `USER` is set). Root can
write into a bind-mounted host directory regardless of its ownership, so the
downloads volume works with no uid matching; the tradeoff is that downloaded
files are owned by `root` on the host (use `sudo` to delete/move them). To run
non-root instead — files owned by your host user, smaller blast radius — add a
service user whose uid/gid match the host (`useradd -u $(id -u) …` + `USER`) and
ensure the bind-mount target is owned by that uid.

---

## Run

```bash
LD_LIBRARY_PATH=/home/thaivd/TDIC/3rdparty/HCNetSDK/lib \
  ./out/build/amd64-debug/main/VisionPlayback \
  --api-key=visionplayback-dev-key \
  --port=8080 \
  --downloads-dir=/var/lib/visionplayback/downloads \
  --login-idle-timeout=600
```

All flags are optional; defaults come from [src/infra/Config.h](src/infra/Config.h).
The binary sets `BUILD_RPATH` so the explicit `LD_LIBRARY_PATH` is only needed
when running from a custom working directory.

---

## Architecture

Onion-style Clean Architecture. Four static libraries (see
[main/Session.cpp](main/Session.cpp) for the composition root):

```
main  ─►  vp_adapter  ─►  vp_usecase  ─►  vp_domain
main  ─►  vp_infra ──────────────────────►  vp_domain
          vp_adapter  ─►  vp_infra   (PRIVATE — see note below)
```

- `vp_domain` is pure C++ and owns the value objects (`PlaybackKey`,
  `PlaybackRequest`, `StreamStatus`, …) and the **port interfaces**
  (`IAuthenticator`, `IPlaybackDownloader`, `IDashPackager`,
  `IStreamCacheRepository`, `IHasher`).
- `vp_usecase` is pure C++ (no Qt) and depends only on `vp_domain`. Currently
  holds the synchronous, cache-first `LoginUseCase`.
- `vp_infra` implements the domain ports using HCNetSDK (DVR auth + downloader),
  ffmpeg (via `QProcess`), the filesystem, and Qt's `QCryptographicHash`.
- `vp_adapter` is the **primary-adapter** layer: the `HttpListener` HTTP
  boundary plus the `PlaybackProcessor` pipeline orchestrator. The two adapters
  are **self-contained**: each takes a plain CLI-derived config struct
  (`HttpListenerConfig`, `PlaybackProcessorConfig`) and builds its own infra
  collaborators internally — so `vp_adapter` links `vp_infra` (**PRIVATE**).
  The concrete infra types are named **only inside the `.cpp` files**, never in
  an adapter header, so headers stay infra-free and the coupling is contained to
  one translation unit per adapter.
- `main` is the composition root, now thin: it parses the CLI into the two
  config structs, constructs the adapters, moves them onto their threads, and
  wires the signal graph. It no longer builds infra/use-case objects by hand.

> **Layering note:** the adapter→infra link is a deliberate relaxation of the
> original "adapter must never see infra" rule. It was adopted so each adapter
> owns its collaborators (`PlaybackProcessor` builds the authenticator,
> `LoginUseCase`, downloader/packager factories, and the `FileSystemStreamCache`;
> `HttpListener` builds its `IHasher`) instead of `Session` wiring a long
> positional argument list. Keep the concrete-type references confined to the
> adapter `.cpp` files.

### Layer responsibilities

| Layer        | CMake target | Direct link deps                          | Audit grep (must be empty)                          |
| ------------ | ------------ | ----------------------------------------- | --------------------------------------------------- |
| `src/domain` | `vp_domain`  | std C++ only                              | `grep -rE 'Q[A-Z]\|NET_DVR_\|HCNetSDK' src/domain/`  |
| `src/usecase`| `vp_usecase` | `vp_domain` (PUBLIC)                      | `grep -rE 'Q[A-Z]\|NET_DVR_\|HCNetSDK' src/usecase/` |
| `src/infra`  | `vp_infra`   | `vp_domain` (PUBLIC) + `Qt::Core` (PUBLIC) + HCNetSDK (PRIVATE) | (no rule — infra *is* the technical layer) |
| `src/adapter`| `vp_adapter` | `vp_usecase` (PUBLIC) + `vp_infra` (PRIVATE) + Qt::Core/Network/HttpServer (PUBLIC) | none — see note (infra refs allowed in adapter `.cpp` only) |
| `main`       | `VisionPlayback` | `vp_adapter` + `vp_infra` + Qt        | composition only                                    |

Hard prohibitions still enforced by CMake:
- `vp_adapter` must NOT directly link `vp_domain` — it reaches domain only
  through `vp_usecase`'s (and `vp_infra`'s) PUBLIC links.
- `vp_domain` and `vp_usecase` must stay Qt- and SDK-free.

The adapter→infra link is allowed (PRIVATE) but infra references must stay in
the adapter `.cpp` files, never in adapter headers.

(Comments mentioning `HCNetSDK` are allowed everywhere — the audit cares
about real code.)

### Pipeline flow

```
HTTP thread (HttpListener)                      processor thread (PlaybackProcessor)
──────────────────────────                      ────────────────────────────────────
POST /playback (X-API-Key) ── playbackRequested ──►  onPlaybackRequested
  (api-key check, JSON codec)        (signal)          ├─ LoginUseCase ──► IAuthenticator (HCNetSDK)
                                                        ├─ IPlaybackDownloader (HCNetSDK, own std::thread)
GET /dash/<hash>/<file> ──── keyAccessed ──────────►   ├─ IDashPackager (ffmpeg / QProcess)
  └─► downloads/<hash>/*             (signal)           └─ IStreamCacheRepository (FS) + evictToCapacity
GET /playback?id=<hash>
  └─ reads status + URL mirror ◄─ statusChanged ──────  emits on every state change
                                  (keyHex,status,url)    (url set only when Ready)

PlaybackProcessor logs [login-ok] [download] [stream-ready] [stream-error] [login-fail] → stdout/stderr
```

### Key files

| File                                              | Role                                                                                                |
| ------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `src/domain/PlaybackKey.h`                        | 16-hex-char hash truncation + inline `makePlaybackKey(Credentials, Channel, TimeRange, IHasher&)`. Algorithm is injected — callers never pick a concrete hash.  |
| `src/domain/PlaybackTime.h`                       | Calendar struct (mirrors `NET_DVR_TIME` shape). Parses `"YYYYMMDDTHHMMSS"`.                         |
| `src/domain/PlaybackRequest.h`                    | `Credentials credentials; SessionToken token; Channel channel; TimeRange range; PlaybackKey key;`   |
| `src/domain/Credentials.h`                        | Pure `std::string` fields.                                                                          |
| `src/domain/CameraIdentity.h`                     | `(ip, port, user)` tuple — login-cache key in `LoginUseCase`.                                       |
| `src/domain/IHasher.h`                            | Port interface for one-shot hex digest. Keeps `makePlaybackKey` algorithm-agnostic.                 |
| `src/domain/I{Authenticator,PlaybackDownloader,DashPackager,StreamCacheRepository}.h` | Qt-free port interfaces. Async ones expose `std::function` callbacks. |
| `src/domain/StreamStatus.h`                       | `enum class StreamStatus` (Unknown → Pending → Downloading → Packaging → Ready / Failed). Drives the polling response and the processor→listener status sync. |
| `src/usecase/LoginUseCase.h/.cpp`                 | Synchronous, cache-first SDK login (Qt-free). Sweep-on-access idle eviction. The only use case.     |
| `src/infra/dvr/HCNetSDKDownloader.h/.cpp`         | `IPlaybackDownloader` impl. Owns its own `std::thread`; SDK polling.                                |
| `src/infra/dvr/HCNetSDKAuthenticator.h/.cpp`      | `IAuthenticator` impl. Wraps `NET_DVR_Login_V40` / `NET_DVR_Logout_V30`.                            |
| `src/infra/dvr/HCNetSDKTimeMapper.h`              | The only file that names both `NET_DVR_TIME` and `PlaybackTime`.                                    |
| `src/infra/packaging/FfmpegDashPackager.h/.cpp`   | `IDashPackager` impl. `ffmpeg -f dash` via `QProcess`.                                              |
| `src/infra/persistence/FileSystemStreamCache.h/.cpp` | `IStreamCacheRepository` impl. Owns `downloads/<hash>.mp4` + `downloads/<hash>/manifest.mpd`. Implements `evictToCapacity()`: scans completed DASH dirs, sorts oldest-first by `lastModified`, removes until total size ≤ `maxBytes`, skipping keys in the provided exclusion list. |
| `src/infra/hashing/QtHasher.h/.cpp`              | `IHasher` impl. Wraps `QCryptographicHash`. Supports `blake2s-128` (default), `sha256`, `sha512`. Selected via `--hash-algorithm` at startup. |
| `src/infra/HCNetSDKBootstrap.{h,cpp}`             | RAII for `NET_DVR_Init` + `NET_DVR_Cleanup`.                                                        |
| `src/infra/Config.h`                              | `kDefaultApiKey`, `kDefaultPort`, `kDefaultLoginIdleSeconds`, `kDefaultMaxDownloadsSizeBytes` (100 GB), `kDefaultHashAlgorithm` (`"blake2s-128"`). |
| `src/adapter/http/HttpListener.{h,cpp}`           | The entire HTTP boundary, on its own `QThread`. Owns `QHttpServer`/`QTcpServer` (created in `started()`); serves `POST /playback` (constant-time API-key check + inline `QJson`↔domain codec → `playbackRequested` signal), `GET /playback?id=` (reads a local status + URL mirror), `GET /dash/<hash>/<file>` (static MPD/segments, emits a single `keyAccessed` touch per served file). Built from `HttpListenerConfig` (which carries the hash-algorithm name); **owns** its `IHasher` (built via `QtHasher::fromName`). Holds no cache — the ready manifest URL arrives via the `statusChanged` signal. |
| `src/adapter/processor/PlaybackProcessor.{h,cpp}` | Pipeline FSM on its own `QThread`: login → download → ffmpeg-DASH → cache eviction. Built from `PlaybackProcessorConfig`; **owns** all its collaborators — builds the `IAuthenticator` (HCNetSDK), `LoginUseCase`, downloader/packager factories, and the `FileSystemStreamCache` internally (concrete types named only in the `.cpp`). Talks to `HttpListener` only via Qt signals/slots (emits `statusChanged(keyHex, status, url)`); marshals downloader/packager callbacks onto its thread with `QMetaObject::invokeMethod`. Prints `[login-ok]`/`[download]`/`[stream-ready]`/`[stream-error]`/`[login-fail]` to stdout/stderr. |
| `main/Session.h/.cpp`                             | Composition root, thin. Parses CLI into `PlaybackProcessorConfig` + `HttpListenerConfig`, constructs the two adapters (which build their own infra collaborators), moves them onto their `QThread`s and wires the signal graph. Owns only the `HCNetSDKBootstrap` and the two threads/adapters. |
| `main/main.cpp`                                   | 25-line entry point: `--version`, construct `QCoreApplication`, construct `Session`, `app.exec()`.   |

### DASH cache layout

```
out/build/amd64-debug/downloads/
├── <hash>.mp4                ← HCNetSDKDownloader output (intermediate)
└── <hash>/                   ← FfmpegDashPackager output
    ├── manifest.mpd
    ├── init-stream0.m4s
    └── chunk-stream0-NNNNN.m4s
```

`FileSystemStreamCache::dashExists()` validates the MPD (size ≥ 100 bytes,
starts with `<?xml` or `<MPD`) to guard against incomplete packages from a
previous crash.

### Threading rules

Three long-lived threads, plus a short-lived worker per active download:

- **Main thread** — runs the `QCoreApplication` event loop and owns the two
  `QThread` objects. Once `Session` has wired the graph it does no request work.
- **HTTP thread** (`m_httpThread`) — hosts `HttpListener` and its
  `QHttpServer`/`QTcpServer`. The servers are created inside
  `HttpListener::started()` (fired by `QThread::started`) so the sockets belong
  to this thread. All route handlers and the status mirror run here.
- **Processor thread** (`m_processorThread`) — hosts `PlaybackProcessor` (the
  pipeline FSM), the synchronous `LoginUseCase`, the `FileSystemStreamCache`
  (sole owner), and the ffmpeg `QProcess`.
- HTTP ↔ processor communication is **exclusively Qt signals/slots**, queued
  across threads: `playbackRequested` / `keyAccessed` → processor;
  `statusChanged(keyHex, status, url)` → the listener's status + URL mirror (the
  `url` is the ready manifest URL, copied across the thread boundary so the
  listener never touches the cache). There is no `IDispatcher` abstraction
  anymore. `keyAccessed` is a single per-file touch (not a start/end pair): the
  processor records the access time in a `keyHex → lastAccessMs` map and, at
  each eviction pass, skips keys touched within `kAccessTtlMs` (60 s) while
  pruning older entries. This needs no refcounting and gives a real protection
  window that spans the gaps between segment fetches.
- `HCNetSDKDownloader` owns its own `std::thread`; `start()` spawns it,
  `cancel()` flips an `std::atomic<bool>` the worker polls, the destructor
  joins. `PlaybackProcessor` wraps the worker's progress/finished callbacks in
  `QMetaObject::invokeMethod(this, …, Qt::QueuedConnection)` so every job-state
  mutation happens on the processor thread.
- `FfmpegDashPackager` runs ffmpeg as an **async** `QProcess` created on the
  processor thread; its finished signal fires on that thread's event loop and is
  still routed through `QMetaObject::invokeMethod` for symmetry.
- `LoginUseCase::ensureLoggedIn()` is **synchronous** and runs on the processor
  thread. Idle eviction is **sweep-on-access** — no timer thread.

---

## HTTP API (default port 8080)

Two-tier surface: a privileged control plane and a public polling plane.
All responses are JSON unless noted; all include `Access-Control-Allow-Origin: *`.

### Control plane (server-to-server)
```
POST /playback
  Header:  X-API-Key: <fixed-string-from-CLI-or-Config.h>
  Body:    {
             "ip":          "192.168.1.64",
             "port":        8000,
             "user":        "admin",
             "pass":        "…",
             "channel_id":  1,
             "start_time":  "20260525T130000",
             "end_time":    "20260525T140000"
           }
  → 200 {"poll_url": "http://<host>:8080/playback?id=<hash>"}
  → 400 {"status":"error","message":"bad payload: <field>"}
  → 401 {"status":"error","message":"invalid api key"}
```

> The `200` only means the request was **accepted**. Login + download run
> asynchronously on the processor thread; failures (bad credentials, SDK or
> ffmpeg errors) are logged to stderr and flip the job to `Failed`, which the
> polling plane currently surfaces as `pending`.

The hash input is `"<ip>:<port>/ch<channel>/<start>-<end>"` →
configurable hash algorithm (default: Blake2s-128) → first 16 hex chars.
`user`/`pass` are **excluded** from the hash:
the artifact identity is the recording, not the credentials, and including
the password would expose it to GPU brute-force against weak passwords.

### Polling plane (frontend, public)
```
GET /playback?id=<hash>
  → 200 {"status":"ready","url":"http://<host>:8080/dash/<hash>/manifest.mpd"}
  → 202 {"status":"pending"}
  → 404 {"status":"unknown_id"}
```

No API key. Pure read of `HttpListener`'s in-memory status + URL mirror, kept in
sync by the processor's `statusChanged` signal — which also carries the ready
manifest URL, so the listener never touches the cache, never reads cross-thread
state, and never triggers downloads. The hash itself is the capability: anyone
holding it can poll status, but they cannot start a new job without the API key.

### Static DASH delivery (frontend, public)
```
GET /dash/<hash>/manifest.mpd     Content-Type: application/dash+xml
GET /dash/<hash>/<file>.m4s       Content-Type: video/iso.segment
```

### Configuration

| CLI flag                       | Default                       | Purpose                                |
| ------------------------------ | ----------------------------- | -------------------------------------- |
| `--api-key=<str>`              | `visionplayback-dev-key`      | X-API-Key the control plane requires.  |
| `--port=<n>`                   | `8080`                        | HTTP listen port.                      |
| `--downloads-dir=<path>`       | `<appDir>/downloads`          | Where MP4 + DASH segments are written. |
| `--login-idle-timeout=<sec>`   | `600`                         | Camera session idle eviction.          |
| `--max-downloads-gb=<n>`       | `100`                         | Capacity cap for the downloads directory. Oldest DASH packages are evicted after each successful packaging until usage falls below the limit. Active jobs and hashes served within the last 60 s are never removed. |
| `--hash-algorithm=<name>`      | `blake2s-128`                 | Hash algorithm for `PlaybackKey` derivation. Supported: `blake2s-128`, `sha256`, `sha512`. Unknown values print an error and exit immediately. |

**Security note** — the default API key is a hard-coded placeholder defined
as `kDefaultApiKey` in [src/infra/Config.h](src/infra/Config.h). Replace
with real secret management (env, vault, rotated keys, HMAC-signed
requests) before any non-development deployment.

---

## Coding Conventions

- **Member variables**: `m_` prefix (e.g., `m_streamUseCase`, `m_cache`).
- **Slots / callbacks**: `on<Subject><Verb>` (e.g., `onPlaybackRequested`,
  `onDownloadFinished`, `onStatusChanged`) for Qt slots; the downloader/packager
  ports expose single-callback `setOn<Event>(std::function<...>)` setters.
- **Header guards**: `#pragma once`.
- **C++ standard**: C++20.
- **Domain and usecase**: pure C++ only — **never** `QString`/`QByteArray`/
  `QObject`/`NET_DVR_*` in `src/domain/` or `src/usecase/`.
- **HCNetSDK types** (`LONG`, `NET_DVR_TIME`, `DWORD`) are confined to
  `src/infra/dvr/`.
- **No comments** unless the WHY is non-obvious.

---

## 3rdparty Dependencies

| Library          | Location                | Used for                                    |
| ---------------- | ----------------------- | ------------------------------------------- |
| HCNetSDK 6.1.9.4 | `../3rdparty/HCNetSDK/` | Camera login, video download                |
| ffmpeg (system)  | PATH                    | DASH packaging (via `QProcess`)             |
| Qt 6.11          | `~/Qt/6.11.0/gcc_64/`   | Core + Network + HttpServer only (no GUI!)  |

OpenCV, ONNX Runtime, and ZXing are present in `../3rdparty/` but **not
linked** — do not add them without discussion.

---

## Quick Smoke Tests

```bash
# 1. Start the daemon (uses kDefaultApiKey)
LD_LIBRARY_PATH=/home/thaivd/TDIC/3rdparty/HCNetSDK/lib \
  ./out/build/amd64-debug/main/VisionPlayback &

# 2. POST a playback request (control plane)
POLL_URL=$(curl -s -X POST http://localhost:8080/playback \
  -H 'X-API-Key: visionplayback-dev-key' \
  -H 'Content-Type: application/json' \
  -d '{
        "ip":"<camera>","port":8000,"user":"admin","pass":"<pw>",
        "channel_id":1,
        "start_time":"20260525T130000",
        "end_time":"20260525T140000"
      }' | jq -r .poll_url)
echo "poll url = $POLL_URL"

# 3. Verify the API key is enforced
curl -i -X POST http://localhost:8080/playback -d '{}'    # → 401

# 4. Poll until ready (frontend plane)
while :; do
  CODE=$(curl -s -o /tmp/resp -w '%{http_code}' "$POLL_URL")
  cat /tmp/resp; echo
  [[ "$CODE" == "200" ]] && break
  sleep 2
done

# 5. Extract MPD URL and play externally
MPD=$(jq -r .url /tmp/resp)
vlc "$MPD"
```

Observe the daemon's stdout/stderr — `PlaybackProcessor` prints one tagged line
per state transition (`[login-ok]`, `[download]`, `[stream-ready]`,
`[stream-error]`, `[login-fail]`).

---

## Layer-boundary audit

The dependency direction is enforced both by CMake and by these mechanical
greps:

```bash
# domain must be Qt- and SDK-free (comments aside)  → must be empty
grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/domain/

# usecase must be Qt- AND SDK-free (Qt was removed in the Onion refactor) → empty
grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/usecase/

# adapter must not directly link vp_domain (reaches it transitively) → empty
grep -E 'target_link_libraries.*vp_domain' src/adapter/CMakeLists.txt

# raw HCNetSDK *types* (NET_DVR_*) stay out of the adapter → must be empty
grep -rE 'NET_DVR_' src/adapter/
```

The first three (plus the raw-`NET_DVR_` check) should produce only comment
lines, never real code references.

One grep that is **no longer empty** by design:

```bash
# adapter now instantiates infra impls — matches PlaybackProcessor.cpp
# (HCNetSDKAuthenticator / HCNetSDKDownloaderFactory) plus the CMakeLists comment
grep -rlE 'NET_DVR_|HCNetSDK' src/adapter/
```

This is expected: the adapters own their collaborators (see the Architecture
layering note). Only the HCNetSDK-named types trip this grep; the other infra
impls the adapters build (`FileSystemStreamCache`, `QtHasher`,
`FfmpegDashPackagerFactory`) don't contain those tokens but are equally
adapter-owned now. The invariant that still holds is that all such concrete
infra references live **only in adapter `.cpp` files** — never in an adapter
header.
