# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [ValidateSet('gamecube','wii')][string]$Platform = 'gamecube',
    [string]$Dolphin = 'D:\Dolphin\Dolphin.exe',
    [string]$Bios,
    [string]$ExpectedBiosSha256 = 'fd2547724b505f487e6dcb29ec2ecff3af35a841a77ab2e85fd87350abd36570',
    [string]$Mp3Fixture,
    [string]$RestartRun
)
. "$PSScriptRoot/Common.ps1"
$root = Get-TunaRoot
if (Get-Process Dolphin -ErrorAction SilentlyContinue) { throw 'Dolphin is already running. Close the owned prior test or ask the user; never start a second instance.' }
if ($RestartRun) {
    $run = (Resolve-Path -LiteralPath $RestartRun).Path
    $previous = Read-TunaRun $run
    $Platform = $previous.platform; $Dolphin = $previous.dolphin; $Bios = $previous.bios
    $ExpectedBiosSha256 = $previous.biosSha256; $local = $previous.localDirectory
    if ($Platform -eq 'wii' -and !(Test-Path "$local/sd.raw")) { throw 'Restart requires the original SD image.' }
} else {
    $stamp = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
    $run = Join-Path $root "artifacts/emulator-$stamp"
    $local = Join-Path $root ".local/emulator-runs/$stamp"
}
if (!$Bios) { $Bios = Join-Path $root '.local/bios/gba_bios.bin' }
$Dolphin = (Resolve-Path -LiteralPath $Dolphin).Path; $Bios = (Resolve-Path -LiteralPath $Bios).Path
$dol = Join-Path $root "build/$Platform/tuna.dol"; $player = Join-Path $root 'build/gba/tuna.mb'
foreach ($file in @($dol,$player)) { if (!(Test-Path -LiteralPath $file)) { throw "Build prerequisite missing: $file" } }
if ((Get-Item -LiteralPath $Bios).Length -ne 16384 -or (Get-FileHash -LiteralPath $Bios).Hash -ne $ExpectedBiosSha256) { throw 'BIOS size/hash does not match the verified dump.' }
New-Item -ItemType Directory -Force $run,"$local/user/Config" | Out-Null
$sdSettings = ''; $fixtureHash = $null
if ($Platform -eq 'wii') {
    if (!$RestartRun) {
        if (!$Mp3Fixture) { $Mp3Fixture = Join-Path $root 'build/mp3-tests/mono.mp3' }
        $Mp3Fixture = (Resolve-Path -LiteralPath $Mp3Fixture).Path
        New-Item -ItemType Directory -Force "$local/sd-folder/music","$local/sd-folder/tuna" | Out-Null
        Copy-Item -LiteralPath $Mp3Fixture -Destination "$local/sd-folder/music/mono.mp3"
        Copy-Item -LiteralPath (Join-Path $root 'build/host/jingle.tuna') -Destination "$local/sd-folder/tuna/stored-jingle.tuna"
        $fixtureHash = (Get-FileHash -LiteralPath $Mp3Fixture).Hash
    } else { $fixtureHash = $previous.fixtureSha256 }
    $sync = if ($RestartRun) { 'False' } else { 'True' }
    $sdSettings = "WiiSDCard = True`nWiiSDCardEnableFolderSync = $sync`nWiiSDCardAllowWrites = True`nWiiSDCardFilesize = 134217728"
}
$profile = "$local/user"
$ini = @"
[Core]
SIDevice0 = 6
SIDevice1 = 13
$sdSettings
[General]
ISOPaths = 0
WiiSDCardPath = $($local.Replace('\','/'))/sd.raw
WiiSDCardSyncFolder = $($local.Replace('\','/'))/sd-folder
[Interface]
PauseOnFocusLost = False
ConfirmStop = False
[GBA]
BIOS = $($Bios.Replace('\','/'))
Rom1 =
Rom2 =
Rom3 =
Rom4 =
[Analytics]
Enabled = False
PermissionAsked = True
"@
[IO.File]::WriteAllText("$profile/Config/Dolphin.ini",$ini)
[IO.File]::WriteAllText("$profile/Config/GCPadNew.ini","[GCPad1]`nDevice = None/0/Test`n")
[IO.File]::WriteAllText("$profile/Config/Hotkeys.ini","[Hotkeys]`nDevice = None/0/Test`n")
$manifest = [ordered]@{
    utc=[DateTime]::UtcNow.ToString('o'); platform=$Platform; port=2; rom='empty'; localDirectory=$local;
    dolphin=$Dolphin; dolphinSha256=(Get-FileHash -LiteralPath $Dolphin).Hash; bios=$Bios; biosSha256=(Get-FileHash -LiteralPath $Bios).Hash;
    dol=$dol; dolSha256=(Get-FileHash -LiteralPath $dol).Hash; playerSha256=(Get-FileHash -LiteralPath $player).Hash;
    fixtureSha256=$fixtureHash; revision=(git -C $root rev-parse HEAD); workingTree=@(git -C $root status --short);
    processId=0; processStartUtc=''; status='starting'; emulatorAcceptance='not run'; hardwareAcceptance='not run'
}
$manifestPath = Join-Path $run 'manifest.json'
if ($RestartRun) { Copy-Item $manifestPath (Join-Path $run ('manifest-before-restart-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.json')) }
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath
$p = $null
try {
    $p = Start-Process -FilePath $Dolphin -ArgumentList @('--user',('"'+$profile+'"'),'--exec',('"'+$dol+'"')) -PassThru
    $manifest.processId=$p.Id; $manifest.processStartUtc=$p.StartTime.ToUniversalTime().ToString('o')
    $manifest.status='running'; $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        if ($p.HasExited) { throw 'Dolphin exited during startup.' }
        $windows = @([TunaEmulator.Native]::Windows($p.Id))
        if (@($windows | Where-Object title -match '^Dolphin .*\|').Count) { break }
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Timed out waiting for the emulator render window.' }
        Start-Sleep -Milliseconds 100
    } while ($true)
    Write-TunaAction $run 'start' @{platform=$Platform;restart=[bool]$RestartRun;processId=$p.Id}
    $run
} catch {
    if ($p -and !$p.HasExited) { $p.Kill(); $p.WaitForExit() }
    $manifest.status='startup failed'; $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath
    throw
}
