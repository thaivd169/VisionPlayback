#pragma once
#include "Channel.h"
#include "Credentials.h"
#include "PlaybackKey.h"
#include "SessionToken.h"
#include "TimeRange.h"

// Qt- and SDK-free playback request. Adapters translate to NET_DVR_PLAYCOND
// (HCNetSDKTimeMapper) at their boundary.
struct PlaybackRequest {
    Credentials  credentials;
    SessionToken token = -1;
    Channel      channel;
    TimeRange    range;
    PlaybackKey  key;
};
