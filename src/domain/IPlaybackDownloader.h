#pragma once
#include <functional>
#include <memory>
#include <string>

#include "PlaybackRequest.h"

// Per-job downloader port. start() may block — implementations are expected
// to run on a worker thread of their own. The use case marshals the
// progress/finished callbacks back onto its own thread via IDispatcher.
class IPlaybackDownloader {
public:
    using ProgressCb = std::function<void(int percent)>;
    using FinishedCb = std::function<void(bool success, std::string errorMessage)>;

    virtual ~IPlaybackDownloader() = default;

    virtual void setOnProgress(ProgressCb cb) = 0;
    virtual void setOnFinished(FinishedCb cb) = 0;

    virtual void start()  = 0;
    virtual void cancel() = 0;
};

// Factory injected into the use case. Concrete factories return downloaders
// bound to a specific (request, outputPath). Caller owns the returned worker.
class IPlaybackDownloaderFactory {
public:
    virtual ~IPlaybackDownloaderFactory() = default;
    virtual std::unique_ptr<IPlaybackDownloader>
        create(const PlaybackRequest& request,
               const std::string& outputPath) = 0;
};
