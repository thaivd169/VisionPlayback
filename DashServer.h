#pragma once
#include <QHttpServer>
#include <QObject>
#include <QString>

class QTcpServer;

class DashServer : public QObject {
    Q_OBJECT
public:
    explicit DashServer(const QString& downloadsDir, quint16 port = 8080,
                        QObject* parent = nullptr);
    ~DashServer() override;

    bool    start();
    quint16 port() const;

signals:
    // Emitted when /stream is requested but the resource isn't cached yet.
    // channel is the integer from channel_id; startTime/endTime are "YYYYMMDDTHHmmss".
    void streamRequested(int channel, const QString& startTime, const QString& endTime);

private:
    QString      m_downloadsDir;
    quint16      m_port;
    QHttpServer  m_server;
    QTcpServer*  m_tcpServer = nullptr;

    void setupRoutes();
    QHttpServerResponse serveStreamEndpoint(const QHttpServerRequest& req);
    QHttpServerResponse serveStaticFile(const QString& relativePath);
};
