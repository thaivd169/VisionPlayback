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

Four layers, each a static library with a strict dependency direction
(see [main/Session.cpp](main/Session.cpp) for the composition root):

```
main  ─►  vp_adapter  ─►  vp_usecase  ─►  vp_domain
                      ╲                     ▲
                       ╲────────────────────┘
vp_adapter ─► vp_infra
vp_usecase ─► vp_infra        (PRIVATE — for Sha256 only)
vp_domain  ─► (nothing)       (header-only INTERFACE library)
```

### Layer responsibilities

| Layer        | CMake target | Allowed deps                  | Audit invariant                                                    |
| ------------ | ------------ | ----------------------------- | ------------------------------------------------------------------ |
| `src/domain` | `vp_domain`  | std C++ only                  | `grep -rE 'Q[A-Z]\|NET_DVR_\|HCNetSDK' src/domain/` returns empty  |
| `src/usecase`| `vp_usecase` | `vp_domain`, `Qt::Core`       | `grep -rE 'NET_DVR_\|HCNetSDK\|hcnetsdk' src/usecase/` returns empty |
| `src/adapter`| `vp_adapter` | usecase, domain, infra, Qt, HCNetSDK, ffmpeg via QProcess | Implements port interfaces declared in `src/usecase/ports/`.       |
| `src/infra`  | `vp_infra`   | HCNetSDK (PRIVATE)            | Technical foundations: SHA-256, HCNetSDKBootstrap, Config defaults. |
| `main`       | `VisionPlayback` | adapter, infra            | Holds composition root (`Session`) + 20-line `main.cpp`.            |

(Comments mentioning `HCNetSDK` are allowed in usecase ports — the audit cares
about real code.)

### Pipeline flow

```
POST /playback (X-API-Key) ──► ControlApi ──► LoginUseCase ──► IAuthenticator (HCNetSDK)
                                          └─► StreamPlaybackUseCase ─► IPlaybackDownloader (HCNetSDK)
                                                                       IDashPackager     (ffmpeg)
                                                                       IStreamCacheRepository (FS)
GET  /playback?id=<hash>   ──► PollingApi  ──► (read-only state lookup)
GET  /dash/<hash>/*        ──► DashFileServer ─► downloads/<hash>/*
                                               ConsoleEventLogger ◄── use-case signals
```

### Key files

| File                                              | Role                                                                                                |
| ------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `src/domain/PlaybackKey.h`                        | 16-hex-char SHA-256 truncation. Cache key + public id in the polling URL.                           |
| `src/domain/PlaybackTime.h`                       | Qt-free calendar struct (mirrors `NET_DVR_TIME` shape). Parses `"YYYYMMDDTHHMMSS"`.                 |
| `src/domain/PlaybackRequest.h`                    | `SessionToken token; Channel channel; TimeRange range; PlaybackKey key;`                            |
| `src/domain/Credentials.h`                        | Pure `std::string` fields (no `NAME_LEN` macros).                                                   |
| `src/domain/CameraIdentity.h`                     | `(ip, port, user)` tuple — login-cache key in `LoginUseCase`.                                       |
| `src/usecase/StreamPlaybackUseCase.h/.cpp`        | Cache-first state machine. Emits `streamReady(PlaybackKey, url)`, `streamError`, `downloadProgress`. |
| `src/usecase/LoginUseCase.h/.cpp`                 | Cache-first SDK login keyed by `CameraIdentity`. Idle eviction via `QTimer`.                        |
| `src/usecase/ports/I{PlaybackDownloader,DashPackager,StreamCacheRepository,Authenticator}.h` | Port interfaces. Async ones inherit `QObject`, sync ones don't.       |
| `src/usecase/PlaybackKeyFactory.h/.cpp`           | `makePlaybackKey(Credentials, Channel, TimeRange)` → 16-hex `PlaybackKey`.                          |
| `src/adapter/dvr/HCNetSDKDownloader.h/.cpp`       | `IPlaybackDownloader` impl. `NET_DVR_GetFileByTime_V40` + RPC polling. Per-job `QThread`-friendly.   |
| `src/adapter/dvr/HCNetSDKAuthenticator.h/.cpp`    | `IAuthenticator` impl. Wraps `NET_DVR_Login_V40` / `NET_DVR_Logout_V30`.                            |
| `src/adapter/dvr/HCNetSDKTimeMapper.h`            | The only file that names both `NET_DVR_TIME` and `PlaybackTime`.                                    |
| `src/adapter/packaging/FfmpegDashPackager.h/.cpp` | `IDashPackager` impl. `ffmpeg -f dash` via `QProcess`.                                              |
| `src/adapter/persistence/FileSystemStreamCache.h/.cpp` | `IStreamCacheRepository` impl. Owns `downloads/<hash>.mp4` + `downloads/<hash>/manifest.mpd`. |
| `src/adapter/http/ControlApi.{h,cpp}`             | `POST /playback` — API-key check, login, dispatch, return poll URL.                                  |
| `src/adapter/http/PollingApi.{h,cpp}`             | `GET /playback?id=<hash>` — read-only status lookup.                                                |
| `src/adapter/http/DashFileServer.{h,cpp}`         | `GET /dash/<hash>/*` — static MPD + segment delivery.                                               |
| `src/adapter/http/ApiKeyGuard.{h,cpp}`            | Constant-time `X-API-Key` comparator.                                                               |
| `src/adapter/http/JsonCodec.{h,cpp}`              | Confines all `QJson*` ↔ domain conversions to one file.                                             |
| `src/adapter/console/ConsoleEventLogger.{h,cpp}`  | Passive subscriber to use-case signals; prints tagged stdout/stderr lines.                          |
| `src/infra/HCNetSDKBootstrap.{h,cpp}`             | RAII for `NET_DVR_Init` + `NET_DVR_Cleanup`.                                                        |
| `src/infra/Sha256.{h,cpp}`                        | Compact public-domain SHA-256 (`sha256_hex(std::string_view)`).                                     |
| `src/infra/Config.h`                              | `kDefaultApiKey`, `kDefaultPort`, `kDefaultLoginIdleSeconds`.                                       |
| `main/Session.h/.cpp`                             | Composition root. Parses CLI, builds adapters → use cases → APIs, registers routes.                  |
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

