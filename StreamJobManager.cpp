#include "StreamJobManager.h"
#include "DashPackager.h"
#include "DownloadWorker.h"
#include "VideoKey.h"

#include <QFile>
#include <QThread>

StreamJobManager::StreamJobManager(const QString& downloadsDir, QObject* parent)
    : QObject(parent), m_downloadsDir(downloadsDir) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void StreamJobManager::requestStream(LONG userId, int channel,
                                     const NET_DVR_TIME& startTime,
                                     const NET_DVR_TIME& endTime) {
    const QString key = VideoKey::make(channel, startTime, endTime);

    if (dashExists(key)) {
        emit streamReady(mpdUrl(key));
        return;
    }

    if (m_activeJobs.contains(key))
        return; // already in flight; streamReady will fire when done

    if (mp4Exists(key)) {
        startPackage(key);
        return;
    }

    startDownload(key, userId, channel, startTime, endTime);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool StreamJobManager::dashExists(const QString& key) const {
    QFile f(dashDir(key) + "/manifest.mpd");
    if (!f.exists() || f.size() < 100)
        return false;
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = f.read(10);
    return header.startsWith("<?xml") || header.startsWith("<MPD");
}

bool StreamJobManager::mp4Exists(const QString& key) const {
    return QFile::exists(mp4Path(key));
}

QString StreamJobManager::mp4Path(const QString& key) const {
    return m_downloadsDir + "/" + key + ".mp4";
}

QString StreamJobManager::dashDir(const QString& key) const {
    return m_downloadsDir + "/" + key;
}

QString StreamJobManager::mpdUrl(const QString& key) const {
    return "http://localhost:8080/dash/" + key + "/manifest.mpd";
}

// ---------------------------------------------------------------------------
// Download phase
// ---------------------------------------------------------------------------

void StreamJobManager::startDownload(const QString& key, LONG userId, int channel,
                                     const NET_DVR_TIME& s, const NET_DVR_TIME& e) {
    PlaybackRequest req;
    req.userId     = userId;
    req.channel    = channel;
    req.startTime  = s;
    req.endTime    = e;
    req.outputPath = mp4Path(key);

    auto* worker = new DownloadWorker(req);
    auto* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, &QThread::started,            worker, &DownloadWorker::run);
    connect(worker, &DownloadWorker::progressChanged, this, &StreamJobManager::onDownloadProgress);
    connect(worker, &DownloadWorker::finished,    this, &StreamJobManager::onDownloadFinished);
    connect(worker, &DownloadWorker::finished,    thread, &QThread::quit);
    connect(thread, &QThread::finished,           worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,           thread, &QObject::deleteLater);

    Job job;
    job.key     = key;
    job.channel = channel;
    job.state   = JobState::Downloading;
    job.worker  = worker;
    job.thread  = thread;
    m_activeJobs.insert(key, job);

    thread->start();
}

void StreamJobManager::onDownloadProgress(int percent) {
    // Find which job this worker belongs to
    for (const auto& job : m_activeJobs)
        if (job.state == JobState::Downloading) {
            emit downloadProgress(job.channel, percent);
            return;
        }
}

void StreamJobManager::onDownloadFinished(bool success, const QString& errorMsg) {
    // Identify the job by its worker sender
    auto* worker = qobject_cast<DownloadWorker*>(sender());
    QString key;
    for (auto it = m_activeJobs.begin(); it != m_activeJobs.end(); ++it) {
        if (it->worker == worker) {
            key = it.key();
            break;
        }
    }
    if (key.isEmpty()) return;

    if (!success) {
        cleanupJob(key);
        emit streamError("Download failed: " + errorMsg);
        return;
    }

    m_activeJobs[key].state  = JobState::Packaging;
    m_activeJobs[key].worker = nullptr;
    m_activeJobs[key].thread = nullptr; // thread self-destructs via its deleteLater connections
    startPackage(key);
}

// ---------------------------------------------------------------------------
// Package phase
// ---------------------------------------------------------------------------

void StreamJobManager::startPackage(const QString& key) {
    auto* packager = new DashPackager(this);
    connect(packager, &DashPackager::finished, this, &StreamJobManager::onPackageFinished);

    if (!m_activeJobs.contains(key)) {
        // mp4Exists() fast-path: create a minimal job entry
        Job job;
        job.key     = key;
        job.state   = JobState::Packaging;
        job.packager = packager;
        m_activeJobs.insert(key, job);
    } else {
        m_activeJobs[key].state   = JobState::Packaging;
        m_activeJobs[key].packager = packager;
    }

    packager->package(mp4Path(key), dashDir(key));
}

void StreamJobManager::onPackageFinished(bool success, const QString& mpdPath,
                                         const QString& errorMsg) {
    auto* packager = qobject_cast<DashPackager*>(sender());
    QString key;
    for (auto it = m_activeJobs.begin(); it != m_activeJobs.end(); ++it) {
        if (it->packager == packager) {
            key = it.key();
            break;
        }
    }
    if (key.isEmpty()) return;

    packager->deleteLater();
    cleanupJob(key);

    if (!success) {
        emit streamError("DASH packaging failed: " + errorMsg);
        return;
    }

    Q_UNUSED(mpdPath)
    emit streamReady(mpdUrl(key));
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void StreamJobManager::cleanupJob(const QString& key) {
    if (!m_activeJobs.contains(key)) return;

    auto& job = m_activeJobs[key];
    if (job.thread && job.thread->isRunning()) {
        if (job.worker) job.worker->cancel();
        job.thread->quit();
        job.thread->wait(3000);
    }
    m_activeJobs.remove(key);
}
