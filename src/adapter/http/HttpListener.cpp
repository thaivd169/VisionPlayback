#include "HttpListener.h"

#include <QFile>
#include <QHostAddress>
#include <QHttpHeaders>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrlQuery>
#include <iostream>
#include <optional>
#include <utility>

#include "IHasher.h"
#include "PlaybackKey.h"
#include "PlaybackTime.h"
// Concrete hasher impl is named only in this translation unit; the adapter
// header keeps just a forward declaration of IHasher.
#include "QtHasher.h"

// ---------------------------------------------------------------------------
// JSON codec (absorbed from the former JsonCodec namespace). All QJson* ↔
// domain conversions live here; the route handlers below never touch QJson*
// types directly.
// ---------------------------------------------------------------------------
namespace {

struct PlaybackPostBody {
    Credentials credentials;
    Channel channel;
    TimeRange range;
};

// Parses a POST /playback body. Returns std::nullopt on malformed JSON or
// missing fields; `missingField` receives the first missing/invalid key name.
std::optional<PlaybackPostBody> parsePlaybackPostBody(const QByteArray& bodyBytes,
                                                      QString* missingField) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bodyBytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (missingField) *missingField = "<malformed json>";
        return std::nullopt;
    }
    const QJsonObject o = doc.object();

    auto requireString = [&](const char* k, QString& sink) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (!v.isString()) {
            if (missingField) *missingField = QString::fromLatin1(k);
            return false;
        }
        sink = v.toString();
        return true;
    };
    auto requireInt = [&](const char* k, int& sink) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (!v.isDouble()) {
            if (missingField) *missingField = QString::fromLatin1(k);
            return false;
        }
        sink = v.toInt();
        return true;
    };

    QString ip, user, pass, start, end;
    int port = 0, channel = 0;
    if (!requireString("ip", ip)) return std::nullopt;
    if (!requireInt("port", port)) return std::nullopt;
    if (!requireString("user", user)) return std::nullopt;
    if (!requireString("pass", pass)) return std::nullopt;
    if (!requireInt("channel_id", channel)) return std::nullopt;
    if (!requireString("start_time", start)) return std::nullopt;
    if (!requireString("end_time", end)) return std::nullopt;

    PlaybackPostBody out;
    out.credentials.ip = ip.toStdString();
    out.credentials.port = static_cast<std::uint16_t>(port);
    out.credentials.user = user.toStdString();
    out.credentials.pass = pass.toStdString();
    out.channel.id = channel;
    out.range.begin = parsePlaybackTimeCompact(start.toStdString());
    out.range.end = parsePlaybackTimeCompact(end.toStdString());
    return out;
}

QByteArray serializePollResponseReady(const std::string& url) {
    QJsonObject o;
    o["status"] = "ready";
    o["url"] = QString::fromStdString(url);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray serializePollResponsePending() {
    QJsonObject o;
    o["status"] = "pending";
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray serializePollResponseUnknown() {
    QJsonObject o;
    o["status"] = "unknown_id";
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray serializePollUrl(const std::string& pollUrl) {
    QJsonObject o;
    o["poll_url"] = QString::fromStdString(pollUrl);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray serializeError(const QString& message) {
    QJsonObject o;
    o["status"] = "error";
    o["message"] = message;
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

}  // namespace

// ---------------------------------------------------------------------------
// HttpListener
// ---------------------------------------------------------------------------
HttpListener::HttpListener(HttpListenerConfig config,
                           QObject* parent)
    : QObject(parent),
      m_httpServer(nullptr),
      m_tcpServer(nullptr),
      m_port(config.port),
      m_apiKey(std::move(config.apiKey)),
      m_hostBase(std::move(config.hostBase)),
      m_downloadsDir(std::move(config.downloadsDir)),
      m_expectedKey(m_apiKey.toStdString()),
      m_hasher(QtHasher::fromName(config.hashAlgorithm)) {}

HttpListener::~HttpListener() {
    if (m_tcpServer && m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
}

void HttpListener::started() {
    m_httpServer = new QHttpServer(this);
    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::Any, m_port)) {
        qFatal() << "[fatal] Could not bind on port " << m_port;
    }
    m_httpServer->bind(m_tcpServer);

    // Shared CORS middleware.
    m_httpServer->addAfterRequestHandler(this,
                                         [](const QHttpServerRequest&, QHttpServerResponse& resp) {
                                             auto headers = resp.headers();
                                             headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowOrigin, "*");
                                             headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowMethods, "GET, POST, OPTIONS");
                                             headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowHeaders, "Content-Type, X-API-Key");
                                             resp.setHeaders(headers);
                                         });

    m_httpServer->route("/playback", QHttpServerRequest::Method::Post, this,
                        [this](const QHttpServerRequest& req) {
                            return handlePlaybackPost(req);
                        });
    m_httpServer->route("/playback", QHttpServerRequest::Method::Get, this,
                        [this](const QHttpServerRequest& req) {
                            return handlePlaybackGet(req);
                        });
    m_httpServer->route("/dash/<arg>/<arg>", QHttpServerRequest::Method::Get, this,
                        [this](const QString& key, const QString& file,
                               const QHttpServerRequest&) {
                            emit keyAccessStarted(key);
                            auto response = serveStaticFile(key + "/" + file);
                            emit keyAccessEnded(key);
                            return response;
                        });

    std::cout << "HTTP listener started" << std::endl;
    std::cout << "VisionPlayback daemon listening on port " << m_port
              << " (api-key=" << m_apiKey.toStdString() << ")" << std::endl;
}

