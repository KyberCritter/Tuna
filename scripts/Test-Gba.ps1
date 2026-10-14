[CmdletBinding()]
param(
    [string]$DevkitPro,
    [string]$MsysBash,
    [string]$Mgba = 'C:\Program Files\mGBA\mgba-sdl.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$DevkitPro) { $DevkitPro = "$root\.local\tools\msys64\opt\devkitpro" }
if (!$MsysBash) { $MsysBash = "$root\.local\tools\msys64\usr\bin\bash.exe" }
$gdb = "$DevkitPro\devkitARM\bin\arm-none-eabi-gdb.exe"
foreach ($tool in @($Mgba,$gdb,$MsysBash)) { if (!(Test-Path $tool)) { throw "Missing tool: $tool" } }
function Gdb-Listening { return [bool]([Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners() | Where-Object Port -eq 2345) }
if (Gdb-Listening) { throw 'Port 2345 already in use; refusing to attach to another process.' }
& "$root\Build.ps1" -Target Gba -DevkitPro $DevkitPro -MsysBash $MsysBash
& "$root\scripts\Embed-Binary.ps1" -InputFile "$root\build\host\jingle.tuna" -OutputDirectory "$root\build\gba-fixture" -Symbol diagnostic_track
$cygpath = Join-Path (Split-Path $MsysBash) 'cygpath.exe'
$dkp = (& $cygpath -u $DevkitPro).Trim(); $unixRoot = (& $cygpath -u $root).Trim()
& $MsysBash --login -c "export DEVKITPRO='$dkp' DEVKITARM='$dkp/devkitARM'; cd '$unixRoot/gba' && make -j4 DIAGNOSTIC=1"
if ($LASTEXITCODE) { throw 'Diagnostic ROM build failed.' }
& "$root\scripts\Test-GbaMemory.ps1" -DevkitPro $DevkitPro -Elf "$root\build\gba-diagnostic\tuna.elf" -Image "$root\build\gba-diagnostic\tuna.mb"
$run = "$root\artifacts\gba-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
New-Item -ItemType Directory -Force "$run\config" | Out-Null
$commands = @"
set pagination off
set confirm off
set remotetimeout 10
target remote localhost:2345
break diagnostic_done
continue
printf "TUNA_RESULT=%x\n", diagnostic_result
printf "TUNA_PASSES=%u\n", diagnostic_passes
printf "TUNA_PLAYED=%u\n", audio_played
printf "TUNA_UNDERRUNS=%u\n", audio_underruns
printf "TUNA_PAUSED_AT=%u\n", diagnostic_pause_samples
disconnect
quit
"@
[IO.File]::WriteAllText("$run\commands.gdb", $commands)
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $Mgba; $start.UseShellExecute = $false
$start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
$start.Arguments = '-g -C useBios=0 -C audioSync=0 -C videoSync=1 "' + "$root\build\gba-diagnostic\tuna.mb" + '"'
$start.EnvironmentVariables['APPDATA'] = "$run\config"
$start.EnvironmentVariables['LOCALAPPDATA'] = "$run\config"
$start.EnvironmentVariables['SDL_VIDEODRIVER'] = 'dummy'
$start.EnvironmentVariables['SDL_AUDIODRIVER'] = 'dummy'
$process = [Diagnostics.Process]::Start($start)
$mgbaOut = $process.StandardOutput.ReadToEndAsync(); $mgbaErr = $process.StandardError.ReadToEndAsync()
$passed = $false
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while (!(Gdb-Listening)) {
        if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw "mGBA did not start its GDB server (exited=$($process.HasExited), exit=$($process.ExitCode))." }
        Start-Sleep -Milliseconds 100
    }
    $debugStart = [Diagnostics.ProcessStartInfo]::new()
    $debugStart.FileName = $gdb; $debugStart.UseShellExecute = $false
    $debugStart.RedirectStandardOutput = $true; $debugStart.RedirectStandardError = $true
    $debugStart.Arguments = '--batch "' + "$root\build\gba-diagnostic\tuna.elf" + '" -x "' + "$run\commands.gdb" + '"'
    $debugger = [Diagnostics.Process]::Start($debugStart)
    $stdout = $debugger.StandardOutput.ReadToEndAsync(); $stderr = $debugger.StandardError.ReadToEndAsync()
    if (!$debugger.WaitForExit(40000)) { $debugger.Kill(); $debugger.WaitForExit(); throw 'GBA diagnostic exceeded its 40-second deadline.' }
    $log = $stdout.Result + $stderr.Result
    [IO.File]::WriteAllText("$run\gdb.log", $log); Write-Output $log
    if ($debugger.ExitCode) { throw 'mGBA/GDB diagnostic failed.' }
    if ($log -notmatch 'TUNA_RESULT=50415353' -or $log -notmatch 'TUNA_PASSES=2' -or $log -notmatch 'TUNA_PLAYED=32768' -or $log -notmatch 'TUNA_UNDERRUNS=0') { throw 'GBA runtime counters failed.' }
    $passed = $true
    Write-Output 'PASS: standalone mGBA HLE fixture, timer/DMA runtime counters, pause retention, two exact plays, zero underruns. NOT a BIOS/link/disconnect or captured-audio pass.'
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $emulatorLog = $mgbaOut.Result + $mgbaErr.Result
    [IO.File]::WriteAllText("$run\mgba.log", $emulatorLog)
    if (!$passed) { Write-Output $emulatorLog }
    @{ mgba = (Get-FileHash $Mgba).Hash; rom = (Get-FileHash "$root\build\gba-diagnostic\tuna.mb").Hash;
       passed = $passed; lane = 'standalone HLE embedded fixture'; multiboot = 'not tested'; audioCapture = 'not captured' } |
       ConvertTo-Json | Set-Content "$run\manifest.json"
}
