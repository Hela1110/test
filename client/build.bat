@echo off
echo ========================================
echo Building Client Application
echo ========================================

:: Check if build directory exists, if not create it
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

:: Navigate to build directory
cd build

:: Run CMake
echo Running CMake...
cmake -G "MinGW Makefiles" ..
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

:: Build the project
echo Building project...
mingw32-make
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo ========================================
echo Build completed successfully!
echo The executable is located in build\
echo ========================================

:: Copy required DLLs with windeployqt (auto-detect)
echo Copying Qt DLLs...
setlocal EnableDelayedExpansion
set "EXE_PATH=shopping_client.exe"
if not exist "!EXE_PATH!" (
    echo Error: Executable not found: !EXE_PATH!
    goto :deploy_end
)
set "DEPLOY_DIR=deploy"
if not exist "!DEPLOY_DIR!" mkdir "!DEPLOY_DIR!"

:: 1) Try windeployqt from PATH
set "WINDEPLOYQT_CMD=windeployqt"
where windeployqt >nul 2>&1
if errorlevel 1 (
    :: 2) Try from QTDIR
    if defined QTDIR (
        if exist "%QTDIR%\bin\windeployqt.exe" set "WINDEPLOYQT_CMD=%QTDIR%\bin\windeployqt.exe"
    )
)

:: 3) Try locating via qmake -query
if /I "%WINDEPLOYQT_CMD%"=="windeployqt" (
    where qmake >nul 2>&1
    if not errorlevel 1 (
        for /f "usebackq tokens=*" %%i in (`qmake -query QT_INSTALL_BINS 2^>nul`) do set "QT_BINS=%%i"
        if defined QT_BINS if exist "%QT_BINS%\windeployqt.exe" set "WINDEPLOYQT_CMD=%QT_BINS%\windeployqt.exe"
    )
)

if /I "%WINDEPLOYQT_CMD%"=="windeployqt" (
    echo Note: windeployqt was not found on PATH/QTDIR/qmake. Skipping deployment.
    echo Hint: Install Qt and ensure windeployqt is on PATH, or set QTDIR, e.g.
    echo   set QTDIR=C:\Qt\6.6.2\mingw_64
    echo   set PATH=%%QTDIR%%\bin;%%PATH%%
) else (
    echo Using: %WINDEPLOYQT_CMD%
    "%WINDEPLOYQT_CMD%" --no-compiler-runtime --dir "!DEPLOY_DIR!" "!EXE_PATH!"
)
:: Ensure exe present in deploy dir
if not exist "!DEPLOY_DIR!\shopping_client.exe" copy /Y "!EXE_PATH!" "!DEPLOY_DIR!\shopping_client.exe" >nul 2>&1
echo Deployed files are in: %CD%\!DEPLOY_DIR!
endlocal

echo ========================================
echo Build and deployment completed!
echo ========================================

goto :eof

:deploy_end
pause