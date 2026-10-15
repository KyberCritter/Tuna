# SPDX-License-Identifier: MIT
[CmdletBinding()]
param()
. "$PSScriptRoot/Common.ps1"
$checks=0
foreach($file in Get-ChildItem $PSScriptRoot -Filter '*.ps1') {
 $tokens=$null;$errors=$null
 [void][Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$errors)
 if($errors){throw ($errors | Out-String)};$checks++
}
$root=Get-TunaRoot
$dir=Join-Path $root ('artifacts/emulator-helper-tests-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
$track=Join-Path $root 'build/host/jingle.tuna'
$bytes=[IO.File]::ReadAllBytes($track)
$state=New-Object byte[] 0x61000
[BitConverter]::GetBytes([uint32]0x01000009).CopyTo($state,0);$bytes.CopyTo($state,0x31000)
$file=Join-Path $dir 'synthetic-helper-state.ss0';[IO.File]::WriteAllBytes($file,$state)
$match=& "$PSScriptRoot/Assert-RetainedTrack.ps1" -State $file -Track $track
if(!$match.exactRetainedTrackMatch){throw 'Helper positive vector failed.'};$checks++
$state[0x31064]=$state[0x31064] -bxor 1;[IO.File]::WriteAllBytes($file,$state)
$rejected=$false
try{$null=& "$PSScriptRoot/Assert-RetainedTrack.ps1" -State $file -Track $track}catch{$rejected=$true}
if(!$rejected){throw 'Helper accepted mismatched RAM.'};$checks++
$state[0]=0;[IO.File]::WriteAllBytes($file,$state);$rejected=$false
try{$null=& "$PSScriptRoot/Assert-RetainedTrack.ps1" -State $file -Track $track}catch{$rejected=$true}
if(!$rejected){throw 'Helper accepted an unsupported state.'};$checks++
$fake=@{processId=$PID;processStartUtc='invalid';dolphin='invalid'};$rejected=$false
try{$null=Assert-TunaProcess $fake}catch{$rejected=$true}
if(!$rejected){throw 'Process ownership guard failed.'};$checks++
@{processId=0;status='helper-only'} | ConvertTo-Json | Set-Content (Join-Path $dir 'manifest.json')
'reviewed fixture' | Set-Content (Join-Path $dir 'fixture.txt')
$null=& "$PSScriptRoot/Record-Result.ps1" -RunDirectory $dir -Gate retained-bytes -Status passed -Evidence fixture.txt -Notes 'Synthetic helper unit test, NOT an emulator run.'
$report=Get-Content (Join-Path $dir 'acceptance.json') -Raw | ConvertFrom-Json
if($report.fullEmulatorAcceptance){throw 'One gate incorrectly implied full acceptance.'};$checks++
Write-Output "PASS: $checks helper checks; no emulator/hardware/audio acceptance. Evidence: $dir"
