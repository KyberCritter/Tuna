[CmdletBinding()]
param(
    [ValidateSet('All','Host','Gba','GameCube','Wii')][string]$Target = 'All',
    [string]$DevkitPro,
    [string]$MsysBash
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
# Optional legacy Windows wrapper. The actual build is entirely in GNU Make/C.
if (!$DevkitPro) {
    foreach ($candidate in @($env:DEVKITPRO, "$root\.local\tools\msys64\opt\devkitpro", "$root\.local\devkitPro", 'D:\devkitPro')) {
        if ($candidate -and (Test-Path "$candidate\devkitARM\bin\arm-none-eabi-gcc.exe")) { $DevkitPro = $candidate; break }
    }
}
if (!$MsysBash) {
    foreach ($candidate in @("$DevkitPro\msys2\usr\bin\bash.exe", "$root\.local\tools\msys64\usr\bin\bash.exe")) {
        if (Test-Path $candidate) { $MsysBash = $candidate; break }
    }
}
if (!$MsysBash -or !(Test-Path $MsysBash)) { throw 'Managed MSYS2 bash missing. Supply -MsysBash.' }
$cygpath = Join-Path (Split-Path $MsysBash) 'cygpath.exe'
$unixRoot = (& $cygpath -u $root).Trim()
$environment = ''
if ($DevkitPro) {
    $unixDkp = (& $cygpath -u $DevkitPro).Trim()
    $environment = "export DEVKITPRO='$unixDkp' DEVKITARM='$unixDkp/devkitARM' DEVKITPPC='$unixDkp/devkitPPC'; "
}
$command = $environment + "export PATH=/ucrt64/bin:`$PATH; cd '$unixRoot' && make -j4 $($Target.ToLower())"
& $MsysBash --login -c $command
if ($LASTEXITCODE) { throw 'GNU Make build failed. See docs/build.md for portable shell instructions.' }
