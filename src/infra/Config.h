#pragma once
#include <cstdint>
#include <string_view>

namespace vp::infra {

// Hard-coded development defaults used when no CLI override is supplied.
//
// TODO: replace with proper secret management (env, vault, rotated keys,
// HMAC-signed requests) before any non-development deployment.
inline constexpr std::string_view kDefaultApiKey = "visionplayback-dev-key";

// Default HTTP listen port.
inline constexpr std::uint16_t kDefaultPort = 18068;

// Idle timeout for the LoginUseCase camera-session cache.
inline constexpr int kDefaultLoginIdleSeconds = 600;

// Maximum total size of the downloads directory before LRU eviction kicks in.
inline constexpr std::uint64_t kDefaultMaxDownloadsSizeBytes =
    2ULL * 1024 * 1024 * 1024;  // 2 GB

// Hash algorithm used to derive PlaybackKey. Supported values: blake2s-128, sha256, sha512.
inline constexpr std::string_view kDefaultHashAlgorithm = "blake2s-128";

}  // namespace vp::infra
