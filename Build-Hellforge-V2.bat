@echo off
setlocal
title DOOM HELLFORGE - V2 forced full rebuild
echo HELLFORGE V2: clean rebuild of all native code, then rebuild the launcher.
echo Use this after restoring or copying source files with older timestamps.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Build-Hellforge.ps1" -ForceRebuild %*
set "build_result=%ERRORLEVEL%"
echo.
if not "%build_result%"=="0" echo Full rebuild failed. See the message and log above.
pause
exit /b %build_result%
