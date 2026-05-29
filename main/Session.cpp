#include "Session.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QHttpHeaders>
#include <QString>
#include <QStringList>
#include <chrono>
#include <iostream>

#include "Config.h"
#include "QtHasher.h"

Session::Session(int argc, char* argv[], QObject* parent)
    : QObject(parent),
      m_apiKey(QString::fromUtf8(vp::infra::kDefaultApiKey.data(),
                                 static_cast<int>(vp::infra::kDefaultApiKey.size()))),
      m_port(vp::infra::kDefaultPort),
      m_downloadsDir(QCoreApplication::applicationDirPath() + "/downloads"),
      m_loginIdleSec(vp::infra::kDefaultLoginIdleSeconds),
      m_hashAlgorithm(vp::infra::kDefaultHashAlgorithm),
      m_hcnetsdk("./sdkLog/") {
    parseArgs(argc, argv);
    QDir().mkpath(m_downloadsDir);
    const std::string hostBase =
        std::string("http://localhost:") + std::to_string(m_port);

    // Infrastructure construction
    m_hasher = QtHasher::fromName(m_hashAlgorithm);
    m_dispatcher = std::make_unique<QtDispatcher>(this);
    m_cache = std::make_unique<FileSystemStreamCache>(m_downloadsDir, m_maxDownloadsBytes);

    // Usecase construction
    m_loginUseCase = std::make_unique<LoginUseCase>(&m_authenticator,
                                                    std::chrono::seconds(m_loginIdleSec));
    m_streamUseCase = std::make_unique<StreamPlaybackUseCase>(m_cache.get(),
                                                              &m_downloaderFactory,
                                                              &m_packagerFactory,
                                                              m_dispatcher.get(),
                                                              hostBase);

    // Adapter construction
    m_apiKeyGuard = std::make_unique<ApiKeyGuard>(m_apiKey.toStdString());
    m_controlApi = std::make_unique<ControlApi>(m_apiKeyGuard.get(),
                                                m_loginUseCase.get(),
                                                m_streamUseCase.get(),
                                                hostBase,
                                                m_hasher.get(),
                                                this);
    m_pollingApi = std::make_unique<PollingApi>(m_streamUseCase.get(),
                                                m_cache.get(),
                                                hostBase,
                                                this);
    m_dashFileServer = std::make_unique<DashFileServer>(m_downloadsDir, this);
    m_streamUseCase->setActiveStreamingCallback(
        [this] { return m_dashFileServer->activeKeys(); });
    m_eventLogger = std::make_unique<ConsoleEventLogger>();
    m_eventLogger->subscribeTo(m_streamUseCase.get());
    m_eventLogger->subscribeTo(m_loginUseCase.get());
}

Session::~Session() = default;

bool Session::start() {
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

    std::cout << "VisionPlayback daemon listening on port " << m_port
              << " (api-key=" << m_apiKey.toStdString() << ")" << std::endl;
    return true;
}

void Session::parseArgs(int argc, char* argv[]) {
    QCommandLineParser parser;
    parser.setApplicationDescription("VisionPlayback daemon");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption apiKeyOpt({"k", "api-key"}, "HTTP API key.", "key");
    QCommandLineOption portOpt({"p", "port"}, "TCP listen port.", "port");
    QCommandLineOption downloadsOpt({"d", "downloads-dir"}, "Downloads directory.", "path");
    QCommandLineOption idleOpt({"t", "login-idle-timeout"}, "Login idle timeout (seconds).", "seconds");
    QCommandLineOption maxGbOpt({"g", "max-downloads-gb"}, "Downloads cache cap (GB).", "gb");
    QCommandLineOption hashOpt({"a", "hash-algorithm"}, "Hash algo (blake2s-128|sha256|sha512).", "name");
    parser.addOptions({apiKeyOpt, portOpt, downloadsOpt, idleOpt, maxGbOpt, hashOpt});

    QStringList args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args << QString::fromLocal8Bit(argv[i]);
    parser.process(args);

    if (parser.isSet(apiKeyOpt)) m_apiKey = parser.value(apiKeyOpt);
    if (parser.isSet(downloadsOpt)) m_downloadsDir = parser.value(downloadsOpt);
    if (parser.isSet(hashOpt)) m_hashAlgorithm = parser.value(hashOpt).toStdString();

    if (parser.isSet(portOpt)) {
        bool ok = false;
        const uint v = parser.value(portOpt).toUInt(&ok);
        if (ok && v <= 0xFFFFu) m_port = static_cast<quint16>(v);
    }
    if (parser.isSet(idleOpt)) {
        bool ok = false;
        const int n = parser.value(idleOpt).toInt(&ok);
        if (ok && n > 0) m_loginIdleSec = n;
    }
    if (parser.isSet(maxGbOpt)) {
        bool ok = false;
        const qulonglong gb = parser.value(maxGbOpt).toULongLong(&ok);
        if (ok && gb > 0) m_maxDownloadsBytes = gb * 1024ULL * 1024 * 1024;
    }
}
