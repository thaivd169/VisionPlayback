#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <optional>
#include <string>

#include "Channel.h"
#include "Credentials.h"
#include "TimeRange.h"

// All QJson* ↔ domain conversions live here. ControlApi / PollingApi /
// DashFileServer never touch QJson* types directly.
namespace JsonCodec {

struct PlaybackPostBody {
    Credentials credentials;
    Channel     channel;
    TimeRange   range;
};

// Parses a POST /playback body. Returns std::nullopt on malformed JSON or
// missing fields; the optional out-parameter `missingField` receives the
// first missing/invalid key name for caller diagnostics.
std::optional<PlaybackPostBody> parsePlaybackPostBody(const QByteArray& bodyBytes,
                                                     QString* missingField = nullptr);

// Serialisers.
QByteArray serializePollResponseReady(const std::string& url);
QByteArray serializePollResponsePending();
QByteArray serializePollResponseUnknown();
QByteArray serializePollUrl(const std::string& pollUrl);
QByteArray serializeError(const QString& message);

} // namespace JsonCodec
