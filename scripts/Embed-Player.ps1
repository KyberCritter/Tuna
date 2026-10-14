[CmdletBinding()]
param([string]$Image, [string]$Output)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Image) { $Image = "$root\build\gba\tuna.mb" }
if (!$Output) { $Output = "$root\build\generated" }
$bytes = [IO.File]::ReadAllBytes($Image)
if ($bytes.Length -lt 512 -or $bytes.Length -gt 65536) { throw 'Multiboot image must fit the resident 64 KiB envelope.' }
& "$PSScriptRoot\Embed-Binary.ps1" -InputFile $Image -OutputDirectory $Output -Symbol player_image
Write-Output "Embedded $($bytes.Length)-byte GBA player (SHA256 $((Get-FileHash $Image).Hash))."
