@echo off
setlocal

REM Always run from this script's folder
pushd "%~dp0" >nul 2>&1

echo ========================================
echo Starting Server Application (via PowerShell)
echo Revision: 2025-09-25 R2
echo ========================================

REM Args: %1=http port (default 8081), %2=socket port (default 8080), %3=--skip-build (optional)
set "SERVER_PORT=%~1"
if "%SERVER_PORT%"=="" set "SERVER_PORT=8081"
set "SOCKET_PORT=%~2"
if "%SOCKET_PORT%"=="" set "SOCKET_PORT=8080"
set "PS_SKIP_FLAG="
if /I "%~3"=="--skip-build" set "PS_SKIP_FLAG=-SkipBuild"

echo Using Spring Boot HTTP port: %SERVER_PORT%
echo Using Socket Server   port: %SOCKET_PORT%
if defined PS_SKIP_FLAG echo Will skip mvn package ^(delegated to run.ps1^).

where powershell >nul 2>&1
if errorlevel 1 (
    echo Error: PowerShell not found on PATH. Please run server\run.ps1 manually.
    set EXIT_CODE=1
    goto :end
)

set "PS_PATH=%~dp0run.ps1"
echo Running: powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_PATH%" -HttpPort %SERVER_PORT% -SocketPort %SOCKET_PORT% %PS_SKIP_FLAG%
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_PATH%" -HttpPort %SERVER_PORT% -SocketPort %SOCKET_PORT% %PS_SKIP_FLAG%
set "EXIT_CODE=%ERRORLEVEL%"

:end
popd >nul 2>&1
exit /b %EXIT_CODE%