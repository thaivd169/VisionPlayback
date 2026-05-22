#pragma once
#include "HCNetSDK.h"
#include <cstdint>

struct LoginInfo {
    char     ip[NET_DVR_DEV_ADDRESS_MAX_LEN] = {};
    uint16_t port = 8000;
    char     user[NAME_LEN] = {};
    char     pass[NAME_LEN] = {};
};
