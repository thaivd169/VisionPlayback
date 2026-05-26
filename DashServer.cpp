#include "DashServer.h"

#include <QFile>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QMimeDatabase>
#include <QMimeType>
#include <QTcpServer>
#include <QUrlQuery>

DashServer::DashServer(const QString& downloadsDir, quint16 port, QObject* parent)
    : QObject(parent), m_downloadsDir(downloadsDir), m_port(port) {}

DashServer::~DashServer() = default;

bool DashServer::start() {
    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::Any, m_port))
        return false;

    m_server.bind(m_tcpServer);
    setupRoutes();
    return true;
}

quint16 DashServer::port() const {
    return m_tcpServer ? m_tcpServer->serverPort() : 0;
}

void DashServer::setupRoutes() {
    // CORS for all responses
    m_server.addAfterRequestHandler(this, [](const QHttpServerRequest&,
                                              QHttpServerResponse& resp) {
        auto headers = resp.headers();
        headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowOrigin, "*");
        headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowMethods, "GET, OPTIONS");
        resp.setHeaders(headers);
    });

    // GET /stream?channel_id=1&start_time=20260525T130000&end_time=20260525T140000
    m_server.route("/stream", QHttpServerRequest::Method::Get, this,
        [this](const QHttpServerRequest& req) -> QHttpServerResponse {
            return serveStreamEndpoint(req);
        });

    // GET /dash/<key>/<filename>
    m_server.route("/dash/<arg>/<arg>", QHttpServerRequest::Method::Get, this,
        [this](const QString& key, const QString& file,
               const QHttpServerRequest&) -> QHttpServerResponse {
            return serveStaticFile(key + "/" + file);
        });
}

QHttpServerResponse DashServer::serveStreamEndpoint(const QHttpServerRequest& req) {
    const QUrlQuery query(req.url().query());
    const QString channelId  = query.queryItemValue("channel_id");
    const QString startTime  = query.queryItemValue("start_time");
    const QString endTime    = query.queryItemValue("end_time");

    if (channelId.isEmpty() || startTime.isEmpty() || endTime.isEmpty()) {
        QHttpServerResponse resp("Missing required query params: channel_id, start_time, end_time",
                                 QHttpServerResponse::StatusCode::BadRequest);
        return resp;
    }

    const QString key      = QString("ch%1_%2_%3").arg(channelId, startTime, endTime);
    const QString mpdPath  = m_downloadsDir + "/" + key + "/manifest.mpd";

    QFile f(mpdPath);
    const bool ready = f.exists() && f.size() >= 100;

    if (!ready)
        emit streamRequested(channelId.toInt(), startTime, endTime);

    const QByteArray body = ready
        ? QByteArray(R"({"status":"ready","url":"http://localhost:8080/dash/)") + key.toUtf8() + R"(/manifest.mpd"})"
        : QByteArray(R"({"status":"pending"})");

    const auto status = ready ? QHttpServerResponse::StatusCode::Ok
                              : QHttpServerResponse::StatusCode::Accepted;
    QHttpServerResponse resp("application/json", body, status);
    return resp;
}

QHttpServerResponse DashServer::serveStaticFile(const QString& relativePath) {
    // Guard against path traversal
    if (relativePath.contains("..")) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
    }

    const QString fullPath = m_downloadsDir + "/" + relativePath;
    QFile file(fullPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    }

    const QByteArray data = file.readAll();

    // Determine MIME type by extension
    QString mimeType = "application/octet-stream";
    if (fullPath.endsWith(".mpd"))
        mimeType = "application/dash+xml";
    else if (fullPath.endsWith(".m4s"))
        mimeType = "video/iso.segment";
    else if (fullPath.endsWith(".mp4"))
        mimeType = "video/mp4";

    QHttpServerResponse resp(mimeType.toLatin1(), data, QHttpServerResponse::StatusCode::Ok);
    return resp;
}
