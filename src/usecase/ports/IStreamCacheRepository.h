#pragma once
#include <string>
#include <string_view>

#include "PlaybackKey.h"

// Synchronous on-disk cache lookup. Implemented in
// adapter/persistence/FileSystemStreamCache.
class IStreamCacheRepository {
public:
    virtual ~IStreamCacheRepository() = default;

    // True iff manifest.mpd exists and looks well-formed for `key`.
    virtual bool dashExists(const PlaybackKey& key) const = 0;
    // True iff the intermediate raw mp4 download exists for `key`.
    virtual bool mp4Exists(const PlaybackKey& key) const = 0;

    // On-disk path where the raw mp4 is (or will be) written.
    virtual std::string mp4Path(const PlaybackKey& key) const = 0;
    // Directory where the DASH manifest + segments are (or will be) written.
    virtual std::string dashDir(const PlaybackKey& key) const = 0;
    // Public URL where manifest.mpd is served (hostBase like "http://localhost:8080").
    virtual std::string mpdUrl(const PlaybackKey& key,
                               std::string_view hostBase) const = 0;
};
