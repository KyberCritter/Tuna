# SPDX-License-Identifier: MIT
[CmdletBinding()]
param([Parameter(Mandatory)][string]$State,[Parameter(Mandatory)][string]$Track,[string]$Output)
$ErrorActionPreference='Stop'
$stateBytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $State).Path)
$trackBytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Track).Path)
if($stateBytes.Length -ne 0x61000 -or [BitConverter]::ToUInt32($stateBytes,0) -ne 0x01000009){throw 'Unsupported state: require raw mGBA v9 (397312 bytes). Do not guess other layouts.'}
if($trackBytes.Length -lt 96 -or $trackBytes.Length -gt 196608 -or [Text.Encoding]::ASCII.GetString($trackBytes,0,4) -ne 'TUNA' -or
   [BitConverter]::ToUInt16($trackBytes,4) -ne 1 -or [BitConverter]::ToUInt16($trackBytes,6) -ne 96 -or
   [BitConverter]::ToUInt32($trackBytes,20)+96 -ne $trackBytes.Length){throw 'Expected track is not a bounded Tuna v1 container.'}
$offset=0x31000
for($i=0;$i -lt $trackBytes.Length;$i++) {
    if($stateBytes[$offset+$i] -ne $trackBytes[$i]){throw "Retained track mismatch at byte $i (GBA EWRAM 0x02010000)."}
}
$result=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');exactRetainedTrackMatch=$true;stateOffset=$offset;gbaAddress='0x02010000';
    trackBytes=$trackBytes.Length;trackSha256=(Get-FileHash -LiteralPath $Track).Hash;stateSha256=(Get-FileHash -LiteralPath $State).Hash;
    payloadCrc=('{0:x8}' -f [BitConverter]::ToUInt32($trackBytes,92));audioAcceptance='not measured';readyAcceptance='requires reviewed sender/GBA evidence'}
if($Output){if(Test-Path -LiteralPath $Output){throw 'Comparison output already exists.'};$result | ConvertTo-Json | Set-Content -LiteralPath $Output}
[pscustomobject]$result