QHttpServerResponse HttpListener::handlePlaybackPost(const QHttpServerRequest& req) {
    const QByteArray providedKey = req.headers().value(QByteArrayLiteral("X-API-Key")).toByteArray();
    if (!validateApiKey(providedKey)) {
        return QHttpServerResponse("application/json",
                                   serializeError("invalid api key"),
                                   QHttpServerResponse::StatusCode::Unauthorized);
    }

    QString missingField;
    const auto parsed = parsePlaybackPostBody(req.body(), &missingField);
    if (!parsed) {
        return QHttpServerResponse("application/json",
                                   serializeError(
                                       QStringLiteral("bad payload: %1").arg(missingField)),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    const PlaybackKey key = makePlaybackKey(parsed->credentials,
                                            parsed->channel,
                                            parsed->range,
                                            *m_hasher);

    PlaybackRequest pr;
    pr.credentials = parsed->credentials;
    pr.channel = parsed->channel;
    pr.range = parsed->range;
    pr.key = key;
    emit playbackRequested(pr);

    const std::string pollUrl = m_hostBase + "/playback?id=" + key.hex;
    return QHttpServerResponse("application/json",
                               serializePollUrl(pollUrl),
                               QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse HttpListener::handlePlaybackGet(const QHttpServerRequest& req) {
    const QUrlQuery query(req.url().query());
    const QString idParam = query.queryItemValue("id");
    if (idParam.isEmpty()) {
        return QHttpServerResponse("application/json",
                                   serializeError("missing id query param"),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    const StreamStatus status = m_statusCache.value(idParam, StreamStatus::Unknown);

    switch (status) {
        case StreamStatus::Ready: {
            const std::string url = m_mpdUrlCache.value(idParam).toStdString();
            return QHttpServerResponse("application/json",
                                       serializePollResponseReady(url),
                                       QHttpServerResponse::StatusCode::Ok);
        }
        case StreamStatus::Unknown:
            return QHttpServerResponse("application/json",
                                       serializePollResponseUnknown(),
                                       QHttpServerResponse::StatusCode::NotFound);
        case StreamStatus::Pending:
        case StreamStatus::Downloading:
        case StreamStatus::Packaging:
        case StreamStatus::Failed:
        default:
            return QHttpServerResponse("application/json",
                                       serializePollResponsePending(),
                                       QHttpServerResponse::StatusCode::Accepted);
    }
}

QHttpServerResponse HttpListener::serveStaticFile(const QString& relativePath) {
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
    if (fullPath.endsWith(".mpd"))
        mimeType = "application/dash+xml";
    else if (fullPath.endsWith(".m4s"))
        mimeType = "video/iso.segment";
    else if (fullPath.endsWith(".mp4"))
        mimeType = "video/mp4";

    return QHttpServerResponse(mimeType.toLatin1(), data,
                               QHttpServerResponse::StatusCode::Ok);
}

void HttpListener::onStatusChanged(QString keyHex, StreamStatus status, QString url) {
    m_statusCache.insert(keyHex, status);
    if (status == StreamStatus::Ready)
        m_mpdUrlCache.insert(keyHex, url);
}

bool HttpListener::validateApiKey(const QByteArray& providedKey) const {
    const auto* a = reinterpret_cast<const unsigned char*>(m_expectedKey.data());
    const auto* b = reinterpret_cast<const unsigned char*>(providedKey.constData());
    const std::size_t aLen = m_expectedKey.size();
    const std::size_t bLen = static_cast<std::size_t>(providedKey.size());

    // Compare up to max(aLen, bLen) bytes so the loop runs the same number of
    // iterations regardless of which input is longer. Length mismatch is folded
    // into the accumulator so equal-length matching keys are required.
    const std::size_t n = (aLen > bLen) ? aLen : bLen;
    unsigned diff = static_cast<unsigned>(aLen ^ bLen);
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned ca = (i < aLen) ? a[i] : 0u;
        const unsigned cb = (i < bLen) ? b[i] : 0u;
        diff |= (ca ^ cb);
    }
    return diff == 0;
}
