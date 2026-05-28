#include "ControlApi.h"

#include <QHttpHeaders>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <utility>

#include "ApiKeyGuard.h"
#include "JsonCodec.h"
#include "LoginUseCase.h"
#include "PlaybackKey.h"
#include "PlaybackRequest.h"
#include "StreamPlaybackUseCase.h"

ControlApi::ControlApi(ApiKeyGuard*           guard,
                       LoginUseCase*          loginUseCase,
                       StreamPlaybackUseCase* streamUseCase,
                       std::string            hostBase,
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
                                            parsed->range);

    const SessionToken token = m_loginUseCase->ensureLoggedIn(parsed->credentials);
    if (token < 0) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError(
                                       QStringLiteral("login failed: %1")
                                           .arg(m_loginUseCase->lastErrorCode())),
                                   QHttpServerResponse::StatusCode::BadGateway);
    }

    PlaybackRequest pr;
    pr.token   = token;
    pr.channel = parsed->channel;
    pr.range   = parsed->range;
    pr.key     = key;
    m_streamUseCase->requestStream(pr);

    const std::string pollUrl = m_hostBase + "/playback?id=" + key.hex;
    return QHttpServerResponse("application/json",
                               JsonCodec::serializePollUrl(pollUrl),
                               QHttpServerResponse::StatusCode::Ok);
}
