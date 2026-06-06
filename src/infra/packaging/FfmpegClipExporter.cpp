#include "FfmpegClipExporter.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <utility>

FfmpegClipExporter::FfmpegClipExporter() = default;

FfmpegClipExporter::~FfmpegClipExporter() {
    cancel();
}

void FfmpegClipExporter::exportClip(const std::string& mpdPath,
                                    const std::string& outputPath,
                                    int startSec,
                                    int endSec) {
    m_outputPath = outputPath;
    m_completed  = false;

    m_process = new QProcess;

    QObject::connect(m_process,
                     qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     m_process,
                     [this](int exitCode, QProcess::ExitStatus) {
                         if (exitCode == 0) {
                             complete(true, m_outputPath, {});
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

    // -ss before -i is fast input seeking (lands on the nearest keyframe with
    // -c copy); -t bounds the output duration. Both sides are optional:
    // startSec <= 0 -> from start, endSec <= 0 -> to end. -t (duration) is used
    // rather than -to so the stop point is unambiguous relative to the seek.
    QStringList args;
    args << "-y";
    const int begin = startSec > 0 ? startSec : 0;
    if (begin > 0)
        args << "-ss" << QString::number(begin);
    args << "-i" << QString::fromStdString(mpdPath);
    if (endSec > 0)
        args << "-t" << QString::number(endSec - begin);
    args << "-c" << "copy"
         << "-movflags" << "+faststart"
         << QString::fromStdString(outputPath);

    m_process->start("ffmpeg", args);
}

void FfmpegClipExporter::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void FfmpegClipExporter::complete(bool success,
                                  std::string outputPath,
                                  std::string errorMessage) {
    if (m_completed) return;
    m_completed = true;
    if (m_onFinished)
        m_onFinished(success, std::move(outputPath), std::move(errorMessage));
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
