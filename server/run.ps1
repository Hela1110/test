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
    Write-Host "[1/3] Packaging server (skip tests)..."
    Write-Host 'Running: mvn -DskipTests package'
    & mvn -DskipTests package
}

# Ensure lib folder exists; only warn if JAXB jars are missing
$libDir = Join-Path $Root 'lib'
if (-not (Test-Path $libDir)) { New-Item -ItemType Directory -Path $libDir | Out-Null }
Write-Host '[2/3] Preparing fallback libs (JAXB API/Activation)...'
if (-not (Test-Path (Join-Path $libDir 'jaxb-api-2.3.1.jar'))) {
    Write-Warning ("$libDir\jaxb-api-2.3.1.jar not found. If startup fails with JAXBException, please place the jar here.")
}
if (-not (Test-Path (Join-Path $libDir 'activation-1.1.1.jar'))) {
    Write-Host ("Note: $libDir\activation-1.1.1.jar not found. Usually optional; ignore if app starts.")
}


# Build java command args explicitly to avoid quoting issues
$jarPath = Join-Path $Root 'target\shopping-server-1.0-SNAPSHOT.jar'
if (-not (Test-Path $jarPath)) {
    throw "Jar not found: $jarPath. Did the build fail?"
}

$javaArgs = @(
    '-Xms512m','-Xmx1024m','-XX:+UseG1GC',
    '-Djava.awt.headless=false',
    "-Dloader.path=$libDir",
    '-cp', $jarPath,
    'org.springframework.boot.loader.PropertiesLauncher',
    "--server.port=$HttpPort",
    "--socket.port=$SocketPort",
    '--probe.db=true'
)

# Append DB overrides if provided
if ($DbUrl)  { $javaArgs += "--spring.datasource.url=$DbUrl" }
if ($DbUser) { $javaArgs += "--spring.datasource.username=$DbUser" }
if ($DbPass) { $javaArgs += "--spring.datasource.password=$DbPass" }
if ($EnsureChat) { $javaArgs += "--probe.db.ensureChat=true" }

Write-Host '[3/3] Launching with PropertiesLauncher...'
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
