#pragma once
#include <functional>
#include <memory>
#include <string>

// Per-job DASH packager port. One instance handles one package() call from
// invocation to finished callback.
class IDashPackager {
public:
    using FinishedCb = std::function<void(bool success,
                                          std::string mpdPath,
                                          std::string errorMessage)>;

    virtual ~IDashPackager() = default;

    virtual void setOnFinished(FinishedCb cb) = 0;

    virtual void package(const std::string& mp4Path,
                         const std::string& outputDir) = 0;
    virtual void cancel() = 0;
};

class IDashPackagerFactory {
public:
    virtual ~IDashPackagerFactory() = default;
    virtual std::unique_ptr<IDashPackager> create() = 0;
};
