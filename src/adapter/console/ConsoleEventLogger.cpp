#include "ConsoleEventLogger.h"

#include <iostream>
#include <string>

#include "LoginUseCase.h"
#include "PlaybackKey.h"
#include "StreamPlaybackUseCase.h"

void ConsoleEventLogger::subscribeTo(StreamPlaybackUseCase* streamUseCase) {
    streamUseCase->onStreamReady([](const PlaybackKey& key, std::string url) {
        std::cout << "[stream-ready] hash=" << key.hex
                  << " url=" << url << std::endl;
    });
    streamUseCase->onStreamError([](const PlaybackKey& key, std::string reason) {
        std::cerr << "[stream-error] hash=" << key.hex
                  << " reason=" << reason << std::endl;
    });
    streamUseCase->onDownloadProgress([](const PlaybackKey& key, int percent) {
        std::cout << "[download] hash=" << key.hex
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
