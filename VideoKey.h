#pragma once
#include "HCNetSDK.h"
#include <QString>

namespace VideoKey {

inline QString formatTime(const NET_DVR_TIME& t) {
    return QString("%1%2%3T%4%5%6")
        .arg(t.dwYear,   4, 10, QChar('0'))
        .arg(t.dwMonth,  2, 10, QChar('0'))
        .arg(t.dwDay,    2, 10, QChar('0'))
        .arg(t.dwHour,   2, 10, QChar('0'))
        .arg(t.dwMinute, 2, 10, QChar('0'))
        .arg(t.dwSecond, 2, 10, QChar('0'));
}

// Returns canonical basename: "ch1_20260525T130000_20260525T140000"
inline QString make(int channel, const NET_DVR_TIME& start, const NET_DVR_TIME& end) {
    return QString("ch%1_%2_%3").arg(channel).arg(formatTime(start)).arg(formatTime(end));
}

// Inverse of formatTime: "YYYYMMDDTHHmmss" → NET_DVR_TIME
inline NET_DVR_TIME parseTime(const QString& s) {
    NET_DVR_TIME t = {};
    t.dwYear   = s.mid(0,  4).toUInt();
    t.dwMonth  = s.mid(4,  2).toUInt();
    t.dwDay    = s.mid(6,  2).toUInt();
    // s[8] == 'T'
    t.dwHour   = s.mid(9,  2).toUInt();
    t.dwMinute = s.mid(11, 2).toUInt();
    t.dwSecond = s.mid(13, 2).toUInt();
    return t;
}

} // namespace VideoKey
