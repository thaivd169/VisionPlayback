#ifndef ADAPTER_HTTP_LISTENER_H
#define ADAPTER_HTTP_LISTENER_H

#include <QByteArray>
#include <QHash>
#include <QHttpServer>
#include <QHttpServerResponder>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "PlaybackRequest.h"
#include "StreamStatus.h"

class IHasher;
class QHttpServerRequest;

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
    // Asks the processor to build a downloadable MP4 for keyHex, optionally
    // trimmed to [startSec, endSec) (-1 = unbounded). The reply arrives on
    // onExportFinished, which completes the deferred HTTP response.
    void exportRequested(quint64 requestId, QString keyHex,
                         int startSec, int endSec);
    // Emitted once per served DASH file. The processor records the access time
    // and treats a key as "in use" for a TTL window afterwards, so a stream the
    // client is actively watching is not evicted in the gap between segment
    // fetches. A single touch (rather than a start/end pair) needs no
    // refcounting and survives overlapping requests for the same key.
    void keyAccessed(QString keyHex);

   public slots:
    void started();
    void onStatusChanged(QString keyHex, StreamStatus status, QString url);
    // Completes a deferred /download response once the processor's export job
    // finishes: streams the MP4 (attachment) on success, deletes the temp file.
    void onExportFinished(quint64 requestId, QString outputPath,
                          bool ok, QString error);

   private:
    // POST /playback — validate key, parse body, emit playbackRequested, 202.
    QHttpServerResponse handlePlaybackPost(const QHttpServerRequest& req);
    // GET /playback?id=<hash> — answer from the locally-cached status.
    QHttpServerResponse handlePlaybackGet(const QHttpServerRequest& req);
    // GET /dash/<hash>/<file> — serve a packaged manifest/segment file.
    QHttpServerResponse serveStaticFile(const QString& relativePath);
    // GET /download/<hash>[?start=&end=] — validate, then defer the response
    // by holding the responder until the export job reports back.
    void handleDownloadGet(const QString& hash, const QHttpServerRequest& req,
                           QHttpServerResponder&& responder);
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

    // In-flight /download requests whose response is deferred until the
    // processor finishes the export. The responder is held here (moved out of
    // the route handler) and used to reply from onExportFinished, all on the
    // HTTP thread. filename feeds the Content-Disposition header.
    struct PendingExport {
        QHttpServerResponder responder;
        QString              filename;
    };
    std::uint64_t                              m_nextExportId = 0;
    std::unordered_map<std::uint64_t, PendingExport> m_pendingExports;
};

#endif  // ADAPTER_HTTP_LISTENER_H
