[CmdletBinding()]
param([Parameter(Mandatory)][string]$DevkitPro, [string]$Elf, [string]$Image)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Elf) { $Elf = "$root\build\gba\tuna.elf" }
if (!$Image) { $Image = "$root\build\gba\tuna.mb" }
$nm = "$DevkitPro\devkitARM\bin\arm-none-eabi-nm.exe"
$symbols = & $nm -n $Elf
if ($LASTEXITCODE) { throw 'Cannot inspect GBA symbols.' }
foreach ($expected in @('02010000 B __track_start','02040000 B __track_end')) {
    if ($symbols -notcontains $expected) { throw "GBA memory reservation mismatch: $expected" }
}
if ($symbols -match '\b(_?malloc(_r)?|_?calloc(_r)?|_?realloc(_r)?|_?free(_r)?)$') { throw 'GBA image unexpectedly links heap allocation functions.' }
if ((Get-Item $Image).Length -gt 65536) { throw 'GBA image exceeds resident budget.' }
$boundary = $symbols | Where-Object { $_ -match '\bboundary$' }
if (!$boundary -or $boundary -notmatch '^0300') { throw 'Audio boundary IRQ must execute from IWRAM.' }
Write-Output 'PASS: GBA NOLOAD track bounds, resident image size, no heap symbols, IWRAM audio IRQ (linker asserts also passed).'
