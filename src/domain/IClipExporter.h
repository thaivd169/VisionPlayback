#pragma once
#include <functional>
#include <memory>
#include <string>

// Per-request clip exporter port. One instance handles one exportClip() call
// from invocation to finished callback. Remuxes an already-packaged DASH
// manifest into a single MP4, optionally trimmed to a time window.
class IClipExporter {
public:
    using FinishedCb = std::function<void(bool success,
                                          std::string outputPath,
                                          std::string errorMessage)>;

    virtual ~IClipExporter() = default;

    virtual void setOnFinished(FinishedCb cb) = 0;

    // Offsets are seconds from the start of the recording. startSec <= 0 means
    // "from the beginning"; endSec <= 0 means "to the end".
    virtual void exportClip(const std::string& mpdPath,
                            const std::string& outputPath,
                            int startSec,
                            int endSec) = 0;
    virtual void cancel() = 0;
};

class IClipExporterFactory {
public:
    virtual ~IClipExporterFactory() = default;
    virtual std::unique_ptr<IClipExporter> create() = 0;
};
