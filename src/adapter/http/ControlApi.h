#pragma once
#include <QHttpServer>
#include <QObject>
#include <string>

#include "IHasher.h"

class ApiKeyGuard;
class LoginUseCase;
class StreamPlaybackUseCase;

// POST /playback — control-plane endpoint. Validates X-API-Key, looks up or
// performs camera login, computes the PlaybackKey hash, dispatches the
// download/package pipeline (no-op on cache hit), and returns the poll URL.
class ControlApi : public QObject {
    Q_OBJECT
public:
    ControlApi(ApiKeyGuard*           guard,
               LoginUseCase*          loginUseCase,
               StreamPlaybackUseCase* streamUseCase,
               std::string            hostBase,
               const IHasher*         hasher,
               QObject*               parent = nullptr);

    void registerRoutes(QHttpServer& server);

private:
    QHttpServerResponse handlePost(const QHttpServerRequest& req);

    ApiKeyGuard*           m_guard;
    LoginUseCase*          m_loginUseCase;
    StreamPlaybackUseCase* m_streamUseCase;
    std::string            m_hostBase;
    const IHasher*         m_hasher;
};
