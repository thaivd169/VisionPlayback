@echo off
rem VisionPlayback launcher. Prepends this folder to PATH so the bundled
rem ffmpeg/ffprobe (spawned via QProcess) and every DLL resolve regardless of
rem the current working directory. The console stays attached so the daemon's
rem [login-ok] / [download] / [stream-ready] log lines are visible.
setlocal
set "PATH=%~dp0;%PATH%"
"%~dp0VisionPlayback.exe" %*
