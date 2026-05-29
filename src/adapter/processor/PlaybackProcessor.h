#pragma once
#include <QObject>
#include <QSet>
#include <QString>
#include <memory>
#include <string>
#include <unordered_map>

#include "IDashPackager.h"
#include "IPlaybackDownloader.h"
#include "IStreamCacheRepository.h"
#include "PlaybackKey.h"
#include "PlaybackRequest.h"
#include "StreamStatus.h"

class LoginUseCase;

// Lives on a dedicated QThread. Owns the full pipeline FSM:
//   receive request → login → download → transcode to DASH → evict old files
//
// Communicates with the HTTP thread exclusively via Qt signals/slots.
class PlaybackProcessor : public QObject {
    Q_OBJECT
public:
    PlaybackProcessor(IStreamCacheRepository*     cache,
                      LoginUseCase*               loginUseCase,
                      IPlaybackDownloaderFactory* downloaderFactory,
                      IDashPackagerFactory*       packagerFactory,
                      std::string                 hostBase,
                      QObject*                    parent = nullptr);

public slots:
    void onPlaybackRequested(PlaybackRequest request);
    void onKeyActivated(QString keyHex);
    void onKeyDeactivated(QString keyHex);

signals:
    void streamReady(QString keyHex, QString mpdUrl);
    void streamError(QString keyHex, QString reason);
    void streamProgress(QString keyHex, int percent);
    // Emitted on every job state transition so PollingApi can maintain its own
    // status cache without any cross-thread data access.
    void statusChanged(QString keyHex, StreamStatus status);

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
    void onDownloadFinished(PlaybackKey key, bool success, QString err);
    void onPackageFinished(PlaybackKey key, bool success, QString err);

    IStreamCacheRepository*              m_cache;
    LoginUseCase*                        m_loginUseCase;
    IPlaybackDownloaderFactory*          m_downloaderFactory;
    IDashPackagerFactory*                m_packagerFactory;
    std::string                          m_hostBase;
    std::unordered_map<PlaybackKey, Job> m_activeJobs;
    QSet<QString>                        m_activeStreamKeys;
};
