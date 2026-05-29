#include "FileSystemStreamCache.h"

#include <algorithm>
#include <unordered_set>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>

FileSystemStreamCache::FileSystemStreamCache(QString downloadsDir,
                                             std::uint64_t maxBytes)
    : m_downloadsDir(std::move(downloadsDir)), m_maxBytes(maxBytes) {}

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

void FileSystemStreamCache::evictToCapacity(const std::vector<PlaybackKey>& skipKeys) {
    if (m_maxBytes == 0) return;

    std::unordered_set<std::string> skip;
    skip.reserve(skipKeys.size());
    for (const auto& k : skipKeys) skip.insert(k.hex);

    static const QRegularExpression kHexDir("^[0-9a-f]{16}$");

    struct Entry {
        QString   path;
        std::string hex;
        quint64   sizeBytes  = 0;
        QDateTime lastModified;
    };

    std::vector<Entry> entries;
    const QDir dir(m_downloadsDir);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!kHexDir.match(info.fileName()).hasMatch()) continue;

        Entry e;
        e.path         = info.filePath();
        e.hex          = info.fileName().toStdString();
        e.lastModified = info.lastModified();

        QDirIterator it(e.path, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().isFile())
                e.sizeBytes += static_cast<quint64>(it.fileInfo().size());
        }
        entries.push_back(std::move(e));
    }

    quint64 total = 0;
    for (const auto& e : entries) total += e.sizeBytes;

    if (total <= m_maxBytes) return;

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.lastModified < b.lastModified;
    });

    for (const auto& e : entries) {
        if (total <= m_maxBytes) break;
        if (skip.count(e.hex)) continue;
        if (QDir(e.path).removeRecursively())
            total -= e.sizeBytes;
    }
}
