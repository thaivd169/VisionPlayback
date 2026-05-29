#pragma once
#include <QHttpServer>
#include <QObject>
#include <string>

#include "IHasher.h"
#include "PlaybackRequest.h"

class ApiKeyGuard;

// POST /playback — validates X-API-Key, parses the body, computes the
// PlaybackKey and immediately returns 202 with the poll URL. The full request
// (including credentials for login) is forwarded to PlaybackProcessor via the
// playbackRequested signal so that login and download happen off the HTTP thread.
class ControlApi : public QObject {
    Q_OBJECT
public:
    ControlApi(ApiKeyGuard*   guard,
               std::string    hostBase,
               const IHasher* hasher,
               QObject*       parent = nullptr);

    void registerRoutes(QHttpServer& server);

signals:
    void playbackRequested(PlaybackRequest request);

private:
    QHttpServerResponse handlePost(const QHttpServerRequest& req);

    ApiKeyGuard*   m_guard;
    std::string    m_hostBase;
    const IHasher* m_hasher;
};
