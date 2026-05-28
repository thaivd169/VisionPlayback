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

Onion-style Clean Architecture. Four static libraries with a strict dependency
direction (see [main/Session.cpp](main/Session.cpp) for the composition root):

```
main  ─►  vp_adapter  ─►  vp_usecase  ─►  vp_domain
main  ─►  vp_infra ──────────────────────►  vp_domain
```

- `vp_domain` is pure C++ and owns the **port interfaces** (`IAuthenticator`,
  `IPlaybackDownloader`, `IDashPackager`, `IStreamCacheRepository`,
  `IDispatcher`) and the SHA-256 helper used by `makePlaybackKey`.
- `vp_usecase` is pure C++ (no Qt) and depends only on `vp_domain`. Async work
  is exposed via `std::function` callbacks; cross-thread marshalling goes
  through an injected `IDispatcher`.
- `vp_infra` implements the domain ports using HCNetSDK, ffmpeg (via
  `QProcess`), the filesystem, and Qt's event loop (`QtDispatcher`).
- `vp_adapter` is the **primary-adapter** layer only: HTTP API + console
  event logger. Its only declared dep is `vp_usecase` — domain types reach
  it transitively. It does NOT link `vp_infra`.
- `main` is the composition root; the one place that sees both adapter and
  infra and wires them together.

### Layer responsibilities

| Layer        | CMake target | Direct link deps                          | Audit grep (must be empty)                          |
| ------------ | ------------ | ----------------------------------------- | --------------------------------------------------- |
| `src/domain` | `vp_domain`  | std C++ only                              | `grep -rE 'Q[A-Z]\|NET_DVR_\|HCNetSDK' src/domain/`  |
| `src/usecase`| `vp_usecase` | `vp_domain` (PUBLIC)                      | `grep -rE 'Q[A-Z]\|NET_DVR_\|HCNetSDK' src/usecase/` |
| `src/infra`  | `vp_infra`   | `vp_domain` (PUBLIC) + `Qt::Core` (PUBLIC) + HCNetSDK (PRIVATE) | (no rule — infra *is* the technical layer) |
| `src/adapter`| `vp_adapter` | `vp_usecase` (PUBLIC) + Qt::Core/Network/HttpServer (PUBLIC) | `grep -rE 'NET_DVR_\|HCNetSDK' src/adapter/` |
| `main`       | `VisionPlayback` | `vp_adapter` + `vp_infra` + Qt        | composition only                                    |

Hard prohibitions enforced by CMake:
- `vp_adapter` must NOT link `vp_infra`.
- `vp_adapter` must NOT directly link `vp_domain` — it reaches domain only
  through `vp_usecase`'s PUBLIC link.

(Comments mentioning `HCNetSDK` are allowed everywhere — the audit cares
about real code.)

### Pipeline flow

```
POST /playback (X-API-Key) ──► ControlApi ──► LoginUseCase ──► IAuthenticator (HCNetSDK)
                                          └─► StreamPlaybackUseCase ─► IPlaybackDownloader (HCNetSDK, own thread)
                                                                       IDashPackager     (ffmpeg / QProcess)
                                                                       IStreamCacheRepository (FS)
                                                                       IDispatcher       (thread-hop)
GET  /playback?id=<hash>   ──► PollingApi  ──► (read-only state lookup)
GET  /dash/<hash>/*        ──► DashFileServer ─► downloads/<hash>/*
                                               ConsoleEventLogger ◄── use-case callbacks
```

### Key files

