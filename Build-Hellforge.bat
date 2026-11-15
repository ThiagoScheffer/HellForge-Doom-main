@echo off
setlocal
title DOOM HELLFORGE - Build engine and launcher
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Build-Hellforge.ps1" %*
set "build_result=%ERRORLEVEL%"
echo.
if not "%build_result%"=="0" echo Build failed. See the message and log above.
pause
exit /b %build_result%
