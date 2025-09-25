@echo off
setlocal enabledelayedexpansion

REM Ensure working directory is the script's folder
pushd "%~dp0" >nul 2>&1

echo ========================================
echo StartAll: Build ^& Run Server and Client
echo Revision: 2025-09-25 R5
echo Script path: %~f0
echo ========================================

REM Usage: StartAll.bat [httpPort] [socketPort] [--skip-build]
REM   httpPort    : Spring Boot server.port (default 8081 if omitted)
REM   socketPort  : Netty socket.port (default 8080 if omitted)
REM   --skip-build: Skip mvn package (faster dev restart)

set "SERVER_PORT=%~1"
if "%SERVER_PORT%"=="" set "SERVER_PORT=8081"
set "SOCKET_PORT=%~2"
if "%SOCKET_PORT%"=="" set "SOCKET_PORT=8080"

set "SKIP_BUILD="
for %%A in (%*) do (
    if /I "%%~A"=="--skip-build" set "SKIP_BUILD=1"
)

echo Using Spring Boot HTTP port: %SERVER_PORT%
echo Using Netty Socket  port: %SOCKET_PORT%
if defined SKIP_BUILD echo Will skip server mvn build.

REM 1) Start server in a new window (delegate to PowerShell script that is known-good)
echo [1/4] Starting server window (run.ps1)...
if defined SKIP_BUILD (
    start "shopping-server" powershell -NoExit -NoProfile -ExecutionPolicy Bypass -Command "& { Set-Location -Path 'server'; .\run.ps1 -HttpPort %SERVER_PORT% -SocketPort %SOCKET_PORT% -SkipBuild }"
) else (
    start "shopping-server" powershell -NoExit -NoProfile -ExecutionPolicy Bypass -Command "& { Set-Location -Path 'server'; .\run.ps1 -HttpPort %SERVER_PORT% -SocketPort %SOCKET_PORT% }"
)

REM 2) Wait for server to be ready (Netty 8080 OR Spring Boot %SERVER_PORT%) max ~60s
echo [2/4] Waiting for server port (Socket %SOCKET_PORT%)...
set /a __retries=90
:wait_loop
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $c=New-Object System.Net.Sockets.TcpClient; try{ $c.Connect('localhost',%SOCKET_PORT%) } catch {}; if($c.Connected){ $c.Close(); exit 0 } else { exit 1 } } catch { exit 1 }"
if %ERRORLEVEL% EQU 0 goto :server_ready
REM NOTE: We no longer abort if the server window title isn't detected, to ensure client still launches.
REM You can still check the server window manually titled "shopping-server".
set /a __retries-=1
if %__retries% LEQ 0 goto :server_timeout
>nul timeout /t 1 /nobreak
goto :wait_loop

:server_timeout
echo Warning: Waited 90s but not all server ports became ready. Continuing to start client anyway.

:server_ready
REM 3) Stop running client if any to avoid file locks during packaging
echo [3/4] Stopping existing client instances (if any)...
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Get-Process -Name shopping_client -ErrorAction Stop | Stop-Process -Force } catch { }"

REM 4) Build latest client before launch
echo [4/4] Building latest client...
if exist "build_client_qt1310.bat" (
    call build_client_qt1310.bat
) else (
    echo build_client_qt1310.bat not found, skipping client build.
)

REM 5) Start client in a new window (prefer freshly built non-dist first)
echo [5/5] Starting client window...
set "CLIENT_EXE=shopping_client.exe"
set "CLIENT_DIR="

REM Highest priority: user-specified dist location
if exist "build-qt1310\dist\shopping_client.exe" (
    set "CLIENT_DIR=build-qt1310\dist"
    goto :launch_client
)

REM Prefer freshly built exe in build directories (non-dist) first
if exist "build-qt1310\shopping_client.exe" (
    set "CLIENT_DIR=build-qt1310"
    goto :launch_client
)
if exist "client\build-qt1310\shopping_client.exe" (
    set "CLIENT_DIR=client\build-qt1310"
    goto :launch_client
)

