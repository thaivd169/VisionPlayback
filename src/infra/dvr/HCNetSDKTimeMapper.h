#pragma once
#include "HCNetSDK.h"
#include "PlaybackTime.h"

// The single allowed translation point between the Qt-free domain time
// representation and HCNetSDK's NET_DVR_TIME calendar struct. Keeping this
// boundary explicit means PlaybackTime can stay SDK-free.
//
// Parsing "YYYYMMDDTHHMMSS" lives in domain/PlaybackTime.h
// (parsePlaybackTimeCompact) — no SDK involvement.
namespace HCNetSDKTimeMapper {

inline NET_DVR_TIME toSdk(const PlaybackTime& t) {
    NET_DVR_TIME out{};
    out.dwYear   = t.year;
    out.dwMonth  = t.month;
    out.dwDay    = t.day;
    out.dwHour   = t.hour;
    out.dwMinute = t.minute;
    out.dwSecond = t.second;
    return out;
}

inline PlaybackTime toDomain(const NET_DVR_TIME& t) {
    PlaybackTime out;
    out.year   = static_cast<std::uint16_t>(t.dwYear);
    out.month  = static_cast<std::uint8_t>(t.dwMonth);
    out.day    = static_cast<std::uint8_t>(t.dwDay);
    out.hour   = static_cast<std::uint8_t>(t.dwHour);
    out.minute = static_cast<std::uint8_t>(t.dwMinute);
    out.second = static_cast<std::uint8_t>(t.dwSecond);
    return out;
}

} // namespace HCNetSDKTimeMapper
