@echo off
echo ========================================
echo Starting Server Application
echo ========================================

:: Check if JAR file exists
if not exist "target\shopping-server-1.0-SNAPSHOT.jar" (
    echo Error: Server application not built!
    echo Please run build.bat first.
    pause
    exit /b 1
)

:: Check Java installation
java -version 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: Java not found!
    echo Please install Java 11 or higher.
    pause
    exit /b 1
)

:: Set Java options
set JAVA_OPTS=-Xms512m -Xmx1024m -XX:+UseG1GC

:: Start the application
echo Starting shopping server...
java %JAVA_OPTS% -jar target/shopping-server-1.0-SNAPSHOT.jar

pause