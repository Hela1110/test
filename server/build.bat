@echo off
setlocal enabledelayedexpansion

:: Set code page to UTF-8
chcp 65001 > nul

echo ========================================
echo Building Full Project
echo ========================================

:: Create necessary directories if they don't exist
if not exist "client" mkdir client
if not exist "server" mkdir server
if not exist "database" mkdir database

:: Check prerequisites
call :check_prerequisites
if !ERRORLEVEL! NEQ 0 goto :end

:: Build Server
echo.
echo ========================================
echo Building Server Application
echo ========================================
cd server
call :build_server
if !ERRORLEVEL! NEQ 0 goto :end
cd ..

:: Build Client
echo.
echo ========================================
echo Building Client Application
echo ========================================
cd client
call :build_client
if !ERRORLEVEL! NEQ 0 goto :end
cd ..

echo.
echo ========================================
echo All builds completed successfully!
echo ========================================
goto :end

:check_prerequisites
echo Checking prerequisites...

:: Check Java
java -version >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo Error: Java not found!
    echo Please install Java 11 or higher.
    exit /b 1
)

:: Check Maven
mvn -version >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo Error: Maven not found!
    echo Please install Maven 3.6 or higher.
    exit /b 1
)

:: Check Qt
qmake --version >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo Error: Qt not found!
    echo Please install Qt 5.15 or higher and set up environment variables.
    exit /b 1
)

:: Check CMake
cmake --version >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    echo Error: CMake not found!
    echo Please install CMake 3.14 or higher.
    exit /b 1
)

echo All prerequisites satisfied.
exit /b 0

:build_server
echo Building server with Maven...
call mvn clean package
if !ERRORLEVEL! NEQ 0 (
    echo Maven build failed!
    exit /b 1
)
echo Server build completed successfully.
exit /b 0

:build_client
if not exist "build" mkdir build
cd build

echo Configuring client with CMake...
cmake -G "MinGW Makefiles" ..
if !ERRORLEVEL! NEQ 0 (
    echo CMake configuration failed!
    exit /b 1
)

echo Building client with MinGW...
mingw32-make
if !ERRORLEVEL! NEQ 0 (
    echo MinGW build failed!
    exit /b 1
)

echo Deploying Qt dependencies...
if exist "bin\shopping-client.exe" (
    windeployqt bin\shopping-client.exe
) else (
    echo Warning: Client executable not found at expected location
)

cd ..
echo Client build completed successfully.
exit /b 0

:end
echo.
if !ERRORLEVEL! NEQ 0 (
    echo Build process failed with errors.
) else (
    echo Build process completed successfully.
)
pause
exit /b !ERRORLEVEL!