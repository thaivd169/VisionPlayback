#include <QCoreApplication>

#include "Session.h"

#ifndef VISION_PLAYBACK_VERSION_STRING
#define VISION_PLAYBACK_VERSION_STRING "unknown"
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("VisionPlayback");
    app.setApplicationVersion(QStringLiteral(VISION_PLAYBACK_VERSION_STRING));

    Session session(argc, argv);
    if (!session.start())
        return 1;

    return app.exec();
}
