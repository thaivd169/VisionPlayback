#pragma once
#include <QString>
#include <string>
#include <string_view>

#include "IStreamCacheRepository.h"

// Hash-keyed on-disk cache: downloads/<hash>.mp4 + downloads/<hash>/manifest.mpd.
class FileSystemStreamCache : public IStreamCacheRepository {
public:
    explicit FileSystemStreamCache(QString downloadsDir);

    bool        dashExists(const PlaybackKey& key) const override;
    bool        mp4Exists(const PlaybackKey& key) const override;
    std::string mp4Path(const PlaybackKey& key) const override;
    std::string dashDir(const PlaybackKey& key) const override;
    std::string mpdUrl(const PlaybackKey& key,
                       std::string_view hostBase) const override;

private:
    QString m_downloadsDir;
};
