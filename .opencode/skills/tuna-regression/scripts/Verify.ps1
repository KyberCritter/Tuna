[CmdletBinding()]
param([ValidateSet('All','Host','Gba','GameCube','Wii')][string]$Target = 'Host', [string]$DevkitPro, [string]$MsysBash)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$run = Join-Path $root ('artifacts\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $run | Out-Null
Start-Transcript -Path "$run\build.log" | Out-Null
$passed = $false
try {
    & "$root\Build.ps1" -Target $Target -DevkitPro $DevkitPro -MsysBash $MsysBash
    $passed = $true
} finally {
    Stop-Transcript | Out-Null
    $revision = (& git -C $root rev-parse HEAD).Trim()
    @{ target = $Target; passed = $passed; revision = $revision; utc = [DateTime]::UtcNow.ToString('o');
       emulatorAcceptance = 'not run'; hardwareAcceptance = 'not run' } |
        ConvertTo-Json | Set-Content "$run\result.json"
    Write-Output "Evidence: $run"
}
