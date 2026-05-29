#pragma once
#include <QString>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "IStreamCacheRepository.h"

// Hash-keyed on-disk cache: downloads/<hash>.mp4 + downloads/<hash>/manifest.mpd.
// `maxBytes` sets the total capacity of the downloads directory; 0 means unlimited.
class FileSystemStreamCache : public IStreamCacheRepository {
public:
    explicit FileSystemStreamCache(QString downloadsDir,
                                   std::uint64_t maxBytes = 0);

    bool        dashExists(const PlaybackKey& key) const override;
    bool        mp4Exists(const PlaybackKey& key) const override;
    std::string mp4Path(const PlaybackKey& key) const override;
    std::string dashDir(const PlaybackKey& key) const override;
    std::string mpdUrl(const PlaybackKey& key,
                       std::string_view hostBase) const override;
    void        deleteMp4(const PlaybackKey& key) override;
    void        evictToCapacity(const std::vector<PlaybackKey>& skipKeys) override;

private:
    QString       m_downloadsDir;
    std::uint64_t m_maxBytes;
};
