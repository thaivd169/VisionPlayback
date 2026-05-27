#include "HCNetSDKAuthenticator.h"
#include "HCNetSDK.h"

#include <cstring>

SessionToken HCNetSDKAuthenticator::login(const Credentials& credentials) {
    NET_DVR_USER_LOGIN_INFO loginInfo{};
    NET_DVR_DEVICEINFO_V40  deviceInfo{};

    std::strncpy(loginInfo.sDeviceAddress,
                 credentials.ip.c_str(),
                 sizeof(loginInfo.sDeviceAddress) - 1);
    loginInfo.wPort = credentials.port;
    std::strncpy(loginInfo.sUserName,
                 credentials.user.c_str(),
                 sizeof(loginInfo.sUserName) - 1);
    std::strncpy(loginInfo.sPassword,
                 credentials.pass.c_str(),
                 sizeof(loginInfo.sPassword) - 1);
    loginInfo.bUseAsynLogin = 0;

    const LONG userId = NET_DVR_Login_V40(&loginInfo, &deviceInfo);
    m_lastError = (userId < 0) ? NET_DVR_GetLastError() : 0;
    return static_cast<SessionToken>(userId);
}

void HCNetSDKAuthenticator::logout(SessionToken token) {
    if (token < 0) return;
    NET_DVR_Logout_V30(static_cast<LONG>(token));
}
