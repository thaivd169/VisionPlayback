#pragma once
#include <cstdio>
#include <functional>
#include <string>

#include "Channel.h"
#include "Credentials.h"
#include "IHasher.h"
#include "PlaybackTime.h"
#include "TimeRange.h"

// 16-hex-char SHA-256 truncation used as the cache key, on-disk directory
// name, and public id in both the polling URL and the static DASH URL.
struct PlaybackKey {
    std::string hex;

    bool operator==(const PlaybackKey& o) const noexcept { return hex == o.hex; }
    bool operator<(const PlaybackKey& o)  const noexcept { return hex <  o.hex; }
};

namespace std {
template <>
struct hash<PlaybackKey> {
    std::size_t operator()(const PlaybackKey& k) const noexcept {
        return std::hash<std::string>{}(k.hex);
    }
};
} // namespace std

// Renders a PlaybackTime as the canonical "YYYYMMDDTHHMMSS" string used in
// both the cache key and the public-facing time fields.
inline std::string formatPlaybackTime(const PlaybackTime& t) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04u%02u%02uT%02u%02u%02u",
                  static_cast<unsigned>(t.year),
                  static_cast<unsigned>(t.month),
                  static_cast<unsigned>(t.day),
                  static_cast<unsigned>(t.hour),
                  static_cast<unsigned>(t.minute),
                  static_cast<unsigned>(t.second));
    return std::string(buf);
}

// Builds the cache key for a given camera + channel + time-range.
// Canonical hash input: "<ip>:<port>/ch<channel>/<begin>-<end>"
// user/pass are deliberately excluded — the artifact identity is the
// recording, not the credentials.
inline PlaybackKey makePlaybackKey(const Credentials& credentials,
                                   const Channel&     channel,
                                   const TimeRange&   range,
                                   const IHasher&     hasher) {
    char portBuf[8];
    std::snprintf(portBuf, sizeof(portBuf), "%u",
                  static_cast<unsigned>(credentials.port));
    char chanBuf[16];
    std::snprintf(chanBuf, sizeof(chanBuf), "%d", channel.id);

    std::string input;
    input.reserve(64);
    input.append(credentials.ip);
    input.push_back(':');
    input.append(portBuf);
    input.append("/ch");
    input.append(chanBuf);
    input.push_back('/');
    input.append(formatPlaybackTime(range.begin));
    input.push_back('-');
    input.append(formatPlaybackTime(range.end));

    return PlaybackKey{ hasher.hexDigest(input).substr(0, 16) };
}
