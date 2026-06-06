#pragma once
#include <string>
#include <utility>

#include "IClipExporter.h"

class QProcess;

// ffmpeg-based clip exporter. Remuxes an existing DASH manifest into a single
// MP4 with stream copy (no re-encode), optionally trimmed. Mirrors
// FfmpegDashPackager: an async QProcess whose connections are routed through
// the process itself as their context object, so this class need not be a
// QObject. Exposes only the Qt-free IClipExporter port.
class FfmpegClipExporter : public IClipExporter {
public:
    FfmpegClipExporter();
    ~FfmpegClipExporter() override;

    void setOnFinished(FinishedCb cb) override { m_onFinished = std::move(cb); }

    void exportClip(const std::string& mpdPath,
                    const std::string& outputPath,
                    int startSec,
                    int endSec) override;
    void cancel() override;

private:
    void complete(bool success, std::string outputPath, std::string errorMessage);

    QProcess*   m_process   = nullptr;
    std::string m_outputPath;
    bool        m_completed = false;
    FinishedCb  m_onFinished;
};

class FfmpegClipExporterFactory : public IClipExporterFactory {
public:
    std::unique_ptr<IClipExporter> create() override {
        return std::make_unique<FfmpegClipExporter>();
    }
};
