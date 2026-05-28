#pragma once
#include <QObject>
#include <functional>

#include "IDispatcher.h"

// IDispatcher impl backed by Qt's event loop. Construct on the thread you
// want callbacks delivered on (typically the main thread). post() can be
// called from any thread; the function is queued via QMetaObject::invokeMethod
// with Qt::QueuedConnection and runs on the target thread's event loop.
class QtDispatcher : public QObject, public IDispatcher {
    Q_OBJECT
public:
    explicit QtDispatcher(QObject* parent = nullptr) : QObject(parent) {}
    void post(std::function<void()> fn) override;
};
