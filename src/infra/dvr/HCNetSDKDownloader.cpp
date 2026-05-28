#include "HCNetSDKDownloader.h"
#include "HCNetSDKTimeMapper.h"

#include <chrono>
#include <string>
#include <utility>

HCNetSDKDownloader::HCNetSDKDownloader(const PlaybackRequest& request,
                                       const std::string& outputPath)
    : m_request(request), m_outputPath(outputPath) {}

HCNetSDKDownloader::~HCNetSDKDownloader() {
    m_cancelled.store(true, std::memory_order_relaxed);
    if (m_thread.joinable())
        m_thread.join();
}

void HCNetSDKDownloader::start() {
    m_thread = std::thread([this] { run(); });
}

void HCNetSDKDownloader::cancel() {
    m_cancelled.store(true, std::memory_order_relaxed);
}

void HCNetSDKDownloader::run() {
    NET_DVR_PLAYCOND cond{};
    cond.dwChannel     = static_cast<DWORD>(m_request.channel.id);
    cond.struStartTime = HCNetSDKTimeMapper::toSdk(m_request.range.begin);
    cond.struStopTime  = HCNetSDKTimeMapper::toSdk(m_request.range.end);

    LONG hDl = NET_DVR_GetFileByTime_V40(static_cast<LONG>(m_request.token),
                                         const_cast<char*>(m_outputPath.c_str()),
                                         &cond);
    if (hDl < 0) {
        if (m_onFinished)
            m_onFinished(false,
                         "GetFileByTime failed (error "
                         + std::to_string(NET_DVR_GetLastError()) + ")");
        return;
    }

    if (!NET_DVR_PlayBackControl(hDl, NET_DVR_PLAYSTART, 0, nullptr)) {
        NET_DVR_StopGetFile(hDl);
        if (m_onFinished)
            m_onFinished(false,
                         "PlayBackControl failed (error "
                         + std::to_string(NET_DVR_GetLastError()) + ")");
        return;
    }

    int pos = 0;
    while (!m_cancelled.load(std::memory_order_relaxed)) {
        pos = NET_DVR_GetDownloadPos(hDl);
        if (pos < 0 || pos > 100)
            break;
        if (m_onProgress)
            m_onProgress(pos);
        if (pos == 100)
            break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    NET_DVR_StopGetFile(hDl);

    if (!m_onFinished) return;
    if (m_cancelled.load(std::memory_order_relaxed)) {
        m_onFinished(false, "Download cancelled.");
    } else if (pos == 100) {
        m_onFinished(true, {});
    } else {
        m_onFinished(false,
                     "Download error (pos=" + std::to_string(pos)
                     + ", error " + std::to_string(NET_DVR_GetLastError()) + ")");
    }
}
