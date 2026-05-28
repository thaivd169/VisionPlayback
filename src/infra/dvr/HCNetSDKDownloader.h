#pragma once
#include <atomic>
#include <string>
#include <thread>

#include "IPlaybackDownloader.h"
#include "PlaybackRequest.h"

// HCNetSDK-based downloader. Owns its own std::thread; cancel() flips an atomic
// flag the worker polls between progress samples. Destructor joins the thread.
class HCNetSDKDownloader : public IPlaybackDownloader {
public:
    HCNetSDKDownloader(const PlaybackRequest& request,
                       const std::string& outputPath);
    ~HCNetSDKDownloader() override;

    void setOnProgress(ProgressCb cb) override { m_onProgress = std::move(cb); }
    void setOnFinished(FinishedCb cb) override { m_onFinished = std::move(cb); }

    void start()  override;
    void cancel() override;

private:
    void run();

    PlaybackRequest   m_request;
    std::string       m_outputPath;
    std::atomic<bool> m_cancelled{false};
    std::thread       m_thread;
    ProgressCb        m_onProgress;
    FinishedCb        m_onFinished;
};

class HCNetSDKDownloaderFactory : public IPlaybackDownloaderFactory {
public:
    std::unique_ptr<IPlaybackDownloader>
        create(const PlaybackRequest& request,
               const std::string& outputPath) override {
        return std::make_unique<HCNetSDKDownloader>(request, outputPath);
    }
};
