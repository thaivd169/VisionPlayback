#include "ControlApi.h"

#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <utility>

#include "ApiKeyGuard.h"
#include "JsonCodec.h"
#include "PlaybackKey.h"

ControlApi::ControlApi(ApiKeyGuard*   guard,
                       std::string    hostBase,
                       const IHasher* hasher,
                       QObject*       parent)
    : QObject(parent),
      m_guard(guard),
      m_hostBase(std::move(hostBase)),
      m_hasher(hasher) {}

void ControlApi::registerRoutes(QHttpServer& server) {
    server.route("/playback", QHttpServerRequest::Method::Post, this,
                 [this](const QHttpServerRequest& req) {
                     return handlePost(req);
                 });
}

QHttpServerResponse ControlApi::handlePost(const QHttpServerRequest& req) {
    const QByteArray providedKey = req.headers().value(QByteArrayLiteral("X-API-Key")).toByteArray();
    if (!m_guard->validate(providedKey)) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError("invalid api key"),
                                   QHttpServerResponse::StatusCode::Unauthorized);
    }

    QString missingField;
    const auto parsed = JsonCodec::parsePlaybackPostBody(req.body(), &missingField);
    if (!parsed) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError(
                                       QStringLiteral("bad payload: %1").arg(missingField)),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    const PlaybackKey key = makePlaybackKey(parsed->credentials,
                                            parsed->channel,
                                            parsed->range,
                                            *m_hasher);

    PlaybackRequest pr;
    pr.credentials = parsed->credentials;
    pr.channel     = parsed->channel;
    pr.range       = parsed->range;
    pr.key         = key;
    emit playbackRequested(pr);

    const std::string pollUrl = m_hostBase + "/playback?id=" + key.hex;
    return QHttpServerResponse("application/json",
                               JsonCodec::serializePollUrl(pollUrl),
                               QHttpServerResponse::StatusCode::Ok);
}
