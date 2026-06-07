#pragma once
#include <string>

#include "IDashPackager.h"

class QProcess;

// ffmpeg-based DASH packager. Uses QProcess internally (Qt is fine in infra)
// but exposes only the Qt-free IDashPackager port. Connections are routed
// through the QProcess itself as their context object so this class need not
// be a QObject.
class FfmpegDashPackager : public IDashPackager {
public:
    FfmpegDashPackager();
    ~FfmpegDashPackager() override;

    void setOnFinished(FinishedCb cb) override { m_onFinished = std::move(cb); }

    void package(const std::string& mp4Path,
                 const std::string& outputDir) override;
    void cancel() override;

private:
    void complete(bool success, std::string mpdPath, std::string errorMessage);

    // Probes one stream's codec_name via ffprobe. streamSpec is an ffmpeg
    // stream specifier ("v:0", "a:0"). Returns e.g. "h264"/"hevc"/"aac", or ""
    // when the stream is absent or ffprobe is unavailable/errors.
    static std::string probeCodec(const std::string& path,
                                  const char*        streamSpec);

    QProcess*   m_process   = nullptr;
    std::string m_mpdPath;
    bool        m_completed = false;
    FinishedCb  m_onFinished;
};

class FfmpegDashPackagerFactory : public IDashPackagerFactory {
public:
    std::unique_ptr<IDashPackager> create() override {
        return std::make_unique<FfmpegDashPackager>();
    }
};
