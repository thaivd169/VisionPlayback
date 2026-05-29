#pragma once

class LoginUseCase;
class PlaybackProcessor;

// Passive subscriber that prints pipeline events to stdout/stderr.
class ConsoleEventLogger {
public:
    void subscribeTo(PlaybackProcessor* processor);
    void subscribeTo(LoginUseCase*      loginUseCase);
};
