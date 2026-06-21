# VisionPlayback

headless HTTP daemon that downloads surveillance video from Hikvision cameras
via HCNetSDK, packages it as MPEG-DASH with ffmpeg, and serves a control +
polling JSON API alongside static DASH segments. It can also hand back a whole
recording — or a time-clipped sub-range — as a single downloadable MP4. No GUI —
clients consume the served MPD URLs in any DASH player (VLC, browser, etc.).

For architecture, build instructions, layer responsibilities, and coding
conventions, see [AGENTS.md](AGENTS.md).

---

## HTTP API — curl reference

### Start the daemon

```bash
LD_LIBRARY_PATH=/home/thaivd/TDIC/3rdparty/HCNetSDK/lib \
  ./out/build/amd64-debug/main/VisionPlayback
# Optional overrides:
#   --api-key=<str>           default: visionplayback-dev-key
#   --port=<n>                default: 18068
#   --downloads-dir=<path>    default: <appDir>/downloads
#   --login-idle-timeout=<s>  default: 600
```

---

### 1. POST `/playback` — control plane (requires API key)

Triggers (or no-ops if cached / in flight) the download → ffmpeg → DASH
pipeline and returns the polling URL.

```bash
curl -s -X POST http://localhost:18068/playback \
  -H 'X-API-Key: visionplayback-dev-key' \
  -H 'Content-Type: application/json' \
  -d '{
        "ip":         "192.168.1.64",
        "port":       8000,
        "user":       "admin",
        "pass":       "your-password",
        "channel_id": 1,
        "start_time": "20260525T130000",
        "end_time":   "20260525T140000"
      }'
```

**Responses:**

| Code | Body                                                            | When                            |
| ---- | --------------------------------------------------------------- | ------------------------------- |
| 200  | `{"poll_url":"http://localhost:18068/playback?id=<hash>"}`      | Job queued (or already cached)  |
| 400  | `{"status":"error","message":"bad payload: <field>"}`           | Missing/wrong JSON field        |
| 401  | `{"status":"error","message":"invalid api key"}`                | Header missing or doesn't match |
| 502  | `{"status":"error","message":"login failed: <hcnetsdk-error>"}` | Synchronous SDK login failed    |

Time format is `YYYYMMDDTHHMMSS` (UTC). The `<hash>` is
`sha256_hex("ip:port/ch<n>/<start>-<end>")[..16]` — `user`/`pass` are
deliberately excluded.

---

### 2. GET `/playback?id=<hash>` — polling (public, no API key)

Pure status read. Replays of the same payload won't trigger a new job — only
POST does.

```bash
HASH=730ae17a983691b9
curl -s -w '\nHTTP %{http_code}\n' "http://localhost:18068/playback?id=$HASH"
```

**Responses:**

| Code | Body                                                                         |
| ---- | ---------------------------------------------------------------------------- |
| 200  | `{"status":"ready","url":"http://localhost:18068/dash/<hash>/manifest.mpd"}` |
| 202  | `{"status":"pending"}`  (Downloading or Packaging)                           |
| 404  | `{"status":"unknown_id"}`  (server has no record — caller never POSTed)      |
| 400  | `{"status":"error","message":"missing id query param"}`                      |

Typical polling loop:

```bash
while :; do
  CODE=$(curl -s -o /tmp/resp -w '%{http_code}' \
              "http://localhost:18068/playback?id=$HASH")
  cat /tmp/resp; echo
  [[ "$CODE" == "200" ]] && break
  [[ "$CODE" == "404" ]] && { echo "unknown hash"; break; }
  sleep 2
done
```

---

### 3. GET `/dash/<hash>/...` — static DASH delivery (public)

The MPEG-DASH manifest and segment files. Feed this URL straight into a DASH
player.

```bash
// Fetch the manifest
curl -i "http://localhost:18068/dash/$HASH/manifest.mpd"

# Or play in VLC
vlc "http://localhost:18068/dash/$HASH/manifest.mpd"
```

| Path                                   | Content-Type           |
| -------------------------------------- | ---------------------- |
| `/dash/<hash>/manifest.mpd`            | `application/dash+xml` |
| `/dash/<hash>/init-stream0.m4s`        | `video/iso.segment`    |
| `/dash/<hash>/chunk-stream0-NNNNN.m4s` | `video/iso.segment`    |

`..` in the path → 403. Missing file → 404.

---

### 4. GET `/download/<hash>` — download a full playback or a clip (public)

Returns the playback as a single saveable MP4, built on demand from the DASH
package. The playback must already be **ready** (request it first via POST
`/playback`). Time bounds are **offsets in seconds from the start of the
recording**; omit both to get the whole thing.

```bash
HASH=730ae17a983691b9

# Whole playback (-OJ saves it under the server-suggested filename)
  curl -OJ "http://localhost:18068/download/$HASH"

# A clip — minute 2 to minute 5 (start=120, end=300)
  curl -OJ "http://localhost:18068/download/$HASH?start=120&end=300"

# From 30s in to the end
  curl -OJ "http://localhost:18068/download/$HASH?start=30"
```

**Responses:**

| Code | Body / result                                                                                  | When                                                               |
| ---- | ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| 200  | MP4 bytes; `Content-Disposition: attachment; filename="playback-<hash>[-<start>s-<end>s].mp4"` | Export succeeded                                                   |
| 400  | `{"status":"error",...}`                                                                       | Bad `<hash>` (not 16 hex) or bad range (`end<=start`, non-numeric) |
| 404  | `{"status":"error","message":"not ready"}`                                                     | Playback absent or not yet ready                                   |
| 500  | `{"status":"error",...}`                                                                       | ffmpeg failed                                                      |

`Content-Type` is `video/mp4`. The remux is a stream copy (no re-encode), so
cuts land on the nearest keyframe — a clip may be off by up to one keyframe
interval. The server prepares the file asynchronously and returns it in a single
request, then deletes it right after sending; nothing accumulates on disk.


### End-to-end example

```bash
# 1. POST and capture the poll URL
POLL=$(curl -s -X POST http://localhost:18068/playback \
  -H 'X-API-Key: visionplayback-dev-key' \
  -H 'Content-Type: application/json' \
  -d '{"ip":"192.168.1.64","port":8000,"user":"admin","pass":"pw",
       "channel_id":1,
       "start_time":"20260525T130000","end_time":"20260525T140000"}' \
  | jq -r .poll_url)
echo "polling $POLL"

# 2. Poll
while :; do
  RESP=$(curl -s "$POLL"); echo "$RESP"
  echo "$RESP" | jq -e '.status == "ready"' >/dev/null && break
  sleep 2
done

# 3. Get manifest and play
MPD=$(echo "$RESP" | jq -r .url)
vlc "$MPD"

# 4. …or download the whole recording (or a clip) as one MP4
HASH=${POLL##*id=}
curl -OJ "http://localhost:18068/download/$HASH"                 # full
curl -OJ "http://localhost:18068/download/$HASH?start=120&end=300"  # minute 2–5
```

CORS is permissive (`Access-Control-Allow-Origin: *`) so browsers can poll,
play, and download directly. Watch the daemon's stdout for live `[login-ok]`,
`[download]`, `[stream-ready]`, and `[export]`/`[export-ready]` events from
`PlaybackProcessor`.
