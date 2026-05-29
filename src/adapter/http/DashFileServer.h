#pragma once
#include <QHash>
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <vector>

#include "PlaybackKey.h"

// GET /dash/<hash>/* — public static-file delivery for the DASH manifest and
// its segment files written by the packager into downloads/<hash>/.
class DashFileServer : public QObject {
    Q_OBJECT
public:
    DashFileServer(QString downloadsDir, QObject* parent = nullptr);

    void registerRoutes(QHttpServer& server);

    // Returns keys for hashes that have been served within the last 30 seconds.
    // Used by the capacity manager to avoid evicting actively-streamed videos.
    std::vector<PlaybackKey> activeKeys() const;

private:
    QHttpServerResponse serveStaticFile(const QString& relativePath);

    QString              m_downloadsDir;
    QHash<QString, qint64> m_lastAccessMs;
};
