#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IDashPackager.h"
#include "IDispatcher.h"
#include "IPlaybackDownloader.h"
#include "IStreamCacheRepository.h"
#include "PlaybackKey.h"
#include "PlaybackRequest.h"
#include "StreamStatus.h"

// Owns the cache-check -> download -> package pipeline. All public methods
// must be called from a single thread (the dispatcher's thread). Worker
// callbacks delivered from other threads are bounced onto that thread via
// the injected IDispatcher.
class StreamPlaybackUseCase {
public:
    using ReadyCb    = std::function<void(PlaybackKey, std::string mpdUrl)>;
    using ProgressCb = std::function<void(PlaybackKey, int percent)>;
    using ErrorCb    = std::function<void(PlaybackKey, std::string reason)>;

    StreamPlaybackUseCase(IStreamCacheRepository*     cache,
                          IPlaybackDownloaderFactory* downloaderFactory,
                          IDashPackagerFactory*       packagerFactory,
                          IDispatcher*                dispatcher,
                          std::string                 hostBase);

    void requestStream(const PlaybackRequest& request);
    StreamStatus currentStatus(const PlaybackKey& key) const;

    void onStreamReady(ReadyCb cb)        { m_readyCbs.push_back(std::move(cb)); }
    void onDownloadProgress(ProgressCb cb){ m_progressCbs.push_back(std::move(cb)); }
    void onStreamError(ErrorCb cb)        { m_errorCbs.push_back(std::move(cb)); }

private:
    enum class JobState { Pending, Downloading, Packaging };

    struct Job {
        PlaybackKey                          key;
        Channel                              channel;
        JobState                             state = JobState::Pending;
        std::unique_ptr<IPlaybackDownloader> downloader;
        std::unique_ptr<IDashPackager>       packager;
    };

    void startDownload(const PlaybackRequest& request);
    void startPackage(const PlaybackKey& key);
    void onDownloadFinished(const PlaybackKey& key, bool success, std::string err);
    void onPackageFinished(const PlaybackKey& key, bool success, std::string err);
    void emitReady(const PlaybackKey& key, std::string mpdUrl);
    void emitError(const PlaybackKey& key, std::string reason);
    void emitProgress(const PlaybackKey& key, int percent);

    IStreamCacheRepository*                m_cache;
    IPlaybackDownloaderFactory*            m_downloaderFactory;
    IDashPackagerFactory*                  m_packagerFactory;
    IDispatcher*                           m_dispatcher;
    std::string                            m_hostBase;
    std::unordered_map<PlaybackKey, Job>   m_activeJobs;

    std::vector<ReadyCb>    m_readyCbs;
    std::vector<ProgressCb> m_progressCbs;
    std::vector<ErrorCb>    m_errorCbs;
};
