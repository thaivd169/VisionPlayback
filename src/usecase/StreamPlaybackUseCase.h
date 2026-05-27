#pragma once
#include <QHash>
#include <QObject>
#include <QString>

#include "PlaybackKey.h"
#include "PlaybackRequest.h"
#include "StreamStatus.h"
#include "ports/IDashPackager.h"
#include "ports/IPlaybackDownloader.h"
#include "ports/IStreamCacheRepository.h"

class QThread;

inline std::size_t qHash(const PlaybackKey& key, std::size_t seed = 0) noexcept {
    return qHash(QByteArray::fromStdString(key.hex), seed);
}

// Owns the cache-check → download → package pipeline. Constructed once at
// startup with port instances pre-built by Session; the use case allocates
// per-job downloaders / packagers via the injected factories.
class StreamPlaybackUseCase : public QObject {
    Q_OBJECT
public:
    StreamPlaybackUseCase(IStreamCacheRepository*        cache,
                          IPlaybackDownloaderFactory*    downloaderFactory,
                          IDashPackagerFactory*          packagerFactory,
                          QString                        hostBase,
                          QObject*                       parent = nullptr);

    // Cache-first dispatch. If DASH already exists, fires streamReady
    // synchronously. If a job for the same key is already in flight, the
    // signal will fire when it completes. Otherwise kicks off the pipeline.
    void requestStream(const PlaybackRequest& request);

    StreamStatus currentStatus(const PlaybackKey& key) const;

signals:
    void streamReady(PlaybackKey key, QString mpdUrl);
    void downloadProgress(PlaybackKey key, int percent);
    void streamError(PlaybackKey key, QString reason);

private slots:
    void onDownloadProgress(int percent);
    void onDownloadFinished(bool success, QString errorMessage);
    void onPackageFinished(bool success, QString mpdPath, QString errorMessage);

private:
    enum class JobState { Pending, Downloading, Packaging };

    struct Job {
        PlaybackKey          key;
        Channel              channel;
        JobState             state    = JobState::Pending;
        IPlaybackDownloader* worker   = nullptr;
        QThread*             thread   = nullptr;
        IDashPackager*       packager = nullptr;
    };

    IStreamCacheRepository*     m_cache;
    IPlaybackDownloaderFactory* m_downloaderFactory;
    IDashPackagerFactory*       m_packagerFactory;
    QString                     m_hostBase;
    QHash<PlaybackKey, Job>     m_activeJobs;

    void startDownload(const PlaybackRequest& request);
    void startPackage(const PlaybackKey& key);
    void cleanupJob(const PlaybackKey& key);
};
