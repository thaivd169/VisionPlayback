#ifndef ADAPTER_HTTP_LISTENER_H
#define ADAPTER_HTTP_LISTENER_H

#include <QByteArray>
#include <QHash>
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <memory>
#include <string>

#include "PlaybackRequest.h"
#include "StreamStatus.h"

class IHasher;

// Plain configuration values for the HTTP listener (no injected collaborators).
// The hasher is built internally from `hashAlgorithm` (a CLI-chosen name).
struct HttpListenerConfig {
    int         port;
    QString     apiKey;
    std::string hostBase;
    QString     downloadsDir;
    std::string hashAlgorithm;
};

// Single HTTP boundary for the daemon. Owns the QHttpServer/QTcpServer and all
// route handling (control POST, polling GET, DASH static files, API-key check,
// JSON codec). Lives on a dedicated thread (moved by Session); the servers are
// created in started() so they belong to that thread, and it talks to
// PlaybackProcessor exclusively via the signals/slots below.
class HttpListener : public QObject {
    Q_OBJECT
   public:
    explicit HttpListener(HttpListenerConfig config,
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
    void onStatusChanged(QString keyHex, StreamStatus status, QString url);

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

    std::unique_ptr<IHasher> m_hasher;       // owned, infra impl

    // Local mirrors kept in sync via onStatusChanged; only touched on the HTTP
    // thread, so handlePlaybackGet never reads cross-thread state. The processor
    // (which owns the cache) supplies the ready URL through the signal, so the
    // listener never touches the cache itself.
    QHash<QString, StreamStatus> m_statusCache;
    QHash<QString, QString>      m_mpdUrlCache;
};

#endif  // ADAPTER_HTTP_LISTENER_H
