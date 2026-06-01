#pragma once
#include <QHttpServer>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QThread>
#include <cstdint>
#include <memory>
#include <string>

// Adapter layer (primary adapters: HTTP).
#include "PlaybackProcessor.h"

// Infra layer (secondary adapters: HCNetSDK, ffmpeg, FS, hashing).
#include "HCNetSDKBootstrap.h"
#include "HttpListener.h"

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
    void createDownloadDir();

    QString m_apiKeyCli;
    quint16 m_portCli = 8080;
    QString m_downloadsDirCli;
    int m_loginIdleSecCli = 600;
    std::uint64_t m_maxDownloadsBytesCli = 100ULL * 1024 * 1024 * 1024;
    std::string m_hashAlgorithmCli;

    std::unique_ptr<vp::infra::HCNetSDKBootstrap> m_hcnetSdkBootstrap;

    QThread* m_processorThread;
    QThread* m_httpThread;
    PlaybackProcessor* m_processor;
    HttpListener* m_httpListener;
};
