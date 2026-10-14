[CmdletBinding()]
param([string]$Image, [string]$Output)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$Image) { $Image = "$root\build\gba\tuna.mb" }
if (!$Output) { $Output = "$root\build\generated" }
$bytes = [IO.File]::ReadAllBytes($Image)
if ($bytes.Length -lt 512 -or $bytes.Length -gt 65536) { throw 'Multiboot image must fit the resident 64 KiB envelope.' }
New-Item -ItemType Directory -Force $Output | Out-Null
$header = "#ifndef TUNA_PLAYER_IMAGE_H`n#define TUNA_PLAYER_IMAGE_H`n#include <stddef.h>`n#include <stdint.h>`nextern const uint8_t player_image[];`nextern const size_t player_image_size;`n#endif`n"
$text = [Text.StringBuilder]::new()
[void]$text.Append("#include `"player_image.h`"`nconst uint8_t player_image[] = {`n")
for ($i = 0; $i -lt $bytes.Length; $i++) {
    [void]$text.Append(('0x{0:x2},' -f $bytes[$i]))
    if ($i % 16 -eq 15) { [void]$text.Append("`n") }
}
[void]$text.Append("`n};`nconst size_t player_image_size = sizeof(player_image);`n")
foreach ($item in @(@('player_image.h', $header), @('player_image.c', $text.ToString()))) {
    $path = Join-Path $Output $item[0]
    if (!(Test-Path $path) -or [IO.File]::ReadAllText($path) -ne $item[1]) { [IO.File]::WriteAllText($path, $item[1]) }
}
Write-Output "Embedded $($bytes.Length)-byte GBA player (SHA256 $((Get-FileHash $Image).Hash))."
