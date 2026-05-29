#ifndef ADAPTER_HTTP_LISTENER_H
#define ADAPTER_HTTP_LISTENER_H

#include <QHttpServer>
#include <QObject>
#include <QTcpServer>
#include <QThread>

class HttpListener : public QObject {
    Q_OBJECT
   public:
    HttpListener(const int& port, QObject* parent = nullptr);
    HttpListener(const HttpListener& other) = delete;
    HttpListener& operator=(const HttpListener& other) = delete;

    ~HttpListener();

   public slots:
    void started();
    void stopped();

   private:
    int m_port;
    QHttpServer* m_httpServer;
    QTcpServer* m_tcpServer;
    std::unique_ptr<ControlApi> m_controlApi;
    std::unique_ptr<PollingApi> m_pollingApi;
    std::unique_ptr<DashFileServer> m_dashFileServer;
    std::unique_ptr<ApiKeyGuard> m_apiKeyGuard;
};

#endif  // ADAPTER_HTTP_LISTENER_H