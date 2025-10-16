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

$JarName = 'shopping-server-1.0-SNAPSHOT.jar'
$JarPath = Join-Path $Root "target\$JarName"

function Stop-LockingJavaProcesses {
    param([string]$JarIdentifier)
    try {
        $procs = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -match 'java(\.exe)?' -and $_.CommandLine -match [regex]::Escape($JarIdentifier) }
        if ($procs) {
            foreach ($p in $procs) {
                Write-Host ("Stopping Java PID {0} locking {1}" -f $p.ProcessId, $JarIdentifier)
                try { Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop } catch { Write-Host "Warn: failed to stop PID $($p.ProcessId): $($_.Exception.Message)" }
            }
            Start-Sleep -Milliseconds 300
        }
    } catch { Write-Host "Warn: process enumeration failed: $($_.Exception.Message)" }
}

$exitCode = 0
try {
    if (-not $SkipBuild) {
        Write-Host "[1/2] Packaging server (skip tests)..."
        Write-Host 'Running: mvn -DskipTests clean package'
        # 先终止可能正在运行的旧实例，避免 repackage 无法将 jar 重命名为 .original（Windows 文件锁）
        Stop-LockingJavaProcesses -JarIdentifier $JarName
        & mvn -DskipTests clean package
        if ($LASTEXITCODE -ne 0) {
            throw "Maven build failed with exit code $LASTEXITCODE"
        }
    }

    # Build java command args explicitly to avoid quoting issues (fat jar)
    if (-not (Test-Path $JarPath)) {
        Write-Error "Jar not found: $JarPath. Did the build fail or did you skip build?"
        $exitCode = 1
        return
    }

    $javaArgs = @(
        '-Xms512m','-Xmx1024m','-XX:+UseG1GC',
        '-Djava.awt.headless=false',
        '-jar', $JarPath,
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
}
catch {
    Write-Host '----------------------------------------'
    Write-Host 'Server startup failed with an exception:' -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace }
    $exitCode = 1
}
finally {
    Write-Host "Server exited with code $exitCode"
    if ($Host.Name -match 'ConsoleHost') {
        Write-Host 'Press any key to close...'
        try { [void][System.Console]::ReadKey($true) } catch {}
    }
}

# Do not explicitly exit here; when launched with -NoExit the window will stay.
