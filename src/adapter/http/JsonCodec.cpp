#include "JsonCodec.h"
#include "PlaybackTime.h"

#include <QJsonValue>

namespace JsonCodec {

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
        if (!v.isString()) { if (missingField) *missingField = QString::fromLatin1(k); return false; }
        sink = v.toString();
        return true;
    };
    auto requireInt = [&](const char* k, int& sink) {
        const QJsonValue v = o.value(QLatin1String(k));
        if (!v.isDouble()) { if (missingField) *missingField = QString::fromLatin1(k); return false; }
        sink = v.toInt();
        return true;
    };

    QString ip, user, pass, start, end;
    int     port = 0, channel = 0;
    if (!requireString("ip",         ip))     return std::nullopt;
    if (!requireInt   ("port",       port))   return std::nullopt;
    if (!requireString("user",       user))   return std::nullopt;
    if (!requireString("pass",       pass))   return std::nullopt;
    if (!requireInt   ("channel_id", channel)) return std::nullopt;
    if (!requireString("start_time", start))  return std::nullopt;
    if (!requireString("end_time",   end))    return std::nullopt;

    PlaybackPostBody out;
    out.credentials.ip   = ip.toStdString();
    out.credentials.port = static_cast<std::uint16_t>(port);
    out.credentials.user = user.toStdString();
    out.credentials.pass = pass.toStdString();
    out.channel.id       = channel;
    out.range.begin      = parsePlaybackTimeCompact(start.toStdString());
    out.range.end        = parsePlaybackTimeCompact(end.toStdString());
    return out;
}

QByteArray serializePollResponseReady(const std::string& url) {
    QJsonObject o;
    o["status"] = "ready";
    o["url"]    = QString::fromStdString(url);
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
    o["status"]  = "error";
    o["message"] = message;
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

} // namespace JsonCodec
