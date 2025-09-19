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
echo The executable is located in build/bin/
echo ========================================

:: Copy required DLLs (adjust paths as needed)
echo Copying Qt DLLs...
windeployqt bin/shopping-client.exe

echo ========================================
echo Build and deployment completed!
echo ========================================

pause