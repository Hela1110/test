@echo off
echo ========================================
echo Starting Client Application
echo ========================================

:: Check if the executable exists
if not exist "build\bin\shopping-client.exe" (
    echo Error: Client application not built!
    echo Please run build.bat first.
    pause
    exit /b 1
)

:: Start the application
echo Starting shopping client...
start "" "build\bin\shopping-client.exe"

exit /b 0