#pragma once
#include <QHash>
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <string>

#include "StreamStatus.h"

class IStreamCacheRepository;

// GET /playback?id=<hash> — public polling. Maintains a local status cache
// kept in sync via the statusChanged signal from PlaybackProcessor, so
// handleGet never touches cross-thread state.
class PollingApi : public QObject {
    Q_OBJECT
public:
    PollingApi(IStreamCacheRepository* cache,
               std::string             hostBase,
               QObject*                parent = nullptr);

    void registerRoutes(QHttpServer& server);

public slots:
    void onStatusChanged(QString keyHex, StreamStatus status);

private:
    QHttpServerResponse handleGet(const QHttpServerRequest& req);

    IStreamCacheRepository*      m_cache;
    std::string                  m_hostBase;
    QHash<QString, StreamStatus> m_statusCache;
};
