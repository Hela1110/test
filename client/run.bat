@echo off
setlocal enabledelayedexpansion

REM Always run from this script's folder
pushd "%~dp0" >nul 2>&1

echo ========================================
echo Starting Client Application
echo ========================================

REM Candidate executable locations (most common first)
set "C1=build\shopping_client.exe"
set "C2=build\dist\shopping_client.exe"
set "C3=build-qt1310\shopping_client.exe"
set "C4=build-qt1310\dist\shopping_client.exe"
set "C5=..\build\shopping_client.exe"
set "C6=..\build\dist\shopping_client.exe"

set "EXE_PATH="
for %%P in ("%C1%" "%C2%" "%C3%" "%C4%" "%C5%" "%C6%") do (
    if not defined EXE_PATH if exist %%~fP set "EXE_PATH=%%~fP"
)

if not defined EXE_PATH (
    echo Error: shopping_client.exe not found.
    echo Looked in:
    echo   %C1%
    echo   %C2%
    echo   %C3%
    echo   %C4%
    echo   %C5%
    echo   %C6%
    echo Please build the client first (see build_client_qt1310.bat).
    goto :end


echo Using executable: %EXE_PATH%
echo Launching shopping client...
start "shopping-client" "%EXE_PATH%"

:end
popd >nul 2>&1
pause
exit /b 0
    set "EXE_PATH=build\dist\shopping_client.exe"
