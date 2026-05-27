#pragma once
#include <string>
#include <string_view>

namespace vp::infra {

// One-shot SHA-256 returning the 64-char lowercase hex digest.
// Used by the usecase layer to derive PlaybackKey hashes.
std::string sha256_hex(std::string_view input);

} // namespace vp::infra
