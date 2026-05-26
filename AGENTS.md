# VisionPlayback — Agent Guide

Qt6/C++ desktop application that downloads surveillance video from Hikvision cameras via HCNetSDK and serves it as MPEG-DASH VOD over an embedded HTTP server.

---

## Build

Prerequisites: Qt 6.11, CMake 3.16+, Ninja, ffmpeg on PATH, HCNetSDK at `../3rdparty/HCNetSDK/`.

```bash
# Configure (debug)
cmake --preset amd64-debug

# Build
cmake --build --preset amd64-debug

# Run
./out/build/amd64-debug/VisionPlayback
```

For release swap `amd64-debug` → `amd64-release`. The presets are in `CMakePresets.json`.

**CMake auto-features** (`AUTOUIC`, `AUTOMOC`, `AUTORCC`) are ON — do not run `moc`/`uic` manually or add generated files to the source list.

---

## Run

```bash
LD_LIBRARY_PATH=/home/thaivd/TDIC/3rdparty/HCNetSDK/lib \
  ./out/build/amd64-debug/VisionPlayback
```

The binary sets `BUILD_RPATH` so the explicit `LD_LIBRARY_PATH` is only needed when running from a custom working directory. The embedded HTTP server starts on port 8080.

---

## Architecture

```
LoginDialog → MainWindow
                ├── StreamJobManager   (download → package → serve pipeline)
                │     ├── DownloadWorker  (QThread, HCNetSDK)
                │     └── DashPackager    (QProcess, ffmpeg)
                ├── DashServer         (QHttpServer, port 8080)
                └── VideoPlayerWidget  (QMediaPlayer, plays MPD URLs)
```

### Key files

| File                           | Role                                                                                    |
| ------------------------------ | --------------------------------------------------------------------------------------- |
| `VideoKey.h`                   | Header-only. Canonical key `ch<N>_<YYYYMMDDTHHMMSS>_<YYYYMMDDTHHMMSS>`.                 |
| `StreamJobManager.h/.cpp`      | Cache-first state machine. Emits `streamReady(url)`, `streamError`, `downloadProgress`. |
| `DownloadWorker.h/.cpp`        | Blocking HCNetSDK download, runs on a `QThread`.                                        |
| `DashPackager.h/.cpp`          | Async `ffmpeg -f dash` via `QProcess`.                                                  |
| `DashServer.h/.cpp`            | Routes `/stream?…` (JSON status) and `/dash/<key>/<file>` (static files).               |
| `VideoPlayerWidget.h/.cpp`     | Play/pause, seek, 0.5×–16× speed. Accepts both `file://` and `http://` URLs.            |
| `MainWindow.h/.cpp`            | Owns `DashServer` and `StreamJobManager`. Wires all signals to UI.                      |
| `PlaybackDialog.h/.cpp`        | Modal for channel + time range selection.                                               |
| `PlaybackManagerDialog.h/.cpp` | File manager for cached videos (rename/delete).                                         |
| `LoginDialog.h/.cpp`           | Camera login via `QtConcurrent::run()`.                                                 |

### DASH cache layout

```
out/build/amd64-debug/downloads/
├── ch1_20260525T130000_20260525T140000.mp4    ← DownloadWorker output
└── ch1_20260525T130000_20260525T140000/       ← DashPackager output
    ├── manifest.mpd
    ├── init-stream0.m4s
    └── chunk-stream0-NNNNN.m4s
```

`StreamJobManager::dashExists()` validates the MPD (size ≥ 100 bytes, starts with `<?xml` or `<MPD`) to guard against incomplete packages from a previous crash.

### Threading rules

- `StreamJobManager`, `DashServer`, and `DashPackager` all live on the **main thread**.
- `DownloadWorker` is `moveToThread()`-ed to a dedicated `QThread`. Its signals are delivered to the main thread via `Qt::QueuedConnection` — no mutex needed.
- After `onDownloadFinished()` succeeds, `job.thread` is set to `nullptr` immediately (the thread self-destructs via its `deleteLater` connections). Do not access `job.thread` after this point.

---

## Coding Conventions

- **Member variables**: `m_` prefix (e.g., `m_userId`, `m_jobManager`).
- **Slots**: `on<Subject><Verb>` (e.g., `onDownloadFinished`, `onStreamReady`).
- **Header guards**: `#pragma once`.
- **Signals/slots**: new-style `connect()` with pointer-to-member; never `SIGNAL()`/`SLOT()` macros.
- **C++ standard**: C++20.
- **Qt types**: prefer `qint64`, `QString`, `QUrl` over bare `int64_t`, `std::string`, `std::string`.
- **HCNetSDK types**: `LONG userId` (not `long`), `NET_DVR_TIME`, `DWORD`; include from `../3rdparty/HCNetSDK/incEn/`.
- **No comments** unless the WHY is non-obvious (a constraint, workaround, or invariant).

---

## 3rdparty Dependencies

| Library          | Location                | Used for                        |
| ---------------- | ----------------------- | ------------------------------- |
| HCNetSDK 6.1.9.4 | `../3rdparty/HCNetSDK/` | Camera login, video download    |
| ffmpeg (system)  | PATH                    | DASH packaging (via `QProcess`) |
| Qt 6.11          | `~/Qt/6.11.0/gcc_64/`   | All UI, networking, multimedia  |

OpenCV, ONNX Runtime, and ZXing are present in `../3rdparty/` but **not linked** — do not add them without discussion.

---

## HTTP API (port 8080)

```
GET /stream?channel_id=1&start_time=20260525T130000&end_time=20260525T140000
  → 200 {"status":"ready","url":"http://localhost:8080/dash/.../manifest.mpd"}
  → 202 {"status":"pending"}

GET /dash/<key>/manifest.mpd    Content-Type: application/dash+xml
GET /dash/<key>/<file>.m4s      Content-Type: video/iso.segment
```

All responses include `Access-Control-Allow-Origin: *`.

---

## Quick Smoke Tests

```bash
# 1. DASH packaging
ffmpeg -y -i downloads/some.mp4 -c copy -f dash -seg_duration 4 /tmp/test_dash/manifest.mpd

# 2. HTTP server (while app is running)
curl -v http://localhost:8080/dash/<key>/manifest.mpd

# 3. VLC external playback
vlc http://localhost:8080/dash/<key>/manifest.mpd
```
