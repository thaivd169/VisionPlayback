#include "HCNetSDKBootstrap.h"
#include "HCNetSDK.h"

namespace vp::infra {

HCNetSDKBootstrap::HCNetSDKBootstrap(const char* sdkLogDir) {
    NET_DVR_Init();
    NET_DVR_SetLogToFile(3, const_cast<char*>(sdkLogDir));
}

HCNetSDKBootstrap::~HCNetSDKBootstrap() {
    NET_DVR_Cleanup();
}

} // namespace vp::infra
