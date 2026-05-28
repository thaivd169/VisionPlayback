#pragma once
#include <QHttpServer>
#include <QObject>
#include <string>

class IStreamCacheRepository;
class StreamPlaybackUseCase;

// GET /playback?id=<hash> — public polling. Pure read of the use case's
// active-job table + the cache repository; never triggers downloads.
class PollingApi : public QObject {
    Q_OBJECT
public:
    PollingApi(StreamPlaybackUseCase*  streamUseCase,
               IStreamCacheRepository* cache,
               std::string             hostBase,
               QObject*                parent = nullptr);

    void registerRoutes(QHttpServer& server);

private:
    QHttpServerResponse handleGet(const QHttpServerRequest& req);

    StreamPlaybackUseCase*  m_streamUseCase;
    IStreamCacheRepository* m_cache;
    std::string             m_hostBase;
};
