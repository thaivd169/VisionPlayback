#include <QCoreApplication>

#include "Session.h"

#ifndef VISION_PLAYBACK_VERSION_STRING
#define VISION_PLAYBACK_VERSION_STRING "unknown"
#endif

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc > 1 &&
        (std::string(argv[1]) == "--version" ||
         std::string(argv[1]) == "-v")) {
        std::cout << VISION_PLAYBACK_VERSION_STRING << std::endl;
        return 0;
    }

    QCoreApplication app(argc, argv);
    app.setApplicationName("VisionPlayback");

    Session session(argc, argv);
    if (!session.start())
        return 1;

    return app.exec();
}