| File                                              | Role                                                                                                |
| ------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `src/domain/PlaybackKey.h`                        | 16-hex-char SHA-256 truncation + inline `makePlaybackKey(Credentials, Channel, TimeRange)`.         |
| `src/domain/PlaybackTime.h`                       | Calendar struct (mirrors `NET_DVR_TIME` shape). Parses `"YYYYMMDDTHHMMSS"`.                         |
| `src/domain/PlaybackRequest.h`                    | `SessionToken token; Channel channel; TimeRange range; PlaybackKey key;`                            |
| `src/domain/Credentials.h`                        | Pure `std::string` fields.                                                                          |
| `src/domain/CameraIdentity.h`                     | `(ip, port, user)` tuple — login-cache key in `LoginUseCase`.                                       |
| `src/domain/Sha256.{h,cpp}`                       | Compact SHA-256 used by `makePlaybackKey`.                                                          |
| `src/domain/I{Authenticator,PlaybackDownloader,DashPackager,StreamCacheRepository,Dispatcher}.h` | Qt-free port interfaces. Async ones expose `std::function` callbacks. |
| `src/usecase/StreamPlaybackUseCase.h/.cpp`        | Cache-first pipeline (Qt-free). Emits `onStreamReady / onStreamError / onDownloadProgress` callbacks. |
| `src/usecase/LoginUseCase.h/.cpp`                 | Cache-first SDK login (Qt-free). Sweep-on-access idle eviction.                                     |
| `src/infra/dvr/HCNetSDKDownloader.h/.cpp`         | `IPlaybackDownloader` impl. Owns its own `std::thread`; SDK polling.                                |
| `src/infra/dvr/HCNetSDKAuthenticator.h/.cpp`      | `IAuthenticator` impl. Wraps `NET_DVR_Login_V40` / `NET_DVR_Logout_V30`.                            |
| `src/infra/dvr/HCNetSDKTimeMapper.h`              | The only file that names both `NET_DVR_TIME` and `PlaybackTime`.                                    |
| `src/infra/packaging/FfmpegDashPackager.h/.cpp`   | `IDashPackager` impl. `ffmpeg -f dash` via `QProcess`.                                              |
| `src/infra/persistence/FileSystemStreamCache.h/.cpp` | `IStreamCacheRepository` impl. Owns `downloads/<hash>.mp4` + `downloads/<hash>/manifest.mpd`.    |
| `src/infra/dispatcher/QtDispatcher.h/.cpp`        | `IDispatcher` impl. Bounces callbacks onto the main thread via `QMetaObject::invokeMethod`.         |
| `src/infra/HCNetSDKBootstrap.{h,cpp}`             | RAII for `NET_DVR_Init` + `NET_DVR_Cleanup`.                                                        |
| `src/infra/Config.h`                              | `kDefaultApiKey`, `kDefaultPort`, `kDefaultLoginIdleSeconds`.                                       |
| `src/adapter/http/ControlApi.{h,cpp}`             | `POST /playback` — API-key check, login, dispatch, return poll URL.                                  |
| `src/adapter/http/PollingApi.{h,cpp}`             | `GET /playback?id=<hash>` — read-only status lookup.                                                |
| `src/adapter/http/DashFileServer.{h,cpp}`         | `GET /dash/<hash>/*` — static MPD + segment delivery.                                               |
| `src/adapter/http/ApiKeyGuard.{h,cpp}`            | Constant-time `X-API-Key` comparator.                                                               |
| `src/adapter/http/JsonCodec.{h,cpp}`              | Confines all `QJson*` ↔ domain conversions to one file.                                             |
| `src/adapter/console/ConsoleEventLogger.{h,cpp}`  | Passive subscriber that registers lambdas via the use cases' callback setters.                      |
| `main/Session.h/.cpp`                             | Composition root. Parses CLI, builds infra → use cases → primary adapters, registers routes.        |
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
- `HCNetSDKDownloader` owns its own `std::thread`; `start()` spawns it,
  `cancel()` flips an `std::atomic<bool>` the worker polls, and the
  destructor joins the thread. The use case wraps the worker's callbacks
  with `IDispatcher::post(...)` so all state mutations happen on the
  main thread.
- `FfmpegDashPackager` runs the ffmpeg `QProcess` on the calling thread
  (main); its callback fires on the same thread but is still routed through
  `IDispatcher::post` for symmetry.
- `LoginUseCase::ensureLoggedIn()` is **synchronous** and runs on the HTTP
  handler's thread (the main thread). Idle eviction is **sweep-on-access**
  — no timer thread.

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
- **Callback setters / handlers**: `on<Subject><Verb>` (e.g., `onStreamReady`,
  `onDownloadFinished`). Use cases expose `void on<Event>(std::function<...>)`
  setters that append to a subscriber vector.
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

Observe the daemon's stdout — `ConsoleEventLogger` prints one tagged line per
state transition (`[login-ok]`, `[download]`, `[stream-ready]`, etc.).

---

## Layer-boundary audit

The dependency direction is enforced both by CMake (no upward link declarations
exist) and by these mechanical greps:

```bash
# domain must be Qt- and SDK-free (comments aside)
grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/domain/

# usecase must be Qt- AND SDK-free (Qt was removed in the Onion refactor)
grep -rE 'Q[A-Z]|NET_DVR_|HCNetSDK' src/usecase/

# adapter must be SDK-free (concrete HCNetSDK code lives in src/infra/dvr/)
grep -rE 'NET_DVR_|HCNetSDK' src/adapter/

# adapter must not directly link vp_domain (reaches it transitively via usecase)
grep -E 'target_link_libraries.*vp_domain' src/adapter/CMakeLists.txt
```

All four should produce only comment lines, never real code references.
