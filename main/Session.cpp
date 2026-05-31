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

Session::Session(int argc, char* argv[], QObject* parent)
    : QObject(parent),
      m_apiKeyCli(QString::fromUtf8(vp::infra::kDefaultApiKey.data(),
                                    static_cast<int>(vp::infra::kDefaultApiKey.size()))),
      m_portCli(vp::infra::kDefaultPort),
      m_downloadsDirCli(QCoreApplication::applicationDirPath() + "/downloads"),
      m_loginIdleSecCli(vp::infra::kDefaultLoginIdleSeconds),
      m_hashAlgorithmCli(vp::infra::kDefaultHashAlgorithm),
      m_hcnetSdkBootstrap(std::make_unique<vp::infra::HCNetSDKBootstrap>("./sdkLog/")),
      m_processorThread(nullptr),
      m_httpThread(nullptr),
      m_processor(nullptr),
      m_httpListener(nullptr) {
    parseArgs(argc, argv);
    createDownloadDir();
    const std::string hostBase =
        std::string("http://localhost:") + std::to_string(m_portCli);

    // Processor (owns the pipeline FSM, the on-disk stream cache, and its
    // login/download/package collaborators; runs on its own thread)
    m_processor = new PlaybackProcessor(
        PlaybackProcessorConfig{.loginIdle = std::chrono::seconds(m_loginIdleSecCli),
                                .downloadsDir = m_downloadsDirCli,
                                .maxDownloadsBytes = m_maxDownloadsBytesCli,
                                .hostBase = hostBase});
    m_processorThread = new QThread(this);
    m_httpThread = new QThread(this);
    m_httpListener = new HttpListener(
        HttpListenerConfig{.port = m_portCli, .apiKey = m_apiKeyCli,
                           .hostBase = hostBase, .downloadsDir = m_downloadsDirCli,
                           .hashAlgorithm = m_hashAlgorithmCli});
    m_httpListener->moveToThread(m_httpThread);
    connect(m_httpThread, &QThread::started, m_httpListener, &HttpListener::started);
    connect(m_httpThread, &QThread::finished, m_httpListener, &HttpListener::deleteLater);

    m_processor->moveToThread(m_processorThread);
    connect(m_processorThread, &QThread::finished,
            m_processor, &QObject::deleteLater);

    // Signal wiring: HTTP → Processor
    connect(m_httpListener, &HttpListener::playbackRequested,
            m_processor, &PlaybackProcessor::onPlaybackRequested);
    connect(m_httpListener, &HttpListener::keyAccessStarted,
            m_processor, &PlaybackProcessor::onKeyActivated);
    connect(m_httpListener, &HttpListener::keyAccessEnded,
            m_processor, &PlaybackProcessor::onKeyDeactivated);

    // Signal wiring: Processor → HTTP
    connect(m_processor, &PlaybackProcessor::statusChanged,
            m_httpListener, &HttpListener::onStatusChanged);

    start();
}

Session::~Session() {
    m_processorThread->quit();
    m_processorThread->wait();

    m_httpThread->quit();
    m_httpThread->wait();
}

bool Session::start() {
    m_processorThread->start();
    m_httpThread->start();
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

    if (parser.isSet(apiKeyOpt)) m_apiKeyCli = parser.value(apiKeyOpt);
    if (parser.isSet(downloadsOpt)) m_downloadsDirCli = parser.value(downloadsOpt);
    if (parser.isSet(hashOpt)) m_hashAlgorithmCli = parser.value(hashOpt).toStdString();

    if (parser.isSet(portOpt)) {
        bool ok = false;
        const uint v = parser.value(portOpt).toUInt(&ok);
        if (ok && v <= 0xFFFFu) m_portCli = static_cast<quint16>(v);
    }
    if (parser.isSet(idleOpt)) {
        bool ok = false;
        const int n = parser.value(idleOpt).toInt(&ok);
        if (ok && n > 0) m_loginIdleSecCli = n;
    }
    if (parser.isSet(maxGbOpt)) {
        bool ok = false;
        const qulonglong gb = parser.value(maxGbOpt).toULongLong(&ok);
        if (ok && gb > 0) m_maxDownloadsBytesCli = gb * 1024ULL * 1024 * 1024;
    }
}

void Session::createDownloadDir() {
    QDir().mkpath(m_downloadsDirCli);
}