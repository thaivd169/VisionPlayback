#pragma once
#include <string>
#include <string_view>

// Port: one-shot hex digest. Domain uses this to derive PlaybackKey without
// depending on any specific algorithm or framework.
class IHasher {
public:
    virtual ~IHasher() = default;
    virtual std::string hexDigest(std::string_view input) const = 0;
};
