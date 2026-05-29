#include "DashFileServer.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QIODevice>
#include <utility>

static constexpr qint64 kActiveStreamingGraceMs = 30LL * 1000;

DashFileServer::DashFileServer(QString downloadsDir, QObject* parent)
    : QObject(parent), m_downloadsDir(std::move(downloadsDir)) {}

void DashFileServer::registerRoutes(QHttpServer& server) {
    server.route("/dash/<arg>/<arg>", QHttpServerRequest::Method::Get, this,
                 [this](const QString& key, const QString& file,
                        const QHttpServerRequest&) {
                     auto response = serveStaticFile(key + "/" + file);
                     if (response.statusCode() == QHttpServerResponse::StatusCode::Ok)
                         m_lastAccessMs[key] = QDateTime::currentMSecsSinceEpoch();
                     return response;
                 });
}

std::vector<PlaybackKey> DashFileServer::activeKeys() const {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::vector<PlaybackKey> result;
    for (auto it = m_lastAccessMs.constBegin(); it != m_lastAccessMs.constEnd(); ++it) {
        if (now - it.value() <= kActiveStreamingGraceMs)
            result.push_back(PlaybackKey{it.key().toStdString()});
    }
    return result;
}

QHttpServerResponse DashFileServer::serveStaticFile(const QString& relativePath) {
    if (relativePath.contains("..")) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
    }

    const QString fullPath = m_downloadsDir + "/" + relativePath;
    QFile file(fullPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    }

    const QByteArray data = file.readAll();

    QString mimeType = "application/octet-stream";
    if (fullPath.endsWith(".mpd"))      mimeType = "application/dash+xml";
    else if (fullPath.endsWith(".m4s")) mimeType = "video/iso.segment";
    else if (fullPath.endsWith(".mp4")) mimeType = "video/mp4";

    return QHttpServerResponse(mimeType.toLatin1(), data,
                               QHttpServerResponse::StatusCode::Ok);
}
