#pragma once
#include <QObject>
#include <string>

#include "IPlaybackDownloader.h"
#include "PlaybackRequest.h"

// Per-job HCNetSDK downloader. Owns no thread — moveToThread() is the use
// case's responsibility (matches existing pipeline ownership).
class HCNetSDKDownloader : public IPlaybackDownloader {
    Q_OBJECT
public:
    HCNetSDKDownloader(const PlaybackRequest& request,
                       const std::string& outputPath,
                       QObject* parent = nullptr);

public slots:
    void start()  override;
    void cancel() override;

private:
    PlaybackRequest m_request;
    std::string     m_outputPath;
    volatile bool   m_cancelled = false;
};

class HCNetSDKDownloaderFactory : public IPlaybackDownloaderFactory {
public:
    IPlaybackDownloader* create(const PlaybackRequest& request,
                                const std::string& outputPath) override {
        return new HCNetSDKDownloader(request, outputPath);
    }
};
