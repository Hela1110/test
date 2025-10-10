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
$ErrorActionPreference = 'Continue'

try {
    $root = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    Push-Location $root
    $logs = Join-Path $root 'logs'
    if (-not (Test-Path $logs)) { New-Item -ItemType Directory -Force -Path $logs | Out-Null }
    $ts = Get-Date -Format 'yyyyMMdd-HHmmss'
    $logFile = Join-Path $logs ("server-$ts.log")
    $lastFile = Join-Path $logs 'server-last.log'
    $markerStarted = Join-Path $logs 'wrapper-started.txt'
    $markerError = Join-Path $logs 'wrapper-error.txt'
    try { "started at $(Get-Date -Format 'u')" | Out-File -Encoding UTF8 -FilePath $markerStarted -Force } catch {}
    try { Start-Transcript -Path $logFile -Force | Out-Null } catch {}

    Write-Host '=== Server Wrapper: launching run.ps1 ==='
    & "$root\run.ps1" @PSBoundParameters
}
catch {
    Write-Host '--- Server wrapper caught an exception ---' -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace }
    try { ("error at $(Get-Date -Format 'u')`n" + $_.Exception.Message) | Out-File -Encoding UTF8 -FilePath $markerError -Force } catch {}
}
finally {
    try { Stop-Transcript | Out-Null } catch {}
    try { Copy-Item -Path $logFile -Destination $lastFile -Force } catch {}
    Write-Host '========================================'
    Write-Host 'Press any key to close this server window...'
    try { [void][System.Console]::ReadKey($true) } catch {}
    Pop-Location
}
