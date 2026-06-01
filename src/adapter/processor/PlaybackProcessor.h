#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "IAuthenticator.h"
#include "IDashPackager.h"
#include "IPlaybackDownloader.h"
#include "IStreamCacheRepository.h"
#include "LoginUseCase.h"
#include "PlaybackKey.h"
#include "PlaybackRequest.h"
#include "StreamStatus.h"

// CLI-derived configuration for the processor. Everything the processor needs —
// including the on-disk stream cache — is built internally from this config.
// hostBase is only used to format the ready-stream URL handed to the HTTP
// listener via the statusChanged signal.
struct PlaybackProcessorConfig {
    std::chrono::seconds loginIdle;
    QString              downloadsDir;
    std::uint64_t        maxDownloadsBytes;
    std::string          hostBase;
};

// Lives on a dedicated QThread. Owns the full pipeline FSM:
//   receive request → login → download → transcode to DASH → evict old files
//
// Communicates with the HTTP thread exclusively via Qt signals/slots.
class PlaybackProcessor : public QObject {
    Q_OBJECT
public:
    explicit PlaybackProcessor(const PlaybackProcessorConfig& config,
                               QObject*                       parent = nullptr);

public slots:
    void onPlaybackRequested(PlaybackRequest request);
    // Records the last access time of a key (see HttpListener::keyAccessed).
    void onKeyAccessed(QString keyHex);

signals:
    // Emitted on every job state transition so the HTTP listener can maintain
    // its own status cache without any cross-thread data access. `url` is the
    // playable manifest URL — non-empty only when status == Ready, empty
    // otherwise — so the listener never needs to touch the cache itself.
    void statusChanged(QString keyHex, StreamStatus status, QString url);

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

    std::unique_ptr<IStreamCacheRepository>     m_cache;             // owned, infra impl
    std::unique_ptr<IAuthenticator>             m_authenticator;     // owned, infra impl
    std::unique_ptr<LoginUseCase>               m_loginUseCase;      // owned
    std::unique_ptr<IPlaybackDownloaderFactory> m_downloaderFactory; // owned, infra impl
    std::unique_ptr<IDashPackagerFactory>       m_packagerFactory;   // owned, infra impl
    std::string                                 m_hostBase;          // for ready-URL only
    std::unordered_map<PlaybackKey, Job>        m_activeJobs;

    // keyHex -> last DASH access (ms since epoch). A key accessed within
    // kAccessTtl of an eviction pass is considered in use and skipped. The
    // window must comfortably exceed the DASH segment duration so a watched
    // stream survives the gaps between segment fetches; stale entries are
    // pruned lazily during eviction.
    static constexpr qint64 kAccessTtlMs = 60'000;
    QHash<QString, qint64>                      m_lastAccessMs;
};
