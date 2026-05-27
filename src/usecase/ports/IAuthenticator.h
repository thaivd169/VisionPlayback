#pragma once
#include "Credentials.h"
#include "SessionToken.h"

// Synchronous SDK login. Implemented in adapter/dvr/HCNetSDKAuthenticator.
// `login` returns >= 0 on success (the SDK user id) and < 0 on failure;
// failure detail is exposed via `lastErrorCode()`.
class IAuthenticator {
public:
    virtual ~IAuthenticator() = default;

    virtual SessionToken login(const Credentials& credentials) = 0;
    virtual void         logout(SessionToken token) = 0;
    virtual int          lastErrorCode() const = 0;
};
