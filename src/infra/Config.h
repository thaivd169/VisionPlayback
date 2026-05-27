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
inline constexpr std::uint16_t kDefaultPort = 8080;

// Idle timeout for the LoginUseCase camera-session cache.
inline constexpr int kDefaultLoginIdleSeconds = 600;

} // namespace vp::infra
