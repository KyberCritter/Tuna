[CmdletBinding()]
param([string]$Ffmpeg = 'ffmpeg')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$convert = "$root\build\host\tuna-convert.exe"
if (!(Test-Path $convert)) { throw 'Run Build.ps1 -Target Host first.' }
if (!(Get-Command $Ffmpeg -ErrorAction SilentlyContinue)) { throw 'FFmpeg is required to generate original MP3 regression fixtures.' }
$fixtures = "$root\build\mp3-tests"
New-Item -ItemType Directory -Force $fixtures | Out-Null
function Encode([string]$Name, [int]$Duration, [int]$Channels) {
    & $Ffmpeg -hide_banner -loglevel error -y -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=$Duration" -ac $Channels -c:a libmp3lame -b:a 128k "$fixtures\$Name.mp3"
    if ($LASTEXITCODE) { throw "Fixture encoding failed: $Name" }
}
Encode 'mono' 4 1
Encode 'stereo' 4 2
Encode 'long' 301 2
Encode 'typical' 180 2
Encode 'five-minute' 300 2
foreach ($name in @('mono','stereo')) {
    & $convert "$fixtures\$name.mp3" "$fixtures\$name.tuna"
    if ($LASTEXITCODE) { throw "MP3 conversion failed: $name" }
    & $convert "$fixtures\$name.mp3" "$fixtures\$name-repeat.tuna"
    if ($LASTEXITCODE) { throw "Repeated conversion failed: $name" }
    if ((Get-FileHash "$fixtures\$name.tuna").Hash -ne (Get-FileHash "$fixtures\$name-repeat.tuna").Hash) { throw 'Conversion is not deterministic.' }
    $bytes = [IO.File]::ReadAllBytes("$fixtures\$name.tuna")
    $samples = [BitConverter]::ToUInt32($bytes, 16)
    if ($samples -lt 4 * 8192 -or $samples -gt 5 * 8192 -or $bytes.Length -gt 196608) { throw 'Unexpected duration/size.' }
}
foreach ($name in @('typical','five-minute')) {
    & $convert "$fixtures\$name.mp3" "$fixtures\$name.tuna"
    if ($LASTEXITCODE) { throw "Complete long MP3 conversion failed: $name" }
    $bytes = [IO.File]::ReadAllBytes("$fixtures\$name.tuna")
    $rate = [BitConverter]::ToUInt32($bytes, 12)
    $samples = [BitConverter]::ToUInt32($bytes, 16)
    $seconds = if ($name -eq 'typical') { 180 } else { 300 }
    $codec = if ($name -eq 'typical') { 2 } else { 3 }
    if ($samples -ne $seconds * $rate -or $bytes[8] -ne $codec -or $bytes.Length -gt 196608) { throw "Unexpected adaptive profile/duration: $name" }
}
& $convert "$fixtures\long.mp3" "$fixtures\should-not-exist.tuna"
if ($LASTEXITCODE -eq 0) { throw 'Oversized complete MP3 was not rejected.' }
& $convert "$fixtures\long.mp3" "$fixtures\excerpt.tuna" --excerpt 30
if ($LASTEXITCODE) { throw 'Explicit excerpt conversion failed.' }
$bytes = [IO.File]::ReadAllBytes("$fixtures\excerpt.tuna")
if ([BitConverter]::ToUInt32($bytes, 16) -ne 30 * 8192) { throw 'Excerpt duration is not exactly 30 seconds.' }
$truncated = [IO.File]::ReadAllBytes("$fixtures\mono.mp3")
[IO.File]::WriteAllBytes("$fixtures\truncated.mp3", $truncated[0..($truncated.Length - 201)])
& $convert "$fixtures\truncated.mp3" "$fixtures\bad.tuna"
if ($LASTEXITCODE -eq 0) { throw 'Truncated MP3 was not rejected.' }
Write-Output 'PASS: scalar MP3 decode/downmix, deterministic conversion, complete 3/5-minute tracks, adaptive fit, over-limit rejection, explicit excerpt, truncated-input rejection.'
