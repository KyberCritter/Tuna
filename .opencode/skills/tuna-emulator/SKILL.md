---
name: Tuna Local Emulator Testing
description: Test Tuna locally in Windows Dolphin with an integrated GBA, including real BIOS multiboot, Wii SD conversion/cache retries, screenshots, exact retained-RAM comparison, and safe single-instance cleanup. Distinguish these checks from mGBA HLE, audio acceptance and real hardware.
---

# Local emulator workflow

This is the workflow used for the local Dolphin 2609a transfer/cache acceptance.
The scripts automate the mechanical parts, **not visual or listening judgments**.
Read `docs/build.md`, `docs/validation.md`, `docs/development.md`, and `git status
--short` at the project root first. Do not delegate automatically.

## Non-negotiable rules

- **One Dolphin at a time.** Start refuses if any Dolphin is running. Never close
  someone else's emulator. Every script targets the PID/start-time recorded in its
  run manifest. Stop the owned instance in `finally`, including after failures.
  Finish and close one lane before starting or restarting another.
- Use isolated `.local/emulator-runs/` profiles/SD images and ignored `artifacts/`
  evidence. Do not change the user's normal Dolphin/mGBA settings or physical SD.
- Use a legally obtained 16,384-byte GBA BIOS. No download or HLE substitution for
  cartridge-free multiboot. The known GBA hash is checked by default; an explicitly
  supplied different hash permits a separately verified dump, not an arbitrary file.
- Leave all integrated **GBA ROM paths empty**. A preloaded ROM/embedded diagnostic
  fixture is not proof of BIOS boot, runtime transfer or retention after disconnect.
- No generic keyboard injection, desktop clicks or screenshots of the whole screen.
  Disable physical input/hotkeys and use Dolphin's own TAS checkboxes through Windows
  UI Automation. This avoids typing in OpenCode triggering emulator shortcuts.
- Never infer READY from a host SI model, a full progress bar, or a screenshot that
  has not been reviewed. Inspect **both** sender READY and GBA SAFE TO UNPLUG.
- Record exact scope. A four-second fixture is not long-profile performance,
  captured audio, GC SD Gecko, real USB/SD, electrical cable or battery acceptance.
- Commit/push only as authorized and after the relevant emulator checks pass. Do
  not commit profiles, BIOS, generated media, screenshots, SD images or states.

## 1. Build and prerequisites

From the repository root, use the existing regression skill's `Verify.sh` through
the devkitPro/MSYS2 UCRT64 shell, not a hand-reconstructed set of compiler commands:

```sh
sh .opencode/skills/tuna-regression/scripts/Verify.sh -j4
```

Preserve host/link-model/build-tool results, maps and image/heap/IWRAM IRQ checks.
Generate original MP3 fixtures with the existing helper (a real FFmpeg executable):

```powershell
powershell.exe -NoProfile -File scripts/Test-Mp3.ps1 -Ffmpeg 'D:\ffmpeg-8.0.1-full_build\bin\ffmpeg.exe'
```

When redirecting expected native stderr under Windows PowerShell, use a child
process, e.g. `cmd.exe /c 'powershell.exe ... > artifacts/mp3.log 2>&1'`; otherwise
the expected over-limit/truncated-input rejection can be promoted to a terminating
PowerShell error. Inspect exit status **and** the final PASS line.

Run the script helper self-tests without launching an emulator:

```powershell
& .opencode/skills/tuna-emulator/scripts/Test-Helpers.ps1
```

These are explicitly helper tests, not emulator acceptance. Default tools are
`D:\Dolphin\Dolphin.exe` and project-local `.local/bios/gba_bios.bin`; supply
`-Dolphin` / `-Bios` for another machine. Start records tool and binary hashes.

## 2. GameCube BIOS/link lane

