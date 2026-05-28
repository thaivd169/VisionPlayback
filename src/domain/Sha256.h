#pragma once
#include <string>
#include <string_view>

// One-shot SHA-256 returning the 64-char lowercase hex digest.
// Pure C++; lives in the domain layer so PlaybackKey derivation stays a
// domain concern.
std::string sha256_hex(std::string_view input);
