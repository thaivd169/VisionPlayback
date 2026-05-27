#pragma once
#include "Channel.h"
#include "PlaybackKey.h"
#include "SessionToken.h"
#include "TimeRange.h"

// Qt- and SDK-free playback request. Adapters translate to NET_DVR_PLAYCOND
// (HCNetSDKTimeMapper) at their boundary.
struct PlaybackRequest {
    SessionToken token = -1;
    Channel      channel;
    TimeRange    range;
    PlaybackKey  key;
};
