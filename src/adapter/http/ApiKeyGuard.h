#pragma once
#include <QByteArray>
#include <string>

// Constant-time X-API-Key validator. Constructed once at startup with the
// resolved key (CLI flag or infra::kDefaultApiKey fallback).
class ApiKeyGuard {
public:
    explicit ApiKeyGuard(std::string expectedKey);

    bool validate(const QByteArray& providedKey) const;

private:
    std::string m_expectedKey;
};
