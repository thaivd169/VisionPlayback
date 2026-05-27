#pragma once
#include "Channel.h"
#include "Credentials.h"
#include "PlaybackKey.h"
#include "TimeRange.h"

// Builds the cache key for a given camera + channel + time-range.
// Implementation lives in PlaybackKeyFactory.cpp and uses vp::infra::sha256_hex.
//
// Canonical hash input: "<ip>:<port>/ch<channel>/<begin>-<end>"
// (user/pass deliberately excluded — see plan, §"Hashing & cache key").
PlaybackKey makePlaybackKey(const Credentials& credentials,
                            const Channel&     channel,
                            const TimeRange&   range);

// Renders a PlaybackTime as the canonical "YYYYMMDDTHHMMSS" string used in
// both the cache key and the public-facing time fields.
std::string formatPlaybackTime(const PlaybackTime& t);
