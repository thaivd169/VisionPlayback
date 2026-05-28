#pragma once
#include <functional>

// Marshalls a callable onto the use-case thread. Concrete implementations
// (e.g. infra/dispatcher/QtDispatcher) ensure the function body runs on the
// thread the dispatcher was bound to — typically the main thread where the
// use cases live. Posting from any thread is safe.
class IDispatcher {
public:
    virtual ~IDispatcher() = default;
    virtual void post(std::function<void()> fn) = 0;
};
