# VisionPlayback

Headless HTTP daemon that downloads surveillance video from Hikvision cameras
via HCNetSDK, packages it as MPEG-DASH with ffmpeg, and serves a control +
polling JSON API alongside static DASH segments. No GUI — clients consume the
served MPD URLs in any DASH player (VLC, browser, etc.).

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
#   --port=<n>                default: 8080
#   --downloads-dir=<path>    default: <appDir>/downloads
#   --login-idle-timeout=<s>  default: 600
```

---

### 1. POST `/playback` — control plane (requires API key)

Triggers (or no-ops if cached / in flight) the download → ffmpeg → DASH
pipeline and returns the polling URL.

```bash
curl -s -X POST http://localhost:8080/playback \
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

| Code | Body                                                              | When                            |
| ---- | ----------------------------------------------------------------- | ------------------------------- |
| 200  | `{"poll_url":"http://localhost:8080/playback?id=<hash>"}`         | Job queued (or already cached)  |
| 400  | `{"status":"error","message":"bad payload: <field>"}`             | Missing/wrong JSON field        |
| 401  | `{"status":"error","message":"invalid api key"}`                  | Header missing or doesn't match |
| 502  | `{"status":"error","message":"login failed: <hcnetsdk-error>"}`   | Synchronous SDK login failed    |

Time format is `YYYYMMDDTHHMMSS` (UTC). The `<hash>` is
`sha256_hex("ip:port/ch<n>/<start>-<end>")[..16]` — `user`/`pass` are
deliberately excluded.

---

### 2. GET `/playback?id=<hash>` — polling (public, no API key)

Pure status read. Replays of the same payload won't trigger a new job — only
POST does.

```bash
HASH=730ae17a983691b9
curl -s -w '\nHTTP %{http_code}\n' "http://localhost:8080/playback?id=$HASH"
```

**Responses:**

| Code | Body                                                                          |
| ---- | ----------------------------------------------------------------------------- |
| 200  | `{"status":"ready","url":"http://localhost:8080/dash/<hash>/manifest.mpd"}`   |
| 202  | `{"status":"pending"}`  (Downloading or Packaging)                            |
| 404  | `{"status":"unknown_id"}`  (server has no record — caller never POSTed)       |
| 400  | `{"status":"error","message":"missing id query param"}`                       |

Typical polling loop:

```bash
while :; do
  CODE=$(curl -s -o /tmp/resp -w '%{http_code}' \
              "http://localhost:8080/playback?id=$HASH")
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
# Fetch the manifest
curl -i "http://localhost:8080/dash/$HASH/manifest.mpd"

# Or play in VLC
vlc "http://localhost:8080/dash/$HASH/manifest.mpd"
```

| Path                                   | Content-Type             |
| -------------------------------------- | ------------------------ |
| `/dash/<hash>/manifest.mpd`            | `application/dash+xml`   |
| `/dash/<hash>/init-stream0.m4s`        | `video/iso.segment`      |
| `/dash/<hash>/chunk-stream0-NNNNN.m4s` | `video/iso.segment`      |

`..` in the path → 403. Missing file → 404.

---

### End-to-end example

```bash
# 1. POST and capture the poll URL
POLL=$(curl -s -X POST http://localhost:8080/playback \
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
```

CORS is permissive (`Access-Control-Allow-Origin: *`) so browsers can poll and
play directly. Watch the daemon's stdout for live `[login-ok]`, `[download]`,
`[stream-ready]` events from `ConsoleEventLogger`.
