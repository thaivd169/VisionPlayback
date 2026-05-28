#include "QtDispatcher.h"

#include <Qt>

void QtDispatcher::post(std::function<void()> fn) {
    QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
}
