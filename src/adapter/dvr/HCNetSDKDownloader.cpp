#include "HCNetSDKDownloader.h"
#include "HCNetSDKTimeMapper.h"

#include <QThread>

HCNetSDKDownloader::HCNetSDKDownloader(const PlaybackRequest& request,
                                       const std::string& outputPath,
                                       QObject* parent)
    : IPlaybackDownloader(parent),
      m_request(request),
      m_outputPath(outputPath) {}

void HCNetSDKDownloader::cancel() {
    m_cancelled = true;
}

void HCNetSDKDownloader::start() {
    NET_DVR_PLAYCOND cond{};
    cond.dwChannel     = static_cast<DWORD>(m_request.channel.id);
    cond.struStartTime = HCNetSDKTimeMapper::toSdk(m_request.range.begin);
    cond.struStopTime  = HCNetSDKTimeMapper::toSdk(m_request.range.end);

    LONG hDl = NET_DVR_GetFileByTime_V40(static_cast<LONG>(m_request.token),
                                         const_cast<char*>(m_outputPath.c_str()),
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
