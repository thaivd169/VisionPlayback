#pragma once
#include <QHttpServer>
#include <QObject>
#include <QString>

// GET /dash/<hash>/* — public static-file delivery for the DASH manifest and
// its segment files written by the packager into downloads/<hash>/.
class DashFileServer : public QObject {
    Q_OBJECT
public:
    DashFileServer(QString downloadsDir, QObject* parent = nullptr);

    void registerRoutes(QHttpServer& server);

private:
    QHttpServerResponse serveStaticFile(const QString& relativePath);

    QString m_downloadsDir;
};
