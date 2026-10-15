# SPDX-License-Identifier: MIT
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDirectory,[switch]$Force,[ValidateRange(1,60)][int]$TimeoutSeconds=10)
. "$PSScriptRoot/Common.ps1"
$run=Read-TunaRun $RunDirectory
if (!(Get-Process -Id $run.processId -ErrorAction SilentlyContinue)) { Write-TunaAction $RunDirectory 'stop' @{alreadyExited=$true}; return }
$p=Assert-TunaProcess $run
$main=$null
try { $main=Get-TunaWindow $run 'Main' } catch { if(!$Force){throw} }
if($main){[void][TunaEmulator.Native]::PostMessage([IntPtr]$main.handle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
$forced=$false
if (!$p.WaitForExit($TimeoutSeconds*1000)) {
    if (!$Force) { throw 'Graceful shutdown timed out. Do not launch another instance; retry Stop with -Force for this owned process.' }
    $p=Assert-TunaProcess $run; $p.Kill(); $p.WaitForExit(); $forced=$true
}
$run.status='closed'; $run | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $RunDirectory 'manifest.json')
Write-TunaAction $RunDirectory 'stop' @{forced=$forced;processId=$run.processId}
