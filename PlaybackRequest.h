#pragma once
#include "HCNetSDK.h"
#include <QString>

struct PlaybackRequest {
    LONG         userId    = -1;
    int          channel   = 1;
    NET_DVR_TIME startTime = {};
    NET_DVR_TIME endTime   = {};
    QString      outputPath;
};
