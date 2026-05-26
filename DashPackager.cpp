#include "DashPackager.h"

#include <QDir>
#include <QProcess>

DashPackager::DashPackager(QObject* parent) : QObject(parent) {}

DashPackager::~DashPackager() {
    cancel();
}

void DashPackager::package(const QString& mp4Path, const QString& outputDir) {
    QDir().mkpath(outputDir);
    m_mpdPath = outputDir + "/manifest.mpd";

    m_process = new QProcess(this);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &DashPackager::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &DashPackager::onProcessError);

    const QStringList args = {
        "-y",
        "-i", mp4Path,
        "-c", "copy",
        "-f", "dash",
        "-seg_duration", "4",
        "-init_seg_name",  "init-stream$RepresentationID$.m4s",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%05d$.m4s",
        m_mpdPath
    };

    m_process->start("ffmpeg", args);
}

void DashPackager::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void DashPackager::onProcessFinished(int exitCode, int /*exitStatus*/) {
    if (exitCode == 0) {
        emit finished(true, m_mpdPath, {});
    } else {
        QString err = m_process->readAllStandardError();
        emit finished(false, {}, err);
    }
}

void DashPackager::onProcessError(int /*processError*/) {
    QString err = "Failed to start ffmpeg. Ensure ffmpeg is installed and on PATH.";
    emit finished(false, {}, err);
}
