#include "FfmpegDashPackager.h"

#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <utility>

FfmpegDashPackager::FfmpegDashPackager() = default;

FfmpegDashPackager::~FfmpegDashPackager() {
    cancel();
}

void FfmpegDashPackager::package(const std::string& mp4Path,
                                 const std::string& outputDir) {
    const QString qOutputDir = QString::fromStdString(outputDir);
    QDir().mkpath(qOutputDir);
    m_mpdPath   = outputDir + "/manifest.mpd";
    m_completed = false;

    m_process = new QProcess;

    QObject::connect(m_process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     m_process,
                     [this](int exitCode, QProcess::ExitStatus) {
                         if (exitCode == 0) {
                             complete(true, m_mpdPath, {});
                         } else {
                             const std::string err =
                                 m_process
                                     ? m_process->readAllStandardError().toStdString()
                                     : std::string{};
                             complete(false, {}, err);
                         }
                     });
    QObject::connect(m_process, &QProcess::errorOccurred,
                     m_process, [this](QProcess::ProcessError) {
                         complete(false, {},
                                  "Failed to start ffmpeg. "
                                  "Ensure ffmpeg is installed and on PATH.");
                     });

    // Browsers can't reliably play HEVC (hev1) video or G.711 audio, so the
    // served DASH must be H.264 + AAC. Probe the source and copy only when it
    // is already web-friendly; otherwise transcode. An empty video probe
    // (ffprobe missing/errored) falls through to transcoding so the output is
    // guaranteed H.264 rather than an unknown passthrough.
    const std::string videoCodec = probeCodec(mp4Path, "v:0");
    const std::string audioCodec = probeCodec(mp4Path, "a:0");

    QStringList args;
    args << "-y"
         << "-i" << QString::fromStdString(mp4Path);

    if (videoCodec == "h264") {
        args << "-c:v" << "copy";
    } else {
        // -force_key_frames interval matches -seg_duration so the dash muxer can
        // split cleanly on keyframes.
        args << "-c:v" << "libx264"
             << "-preset" << "veryfast"
             << "-crf" << "23"
             << "-profile:v" << "high"
             << "-pix_fmt" << "yuv420p"
             << "-force_key_frames" << "expr:gte(t,n_forced*4)";
    }

    if (audioCodec.empty()) {
        args << "-an";
    } else if (audioCodec == "aac") {
        args << "-c:a" << "copy";
    } else {
        args << "-c:a" << "aac" << "-b:a" << "128k";
    }

    args << "-f" << "dash"
         << "-seg_duration" << "4"
         << "-init_seg_name"  << "init-stream$RepresentationID$.m4s"
         << "-media_seg_name" << "chunk-stream$RepresentationID$-$Number%05d$.m4s"
         << QString::fromStdString(m_mpdPath);

    m_process->start("ffmpeg", args);
}

std::string FfmpegDashPackager::probeCodec(const std::string& path,
                                           const char*        streamSpec) {
    QProcess probe;
    const QStringList args = {
        "-v", "error",
        "-select_streams", streamSpec,
        "-show_entries", "stream=codec_name",
        "-of", "default=nw=1:nk=1",
        QString::fromStdString(path)
    };
    probe.start("ffprobe", args);
    if (!probe.waitForStarted(3000) || !probe.waitForFinished(5000) ||
        probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        return {};
    }
    return probe.readAllStandardOutput().trimmed().toStdString();
}

void FfmpegDashPackager::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void FfmpegDashPackager::complete(bool success,
                                  std::string mpdPath,
                                  std::string errorMessage) {
    if (m_completed) return;
    m_completed = true;
    if (m_onFinished)
        m_onFinished(success, std::move(mpdPath), std::move(errorMessage));
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
