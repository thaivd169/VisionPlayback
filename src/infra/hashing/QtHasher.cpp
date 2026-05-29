#include "QtHasher.h"

#include <QByteArray>
#include <cstdio>
#include <cstdlib>

QtHasher::QtHasher(QCryptographicHash::Algorithm algo) : m_algo(algo) {}

std::unique_ptr<IHasher> QtHasher::fromName(std::string_view name) {
    if (name == "blake2s-128") return std::make_unique<QtHasher>(QCryptographicHash::Blake2s_128);
    if (name == "sha256")      return std::make_unique<QtHasher>(QCryptographicHash::Sha256);
    if (name == "sha512")      return std::make_unique<QtHasher>(QCryptographicHash::Sha512);
    std::fprintf(stderr,
                 "[fatal] unknown hash algorithm '%.*s' — supported: blake2s-128, sha256, sha512\n",
                 static_cast<int>(name.size()), name.data());
    std::exit(1);
}

std::string QtHasher::hexDigest(std::string_view input) const {
    const QByteArray data = QByteArray::fromRawData(input.data(),
                                                    static_cast<qsizetype>(input.size()));
    return QCryptographicHash::hash(data, m_algo).toHex().toStdString();
}
