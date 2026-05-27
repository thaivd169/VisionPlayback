#pragma once

namespace vp::infra {

// RAII wrapper around NET_DVR_Init / NET_DVR_Cleanup. Construct once at
// startup (typically in Session); destruction guarantees Cleanup runs.
//
// Header is SDK-free so callers don't need HCNetSDK on their include path.
class HCNetSDKBootstrap {
public:
    explicit HCNetSDKBootstrap(const char* sdkLogDir = "./sdkLog/");
    ~HCNetSDKBootstrap();

    HCNetSDKBootstrap(const HCNetSDKBootstrap&)            = delete;
    HCNetSDKBootstrap& operator=(const HCNetSDKBootstrap&) = delete;
};

} // namespace vp::infra
