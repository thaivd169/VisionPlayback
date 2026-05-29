#include "HttpListener.h"

#include <iostream>

HttpListener::HttpListener(const int& port, QObject* parent)
    : QObject(parent),
      m_port(port),
      m_httpServer(new QHttpServer(this)),
      m_tcpServer(new QTcpServer(this)) {
    m_apiKeyGuard = std::make_unique<ApiKeyGuard>(m_apiKeyCli.toStdString());
    m_controlApi = std::make_unique<ControlApi>(m_apiKeyGuard.get(),
                                                hostBase,
                                                m_hasher.get(),
                                                this);
    m_pollingApi = std::make_unique<PollingApi>(m_cache.get(), hostBase, this);
    m_dashFileServer = std::make_unique<DashFileServer>(m_downloadsDirCli, this);
}

HttpListener::~HttpListener() = default;

void HttpListener::started() {
    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::Any, m_port)) {
        std::cerr << "[fatal] Could not bind on port " << m_port << std::endl;
        return false;
    }
    m_httpServer.bind(m_tcpServer);

    // Shared CORS middleware.
    m_httpServer.addAfterRequestHandler(this,
                                        [](const QHttpServerRequest&, QHttpServerResponse& resp) {
                                            auto headers = resp.headers();
                                            headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowOrigin, "*");
                                            headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowMethods, "GET, POST, OPTIONS");
                                            headers.append(QHttpHeaders::WellKnownHeader::AccessControlAllowHeaders, "Content-Type, X-API-Key");
                                            resp.setHeaders(headers);
                                        });

    m_controlApi->registerRoutes(m_httpServer);
    m_pollingApi->registerRoutes(m_httpServer);
    m_dashFileServer->registerRoutes(m_httpServer);
    std::cout << "HTTP listener started" << std::endl;
    std::cout << "VisionPlayback daemon listening on port " << m_port
              << " (api-key=" << m_apiKeyCli.toStdString() << ")" << std::endl;
}