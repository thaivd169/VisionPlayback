#include "FfmpegDashPackager.h"

#include <QDir>
#include <QProcess>
#include <QString>

FfmpegDashPackager::FfmpegDashPackager(QObject* parent) : IDashPackager(parent) {}

FfmpegDashPackager::~FfmpegDashPackager() {
    cancel();
}

void FfmpegDashPackager::package(const std::string& mp4Path,
                                 const std::string& outputDir) {
    const QString qOutputDir = QString::fromStdString(outputDir);
    QDir().mkpath(qOutputDir);
    m_mpdPath = qOutputDir + "/manifest.mpd";

    m_process = new QProcess(this);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &FfmpegDashPackager::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &FfmpegDashPackager::onProcessError);

    const QStringList args = {
        "-y",
        "-i", QString::fromStdString(mp4Path),
        "-c", "copy",
        "-f", "dash",
        "-seg_duration", "4",
        "-init_seg_name",  "init-stream$RepresentationID$.m4s",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%05d$.m4s",
        m_mpdPath
    };

    m_process->start("ffmpeg", args);
}

void FfmpegDashPackager::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void FfmpegDashPackager::onProcessFinished(int exitCode, int /*exitStatus*/) {
    if (exitCode == 0) {
        emit finished(true, m_mpdPath, {});
    } else {
        const QString err = QString::fromUtf8(m_process->readAllStandardError());
        emit finished(false, {}, err);
    }
}

void FfmpegDashPackager::onProcessError(int /*processError*/) {
    const QString err = "Failed to start ffmpeg. Ensure ffmpeg is installed and on PATH.";
    emit finished(false, {}, err);
}
