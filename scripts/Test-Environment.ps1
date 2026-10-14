[CmdletBinding()]
param(
    [string]$DevkitPro,
    [string]$Dolphin,
    [string]$Mgba = 'C:\Program Files\mGBA\mGBA.exe',
    [string]$Bios
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $DevkitPro) {
    foreach ($candidate in @($env:DEVKITPRO, "$root\.local\tools\msys64\opt\devkitpro", 'D:\devkitPro')) {
        if ($candidate -and (Test-Path "$candidate\devkitARM")) { $DevkitPro = $candidate; break }
    }
    if (!$DevkitPro) { $DevkitPro = 'D:\devkitPro' }
}
if (-not $Dolphin) {
    $Dolphin = Join-Path $root '.local\tools\dolphin\Dolphin.exe'
    if (!(Test-Path $Dolphin) -and (Test-Path 'D:\Dolphin\Dolphin.exe')) { $Dolphin = 'D:\Dolphin\Dolphin.exe' }
}
if (-not $Bios) { $Bios = Join-Path $root '.local\bios\gba_bios.bin' }

$checks = @(
    [pscustomobject]@{ Component = 'Git'; Available = [bool](Get-Command git -ErrorAction SilentlyContinue); Path = 'PATH' }
    [pscustomobject]@{ Component = 'Dolphin'; Available = (Test-Path -LiteralPath $Dolphin -PathType Leaf); Path = $Dolphin }
    [pscustomobject]@{ Component = 'mGBA'; Available = (Test-Path -LiteralPath $Mgba -PathType Leaf); Path = $Mgba }
    [pscustomobject]@{ Component = 'devkitPPC compiler'; Available = (Test-Path -LiteralPath (Join-Path $DevkitPro 'devkitPPC\bin\powerpc-eabi-gcc.exe')); Path = $DevkitPro }
    [pscustomobject]@{ Component = 'devkitARM compiler'; Available = (Test-Path -LiteralPath (Join-Path $DevkitPro 'devkitARM\bin\arm-none-eabi-gcc.exe')); Path = $DevkitPro }
    [pscustomobject]@{ Component = 'GBA BIOS (size only)'; Available = ((Test-Path -LiteralPath $Bios -PathType Leaf) -and ((Get-Item -LiteralPath $Bios).Length -eq 16384)); Path = $Bios }
)
$checks | Format-Table -AutoSize

if (Test-Path -LiteralPath $Mgba -PathType Leaf) {
    $info = (Get-Item -LiteralPath $Mgba).VersionInfo
    Write-Output "mGBA file version: $($info.FileVersion); product version: $($info.ProductVersion)"
}
Write-Output 'File presence/BIOS size do not prove emulator features, BIOS authenticity, or a passing link test.'
Write-Output 'See docs/development.md for provisioning and required end-to-end acceptance gates.'
