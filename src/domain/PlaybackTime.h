#pragma once
#include <cstdint>
#include <string_view>

// Broken-down calendar fields, mirroring HCNetSDK's NET_DVR_TIME but free of
// any SDK dependency. The adapter layer (HCNetSDKTimeMapper) is the only
// translation point between this and NET_DVR_TIME.
struct PlaybackTime {
    std::uint16_t year   = 0;
    std::uint8_t  month  = 0;
    std::uint8_t  day    = 0;
    std::uint8_t  hour   = 0;
    std::uint8_t  minute = 0;
    std::uint8_t  second = 0;
};

// Parses "YYYYMMDDTHHMMSS". Returns an all-zero PlaybackTime on malformed
// input — caller is responsible for validation.
inline PlaybackTime parsePlaybackTimeCompact(std::string_view s) {
    PlaybackTime out;
    if (s.size() < 15) return out;
    auto toU = [&](std::size_t pos, std::size_t len) -> unsigned {
        unsigned v = 0;
        for (std::size_t i = 0; i < len; ++i) {
            const char c = s[pos + i];
            if (c < '0' || c > '9') return 0u;
            v = v * 10 + static_cast<unsigned>(c - '0');
        }
        return v;
    };
    out.year   = static_cast<std::uint16_t>(toU(0,  4));
    out.month  = static_cast<std::uint8_t>(toU(4,  2));
    out.day    = static_cast<std::uint8_t>(toU(6,  2));
    // s[8] == 'T'
    out.hour   = static_cast<std::uint8_t>(toU(9,  2));
    out.minute = static_cast<std::uint8_t>(toU(11, 2));
    out.second = static_cast<std::uint8_t>(toU(13, 2));
    return out;
}
