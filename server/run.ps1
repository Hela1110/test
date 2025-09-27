param(
    [int]$HttpPort = 8081,
    [int]$SocketPort = 8080,
    [switch]$SkipBuild,
    [switch]$EnsureChat,
    [string]$DbUrl,
    [string]$DbUser,
    [string]$DbPass
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve script root robustly (handles interactive paste where $PSScriptRoot is empty)
if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) {
    $ScriptPath = $MyInvocation.MyCommand.Path
    if ([string]::IsNullOrWhiteSpace($ScriptPath)) { $ScriptPath = (Get-Location).Path }
    $Root = Split-Path -Parent $ScriptPath
} else {
    $Root = $PSScriptRoot
}
Set-Location -Path $Root

Write-Host '========================================'
Write-Host 'Starting Server Application (PowerShell)'
Write-Host '========================================'
Write-Host "Using Spring Boot HTTP port: $HttpPort"
Write-Host "Using Socket Server   port: $SocketPort"
if ($DbUrl)  { Write-Host "Using DB URL      : $DbUrl" }
if ($DbUser) { Write-Host "Using DB User     : $DbUser" }
if ($EnsureChat) { Write-Host "Ensure chat_messages table on startup: ON" }

if (-not $SkipBuild) {
    Write-Host "[1/2] Packaging server (skip tests)..."
    Write-Host 'Running: mvn -DskipTests package'
    & mvn -DskipTests package
}

# Build java command args explicitly to avoid quoting issues (fat jar)
$jarPath = Join-Path $Root 'target\shopping-server-1.0-SNAPSHOT.jar'
if (-not (Test-Path $jarPath)) {
    throw "Jar not found: $jarPath. Did the build fail?"
}

$javaArgs = @(
    '-Xms512m','-Xmx1024m','-XX:+UseG1GC',
    '-Djava.awt.headless=false',
    '-jar', $jarPath,
    "--server.port=$HttpPort",
    "--socket.port=$SocketPort",
    '--probe.db=true'
)

# Append DB overrides if provided
if ($DbUrl)  { $javaArgs += "--spring.datasource.url=$DbUrl" }
if ($DbUser) { $javaArgs += "--spring.datasource.username=$DbUser" }
if ($DbPass) { $javaArgs += "--spring.datasource.password=$DbPass" }
if ($EnsureChat) { $javaArgs += "--probe.db.ensureChat=true" }

Write-Host '[2/2] Launching fat-jar...'
Write-Host ('java ' + ($javaArgs -join ' '))

# Run java and stream output; this blocks until app exits
& java @javaArgs
$exitCode = $LASTEXITCODE
Write-Host "Server exited with code $exitCode"

if ($Host.Name -match 'ConsoleHost') {
    Write-Host 'Press any key to close...'
    [void][System.Console]::ReadKey($true)
}
exit $exitCode