- `StreamPlaybackUseCase`, `LoginUseCase`, all HTTP route handlers, and
  `ConsoleEventLogger` all live on the **main thread**.
- Per-job `IPlaybackDownloader` workers (concrete: `HCNetSDKDownloader`) are
  `moveToThread()`-ed onto a dedicated `QThread`. Their signals reach the
  use case via `Qt::QueuedConnection` (auto-detected by thread affinity).
- After `onDownloadFinished()` succeeds, `job.thread` is cleared to `nullptr`
  immediately — the thread self-destructs via its `deleteLater` connections.
  Do not access `job.thread` after this point.
- `LoginUseCase::ensureLoggedIn()` is **synchronous** and runs on the HTTP
  handler's thread (the main thread). Login is fast enough that this hasn't
  required dispatch to a worker thread; if blocking becomes a concern, wrap
  with a one-shot `QThread`.

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
  → 502 {"status":"error","message":"login failed: <hcnetsdk code>"}
```

The hash input is `"<ip>:<port>/ch<channel>/<start>-<end>"` →
SHA-256 → first 16 hex chars. `user`/`pass` are **excluded** from the hash:
the artifact identity is the recording, not the credentials, and including
the password would expose it to GPU brute-force against weak passwords.

### Polling plane (frontend, public)
```
GET /playback?id=<hash>
  → 200 {"status":"ready","url":"http://<host>:8080/dash/<hash>/manifest.mpd"}
  → 202 {"status":"pending"}
  → 404 {"status":"unknown_id"}
```

No API key. Pure read of the cache repo + the use case's active-job table —
never triggers downloads. The hash itself is the capability: anyone holding
it can poll status, but they cannot start a new job without the API key.

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

**Security note** — the default API key is a hard-coded placeholder defined
as `kDefaultApiKey` in [src/infra/Config.h](src/infra/Config.h). Replace
with real secret management (env, vault, rotated keys, HMAC-signed
requests) before any non-development deployment.

---

## Coding Conventions

- **Member variables**: `m_` prefix (e.g., `m_streamUseCase`, `m_cache`).
- **Slots**: `on<Subject><Verb>` (e.g., `onDownloadFinished`, `onStreamReady`).
- **Header guards**: `#pragma once`.
- **Signals/slots**: new-style `connect()` with pointer-to-member.
- **C++ standard**: C++20.
- **Domain types**: `std::string`, `std::uint16_t`, plain structs — **never**
  `QString`/`QByteArray`/`NET_DVR_*` inside `src/domain/`.
- **HCNetSDK types** (`LONG`, `NET_DVR_TIME`, `DWORD`) are confined to
  `src/adapter/dvr/` and `src/infra/`.
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

Observe the daemon's stdout — `ConsoleEventLogger` prints one tagged line per
state transition (`[login-ok]`, `[download]`, `[stream-ready]`, etc.).

---

## Layer-boundary audit

The dependency direction is enforced both by CMake (no upward link declarations
exist) and by these mechanical greps:

```bash
# domain must be Qt- and SDK-free (comments aside)
grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/domain/

# usecase must be SDK-free
grep -rE 'NET_DVR_|HCNetSDK|hcnetsdk' src/usecase/
```

Both should produce only comment lines, never real code references.
