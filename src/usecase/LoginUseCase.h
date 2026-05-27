#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <chrono>
#include <cstddef>

#include "CameraIdentity.h"
#include "Credentials.h"
#include "IAuthenticator.h"
#include "SessionToken.h"

// QHash needs qHash() for CameraIdentity.
inline std::size_t qHash(const CameraIdentity& c, std::size_t seed = 0) noexcept {
    return ::qHashMulti(seed,
                        QByteArray::fromStdString(c.ip),
                        c.port,
                        QByteArray::fromStdString(c.user));
}

// Cache-first SDK login. Sessions keyed by (ip, port, user) — password
// changes invalidate on next use via SDK failure. Idle-evicted entries
// trigger NET_DVR_Logout_V30 via the injected IAuthenticator.
class LoginUseCase : public QObject {
    Q_OBJECT
public:
    LoginUseCase(IAuthenticator* authenticator,
                 std::chrono::seconds idleTimeout,
                 QObject* parent = nullptr);

    // Returns a valid SessionToken on success, < 0 on failure (use
    // lastErrorCode() to inspect SDK error).
    SessionToken ensureLoggedIn(const Credentials& credentials);

    int lastErrorCode() const { return m_authenticator->lastErrorCode(); }

signals:
    void loginSucceeded(QString ip, int userId);
    void loginFailed(QString ip, int errorCode);

private slots:
    void sweepIdle();

private:
    struct Entry {
        SessionToken     token;
        std::int64_t     lastUsedMs;   // monotonic, ms since epoch
    };

    IAuthenticator*                m_authenticator;
    std::chrono::seconds           m_idleTimeout;
    QHash<CameraIdentity, Entry>   m_cache;
    QTimer                         m_sweepTimer;
};
