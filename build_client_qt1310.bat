@echo off
setlocal enabledelayedexpansion

pushd "%~dp0" >nul 2>&1

echo ========================================
echo Configure & Build (Qt MinGW 13.1)
echo ========================================

set "QT_MINGW=D:\Qt\Tools\mingw1310_64\bin"
if not exist "%QT_MINGW%\g++.exe" (
    echo Error: Cannot find Qt MinGW 13.1 toolchain at %QT_MINGW%.
    echo Please install Qt MinGW 13.1 and adjust QT_MINGW path in this script.
    pause
    exit /b 1
)

echo Using compiler: %QT_MINGW%\g++.exe

rem Configure CMake with explicit compilers and make program
cmake -S client -B build-qt1310 -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER="%QT_MINGW%\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%QT_MINGW%\g++.exe" ^
  -DCMAKE_MAKE_PROGRAM="%QT_MINGW%\mingw32-make.exe"
if errorlevel 1 (
    echo CMake configure failed.
    pause
    exit /b 1
)

echo Building client...
cmake --build build-qt1310 --target shopping_client --config Release -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo Packaging (windeployqt)...
cmake --build build-qt1310 --target package_app --config Release -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo Package failed.
    pause
    exit /b 1
)

rem Ensure PNG/WebP plugins are present in dist/imageformats
set "DIST_DIR=%CD%\build-qt1310\dist"
set "DIST_DIR_CLIENT=%CD%\client\build-qt1310\dist"
if exist "%DIST_DIR%\shopping_client.exe" (
    if not exist "%DIST_DIR%\imageformats" mkdir "%DIST_DIR%\imageformats" >nul 2>&1
    set "PLUG_IF_DIR="
    rem Prefer QTDIR if set
    if defined QTDIR if exist "%QTDIR%\plugins\imageformats\qpng.dll" set "PLUG_IF_DIR=%QTDIR%\plugins\imageformats"
    rem Robust search: use where /r to find qpng.dll under common roots (handles nested mingw_64\plugins)
    if not defined PLUG_IF_DIR for /f "delims=" %%P in ('where /r "D:\Qt" qpng.dll 2^>nul ^| findstr /i /r "\\plugins\\imageformats\\qpng\.dll$"') do if not defined PLUG_IF_DIR set "PLUG_IF_DIR=%%~dpP"
    if not defined PLUG_IF_DIR for /f "delims=" %%P in ('where /r "C:\Qt" qpng.dll 2^>nul ^| findstr /i /r "\\plugins\\imageformats\\qpng\.dll$"') do if not defined PLUG_IF_DIR set "PLUG_IF_DIR=%%~dpP"
    if defined PLUG_IF_DIR (
        echo Copying imageformats from: %PLUG_IF_DIR%
        if exist "%PLUG_IF_DIR%\qpng.dll" copy /Y "%PLUG_IF_DIR%\qpng.dll" "%DIST_DIR%\imageformats\" >nul 2>&1
        if exist "%PLUG_IF_DIR%\qwebp.dll" copy /Y "%PLUG_IF_DIR%\qwebp.dll" "%DIST_DIR%\imageformats\" >nul 2>&1
    ) else (
        echo Warning: Could not locate qpng.dll automatically. PNG decoding may fail. Please set QTDIR or adjust this script.
    )
)

rem If client\build-qt1310\dist exists (used by VS Code tasks), mirror plugins there too
if exist "%DIST_DIR_CLIENT%\shopping_client.exe" (
    if not exist "%DIST_DIR_CLIENT%\imageformats" mkdir "%DIST_DIR_CLIENT%\imageformats" >nul 2>&1
    if defined PLUG_IF_DIR (
        if exist "%PLUG_IF_DIR%\qpng.dll" copy /Y "%PLUG_IF_DIR%\qpng.dll" "%DIST_DIR_CLIENT%\imageformats\" >nul 2>&1
        if exist "%PLUG_IF_DIR%\qwebp.dll" copy /Y "%PLUG_IF_DIR%\qwebp.dll" "%DIST_DIR_CLIENT%\imageformats\" >nul 2>&1
    ) else (
        echo Note: %DIST_DIR_CLIENT% exists but imageformats plugins could not be located automatically.
    )
)

echo Done. Dist at: %CD%\build-qt1310\dist
popd >nul 2>&1
exit /b 0
