#pragma once
#include <QObject>
#include <QString>
#include <string>

// Per-job ffmpeg packager. One instance handles one package() call from
// invocation to finished signal.
class IDashPackager : public QObject {
    Q_OBJECT
public:
    explicit IDashPackager(QObject* parent = nullptr) : QObject(parent) {}
    ~IDashPackager() override = default;

public slots:
    virtual void package(const std::string& mp4Path,
                         const std::string& outputDir) = 0;
    virtual void cancel() = 0;

signals:
    void finished(bool success, QString mpdPath, QString errorMessage);
};

class IDashPackagerFactory {
public:
    virtual ~IDashPackagerFactory() = default;
    virtual IDashPackager* create(QObject* parent = nullptr) = 0;
};
