#include "PlaybackProcessor.h"

#include <iostream>
#include <utility>
#include <vector>

// Concrete infra implementations are named only in this translation unit; the
// adapter header stays free of any HCNetSDK/ffmpeg/filesystem type.
#include "FfmpegDashPackager.h"
#include "FileSystemStreamCache.h"
#include "HCNetSDKAuthenticator.h"
#include "HCNetSDKDownloader.h"

PlaybackProcessor::PlaybackProcessor(const PlaybackProcessorConfig& config,
                                     QObject*                       parent)
    : QObject(parent),
      m_cache(std::make_unique<FileSystemStreamCache>(config.downloadsDir,
                                                      config.maxDownloadsBytes)),
      m_authenticator(std::make_unique<HCNetSDKAuthenticator>()),
      m_loginUseCase(std::make_unique<LoginUseCase>(m_authenticator.get(), config.loginIdle)),
      m_downloaderFactory(std::make_unique<HCNetSDKDownloaderFactory>()),
      m_packagerFactory(std::make_unique<FfmpegDashPackagerFactory>()),
      m_hostBase(config.hostBase) {}

void PlaybackProcessor::onPlaybackRequested(PlaybackRequest request) {
    const PlaybackKey& key = request.key;
    const QString keyHex   = QString::fromStdString(key.hex);

    if (m_cache->dashExists(key)) {
        emit statusChanged(keyHex, StreamStatus::Ready,
                           QString::fromStdString(m_cache->mpdUrl(key, m_hostBase)));
        std::cout << "[stream-ready] hash=" << key.hex << std::endl;
        return;
    }

    if (m_activeJobs.find(key) != m_activeJobs.end())
        return;

    const SessionToken token = m_loginUseCase->ensureLoggedIn(request.credentials);
    if (token < 0) {
        const int sdkErr = m_loginUseCase->lastErrorCode();
        std::cerr << "[login-fail] ip=" << request.credentials.ip
                  << " sdkError=" << sdkErr << std::endl;
        emit statusChanged(keyHex, StreamStatus::Failed, QString());
        std::cerr << "[stream-error] hash=" << key.hex
                  << " reason=login failed (sdk error " << sdkErr << ")" << std::endl;
        return;
    }
    std::cout << "[login-ok] ip=" << request.credentials.ip
              << " userId=" << static_cast<int>(token) << std::endl;
    request.token = token;

    if (m_cache->mp4Exists(key)) {
        Job job;
        job.key     = key;
        job.channel = request.channel;
        job.state   = JobState::Pending;
        m_activeJobs.emplace(key, std::move(job));
        emit statusChanged(keyHex, StreamStatus::Pending, QString());
        startPackage(key);
        return;
    }

    startDownload(request);
}

void PlaybackProcessor::onKeyActivated(QString keyHex) {
    m_activeStreamKeys.insert(keyHex);
}

void PlaybackProcessor::onKeyDeactivated(QString keyHex) {
    m_activeStreamKeys.remove(keyHex);
}

void PlaybackProcessor::startDownload(const PlaybackRequest& request) {
    const PlaybackKey key = request.key;
    const QString keyHex  = QString::fromStdString(key.hex);

    auto worker          = m_downloaderFactory->create(request, m_cache->mp4Path(key));
    IPlaybackDownloader* raw = worker.get();

    Job job;
    job.key        = key;
    job.channel    = request.channel;
    job.state      = JobState::Downloading;
    job.downloader = std::move(worker);
    m_activeJobs.emplace(key, std::move(job));
    emit statusChanged(keyHex, StreamStatus::Downloading, QString());

    raw->setOnProgress([this, key](int percent) {
        QMetaObject::invokeMethod(this, [key, percent] {
            std::cout << "[download] hash=" << key.hex
                      << " " << percent << "%" << std::endl;
        }, Qt::QueuedConnection);
    });
    raw->setOnFinished([this, key](bool ok, std::string err) {
        QString qerr = QString::fromStdString(err);
        QMetaObject::invokeMethod(this, [this, key, ok, qerr] {
            onDownloadFinished(key, ok, qerr);
        }, Qt::QueuedConnection);
    });

    raw->start();
}

void PlaybackProcessor::startPackage(const PlaybackKey& key) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    auto packager        = m_packagerFactory->create();
    IDashPackager* raw   = packager.get();
    it->second.state     = JobState::Packaging;
    it->second.packager  = std::move(packager);
    emit statusChanged(QString::fromStdString(key.hex), StreamStatus::Packaging, QString());

    raw->setOnFinished([this, key](bool ok, std::string /*mpdPath*/, std::string err) {
        QString qerr = QString::fromStdString(err);
        QMetaObject::invokeMethod(this, [this, key, ok, qerr] {
            onPackageFinished(key, ok, qerr);
        }, Qt::QueuedConnection);
    });

    raw->package(m_cache->mp4Path(key), m_cache->dashDir(key));
}

void PlaybackProcessor::onDownloadFinished(PlaybackKey key, bool success, QString err) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    if (!success) {
        m_activeJobs.erase(it);
        emit statusChanged(QString::fromStdString(key.hex), StreamStatus::Failed, QString());
        std::cerr << "[stream-error] hash=" << key.hex
                  << " reason=download failed: " << err.toStdString() << std::endl;
        return;
    }

    it->second.state = JobState::Packaging;
    it->second.downloader.reset();
    startPackage(key);
}

void PlaybackProcessor::onPackageFinished(PlaybackKey key, bool success, QString err) {
    auto it = m_activeJobs.find(key);
    if (it == m_activeJobs.end()) return;

    m_activeJobs.erase(it);

    if (!success) {
        emit statusChanged(QString::fromStdString(key.hex), StreamStatus::Failed, QString());
        std::cerr << "[stream-error] hash=" << key.hex
                  << " reason=DASH packaging failed: " << err.toStdString() << std::endl;
        return;
    }
    m_cache->deleteMp4(key);

    std::vector<PlaybackKey> skip;
    skip.reserve(m_activeJobs.size() + static_cast<size_t>(m_activeStreamKeys.size()));
    for (const auto& [k, _] : m_activeJobs)
        skip.push_back(k);
    for (const QString& k : m_activeStreamKeys)
        skip.push_back(PlaybackKey{k.toStdString()});
    m_cache->evictToCapacity(skip);

    emit statusChanged(QString::fromStdString(key.hex), StreamStatus::Ready,
                       QString::fromStdString(m_cache->mpdUrl(key, m_hostBase)));
    std::cout << "[stream-ready] hash=" << key.hex << std::endl;
}
