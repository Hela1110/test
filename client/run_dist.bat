@echo off
setlocal enableextensions
pushd "%~dp0" >nul 2>&1

echo ========================================
echo Run Client (packaged dist)
echo ========================================

set "DIST1=build-qt1310\dist\shopping_client.exe"
set "DIST2=..\build-qt1310\dist\shopping_client.exe"
set "EXE="
for %%P in ("%DIST1%" "%DIST2%") do (
  if not defined EXE if exist %%~fP set "EXE=%%~fP"
)
if not defined EXE (
  echo Error: dist exe not found. Expected:
  echo   %DIST1%
  echo   %DIST2%
  echo Please build package_app first.
  goto :end
)

echo Using: %EXE%
start "shopping-client" "%EXE%"

:end
popd >nul 2>&1
exit /b 0
