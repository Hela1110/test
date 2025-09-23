@echo off
setlocal enabledelayedexpansion

REM Ensure working directory is the script's folder when double-clicked
pushd "%~dp0" >nul 2>&1

echo ========================================
echo Starting Server Application
echo ========================================

REM Allow optional port override via first argument (default 8081)
set "SERVER_PORT=%~1"
if "%SERVER_PORT%"=="" set "SERVER_PORT=8081"
echo Using Spring Boot HTTP port: %SERVER_PORT%

set "JAR=target\shopping-server-1.0-SNAPSHOT.jar"

echo Packaging server (skip tests)...
where mvn >nul 2>&1
if errorlevel 1 (
    echo(
    echo Error: Maven ^(mvn^) is not available on PATH.
    echo Please open a terminal in the server folder and run:
    echo   mvn -DskipTests package
    echo Or install Maven and try again.
    goto :endWithPause
)
mvn -q -DskipTests package
if errorlevel 1 (
    echo Error: Maven package failed.
    goto :endWithPause
)

if not exist "%JAR%" (
    echo Error: Build completed but jar not found at %JAR%
    goto :endWithPause
)

:runJar
REM Check Java installation
where java >nul 2>&1
if errorlevel 1 (
    echo Error: Java not found on PATH. Please install Java 11 or higher.
    goto :endWithPause
)

REM Set Java options
set "JAVA_OPTS=-Xms512m -Xmx1024m -XX:+UseG1GC"

echo Starting shopping server...
java %JAVA_OPTS% -jar "%JAR%" --server.port=%SERVER_PORT%

echo(
echo Server exited with code %ERRORLEVEL%.

:endWithPause
popd >nul 2>&1
pause
exit /b %ERRORLEVEL%