```powershell
$s = '.opencode/skills/tuna-emulator/scripts'
$run = & "$s/Start-Dolphin.ps1" -Platform gamecube
try {
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action OpenTas
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action Screenshot -Target Sender -Name browser
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action Button -Target GC -Button A
    # Capture/review preview; it must NOT have booted/sent on the first press.
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action Screenshot -Target Sender -Name preview
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action Button -Target GC -Button A
    # Allow bounded time for boot/transfer, then inspect BOTH screenshots.
    Start-Sleep -Seconds 12
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action Screenshot -Target Sender -Name ready-host
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action Screenshot -Target GBA -Name ready-gba
    & "$s/Control-Dolphin.ps1" -RunDirectory $run -Action ExportState -Name retained
    & "$s/Assert-RetainedTrack.ps1" -State "$run/retained.ss0" -Track build/host/jingle.tuna -Output "$run/retained-match.json"
} finally {
    & "$s/Stop-Dolphin.ps1" -RunDirectory $run -Force
}
```

There must be port-2 integrated GBA BIOS boot, application HELLO/CAPS, jingle title,
sender CRC/READY and GBA SAFE TO UNPLUG. Preserve screenshots and exact RAM match.
`ExportState` only exports; it does not reset/load a ROM or destroy the core.
Current byte verification supports **raw mGBA state v9**, track at EWRAM
`0x02010000` (state offset `0x31000`). Other formats fail; inspect their layout
rather than guessing or doing an unconstrained byte search.

## 3. Native Wii SD/cache and failed-send lane

Close the GC run first. Start with the generated four-second mono fixture:

```powershell
$run = & "$s/Start-Dolphin.ps1" -Platform wii -Mp3Fixture build/mp3-tests/mono.mp3
```

The script prepares `music/mono.mp3` and `tuna/stored-jingle.tuna`, enables FAT SD
folder synchronization for the **first** boot, and records the fixture hash. It
uses a private 128 MiB image; this is not a GC SD Gecko image or a physical SD test.
Use `OpenTas`. The presence of Wii TAS input and the sender's native-Wii heading
help verify this is actually Wii mode.

Perform these actions with `Control-Dolphin.ps1 -Action Button -Target GC`:

1. Down selects `sd:/music/mono.mp3`. X selects the deliberately wrong port 3.
2. A opens conversion. Allow up to 60 seconds for this fixture, taking/reviewing a
   screenshot after work has finished. Require **Saved conversion: sd:/tuna/...**
   before confirmation and a valid four-second preview. Do not assume that an
   arbitrary sleep means conversion completed. Long songs can take minutes.
3. A confirms. Wait up to 20 seconds; screenshot the wrong-port timeout and **No
   READY**. This is a boot/link failure test, not a disconnect-during-DATA test.
4. B returns to browser. X twice cycles 3 -> 4 -> 2. A opens the same MP3. Require
   **Using completed conversion from sender RAM**, no MP3 conversion, and no send
   until a second A press. Use `-Milliseconds 500` to exercise a held opening A.
5. A confirms. Inspect sender/GBA READY. Export the GBA state; compare the full
   container against `build/mp3-tests/mono.tuna`, not just title/duration/CRC text.
6. Stop in `finally`. Then cold-restart the same image with the newly built binary:

   ```powershell
   & "$s/Stop-Dolphin.ps1" -RunDirectory $run -Force
   $run = & "$s/Start-Dolphin.ps1" -RestartRun $run
   ```

   Restart disables folder-to-image synchronization so the original fixture folder
   cannot replace the image's newly saved conversions. The image, not the host
   source folder, is authoritative. A forced emulator exit may skip folder export;
   do not interpret the absence of a host-folder copy as absence from guest FAT.
7. Open TAS; Down selects MP3, A must show **Using saved conversion: sd:/tuna/...**.
   Held A must leave the preview waiting. Separate A must reach READY. B returns;
   B rescans; browse and screenshot the saved `.tuna` entry (normally Down twice).
   Selection order is not guaranteed: inspect the displayed path before acting.
8. Finish and close this instance. Recheck that no owned emulator remains.

Also test excerpt/full keys, changed source contents, corrupt cache, missing/full/
read-only SD and RAM fallback when changes touch those cases. Host tests cover
boundaries; they do not prove actual guest FAT/device behavior.

## 4. Disconnect and playback acceptance (same live GBA)

