#pragma once
#include <cstdint>

// Replaces HCNetSDK's LONG so domain headers don't pull <HCNetSDK.h>.
using SessionToken = std::int64_t;
