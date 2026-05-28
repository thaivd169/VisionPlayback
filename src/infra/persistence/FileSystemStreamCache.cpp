#include "FileSystemStreamCache.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>

FileSystemStreamCache::FileSystemStreamCache(QString downloadsDir)
    : m_downloadsDir(std::move(downloadsDir)) {}

bool FileSystemStreamCache::dashExists(const PlaybackKey& key) const {
    QFile f(QString::fromStdString(dashDir(key)) + "/manifest.mpd");
    if (!f.exists() || f.size() < 100)
        return false;
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = f.read(10);
    return header.startsWith("<?xml") || header.startsWith("<MPD");
}

bool FileSystemStreamCache::mp4Exists(const PlaybackKey& key) const {
    return QFile::exists(QString::fromStdString(mp4Path(key)));
}

std::string FileSystemStreamCache::mp4Path(const PlaybackKey& key) const {
    return (m_downloadsDir + "/" + QString::fromStdString(key.hex) + ".mp4").toStdString();
}

std::string FileSystemStreamCache::dashDir(const PlaybackKey& key) const {
    return (m_downloadsDir + "/" + QString::fromStdString(key.hex)).toStdString();
}

void FileSystemStreamCache::deleteMp4(const PlaybackKey& key) {
    QFile::remove(QString::fromStdString(mp4Path(key)));
}

std::string FileSystemStreamCache::mpdUrl(const PlaybackKey& key,
                                          std::string_view hostBase) const {
    std::string out;
    out.append(hostBase);
    out.append("/dash/");
    out.append(key.hex);
    out.append("/manifest.mpd");
    return out;
}
