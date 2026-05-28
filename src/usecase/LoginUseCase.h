#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "CameraIdentity.h"
#include "Credentials.h"
#include "IAuthenticator.h"
#include "SessionToken.h"

// Cache-first SDK login. Sessions keyed by (ip, port, user) — password
// changes invalidate on next use via SDK failure. Expired entries are evicted
// on every ensureLoggedIn() call (sweep-on-access).
class LoginUseCase {
public:
    using SucceededCb = std::function<void(std::string ip, int userId)>;
    using FailedCb    = std::function<void(std::string ip, int errorCode)>;

    LoginUseCase(IAuthenticator* authenticator,
                 std::chrono::seconds idleTimeout);

    SessionToken ensureLoggedIn(const Credentials& credentials);
    int          lastErrorCode() const { return m_authenticator->lastErrorCode(); }

    void onLoginSucceeded(SucceededCb cb) { m_succeededCbs.push_back(std::move(cb)); }
    void onLoginFailed   (FailedCb cb)    { m_failedCbs   .push_back(std::move(cb)); }

private:
    struct Entry {
        SessionToken                          token;
        std::chrono::steady_clock::time_point lastUsed;
    };

    void sweepIdle();

    IAuthenticator*                                m_authenticator;
    std::chrono::seconds                           m_idleTimeout;
    std::unordered_map<CameraIdentity, Entry>      m_cache;

    std::vector<SucceededCb> m_succeededCbs;
    std::vector<FailedCb>    m_failedCbs;
};
