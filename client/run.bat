@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Starting Client Application
echo ========================================

:: Prefer running from CMake dist directory (windeployqt output)
set DIST_EXE=build\client-qt1310\dist\shopping_client.exe
set ALT_DIST_EXE=build\client-qtmingw\dist\shopping_client.exe
set ALT2_DIST_EXE=build\dist\shopping_client.exe
set LEGACY_EXE=build\bin\shopping-client.exe

set EXE_PATH=
if exist "%DIST_EXE%" set EXE_PATH=%DIST_EXE%
if not defined EXE_PATH if exist "%ALT_DIST_EXE%" set EXE_PATH=%ALT_DIST_EXE%
if not defined EXE_PATH if exist "%ALT2_DIST_EXE%" set EXE_PATH=%ALT2_DIST_EXE%
if not defined EXE_PATH if exist "%LEGACY_EXE%" set EXE_PATH=%LEGACY_EXE%

if not defined EXE_PATH (
    echo( Error: Client executable not found!
    echo( Expected at one of:
    echo(   %DIST_EXE%
    echo(   %ALT_DIST_EXE%
    echo(   %ALT2_DIST_EXE%
    echo(   %LEGACY_EXE%
    echo( Please build the client first ^(CMake target 'package_app'^).
    pause
    exit /b 1
)

echo Using executable: %EXE_PATH%
echo Starting shopping client...
"%EXE_PATH%"

echo(
echo Client exited with code %ERRORLEVEL%.
pause
exit /b %ERRORLEVEL%
