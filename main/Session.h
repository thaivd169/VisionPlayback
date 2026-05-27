#pragma once
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <memory>

#include "ApiKeyGuard.h"
#include "ConsoleEventLogger.h"
#include "ControlApi.h"
#include "DashFileServer.h"
#include "FfmpegDashPackager.h"
#include "FileSystemStreamCache.h"
#include "HCNetSDKAuthenticator.h"
#include "HCNetSDKBootstrap.h"
#include "HCNetSDKDownloader.h"
#include "LoginUseCase.h"
#include "PollingApi.h"
#include "StreamPlaybackUseCase.h"

// Composition root. Builds adapters, injects them into use cases, registers
// HTTP routes, and wires console-logging subscriptions. All ownership lives
// here so main.cpp stays a 20-line entry point.
class Session : public QObject {
    Q_OBJECT
public:
    Session(int argc, char* argv[], QObject* parent = nullptr);
    ~Session() override;

    bool start();

private:
    void parseArgs(int argc, char* argv[]);

    // ---- Parsed CLI options ----
    QString  m_apiKey;
    quint16  m_port = 8080;
    QString  m_downloadsDir;
    int      m_loginIdleSec = 600;

    // ---- Order-sensitive ownership (RAII bootstrap first) ----
    vp::infra::HCNetSDKBootstrap m_hcnetsdk;

    HCNetSDKAuthenticator     m_authenticator;
    HCNetSDKDownloaderFactory m_downloaderFactory;
    FfmpegDashPackagerFactory m_packagerFactory;

    std::unique_ptr<FileSystemStreamCache>  m_cache;
    std::unique_ptr<LoginUseCase>           m_loginUseCase;
    std::unique_ptr<StreamPlaybackUseCase>  m_streamUseCase;

    std::unique_ptr<ApiKeyGuard>            m_apiKeyGuard;

    QHttpServer            m_httpServer;
    QTcpServer*            m_tcpServer = nullptr;

    std::unique_ptr<ControlApi>             m_controlApi;
    std::unique_ptr<PollingApi>             m_pollingApi;
    std::unique_ptr<DashFileServer>         m_dashFileServer;
    std::unique_ptr<ConsoleEventLogger>     m_eventLogger;
};
