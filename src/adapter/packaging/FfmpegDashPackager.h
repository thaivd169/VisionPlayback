#pragma once
#include <QObject>
#include <QString>
#include <string>

#include "IDashPackager.h"

class QProcess;

class FfmpegDashPackager : public IDashPackager {
    Q_OBJECT
public:
    explicit FfmpegDashPackager(QObject* parent = nullptr);
    ~FfmpegDashPackager() override;

public slots:
    void package(const std::string& mp4Path,
                 const std::string& outputDir) override;
    void cancel() override;

private slots:
    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessError(int processError);

private:
    QProcess* m_process = nullptr;
    QString   m_mpdPath;
};

class FfmpegDashPackagerFactory : public IDashPackagerFactory {
public:
    IDashPackager* create(QObject* parent = nullptr) override {
        return new FfmpegDashPackager(parent);
    }
};
