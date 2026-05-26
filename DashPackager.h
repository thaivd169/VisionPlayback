#pragma once
#include <QObject>
#include <QString>

class QProcess;

class DashPackager : public QObject {
    Q_OBJECT
public:
    explicit DashPackager(QObject* parent = nullptr);
    ~DashPackager() override;

    // Non-blocking. Creates outputDir if needed, then runs ffmpeg.
    void package(const QString& mp4Path, const QString& outputDir);
    void cancel();

signals:
    void finished(bool success, const QString& mpdPath, const QString& errorMsg);

private slots:
    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessError(int processError);

private:
    QProcess* m_process = nullptr;
    QString   m_mpdPath;
};
