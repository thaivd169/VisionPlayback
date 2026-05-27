#include "ControlApi.h"

#include <QHttpHeaders>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <utility>

#include "ApiKeyGuard.h"
#include "JsonCodec.h"
#include "LoginUseCase.h"
#include "PlaybackKeyFactory.h"
#include "PlaybackRequest.h"
#include "StreamPlaybackUseCase.h"

ControlApi::ControlApi(ApiKeyGuard*           guard,
                       LoginUseCase*          loginUseCase,
                       StreamPlaybackUseCase* streamUseCase,
                       QString                hostBase,
                       QObject*               parent)
    : QObject(parent),
      m_guard(guard),
      m_loginUseCase(loginUseCase),
      m_streamUseCase(streamUseCase),
      m_hostBase(std::move(hostBase)) {}

void ControlApi::registerRoutes(QHttpServer& server) {
    server.route("/playback", QHttpServerRequest::Method::Post, this,
                 [this](const QHttpServerRequest& req) {
                     return handlePost(req);
                 });
}

QHttpServerResponse ControlApi::handlePost(const QHttpServerRequest& req) {
    // X-API-Key
    const QByteArray providedKey = req.headers().value(QByteArrayLiteral("X-API-Key")).toByteArray();
    if (!m_guard->validate(providedKey)) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError("invalid api key"),
                                   QHttpServerResponse::StatusCode::Unauthorized);
    }

    // Parse body
    QString missingField;
    const auto parsed = JsonCodec::parsePlaybackPostBody(req.body(), &missingField);
    if (!parsed) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError(
                                       QStringLiteral("bad payload: %1").arg(missingField)),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    // Compute the cache key
    const PlaybackKey key = makePlaybackKey(parsed->credentials,
                                            parsed->channel,
                                            parsed->range);

    // Cache-first login
    const SessionToken token = m_loginUseCase->ensureLoggedIn(parsed->credentials);
    if (token < 0) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError(
                                       QStringLiteral("login failed: %1")
                                           .arg(m_loginUseCase->lastErrorCode())),
                                   QHttpServerResponse::StatusCode::BadGateway);
    }

    // Dispatch the pipeline (no-op on cache hit / job already in flight)
    PlaybackRequest pr;
    pr.token   = token;
    pr.channel = parsed->channel;
    pr.range   = parsed->range;
    pr.key     = key;
    m_streamUseCase->requestStream(pr);

    const std::string pollUrl = m_hostBase.toStdString() + "/playback?id=" + key.hex;
    return QHttpServerResponse("application/json",
                               JsonCodec::serializePollUrl(pollUrl),
                               QHttpServerResponse::StatusCode::Ok);
}
