param(
  [Parameter(Mandatory=$true)]
  [int]$Port
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

Write-Host ("Freeing port {0}..." -f $Port)

function Get-ListeningPidsFromNetstat([int]$p){
  netstat -ano |
    Select-String -Pattern 'LISTENING' |
    Select-String -Pattern (':{0}' -f $p) |
    ForEach-Object { ($_ -split '\s+')[-1] } |
    Select-Object -Unique
}

$pids = @()
try {
  $pids = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction Stop |
    Select-Object -ExpandProperty OwningProcess -Unique
} catch {
  $pids = @()
}

if(-not $pids -or $pids.Count -eq 0){
  $pids = Get-ListeningPidsFromNetstat -p $Port
}

if(-not $pids -or $pids.Count -eq 0){
  Write-Host ("No process is listening on {0}" -f $Port)
  exit 0
}

Write-Host ("Listening PIDs on {0}: {1}" -f $Port, ($pids -join ', '))

$killed = @()
foreach($pidToKill in $pids){
  if([int]$pidToKill -eq $PID){
    Write-Warning ("Skip current PowerShell (PID {0})" -f $PID)
    continue
  }
  if([int]$pidToKill -eq 4){
    Write-Warning "PID 4 (System) cannot be terminated; consider changing port."
    continue
  }
  try{
    Stop-Process -Id $pidToKill -Force -ErrorAction Stop
    $killed += $pidToKill
    Write-Host ("Killed PID {0} on port {1}" -f $pidToKill, $Port)
  } catch {
    Write-Warning ("Failed to kill PID {0}: {1}" -f $pidToKill, $_.Exception.Message)
  }
}

if($killed.Count -gt 0){
  Write-Host ("Freed port {0}." -f $Port)
  exit 0
}
else{
  Write-Warning ("No process was terminated for port {0}." -f $Port)
  exit 1
}
