[CmdletBinding()]
param([switch]$TestOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build\host'
New-Item -ItemType Directory -Force $build | Out-Null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio C++ build tools, or use make host on a GCC/Clang machine.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools not found.' }
$sources = @(Get-ChildItem "$root\shared\*.c" | ForEach-Object { '"' + $_.FullName + '"' })
$common = '/nologo /std:c11 /W4 /O2 /D_CRT_SECURE_NO_WARNINGS /I"' + "$root\include" + '" /I"' + "$root\third_party\minimp3" + '" '
$vc = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
foreach ($target in @('tests','tuna-convert')) {
    $main = if ($target -eq 'tests') { "$root\tests\test.c" } else { "$root\host\main.c" }
    $line = 'call "' + $vc + '" -arch=x64 >nul && cl ' + $common + ($sources -join ' ') + ' "' + "$root\host\convert.c" + '" "' + $main + '" /Fe:"' + "$build\$target.exe" + '"'
    Push-Location $build
    try { & cmd.exe /d /s /c $line; if ($LASTEXITCODE) { throw "Compilation failed: $target" } } finally { Pop-Location }
}
& "$build\tests.exe"
if ($LASTEXITCODE) { throw 'Unit tests failed.' }
