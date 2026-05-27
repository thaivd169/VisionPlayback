#include "ConsoleEventLogger.h"

#include <iostream>

#include "LoginUseCase.h"
#include "StreamPlaybackUseCase.h"

ConsoleEventLogger::ConsoleEventLogger(QObject* parent) : QObject(parent) {}

void ConsoleEventLogger::subscribeTo(StreamPlaybackUseCase* streamUseCase) {
    connect(streamUseCase, &StreamPlaybackUseCase::streamReady,
            this, &ConsoleEventLogger::onStreamReady);
    connect(streamUseCase, &StreamPlaybackUseCase::streamError,
            this, &ConsoleEventLogger::onStreamError);
    connect(streamUseCase, &StreamPlaybackUseCase::downloadProgress,
            this, &ConsoleEventLogger::onDownloadProgress);
}

void ConsoleEventLogger::subscribeTo(LoginUseCase* loginUseCase) {
    connect(loginUseCase, &LoginUseCase::loginSucceeded,
            this, &ConsoleEventLogger::onLoginSucceeded);
    connect(loginUseCase, &LoginUseCase::loginFailed,
            this, &ConsoleEventLogger::onLoginFailed);
}

void ConsoleEventLogger::onStreamReady(const PlaybackKey& key, const QString& url) {
    std::cout << "[stream-ready] hash=" << key.hex
              << " url=" << url.toStdString() << std::endl;
}

void ConsoleEventLogger::onStreamError(const PlaybackKey& key, const QString& reason) {
    std::cerr << "[stream-error] hash=" << key.hex
              << " reason=" << reason.toStdString() << std::endl;
}

void ConsoleEventLogger::onDownloadProgress(const PlaybackKey& key, int percent) {
    std::cout << "[download] hash=" << key.hex
              << " " << percent << "%" << std::endl;
}

void ConsoleEventLogger::onLoginSucceeded(const QString& ip, int userId) {
    std::cout << "[login-ok] ip=" << ip.toStdString()
              << " userId=" << userId << std::endl;
}

void ConsoleEventLogger::onLoginFailed(const QString& ip, int errorCode) {
    std::cerr << "[login-fail] ip=" << ip.toStdString()
              << " sdkError=" << errorCode << std::endl;
}
