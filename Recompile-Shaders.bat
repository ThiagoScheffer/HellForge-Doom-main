@echo off
setlocal
title DOOM HELLFORGE - Recompile shaders
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Recompile-Shaders.ps1" %*
set "shader_result=%ERRORLEVEL%"
echo.
if not "%shader_result%"=="0" echo Shader compilation failed. See the message and log above.
pause
exit /b %shader_result%
