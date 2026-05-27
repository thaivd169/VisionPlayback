#pragma once
#include <QObject>
#include <QString>

#include "PlaybackKey.h"

class LoginUseCase;
class StreamPlaybackUseCase;

// Passive subscriber that prints use-case events to stdout/stderr. Replaces
// the status labels / message boxes that previously lived in MainWindow.
class ConsoleEventLogger : public QObject {
    Q_OBJECT
public:
    explicit ConsoleEventLogger(QObject* parent = nullptr);

    void subscribeTo(StreamPlaybackUseCase* streamUseCase);
    void subscribeTo(LoginUseCase*          loginUseCase);

private slots:
    void onStreamReady(const PlaybackKey& key, const QString& url);
    void onStreamError(const PlaybackKey& key, const QString& reason);
    void onDownloadProgress(const PlaybackKey& key, int percent);
    void onLoginSucceeded(const QString& ip, int userId);
    void onLoginFailed(const QString& ip, int errorCode);
};
