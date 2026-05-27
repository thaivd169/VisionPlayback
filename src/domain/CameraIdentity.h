#pragma once
#include <cstdint>
#include <functional>
#include <string>

// (ip, port, user) tuple used as the login-cache key in LoginUseCase.
// Password is excluded — a changed password invalidates on next use via SDK
// failure, which is more robust than wedging the cache when the user rotates.
struct CameraIdentity {
    std::string   ip;
    std::uint16_t port = 0;
    std::string   user;

    bool operator==(const CameraIdentity& o) const noexcept {
        return ip == o.ip && port == o.port && user == o.user;
    }
};

namespace std {
template <>
struct hash<CameraIdentity> {
    std::size_t operator()(const CameraIdentity& c) const noexcept {
        const std::size_t h1 = std::hash<std::string>{}(c.ip);
        const std::size_t h2 = std::hash<std::uint16_t>{}(c.port);
        const std::size_t h3 = std::hash<std::string>{}(c.user);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
} // namespace std
