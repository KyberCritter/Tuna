[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputFile,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z_][A-Za-z0-9_]*$')][string]$Symbol,
    [int]$MaximumBytes = 65536
)
$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes($InputFile)
if (!$bytes.Length -or $bytes.Length -gt $MaximumBytes) { throw "Binary must be 1..$MaximumBytes bytes." }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$guard = $Symbol.ToUpper() + '_H'
$header = "#ifndef $guard`n#define $guard`n#include <stddef.h>`n#include <stdint.h>`nextern const uint8_t ${Symbol}[];`nextern const size_t ${Symbol}_size;`n#endif`n"
$text = [Text.StringBuilder]::new()
[void]$text.Append("#include `"$Symbol.h`"`nconst uint8_t ${Symbol}[] = {`n")
for ($i = 0; $i -lt $bytes.Length; $i++) {
    [void]$text.Append(('0x{0:x2},' -f $bytes[$i]))
    if ($i % 16 -eq 15) { [void]$text.Append("`n") }
}
[void]$text.Append("`n};`nconst size_t ${Symbol}_size = sizeof($Symbol);`n")
foreach ($item in @(@("$Symbol.h", $header), @("$Symbol.c", $text.ToString()))) {
    $path = Join-Path $OutputDirectory $item[0]
    if (!(Test-Path $path) -or [IO.File]::ReadAllText($path) -ne $item[1]) { [IO.File]::WriteAllText($path, $item[1]) }
}
