#pragma once
#include "IAuthenticator.h"

// Synchronous wrapper around NET_DVR_Login_V40 / NET_DVR_Logout_V30.
// Carries the login logic previously lived in LoginDialog.cpp, sans Qt UI.
// Login is fast enough to run on the HTTP handler's thread; if blocking
// becomes a concern, callers can dispatch via a one-shot QThread.
class HCNetSDKAuthenticator : public IAuthenticator {
public:
    HCNetSDKAuthenticator() = default;

    SessionToken login(const Credentials& credentials) override;
    void         logout(SessionToken token) override;
    int          lastErrorCode() const override { return m_lastError; }

private:
    int m_lastError = 0;
};