REM Fallback to packaged dist locations
if exist "build-qt1310\dist\shopping_client.exe" (
    set "CLIENT_DIR=build-qt1310\dist"
    goto :launch_client
)
if exist "client\build-qt1310\dist\shopping_client.exe" (
    set "CLIENT_DIR=client\build-qt1310\dist"
    goto :launch_client
)
if exist "build\dist\shopping_client.exe" (
    set "CLIENT_DIR=build\dist"
    goto :launch_client
)
if exist "client\build\dist\shopping_client.exe" (
    set "CLIENT_DIR=client\build\dist"
    goto :launch_client
)

echo Client executable not found. Searched candidates:
echo   build-qt1310\shopping_client.exe
echo   client\build-qt1310\shopping_client.exe
echo   build-qt1310\dist\shopping_client.exe
echo   client\build-qt1310\dist\shopping_client.exe
echo Please build the client (package_app) and try again.
goto :done

:launch_client
for %%I in ("%CLIENT_DIR%\%CLIENT_EXE%") do set "CLIENT_ABS=%%~fI"
echo Launching client: "%CLIENT_ABS%"
REM If launching from a non-dist folder, ensure Qt DLLs are present; try windeployqt
set "__NEED_DEPLOY=0"
if /I not "%CLIENT_DIR:~-5%"=="\dist" (
    if not exist "%CLIENT_DIR%\Qt6Core.dll" set "__NEED_DEPLOY=1"
)

if "%__NEED_DEPLOY%"=="1" (
    echo Qt6 DLLs not found in "%CLIENT_DIR%". Attempting windeployqt...
    set "WDEPLOYQT="
    if defined QTDIR if exist "%QTDIR%\bin\windeployqt.exe" set "WDEPLOYQT=%QTDIR%\bin\windeployqt.exe"
    if not defined WDEPLOYQT for /f "delims=" %%W in ('where /r "D:\Qt" windeployqt.exe 2^>nul') do if not defined WDEPLOYQT set "WDEPLOYQT=%%W"
    if not defined WDEPLOYQT for /f "delims=" %%W in ('where /r "C:\Qt" windeployqt.exe 2^>nul') do if not defined WDEPLOYQT set "WDEPLOYQT=%%W"

    if defined WDEPLOYQT (
        echo Using windeployqt: !WDEPLOYQT!
        pushd "%CLIENT_DIR%" >nul 2>&1
        "!WDEPLOYQT!" --release "%CLIENT_ABS%"
        popd >nul 2>&1
        REM Try to copy MinGW runtime DLLs if toolchain folder exists
        set "MINGW_BIN="
        if exist "D:\Qt\Tools\mingw1310_64\bin" set "MINGW_BIN=D:\Qt\Tools\mingw1310_64\bin"
        if not defined MINGW_BIN if exist "C:\Qt\Tools\mingw1310_64\bin" set "MINGW_BIN=C:\Qt\Tools\mingw1310_64\bin"
        if not defined MINGW_BIN for /f "delims=" %%B in ('dir /b /s /ad "D:\Qt\Tools\*\bin" 2^>nul ^| findstr /i /r "mingw.*_64\\bin$"') do set "MINGW_BIN=%%B"
        if defined MINGW_BIN (
            for %%D in (libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll) do (
                if exist "!MINGW_BIN!\%%D" copy /Y "!MINGW_BIN!\%%D" "%CLIENT_DIR%\" >nul 2>&1
            )
        )
    ) else (
        echo Could not find windeployqt.exe automatically.
    )

    if not exist "%CLIENT_DIR%\Qt6Core.dll" (
        echo Deploy failed or incomplete. Falling back to packaged dist client if available...
        REM Try known dist locations as fallback
        if exist "build-qt1310\dist\shopping_client.exe" (
            set "CLIENT_DIR=build-qt1310\dist"
            goto :launch_client
        )
        if exist "client\build-qt1310\dist\shopping_client.exe" (
            set "CLIENT_DIR=client\build-qt1310\dist"
            goto :launch_client
        )
        if exist "build\dist\shopping_client.exe" (
            set "CLIENT_DIR=build\dist"
            goto :launch_client
        )
        if exist "client\build\dist\shopping_client.exe" (
            set "CLIENT_DIR=client\build\dist"
            goto :launch_client
        )
        echo No dist client found. You may need to run windeployqt manually or set QTDIR.
    )
)
REM Launch client by absolute path with a clear window title and working directory
start "shopping-client" /D "%CLIENT_DIR%" "%CLIENT_ABS%"
goto :done

:done

echo Done. Two windows should be open now.
popd >nul 2>&1
exit /b 0
