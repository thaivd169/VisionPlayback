#include "PlaybackKeyFactory.h"
#include "Sha256.h"

#include <array>
#include <cstdio>
#include <string>

namespace {

constexpr std::size_t kKeyHexLen = 16;

void appendZeroPadded(std::string& out, unsigned value, int width) {
    char tmp[16];
    std::snprintf(tmp, sizeof(tmp), "%0*u", width, value);
    out.append(tmp);
}

} // namespace

std::string formatPlaybackTime(const PlaybackTime& t) {
    std::string s;
    s.reserve(15);
    appendZeroPadded(s, t.year,   4);
    appendZeroPadded(s, t.month,  2);
    appendZeroPadded(s, t.day,    2);
    s.push_back('T');
    appendZeroPadded(s, t.hour,   2);
    appendZeroPadded(s, t.minute, 2);
    appendZeroPadded(s, t.second, 2);
    return s;
}

PlaybackKey makePlaybackKey(const Credentials& credentials,
                            const Channel&     channel,
                            const TimeRange&   range) {
    std::string input;
    input.reserve(64);
    input.append(credentials.ip);
    input.push_back(':');
    appendZeroPadded(input, credentials.port, 0);
    input.append("/ch");
    appendZeroPadded(input, static_cast<unsigned>(channel.id), 0);
    input.push_back('/');
    input.append(formatPlaybackTime(range.begin));
    input.push_back('-');
    input.append(formatPlaybackTime(range.end));

    const std::string full = vp::infra::sha256_hex(input);
    return PlaybackKey{ full.substr(0, kKeyHexLen) };
}
