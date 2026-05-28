#include "PollingApi.h"

#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QUrlQuery>
#include <utility>

#include "IStreamCacheRepository.h"
#include "JsonCodec.h"
#include "PlaybackKey.h"
#include "StreamPlaybackUseCase.h"

PollingApi::PollingApi(StreamPlaybackUseCase*  streamUseCase,
                       IStreamCacheRepository* cache,
                       std::string             hostBase,
                       QObject*                parent)
    : QObject(parent),
      m_streamUseCase(streamUseCase),
      m_cache(cache),
      m_hostBase(std::move(hostBase)) {}

void PollingApi::registerRoutes(QHttpServer& server) {
    server.route("/playback", QHttpServerRequest::Method::Get, this,
                 [this](const QHttpServerRequest& req) {
                     return handleGet(req);
                 });
}

QHttpServerResponse PollingApi::handleGet(const QHttpServerRequest& req) {
    const QUrlQuery query(req.url().query());
    const QString idParam = query.queryItemValue("id");
    if (idParam.isEmpty()) {
        return QHttpServerResponse("application/json",
                                   JsonCodec::serializeError("missing id query param"),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    const PlaybackKey key{ idParam.toStdString() };
    const StreamStatus status = m_streamUseCase->currentStatus(key);

    switch (status) {
        case StreamStatus::Ready: {
            const std::string url = m_cache->mpdUrl(key, m_hostBase);
            return QHttpServerResponse("application/json",
                                       JsonCodec::serializePollResponseReady(url),
                                       QHttpServerResponse::StatusCode::Ok);
        }
        case StreamStatus::Unknown:
            return QHttpServerResponse("application/json",
                                       JsonCodec::serializePollResponseUnknown(),
                                       QHttpServerResponse::StatusCode::NotFound);
        case StreamStatus::Pending:
        case StreamStatus::Downloading:
        case StreamStatus::Packaging:
        case StreamStatus::Failed:
        default:
            return QHttpServerResponse("application/json",
                                       JsonCodec::serializePollResponsePending(),
                                       QHttpServerResponse::StatusCode::Accepted);
    }
}
