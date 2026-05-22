#pragma once
#include <QObject>
#include "PlaybackRequest.h"

class DownloadWorker : public QObject {
    Q_OBJECT
public:
    explicit DownloadWorker(const PlaybackRequest& req, QObject* parent = nullptr);

public slots:
    void run();
    void cancel();

signals:
    void progressChanged(int percent);
    void finished(bool success, QString errorMessage);

private:
    PlaybackRequest m_req;
    volatile bool   m_cancelled = false;
};