After READY, `-Action Connected -Connected $false` invokes the integrated GBA's
**Connected** menu toggle. Do NOT remove the SI device, unload ROM, Reset, stop
Dolphin, or start another ROM while measuring retention.

Use `-Action Button -Target GBATas -Button A`; preserve title and play-to-end
screens. Repeat A for replay; test pause/resume and held-A debounce. Export state
again if comparing retained bytes after disconnect. Reconnect via `-Connected $true`
only when needed. For DATA/ACK failure tests, toggle Connected during the respective
transfer phase and require error/timeout without READY or incomplete playback.

**A displayed 0:04 end is not exact sample count or audible output.** Full audio
acceptance additionally needs validated GBA audio capture/loopback, two complete
plays, pause/resume timing, no clicks, exact final samples, zero underruns and DMA/
FIFO bounds/deadlines measured with compatible counters/debugging. No audio capture
script was used in the accepted transfer/cache run; do not invent a passed lane.
GC SD Gecko, long low-rate profiles, standalone-state handoff and real hardware
remain separate checks in `docs/validation.md`.

## 5. Standalone mGBA is a separate lane

Use root `scripts/Test-Gba.ps1 -Mgba <verified executable>` for the existing isolated
embedded-fixture/HLE/GDB counter harness. Installed mGBA SDL 0.10.5 failed to expose
port 2345 in the recorded run. Missing listener means **blocked/failed**, not pass.
Even success is only the script's stated counter/HLE lane, not BIOS multiboot,
transferred RAM, real disconnect or captured audio. The script owns/cleans up its
child process. Do not change normal mGBA profiles, download BIOS, or silently swap
in an embedded ROM when Dolphin's real boot gate is unavailable.

## 6. Evidence, result and cleanup

- Start/restart manifests record PID, process start time, platform, empty ROMs, port,
  BIOS/emulator/DOL/player/fixture hashes, revision and working-tree status.
- Control appends JSONL actions; screenshots, raw GBA states and comparisons stay
  in the run directory. A screenshot can fail/contain black or stale output: inspect
  it; `PrintWindow` success is not image/content validation.
- `Stop-Dolphin.ps1` tries WM_CLOSE with confirmation disabled. `-Force` permits
  terminating **only the recorded owned instance** if graceful shutdown stalls.
  PID reuse or a changed process executable/start-time is a hard refusal.
- Record reviewed gates explicitly, with evidence and limits, for example:

  ```powershell
  & "$s/Record-Result.ps1" -RunDirectory $run -Gate cache-restart -Status passed `
    -Evidence final-saved-preview.png,saved-track-browser.png `
    -Notes 'Reviewed saved cache reuse and browser path; no audio claim.'
  ```

  Unrecorded gates remain `not-run`. `fullEmulatorAcceptance` is true only when
  **all** listed emulator gates are explicitly passed; hardware is never inferred.
- For source changes, rerun the relevant emulator lane against the **final binary**,
  compare its hash, preserve maps/build logs, then commit only intended files and
  push only if authorized. Never say "everything works" when audio, GC storage,
  timing, long profiles or hardware gates are still open.

## Troubleshooting learned locally

- Web/Playwright tools do not control native Dolphin windows.
- `CopyFromScreen` captured the wrong desktop region under DPI scaling/focus loss.
  Use process-owned `PrintWindow(..., 2)` with DPI awareness; review the PNG.
- Foreground/key injection was unreliable; TAS + UI Automation worked. Ensure
  emulation is running (`Resume` checks for a Play toolbar button); modal dialogs or
  paused emulation make button tests misleading. Never type blind keys to dismiss.
- Enumerate HWNDs every time, not hard-coded IDs from old runs. Qt menu popups are
  separate top-level windows. Native file-dialog recursive UI Automation hung;
  state export uses bounded Win32 edit/Save messages instead.
- State export's Invoke can time out even though the dialog opened. Check the owned
  dialog before retrying, not another emulator launch.
- Do not leave finished Dolphin instances open. This rule applies even when a test
  is interrupted or when a new session continues the work.
