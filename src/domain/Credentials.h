#pragma once
#include <cstdint>
#include <string>

struct Credentials {
    std::string   ip;
    std::uint16_t port = 8000;
    std::string   user;
    std::string   pass;
};
