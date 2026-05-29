#pragma once
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <cstdint>
#include <memory>
#include <string>

// Adapter layer (primary adapters: HTTP, console).
#include "ApiKeyGuard.h"
#include "ConsoleEventLogger.h"
#include "ControlApi.h"
#include "DashFileServer.h"
#include "PollingApi.h"

// Infra layer (secondary adapters: HCNetSDK, ffmpeg, FS, dispatcher, hashing).
#include "IHasher.h"
#include "FfmpegDashPackager.h"
#include "FileSystemStreamCache.h"
#include "HCNetSDKAuthenticator.h"
#include "HCNetSDKBootstrap.h"
#include "HCNetSDKDownloader.h"
#include "QtDispatcher.h"

// Use cases.
#include "LoginUseCase.h"
#include "StreamPlaybackUseCase.h"

// Composition root. Builds infra impls, injects them into use cases, then
// wires HTTP routes + console logging on top. The one place that sees both
// adapter and infra.
class Session : public QObject {
    Q_OBJECT
public:
    Session(int argc, char* argv[], QObject* parent = nullptr);
    ~Session() override;

    bool start();

private:
    void parseArgs(int argc, char* argv[]);

    // ---- Parsed CLI options ----
    QString       m_apiKey;
    quint16       m_port = 8080;
    QString       m_downloadsDir;
    int           m_loginIdleSec = 600;
    std::uint64_t m_maxDownloadsBytes = 100ULL * 1024 * 1024 * 1024;
    std::string   m_hashAlgorithm;

    // ---- Order-sensitive ownership (RAII bootstrap first) ----
    vp::infra::HCNetSDKBootstrap m_hcnetsdk;

    std::unique_ptr<IHasher> m_hasher;

    // Infra adapters (concrete impls of domain ports).
    HCNetSDKAuthenticator     m_authenticator;
    HCNetSDKDownloaderFactory m_downloaderFactory;
    FfmpegDashPackagerFactory m_packagerFactory;

    std::unique_ptr<QtDispatcher>          m_dispatcher;
    std::unique_ptr<FileSystemStreamCache> m_cache;

    // Use cases.
    std::unique_ptr<LoginUseCase>          m_loginUseCase;
    std::unique_ptr<StreamPlaybackUseCase> m_streamUseCase;

    // Primary adapters (HTTP + console).
    std::unique_ptr<ApiKeyGuard>           m_apiKeyGuard;
    QHttpServer                            m_httpServer;
    QTcpServer*                            m_tcpServer = nullptr;
    std::unique_ptr<ControlApi>            m_controlApi;
    std::unique_ptr<PollingApi>            m_pollingApi;
    std::unique_ptr<DashFileServer>        m_dashFileServer;
    std::unique_ptr<ConsoleEventLogger>    m_eventLogger;
};
