#include "ConsoleEventLogger.h"

#include <iostream>

#include "LoginUseCase.h"
#include "PlaybackProcessor.h"

void ConsoleEventLogger::subscribeTo(PlaybackProcessor* processor) {
    QObject::connect(processor, &PlaybackProcessor::streamReady,
                     [](QString keyHex, QString url) {
                         std::cout << "[stream-ready] hash=" << keyHex.toStdString()
                                   << " url=" << url.toStdString() << std::endl;
                     });
    QObject::connect(processor, &PlaybackProcessor::streamError,
                     [](QString keyHex, QString reason) {
                         std::cerr << "[stream-error] hash=" << keyHex.toStdString()
                                   << " reason=" << reason.toStdString() << std::endl;
                     });
    QObject::connect(processor, &PlaybackProcessor::streamProgress,
                     [](QString keyHex, int percent) {
                         std::cout << "[download] hash=" << keyHex.toStdString()
                                   << " " << percent << "%" << std::endl;
                     });
}

void ConsoleEventLogger::subscribeTo(LoginUseCase* loginUseCase) {
    loginUseCase->onLoginSucceeded([](std::string ip, int userId) {
        std::cout << "[login-ok] ip=" << ip
                  << " userId=" << userId << std::endl;
    });
    loginUseCase->onLoginFailed([](std::string ip, int errorCode) {
        std::cerr << "[login-fail] ip=" << ip
                  << " sdkError=" << errorCode << std::endl;
    });
}
