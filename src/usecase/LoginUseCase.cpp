#include "LoginUseCase.h"

LoginUseCase::LoginUseCase(IAuthenticator* authenticator,
                           std::chrono::seconds idleTimeout)
    : m_authenticator(authenticator),
      m_idleTimeout(idleTimeout) {}

SessionToken LoginUseCase::ensureLoggedIn(const Credentials& credentials) {
    sweepIdle();

    const CameraIdentity id{ credentials.ip, credentials.port, credentials.user };
    const auto now = std::chrono::steady_clock::now();

    if (auto it = m_cache.find(id); it != m_cache.end()) {
        it->second.lastUsed = now;
        return it->second.token;
    }

    const SessionToken token = m_authenticator->login(credentials);
    if (token < 0) {
        return token;
    }

    m_cache.emplace(id, Entry{ token, now });
    return token;
}

void LoginUseCase::sweepIdle() {
    const auto cutoff = std::chrono::steady_clock::now() - m_idleTimeout;
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->second.lastUsed < cutoff) {
            m_authenticator->logout(it->second.token);
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}
