#include "LoginUseCase.h"

#include <QDateTime>

namespace {
std::int64_t nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}
} // namespace

LoginUseCase::LoginUseCase(IAuthenticator* authenticator,
                           std::chrono::seconds idleTimeout,
                           QObject* parent)
    : QObject(parent),
      m_authenticator(authenticator),
      m_idleTimeout(idleTimeout) {
    m_sweepTimer.setInterval(30 * 1000); // sweep every 30s
    connect(&m_sweepTimer, &QTimer::timeout, this, &LoginUseCase::sweepIdle);
    m_sweepTimer.start();
}

SessionToken LoginUseCase::ensureLoggedIn(const Credentials& credentials) {
    const CameraIdentity id{ credentials.ip, credentials.port, credentials.user };

    if (auto it = m_cache.find(id); it != m_cache.end()) {
        it->lastUsedMs = nowMs();
        return it->token;
    }

    const SessionToken token = m_authenticator->login(credentials);
    if (token < 0) {
        emit loginFailed(QString::fromStdString(credentials.ip),
                         m_authenticator->lastErrorCode());
        return token;
    }

    m_cache.insert(id, Entry{ token, nowMs() });
    emit loginSucceeded(QString::fromStdString(credentials.ip),
                        static_cast<int>(token));
    return token;
}

void LoginUseCase::sweepIdle() {
    const std::int64_t cutoff = nowMs() -
        static_cast<std::int64_t>(m_idleTimeout.count()) * 1000;

    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->lastUsedMs < cutoff) {
            m_authenticator->logout(it->token);
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}
