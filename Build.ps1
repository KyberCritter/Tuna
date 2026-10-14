[CmdletBinding()]
param(
    [ValidateSet('All','Host','Gba','GameCube','Wii')][string]$Target = 'All',
    [string]$DevkitPro,
    [string]$MsysBash
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& "$root\scripts\Build-Host.ps1"
& "$root\build\host\tuna-convert.exe" --jingle "$root\build\host\jingle.tuna"
if ($LASTEXITCODE) { throw 'Fixture generation failed.' }
if ($Target -eq 'Host') { return }
if (!$DevkitPro) {
    foreach ($candidate in @($env:DEVKITPRO, "$root\.local\tools\msys64\opt\devkitpro", "$root\.local\devkitPro", 'D:\devkitPro')) {
        if ($candidate -and (Test-Path "$candidate\devkitARM\bin\arm-none-eabi-gcc.exe")) { $DevkitPro = $candidate; break }
    }
}
if (!$DevkitPro) { throw 'devkitPro missing. Install gba-dev, gamecube-dev, wii-dev; see docs/build.md. Host tests passed; console build NOT complete.' }
if (!$MsysBash) {
    foreach ($candidate in @("$DevkitPro\msys2\usr\bin\bash.exe", "$root\.local\tools\msys64\usr\bin\bash.exe")) {
        if (Test-Path $candidate) { $MsysBash = $candidate; break }
    }
}
if (!$MsysBash -or !(Test-Path $MsysBash)) { throw 'Managed MSYS2 bash missing. Supply -MsysBash.' }
$cygpath = Join-Path (Split-Path $MsysBash) 'cygpath.exe'
$unixRoot = (& $cygpath -u $root).Trim()
$unixDkp = (& $cygpath -u $DevkitPro).Trim()
function Invoke-Make([string]$Directory, [string]$Arguments = '') {
    $command = "export DEVKITPRO='$unixDkp' DEVKITARM='$unixDkp/devkitARM' DEVKITPPC='$unixDkp/devkitPPC'; export PATH=`"`$DEVKITPRO/tools/bin:`$PATH`"; cd '$unixRoot/$Directory' && make -j4 $Arguments"
    & $MsysBash --login -c $command
    if ($LASTEXITCODE) { throw "$Directory build failed ($Arguments)." }
}
Invoke-Make 'gba'
& "$root\scripts\Test-GbaMemory.ps1" -DevkitPro $DevkitPro
& "$root\scripts\Embed-Player.ps1"
if ($Target -eq 'Gba') { return }
if ($Target -in @('All','GameCube')) { Invoke-Make 'sender' 'PLATFORM=gamecube' }
if ($Target -in @('All','Wii')) { Invoke-Make 'sender' 'PLATFORM=wii' }
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "$root\build\gba\tuna.mb" $dist
Copy-Item "$root\build\host\tuna-convert.exe" $dist
Copy-Item "$root\build\host\jingle.tuna" $dist
if ($Target -in @('All','GameCube')) { Copy-Item "$root\build\gamecube\tuna.dol" "$dist\tuna-gamecube.dol" }
if ($Target -in @('All','Wii')) {
    New-Item -ItemType Directory -Force "$dist\apps\tuna" | Out-Null
    Copy-Item "$root\build\wii\tuna.dol" "$dist\apps\tuna\boot.dol"
    Copy-Item "$root\assets\meta.xml" "$dist\apps\tuna\meta.xml"
}
Copy-Item "$root\README.md" "$dist\README.md"
Copy-Item "$root\LICENSE" "$dist\LICENSE"
Copy-Item "$root\LICENSES" "$dist\LICENSES" -Recurse -Force
Copy-Item "$root\third_party\minimp3\LICENSE" "$dist\LICENSES\minimp3-CC0.txt"
Copy-Item "$DevkitPro\libgba\libgba_license.txt" "$dist\LICENSES\libgba.txt"
Copy-Item "$DevkitPro\libogc\LICENSE" "$dist\LICENSES\libogc.txt"
$files = Get-ChildItem $dist -File -Recurse | Where-Object Name -ne 'sha256.txt' | Sort-Object FullName
$lines = $files | ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() + '  ' + $_.FullName.Substring($dist.Length + 1).Replace('\','/') }
[IO.File]::WriteAllLines("$dist\sha256.txt", [string[]]$lines)
Write-Output "Build and unit tests complete: $dist (hardware/emulator acceptance is separate)."
