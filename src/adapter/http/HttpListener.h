#ifndef ADAPTER_HTTP_LISTENER_H
#define ADAPTER_HTTP_LISTENER_H

#include <QByteArray>
#include <QHash>
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <string>

#include "PlaybackRequest.h"
#include "StreamStatus.h"

class IHasher;
class IStreamCacheRepository;

// Plain configuration values for the HTTP listener (no injected collaborators).
struct HttpListenerConfig {
    int         port;
    QString     apiKey;
    std::string hostBase;
    QString     downloadsDir;
};

// Single HTTP boundary for the daemon. Owns the QHttpServer/QTcpServer and all
// route handling (control POST, polling GET, DASH static files, API-key check,
// JSON codec). Lives on a dedicated thread (moved by Session); the servers are
// created in started() so they belong to that thread, and it talks to
// PlaybackProcessor exclusively via the signals/slots below.
class HttpListener : public QObject {
    Q_OBJECT
   public:
    HttpListener(HttpListenerConfig config,
                 const IHasher* hasher,
                 IStreamCacheRepository* cache,
                 QObject* parent = nullptr);
    HttpListener(const HttpListener& other) = delete;
    HttpListener& operator=(const HttpListener& other) = delete;

    ~HttpListener() override;

   signals:
    void playbackRequested(PlaybackRequest request);
    void keyAccessStarted(QString keyHex);
    void keyAccessEnded(QString keyHex);

   public slots:
    void started();
    void onStatusChanged(QString keyHex, StreamStatus status);

   private:
    // POST /playback — validate key, parse body, emit playbackRequested, 202.
    QHttpServerResponse handlePlaybackPost(const QHttpServerRequest& req);
    // GET /playback?id=<hash> — answer from the locally-cached status.
    QHttpServerResponse handlePlaybackGet(const QHttpServerRequest& req);
    // GET /dash/<hash>/<file> — serve a packaged manifest/segment file.
    QHttpServerResponse serveStaticFile(const QString& relativePath);
    // Constant-time X-API-Key comparison.
    bool validateApiKey(const QByteArray& providedKey) const;

    QHttpServer* m_httpServer;
    QTcpServer*  m_tcpServer;
    int          m_port;
    QString      m_apiKey;
    std::string  m_hostBase;
    QString      m_downloadsDir;
    std::string  m_expectedKey;

    const IHasher*          m_hasher;
    IStreamCacheRepository* m_cache;

    // Local status mirror kept in sync via onStatusChanged; only touched on the
    // HTTP thread, so handlePlaybackGet never reads cross-thread state.
    QHash<QString, StreamStatus> m_statusCache;
};

#endif  // ADAPTER_HTTP_LISTENER_H
