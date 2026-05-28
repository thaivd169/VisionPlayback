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

    const QStringList args = {
        "-y",
        "-i", QString::fromStdString(mp4Path),
        "-c", "copy",
        "-f", "dash",
        "-seg_duration", "4",
        "-init_seg_name",  "init-stream$RepresentationID$.m4s",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%05d$.m4s",
        QString::fromStdString(m_mpdPath)
    };

    m_process->start("ffmpeg", args);
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
