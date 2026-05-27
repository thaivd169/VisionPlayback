#include "Session.h"

#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QHttpHeaders>
#include <QString>
#include <iostream>

#include "Config.h"

namespace {
bool parseUInt16(const QString& s, quint16& out) {
    bool ok = false;
    const uint v = s.toUInt(&ok);
    if (!ok || v > 0xFFFFu) return false;
    out = static_cast<quint16>(v);
    return true;
}
} // namespace

Session::Session(int argc, char* argv[], QObject* parent)
    : QObject(parent),
      m_apiKey(QString::fromUtf8(vp::infra::kDefaultApiKey.data(),
                                 static_cast<int>(vp::infra::kDefaultApiKey.size()))),
      m_port(vp::infra::kDefaultPort),
      m_downloadsDir(QCoreApplication::applicationDirPath() + "/downloads"),
      m_loginIdleSec(vp::infra::kDefaultLoginIdleSeconds),
      m_hcnetsdk("./sdkLog/") {
    parseArgs(argc, argv);

    QDir().mkpath(m_downloadsDir);

    const QString hostBase = QStringLiteral("http://localhost:%1").arg(m_port);

    m_cache         = std::make_unique<FileSystemStreamCache>(m_downloadsDir);
    m_loginUseCase  = std::make_unique<LoginUseCase>(&m_authenticator,
                                                     std::chrono::seconds(m_loginIdleSec),
                                                     this);
    m_streamUseCase = std::make_unique<StreamPlaybackUseCase>(m_cache.get(),
                                                              &m_downloaderFactory,
                                                              &m_packagerFactory,
                                                              hostBase,
                                                              this);

    m_apiKeyGuard   = std::make_unique<ApiKeyGuard>(m_apiKey.toStdString());

    m_controlApi     = std::make_unique<ControlApi>(m_apiKeyGuard.get(),
                                                    m_loginUseCase.get(),
                                                    m_streamUseCase.get(),
                                                    hostBase,
                                                    this);
    m_pollingApi     = std::make_unique<PollingApi>(m_streamUseCase.get(),
                                                    m_cache.get(),
                                                    hostBase,
                                                    this);
    m_dashFileServer = std::make_unique<DashFileServer>(m_downloadsDir, this);

    m_eventLogger    = std::make_unique<ConsoleEventLogger>(this);
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
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--api-key=")) {
            m_apiKey = arg.mid(std::char_traits<char>::length("--api-key="));
        } else if (arg.startsWith("--port=")) {
            quint16 p = 0;
            if (parseUInt16(arg.mid(std::char_traits<char>::length("--port=")), p))
                m_port = p;
        } else if (arg.startsWith("--downloads-dir=")) {
            m_downloadsDir = arg.mid(std::char_traits<char>::length("--downloads-dir="));
        } else if (arg.startsWith("--login-idle-timeout=")) {
            bool ok = false;
            const int n = arg.mid(std::char_traits<char>::length("--login-idle-timeout="))
                              .toInt(&ok);
            if (ok && n > 0) m_loginIdleSec = n;
        }
    }
}
