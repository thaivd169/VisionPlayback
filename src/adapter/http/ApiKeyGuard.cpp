#include "ApiKeyGuard.h"

#include <utility>

ApiKeyGuard::ApiKeyGuard(std::string expectedKey)
    : m_expectedKey(std::move(expectedKey)) {}

bool ApiKeyGuard::validate(const QByteArray& providedKey) const {
    const auto* a = reinterpret_cast<const unsigned char*>(m_expectedKey.data());
    const auto* b = reinterpret_cast<const unsigned char*>(providedKey.constData());
    const std::size_t aLen = m_expectedKey.size();
    const std::size_t bLen = static_cast<std::size_t>(providedKey.size());

    // Compare up to max(aLen, bLen) bytes so the loop runs the same number
    // of iterations regardless of which input is longer. Length mismatch is
    // folded into the accumulator so equal-length matching keys are required.
    const std::size_t n = (aLen > bLen) ? aLen : bLen;
    unsigned diff = static_cast<unsigned>(aLen ^ bLen);
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned ca = (i < aLen) ? a[i] : 0u;
        const unsigned cb = (i < bLen) ? b[i] : 0u;
        diff |= (ca ^ cb);
    }
    return diff == 0;
}
