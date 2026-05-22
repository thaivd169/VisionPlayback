#include "DownloadWorker.h"

#include <QThread>

DownloadWorker::DownloadWorker(const PlaybackRequest& req, QObject* parent)
    : QObject(parent), m_req(req) {}

void DownloadWorker::cancel() {
    m_cancelled = true;
}

void DownloadWorker::run() {
    NET_DVR_PLAYCOND cond{};
    cond.dwChannel    = static_cast<DWORD>(m_req.channel);
    cond.struStartTime = m_req.startTime;
    cond.struStopTime  = m_req.endTime;

    QByteArray pathBytes = m_req.outputPath.toLocal8Bit();

    LONG hDl = NET_DVR_GetFileByTime_V40(m_req.userId,
                                          pathBytes.data(),
                                          &cond);
    if (hDl < 0) {
        emit finished(false, QString("GetFileByTime failed (error %1)")
                             .arg(NET_DVR_GetLastError()));
        return;
    }

    if (!NET_DVR_PlayBackControl(hDl, NET_DVR_PLAYSTART, 0, nullptr)) {
        NET_DVR_StopGetFile(hDl);
        emit finished(false, QString("PlayBackControl failed (error %1)")
                             .arg(NET_DVR_GetLastError()));
        return;
    }

    int pos = 0;
    while (!m_cancelled) {
        pos = NET_DVR_GetDownloadPos(hDl);
        if (pos < 0 || pos > 100)
            break;
        emit progressChanged(pos);
        if (pos == 100)
            break;
        QThread::sleep(1);
    }

    NET_DVR_StopGetFile(hDl);

    if (m_cancelled) {
        emit finished(false, "Download cancelled.");
    } else if (pos == 100) {
        emit finished(true, {});
    } else {
        emit finished(false, QString("Download error (pos=%1, error %2)")
                             .arg(pos).arg(NET_DVR_GetLastError()));
    }
}
