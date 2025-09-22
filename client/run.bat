@echo off
setlocal enabledelayedexpansion

REM Ensure working directory is the script's folder when double-clicked
pushd "%~dp0" >nul 2>&1

echo ========================================
echo Starting Client Application
echo ========================================

REM Prefer running from CMake dist directory (windeployqt output)
set "DIST_EXE=build\client-qt1310\dist\shopping_client.exe"
set "ALT_DIST_EXE=build\client-qtmingw\dist\shopping_client.exe"
set "ALT2_DIST_EXE=build\dist\shopping_client.exe"
set "LEGACY_EXE=build\bin\shopping-client.exe"

set "EXE_PATH="
if exist "%DIST_EXE%" set "EXE_PATH=%DIST_EXE%"
if not defined EXE_PATH if exist "%ALT_DIST_EXE%" set "EXE_PATH=%ALT_DIST_EXE%"
if not defined EXE_PATH if exist "%ALT2_DIST_EXE%" set "EXE_PATH=%ALT2_DIST_EXE%"
if not defined EXE_PATH if exist "%LEGACY_EXE%" set "EXE_PATH=%LEGACY_EXE%"

REM As a fallback, search recursively under build for shopping_client.exe
if not defined EXE_PATH for /f "delims=" %%F in ('dir /b /s "build\shopping_client.exe" 2^>nul') do if not defined EXE_PATH set "EXE_PATH=%%F"
if not defined EXE_PATH (
    echo(
    echo Error: Client executable not found!
    echo Expected at one of:
    echo   %DIST_EXE%
    echo   %ALT_DIST_EXE%
    echo   %ALT2_DIST_EXE%
    echo   %LEGACY_EXE%
    echo Or anywhere under build\ as shopping_client.exe
    echo Please build the client first (CMake target 'package_app').
    @echo off
    setlocal

    REM Ensure working directory is the script's folder when double-clicked
    pushd "%~dp0" >nul 2>&1

    echo ========================================
    echo Starting Client Application
    echo ========================================

    set "EXE_PATH=build\dist\shopping_client.exe"
    if not exist "%EXE_PATH%" (
        echo Error: %EXE_PATH% not found.
        echo Please build the client first (CMake target package_app).
        goto :end
    )

    echo Using executable: %EXE_PATH%
    echo Launching shopping client...
    start "shopping-client" "%EXE_PATH%"

    :end
    popd >nul 2>&1
    pause
    exit /b 0
