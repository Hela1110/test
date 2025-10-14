@echo off
setlocal enabledelayedexpansion

REM Ensure working directory is the script's folder
pushd "%~dp0" >nul 2>&1

echo ========================================
echo StartAll: Build ^& Run Server and Client
echo Revision: 2025-09-25 R6
echo Script path: %~f0
echo ========================================

REM Usage: StartAll.bat [httpPort] [socketPort] [--skip-build] [--no-kill]
REM   httpPort    : Spring Boot server.port (default 8081 if omitted)
REM   socketPort  : Netty socket.port (default 8080 if omitted)
REM   --skip-build: Skip mvn package (faster dev restart)
REM   --no-kill   : Do not stop existing client before launching new one

set "SERVER_PORT=%~1"
if "%SERVER_PORT%"=="" set "SERVER_PORT=8081"
set "SOCKET_PORT=%~2"
if "%SOCKET_PORT%"=="" set "SOCKET_PORT=8080"

REM If flags were passed in place of ports, restore defaults
if "%SERVER_PORT:~0,1%"=="-" set "SERVER_PORT=8081"
if "%SERVER_PORT:~0,1%"=="/" set "SERVER_PORT=8081"
if "%SOCKET_PORT:~0,1%"=="-" set "SOCKET_PORT=8080"
if "%SOCKET_PORT:~0,1%"=="/" set "SOCKET_PORT=8080"

set "SKIP_BUILD="
set "NO_KILL="
set "LAUNCHED_CLIENT=0"
for %%A in (%*) do (
    if /I "%%~A"=="--skip-build" set "SKIP_BUILD=1"
    if /I "%%~A"=="--no-kill" set "NO_KILL=1"
)

echo Using Spring Boot HTTP port: %SERVER_PORT%
echo Using Netty Socket  port: %SOCKET_PORT%
if defined SKIP_BUILD echo Will skip server mvn build.

REM 1) Start server in a new window (use run.bat to avoid PS policy issues)
echo [1/4] Starting server window (run.bat)...
set "RB_ARGS=%SERVER_PORT% %SOCKET_PORT%"
if defined SKIP_BUILD set "RB_ARGS=%RB_ARGS% --skip-build"
start "shopping-server" cmd /k "server\run.bat %RB_ARGS%"

REM 2) Give the server a moment (avoid complex parsing)
echo [2/4] Giving server a moment to start (Socket %SOCKET_PORT%)...
timeout /t 5 /nobreak >nul

REM 3) Stop running client if any to avoid file locks during packaging
REM    Note: This can cause a brief "Connection reset" on the server for the old socket.
if defined NO_KILL goto :step4
echo [3/4] Stopping existing client instances (if any)...
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { Get-Process -Name shopping_client -ErrorAction Stop | ForEach-Object { if($_.MainWindowHandle -ne 0){ $_.CloseMainWindow() | Out-Null }; try{ Wait-Process -Id $_.Id -Timeout 3 } catch {}; if(-not $_.HasExited){ Stop-Process -Id $_.Id -Force } } } catch {}"

:step4

REM 4) Build latest client before launch
echo [4/4] Building latest client...
if exist "build_client_qt1310.bat" (
    call build_client_qt1310.bat
    if errorlevel 1 (
        echo Build reported an error. Leaving this window open for review.
        goto :done_pause
    )
) else (
    echo build_client_qt1310.bat not found, skipping client build.
)

REM 5) Start client in a new window (prefer top-level freshly packaged dist first)
echo [5/5] Starting client window...
set "CLIENT_EXE=shopping_client.exe"
set "CLIENT_DIR="

REM Prefer top-level dist (built by build_client_qt1310.bat) first
if exist "build-qt1310\dist\shopping_client.exe" (
    set "CLIENT_DIR=build-qt1310\dist"
    goto :launch_client
)
if exist "client\build-qt1310\dist\shopping_client.exe" (
    set "CLIENT_DIR=client\build-qt1310\dist"
    goto :launch_client
)
if exist "build-qt1310\shopping_client.exe" (
    set "CLIENT_DIR=build-qt1310"
    goto :launch_client
)
if exist "client\build-qt1310\shopping_client.exe" (
    set "CLIENT_DIR=client\build-qt1310"
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
REM Fallback: try helper scripts if present
if exist "client\run_dist.bat" (
    echo Trying fallback: client\run_dist.bat
    call client\run_dist.bat
    for /f %%S in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "if(Get-Process -Name shopping_client -ErrorAction SilentlyContinue){'1'}else{'0'}"') do set "LAUNCHED_CLIENT=%%S"
    goto :done
)
if exist "client\run.bat" (
    echo Trying fallback: client\run.bat
    call client\run.bat
    for /f %%S in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "if(Get-Process -Name shopping_client -ErrorAction SilentlyContinue){'1'}else{'0'}"') do set "LAUNCHED_CLIENT=%%S"
    goto :done
)
goto :done

:launch_client
for %%I in ("%CLIENT_DIR%\%CLIENT_EXE%") do set "CLIENT_ABS=%%~fI"
echo Launching client: "%CLIENT_ABS%"
REM Pass ports via environment variables so client can adapt
set "APP_SOCKET_PORT=%SOCKET_PORT%"
set "APP_HTTP_PORT=%SERVER_PORT%"
set "APP_HOST=127.0.0.1"

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
        if exist "client\build-qt1310\dist\shopping_client.exe" (
            set "CLIENT_DIR=client\build-qt1310\dist"
            goto :launch_client
        )
        if exist "build-qt1310\dist\shopping_client.exe" (
            set "CLIENT_DIR=build-qt1310\dist"
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
set "LAUNCHED_CLIENT=1"
goto :done

:done
if "%LAUNCHED_CLIENT%"=="0" goto :done_pause
echo Done. Two windows should be open now.
goto :epilog

:done_pause
echo Note: Client was not launched successfully. Keeping this window open.
echo You can check the logs above and press any key to close.
pause
goto :epilog

:epilog
popd >nul 2>&1
exit /b 0
