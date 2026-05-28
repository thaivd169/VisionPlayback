#include "StreamPlaybackUseCase.h"

#include <utility>

StreamPlaybackUseCase::StreamPlaybackUseCase(IStreamCacheRepository*     cache,
                                             IPlaybackDownloaderFactory* downloaderFactory,
                                             IDashPackagerFactory*       packagerFactory,
                                             IDispatcher*                dispatcher,
                                             std::string                 hostBase)
    : m_cache(cache),
      m_downloaderFactory(downloaderFactory),
      m_packagerFactory(packagerFactory),
      m_dispatcher(dispatcher),
      m_hostBase(std::move(hostBase)) {}

void StreamPlaybackUseCase::requestStream(const PlaybackRequest& request) {
    const PlaybackKey& key = request.key;

    if (m_cache->dashExists(key)) {
        emitReady(key, m_cache->mpdUrl(key, m_hostBase));
        return;
    }

    if (m_activeJobs.find(key) != m_activeJobs.end())
        return;

    if (m_cache->mp4Exists(key)) {
        Job job;
        job.key     = key;
        job.channel = request.channel;
        job.state   = JobState::Pending;
        m_activeJobs.emplace(key, std::move(job));
        startPackage(key);
        return;
    }

    startDownload(request);
}

StreamStatus StreamPlaybackUseCase::currentStatus(const PlaybackKey& key) const {
    if (m_cache->dashExists(key))
        return StreamStatus::Ready;

    const auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end())
        return StreamStatus::Unknown;

    switch (it->second.state) {
        case JobState::Pending:     return StreamStatus::Pending;
        case JobState::Downloading: return StreamStatus::Downloading;
        case JobState::Packaging:   return StreamStatus::Packaging;
    }
    return StreamStatus::Unknown;
}

void StreamPlaybackUseCase::startDownload(const PlaybackRequest& request) {
    const std::string mp4Path = m_cache->mp4Path(request.key);
    const PlaybackKey key     = request.key;

    auto worker = m_downloaderFactory->create(request, mp4Path);
    IPlaybackDownloader* raw = worker.get();

    Job job;
    job.key        = key;
    job.channel    = request.channel;
    job.state      = JobState::Downloading;
    job.downloader = std::move(worker);
    m_activeJobs.emplace(key, std::move(job));

    raw->setOnProgress([this, key](int percent) {
        m_dispatcher->post([this, key, percent] {
            emitProgress(key, percent);
        });
    });
    raw->setOnFinished([this, key](bool success, std::string err) {
        m_dispatcher->post([this, key, success, err = std::move(err)]() mutable {
            onDownloadFinished(key, success, std::move(err));
        });
    });

    raw->start();
}

void StreamPlaybackUseCase::onDownloadFinished(const PlaybackKey& key,
                                               bool success,
                                               std::string err) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    if (!success) {
        m_activeJobs.erase(it);
        emitError(key, "Download failed: " + err);
        return;
    }

    it->second.state = JobState::Packaging;
    it->second.downloader.reset();
    startPackage(key);
}

void StreamPlaybackUseCase::startPackage(const PlaybackKey& key) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    auto packager = m_packagerFactory->create();
    IDashPackager* raw = packager.get();
    it->second.state    = JobState::Packaging;
    it->second.packager = std::move(packager);

    raw->setOnFinished([this, key](bool success,
                                   std::string /*mpdPath*/,
                                   std::string err) {
        m_dispatcher->post([this, key, success, err = std::move(err)]() mutable {
            onPackageFinished(key, success, std::move(err));
        });
    });

    raw->package(m_cache->mp4Path(key), m_cache->dashDir(key));
}

void StreamPlaybackUseCase::onPackageFinished(const PlaybackKey& key,
                                              bool success,
                                              std::string err) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    m_activeJobs.erase(it);

    if (!success) {
        emitError(key, "DASH packaging failed: " + err);
        return;
    }
    emitReady(key, m_cache->mpdUrl(key, m_hostBase));
}

void StreamPlaybackUseCase::emitReady(const PlaybackKey& key, std::string mpdUrl) {
    for (const auto& cb : m_readyCbs) cb(key, mpdUrl);
}

void StreamPlaybackUseCase::emitError(const PlaybackKey& key, std::string reason) {
    for (const auto& cb : m_errorCbs) cb(key, reason);
}

void StreamPlaybackUseCase::emitProgress(const PlaybackKey& key, int percent) {
    for (const auto& cb : m_progressCbs) cb(key, percent);
}
