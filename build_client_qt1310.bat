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

echo Done. Dist at: %CD%\build-qt1310\dist
popd >nul 2>&1
exit /b 0
