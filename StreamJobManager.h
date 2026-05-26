#pragma once
#include <QMap>
#include <QObject>
#include <QString>
#include "HCNetSDK.h"

class DownloadWorker;
class DashPackager;
class QThread;

class StreamJobManager : public QObject {
    Q_OBJECT
public:
    explicit StreamJobManager(const QString& downloadsDir, QObject* parent = nullptr);

    // Kicks off the cache-check → download → package pipeline.
    // If DASH already exists, emits streamReady() immediately.
    // If a job for the same key is already in flight, the signal fires when done.
    void requestStream(LONG userId, int channel,
                       const NET_DVR_TIME& startTime,
                       const NET_DVR_TIME& endTime);

signals:
    void streamReady(const QString& mpdUrl);
    void downloadProgress(int channel, int percent);
    void streamError(const QString& reason);

private slots:
    void onDownloadProgress(int percent);
    void onDownloadFinished(bool success, const QString& errorMsg);
    void onPackageFinished(bool success, const QString& mpdPath, const QString& errorMsg);

private:
    enum class JobState { Downloading, Packaging };

    struct Job {
        QString         key;
        int             channel  = 0;
        JobState        state    = JobState::Downloading;
        DownloadWorker* worker   = nullptr;
        QThread*        thread   = nullptr;
        DashPackager*   packager = nullptr;
    };

    QString           m_downloadsDir;
    QMap<QString,Job> m_activeJobs;

    bool    dashExists(const QString& key) const;
    bool    mp4Exists(const QString& key)  const;
    QString mp4Path(const QString& key)   const;
    QString dashDir(const QString& key)   const;
    QString mpdUrl(const QString& key)    const;

    void startDownload(const QString& key, LONG userId, int channel,
                       const NET_DVR_TIME& s, const NET_DVR_TIME& e);
    void startPackage(const QString& key);
    void cleanupJob(const QString& key);
};
