[CmdletBinding()]
param(
    [ValidateSet('gamecube','wii')][string]$Platform = 'gamecube',
    [string]$Dolphin,
    [string]$Bios,
    [ValidateRange(2,4)][int]$Port = 2
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Dolphin) {
    foreach ($path in @("$root\.local\tools\dolphin\Dolphin.exe", 'D:\Dolphin\Dolphin.exe')) { if (Test-Path $path) { $Dolphin = $path; break } }
}
if (!$Dolphin -or !(Test-Path $Dolphin)) { throw 'Dolphin missing. Supply -Dolphin.' }
if (!$Bios) { $Bios = "$root\.local\bios\gba_bios.bin" }
if (!(Test-Path $Bios) -or (Get-Item $Bios).Length -ne 16384) { throw 'A user-supplied 16384-byte GBA BIOS dump is required. Do not download a BIOS.' }
$dol = "$root\build\$Platform\tuna.dol"
if (!(Test-Path $dol)) { throw "Build the $Platform DOL first." }
$run = "$root\artifacts\dolphin-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
$profile = "$run\user"
New-Item -ItemType Directory -Force "$profile\Config" | Out-Null
# Dolphin INI keys are verified against its published source; values remain a smoke-test gate.
$config = @"
[Core]
SIDevice0 = 6
SIDevice$($Port - 1) = 13
[General]
ISOPaths = 0
[Interface]
PauseOnFocusLost = False
[GBA]
BIOS = $($Bios.Replace('\','/'))
Rom1 =
Rom2 =
Rom3 =
Rom4 =
"@
[IO.File]::WriteAllText("$profile\Config\Dolphin.ini", $config)
@{ utc = [DateTime]::UtcNow.ToString('o'); platform = $Platform; port = $Port; rom = 'empty';
   dolphin = (Get-FileHash $Dolphin).Hash; bios = (Get-FileHash $Bios).Hash; dol = (Get-FileHash $dol).Hash;
   acceptance = 'not run; supervised UI/audio/disconnect checks required' } |
   ConvertTo-Json | Set-Content "$run\manifest.json"
Write-Output "Isolated profile/evidence: $run"
Write-Output 'Confirm BIOS and empty GBA ROM paths in Dolphin before the multiboot test. Configure controls; keep the same GBA instance when toggling Connected.'
Start-Process -FilePath $Dolphin -ArgumentList @('--user', ('"' + $profile + '"'), '--exec', ('"' + $dol + '"'))
