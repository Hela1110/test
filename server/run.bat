@echo off
echo ========================================
echo Starting Server Application
echo ========================================

:: Build (ensure executable jar with main manifest)
echo Packaging server (skip tests)...
mvn -q -DskipTests package
if %ERRORLEVEL% NEQ 0 (
    echo Error: Maven package failed.
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