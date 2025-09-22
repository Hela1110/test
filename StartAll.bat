@echo off
setlocal enabledelayedexpansion

REM Ensure working directory is the script's folder
pushd "%~dp0" >nul 2>&1

echo ========================================
echo StartAll: Build & Run Server and Client
echo ========================================

set "SERVER_PORT=%~1"
if "%SERVER_PORT%"=="" set "SERVER_PORT=8081"
echo Using Spring Boot HTTP port: %SERVER_PORT%

REM 1) Start server in a new window
echo [1/3] Starting server window...
start "shopping-server" cmd /c "cd /d server && run.bat %SERVER_PORT%"

REM 2) Wait for Netty port 8080 to be ready (max ~60s)
echo [2/3] Waiting for server (localhost:8080)...
set /a __retries=60
:wait_loop
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $c = New-Object System.Net.Sockets.TcpClient; $c.Connect('localhost',8080); if($c.Connected){$c.Close(); exit 0} else {exit 1} } catch { exit 1 }"
if %ERRORLEVEL% EQU 0 goto :server_ready
set /a __retries-=1
if %__retries% LEQ 0 goto :server_timeout
>nul timeout /t 1 /nobreak
goto :wait_loop

:server_timeout
echo Warning: Waited 60s but 8080 not ready. Continuing to start client anyway.

:server_ready
REM 3) Start client in a new window
echo [3/3] Starting client window...
if exist "client\build-qt1310\dist\shopping_client.exe" (
	start "shopping-client" "client\build-qt1310\dist\shopping_client.exe"
	goto :done
)
if exist "client\build\dist\shopping_client.exe" (
	start "shopping-client" "client\build\dist\shopping_client.exe"
	goto :done
)
echo Client executable not found at:
echo   client\build-qt1310\dist\shopping_client.exe
echo   client\build\dist\shopping_client.exe
echo Please build the client (package_app) and try again.

:done

echo Done. Two windows should be open now.
popd >nul 2>&1
exit /b 0
