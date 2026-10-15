# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$RunDirectory,
 [Parameter(Mandatory)][ValidateSet('gc-multiboot','wii-multiboot','wii-sd-conversion','cache-failed-retry','cache-restart','cache-browser','confirmation','retained-bytes','offline-controls','data-ack-disconnect','captured-audio','sample-underrun-dma','long-profiles','gc-sd-gecko','standalone-handoff')][string]$Gate,
 [Parameter(Mandatory)][ValidateSet('passed','failed','blocked','not-run')][string]$Status,
 [string[]]$Evidence=@(),[Parameter(Mandatory)][string]$Notes
)
. "$PSScriptRoot/Common.ps1"
$null=Read-TunaRun $RunDirectory
$gates=@('gc-multiboot','wii-multiboot','wii-sd-conversion','cache-failed-retry','cache-restart','cache-browser','confirmation','retained-bytes','offline-controls','data-ack-disconnect','captured-audio','sample-underrun-dma','long-profiles','gc-sd-gecko','standalone-handoff')
if($Status -eq 'passed' -and !$Evidence.Count){throw 'Passed gates need reviewed/measured evidence.'}
foreach($file in $Evidence) {
 if([IO.Path]::IsPathRooted($file) -or $file -match '(^|[\\/])\.\.([\\/]|$)'){throw 'Evidence must be relative to this run directory.'}
 if(!(Test-Path -LiteralPath (Join-Path $RunDirectory $file))){throw "Missing evidence: $file"}
}
$path=Join-Path $RunDirectory 'acceptance.json'
$report=@{}
if(Test-Path $path){$old=Get-Content $path -Raw | ConvertFrom-Json;foreach($property in $old.gates.PSObject.Properties){$report[$property.Name]=$property.Value}}
foreach($name in $gates){if(!$report.ContainsKey($name)){$report[$name]=@{status='not-run';evidence=@();notes='Not measured.'}}}
$report[$Gate]=@{utc=[DateTime]::UtcNow.ToString('o');status=$Status;evidence=$Evidence;notes=$Notes}
$full=@($gates | Where-Object {$report[$_].status -ne 'passed'}).Count -eq 0
@{gates=$report;fullEmulatorAcceptance=$full;hardwareAcceptance='not measured by emulator';utc=[DateTime]::UtcNow.ToString('o')} |
 ConvertTo-Json -Depth 8 | Set-Content $path
$path
