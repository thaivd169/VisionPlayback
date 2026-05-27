#include "StreamPlaybackUseCase.h"

#include <QThread>
#include <utility>

StreamPlaybackUseCase::StreamPlaybackUseCase(IStreamCacheRepository*     cache,
                                             IPlaybackDownloaderFactory* downloaderFactory,
                                             IDashPackagerFactory*       packagerFactory,
                                             QString                     hostBase,
                                             QObject*                    parent)
    : QObject(parent),
      m_cache(cache),
      m_downloaderFactory(downloaderFactory),
      m_packagerFactory(packagerFactory),
      m_hostBase(std::move(hostBase)) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void StreamPlaybackUseCase::requestStream(const PlaybackRequest& request) {
    const PlaybackKey& key = request.key;

    if (m_cache->dashExists(key)) {
        emit streamReady(key,
                         QString::fromStdString(m_cache->mpdUrl(key,
                                                                m_hostBase.toStdString())));
        return;
    }

    if (m_activeJobs.contains(key))
        return; // already in flight; streamReady will fire when done

    if (m_cache->mp4Exists(key)) {
        Job job;
        job.key     = key;
        job.channel = request.channel;
        job.state   = JobState::Pending;
        m_activeJobs.insert(key, job);
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

    switch (it->state) {
        case JobState::Pending:     return StreamStatus::Pending;
        case JobState::Downloading: return StreamStatus::Downloading;
        case JobState::Packaging:   return StreamStatus::Packaging;
    }
    return StreamStatus::Unknown;
}

// ---------------------------------------------------------------------------
// Download phase
// ---------------------------------------------------------------------------

void StreamPlaybackUseCase::startDownload(const PlaybackRequest& request) {
    const std::string mp4Path = m_cache->mp4Path(request.key);

    IPlaybackDownloader* worker = m_downloaderFactory->create(request, mp4Path);
    auto* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, &QThread::started,                worker, &IPlaybackDownloader::start);
    connect(worker, &IPlaybackDownloader::progressChanged, this, &StreamPlaybackUseCase::onDownloadProgress);
    connect(worker, &IPlaybackDownloader::finished,   this,   &StreamPlaybackUseCase::onDownloadFinished);
    connect(worker, &IPlaybackDownloader::finished,   thread, &QThread::quit);
    connect(thread, &QThread::finished,               worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,               thread, &QObject::deleteLater);

    Job job;
    job.key     = request.key;
    job.channel = request.channel;
    job.state   = JobState::Downloading;
    job.worker  = worker;
    job.thread  = thread;
    m_activeJobs.insert(request.key, job);

    thread->start();
}

void StreamPlaybackUseCase::onDownloadProgress(int percent) {
    auto* worker = qobject_cast<IPlaybackDownloader*>(sender());
    for (auto it = m_activeJobs.begin(); it != m_activeJobs.end(); ++it) {
        if (it->worker == worker) {
            emit downloadProgress(it.key(), percent);
            return;
        }
    }
}

void StreamPlaybackUseCase::onDownloadFinished(bool success, QString errorMessage) {
    auto* worker = qobject_cast<IPlaybackDownloader*>(sender());
    PlaybackKey key;
    bool found = false;
    for (auto it = m_activeJobs.begin(); it != m_activeJobs.end(); ++it) {
        if (it->worker == worker) {
            key = it.key();
            found = true;
            break;
        }
    }
    if (!found) return;

    if (!success) {
        cleanupJob(key);
        emit streamError(key, "Download failed: " + errorMessage);
        return;
    }

    m_activeJobs[key].state  = JobState::Packaging;
    m_activeJobs[key].worker = nullptr;
    m_activeJobs[key].thread = nullptr; // thread self-destructs via deleteLater
    startPackage(key);
}

// ---------------------------------------------------------------------------
// Package phase
// ---------------------------------------------------------------------------

void StreamPlaybackUseCase::startPackage(const PlaybackKey& key) {
    IDashPackager* packager = m_packagerFactory->create(this);
    connect(packager, &IDashPackager::finished, this, &StreamPlaybackUseCase::onPackageFinished);

    m_activeJobs[key].state    = JobState::Packaging;
    m_activeJobs[key].packager = packager;

    packager->package(m_cache->mp4Path(key), m_cache->dashDir(key));
}

void StreamPlaybackUseCase::onPackageFinished(bool success, QString mpdPath,
                                              QString errorMessage) {
    auto* packager = qobject_cast<IDashPackager*>(sender());
    PlaybackKey key;
    bool found = false;
    for (auto it = m_activeJobs.begin(); it != m_activeJobs.end(); ++it) {
        if (it->packager == packager) {
            key = it.key();
            found = true;
            break;
        }
    }
    if (!found) return;

    packager->deleteLater();

    if (!success) {
        cleanupJob(key);
        emit streamError(key, "DASH packaging failed: " + errorMessage);
        return;
    }

    Q_UNUSED(mpdPath)
    cleanupJob(key);
    emit streamReady(key,
                     QString::fromStdString(m_cache->mpdUrl(key,
                                                            m_hostBase.toStdString())));
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void StreamPlaybackUseCase::cleanupJob(const PlaybackKey& key) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    Job& job = it.value();
    if (job.thread && job.thread->isRunning()) {
        if (job.worker) job.worker->cancel();
        job.thread->quit();
        job.thread->wait(3000);
    }
    m_activeJobs.erase(it);
}
