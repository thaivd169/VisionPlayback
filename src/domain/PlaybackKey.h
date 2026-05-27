#pragma once
#include <functional>
#include <string>

// 16-hex-char SHA-256 truncation used as the cache key, on-disk directory
// name, and public id in both the polling URL and the static DASH URL.
// Computed in usecase via infra/Sha256 — domain stays oblivious to hashing.
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
