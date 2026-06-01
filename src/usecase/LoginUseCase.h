#pragma once
#include <chrono>
#include <string>
#include <unordered_map>

#include "CameraIdentity.h"
#include "Credentials.h"
#include "IAuthenticator.h"
#include "SessionToken.h"

// Cache-first SDK login. Sessions keyed by (ip, port, user) — password
// changes invalidate on next use via SDK failure. Expired entries are evicted
// on every ensureLoggedIn() call (sweep-on-access).
class LoginUseCase {
public:
    LoginUseCase(IAuthenticator* authenticator,
                 std::chrono::seconds idleTimeout);

    SessionToken ensureLoggedIn(const Credentials& credentials);
    int          lastErrorCode() const { return m_authenticator->lastErrorCode(); }

private:
    struct Entry {
        SessionToken                          token;
        std::chrono::steady_clock::time_point lastUsed;
    };

    void sweepIdle();

    IAuthenticator*                                m_authenticator;
    std::chrono::seconds                           m_idleTimeout;
    std::unordered_map<CameraIdentity, Entry>      m_cache;
};
