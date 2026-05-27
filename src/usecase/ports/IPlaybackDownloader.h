#pragma once
#include <QObject>
#include <QString>
#include <string>

#include "PlaybackRequest.h"

// Per-job worker. Implementations live on their own QThread, owned by the
// use case. The use case connects() to progressChanged / finished with
// Qt::QueuedConnection so signals cross thread boundaries safely.
class IPlaybackDownloader : public QObject {
    Q_OBJECT
public:
    explicit IPlaybackDownloader(QObject* parent = nullptr) : QObject(parent) {}
    ~IPlaybackDownloader() override = default;

public slots:
    virtual void start() = 0;
    virtual void cancel() = 0;

signals:
    void progressChanged(int percent);
    void finished(bool success, QString errorMessage);
};

// Factory injected into the use case. Concrete adapter
// (HCNetSDKDownloaderFactory) returns HCNetSDKDownloader instances bound to
// a specific (request, outputPath). Caller owns the returned worker.
class IPlaybackDownloaderFactory {
public:
    virtual ~IPlaybackDownloaderFactory() = default;
    virtual IPlaybackDownloader* create(const PlaybackRequest& request,
                                        const std::string& outputPath) = 0;
};
