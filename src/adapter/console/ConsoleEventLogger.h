#pragma once

class LoginUseCase;
class StreamPlaybackUseCase;

// Passive subscriber that prints use-case events to stdout/stderr.
// Subscribes via the use cases' callback registration setters; no Qt
// signal/slot machinery.
class ConsoleEventLogger {
public:
    void subscribeTo(StreamPlaybackUseCase* streamUseCase);
    void subscribeTo(LoginUseCase*          loginUseCase);
};
