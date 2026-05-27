#pragma once

enum class StreamStatus {
    Unknown,      // no record of the key — caller never asked for it
    Pending,      // queued but not started
    Downloading,  // raw mp4 transfer in progress
    Packaging,    // ffmpeg → DASH in progress
    Ready,        // manifest.mpd written and servable
    Failed,       // terminal error
};
