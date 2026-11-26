@echo off
setlocal EnableDelayedExpansion
title DOOM HELLFORGE - Update embedded gib assets
pushd "%~dp0"

echo Regenerating and validating the embedded giblet atlas...
where python >nul 2>nul
if not errorlevel 1 (
    python tools\pack_giblets.py
    set "asset_result=!ERRORLEVEL!"
) else (
    where py >nul 2>nul
    if errorlevel 1 (
        echo ERROR: Python 3 was not found. Install Python or add it to PATH.
        set "asset_result=1"
    ) else (
        py -3 tools\pack_giblets.py
        set "asset_result=!ERRORLEVEL!"
    )
)

if not "%asset_result%"=="0" goto :failed

echo.
echo Checking generated atlas against source PNGs...
where python >nul 2>nul
if not errorlevel 1 (
    python tools\pack_giblets.py --check
) else (
    py -3 tools\pack_giblets.py --check
)
if errorlevel 1 goto :failed

echo.
echo Building the updated game and launcher...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Build-Hellforge.ps1" -ForceRebuild
set "build_result=%ERRORLEVEL%"
if not "%build_result%"=="0" goto :failed

echo.
echo SUCCESS: embedded gib assets, engine, and launcher are updated.
popd
pause
exit /b 0

:failed
echo.
echo Update failed. Fix the reported issue before using the build.
popd
pause
exit /b 1
