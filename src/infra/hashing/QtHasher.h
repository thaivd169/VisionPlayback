#pragma once
#include "IHasher.h"
#include <QCryptographicHash>
#include <memory>

// Qt-backed IHasher implementation. Algorithm is chosen at startup via CLI
// (the name flows through HttpListenerConfig and HttpListener builds the hasher
// via fromName). Default: Blake2s_128.
class QtHasher : public IHasher {
public:
    explicit QtHasher(QCryptographicHash::Algorithm algo);

    // Parses "blake2s-128", "sha256", "sha512".
    // Prints an error and calls std::exit(1) on an unknown name.
    static std::unique_ptr<IHasher> fromName(std::string_view name);

    std::string hexDigest(std::string_view input) const override;

private:
    QCryptographicHash::Algorithm m_algo;
};
