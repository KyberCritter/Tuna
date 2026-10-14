# Executable local development plan (Windows)

Implementation update: console sources, adaptive five-minute conversion, host
tests and the single `Build.ps1` entry point now exist. Managed devkitPro tools
were provisioned without admin changes under `.local/tools/msys64/opt/devkitpro`;
all three cross-builds pass. Dolphin was found at `D:\Dolphin\Dolphin.exe`.
The original observations below are retained as historical setup context.
See [current execution evidence and open gates](validation.md); no BIOS/link/audio
acceptance is implied by the successful builds.

## Observed machine, 2026-10-14

- Windows x64, Ryzen 7 5800H, 16 logical processors, about 32 GiB RAM,
  NVIDIA RTX 3070 Laptop GPU. Expected ample performance; not yet benchmarked.
- Git and PowerShell available. Standard PATH has no make/CMake/compiler.
- `C:\Program Files\mGBA\mGBA.exe` and `mgba-sdl.exe` exist. Installed-file
  metadata reports mGBA 0.10.5.0. This is not a verified scripting build, BIOS
  configuration, or link test.
- No Dolphin/devkitPro found in the checked standard locations. They could exist
  elsewhere; this was not a whole-machine scan.
- Approximately 3.4 GB free on C: versus 104.7 GB on D: at inspection time.
  Put new tools, caches, SD images, profiles, and recordings on D: where possible.
- Python PATH entries point to Windows application aliases; do not count that as
  a working Python interpreter. The proposed orchestration needs only PowerShell
  plus devkitPro's supplied MSYS2 shell/Make.

## Provisioning layout

Use official Windows Dolphin x64 binary with integrated mGBA enabled, the existing
standalone mGBA (or a pinned portable build), and official devkitPro installation
with `gba-dev`, `wii-dev`, and `gamecube-dev`. Install devkitPro to
`D:\devkitPro` if the installer supports the selected location; keep its managed
MSYS2 environment rather than mixing arbitrary system GCC packages.

Proposed Git-ignored project paths:

```text
.local/tools/dolphin/       official extracted runtime; portable.txt when appropriate
.local/dolphin-user/        isolated Dolphin --user profile, logs, NAND/SD settings
.local/bios/gba_bios.bin    user-supplied, legally obtained BIOS dump
.local/mgba/               separate test profile/state files
.local/sd-root/music/       original synthetic audio fixtures
artifacts/<run-id>/         logs, screenshots, audio capture, checksums, reports
.agents/docs/              all downloaded research/reference files
```

Do not overwrite the user's existing mGBA settings. Select portable/separate
configuration for tests. No commercial ROM is needed: use self-built homebrew
and original generated test tones/jingles. Do not download Nintendo BIOS files.
For cartridge-free boot tests, provide the actual 16,384-byte GBA BIOS dumped
from owned hardware; HLE replacement behavior is not a substitute for verifying
the proprietary Joybus multiboot path. Record a local checksum; size alone does
not establish BIOS validity. Dumping can itself require temporary homebrew/cart
access: arrange this before treating the automated pipeline as unblocked.

Record tool versions, archive hashes, source revisions, package inventory, and
configuration with every accepted run. Pin an actual tested Dolphin/mGBA pair;
"latest" is a discovery mechanism, not a reproducible version specification.

## Primary emulator: Dolphin with integrated mGBA

Use one process for synchronized Wii/PowerPC and GBA emulation. This executes the
real sender and receiver binaries through emulated SI/Joybus, not a mocked
network file transfer. Start at native resolution, 100% speed, no cheats or timing
overrides. Try D3D11/Vulkan; correctness does not require graphical enhancements.

Configure in the isolated Dolphin profile:

1. Port 1 standard emulated GC controller for sender UI, port 2 **GBA (Integrated)**.
2. Set GBA BIOS path. Leave the GBA ROM path EMPTY for the production multiboot
   test: preloading a cartridge ROM would bypass the very feature being tested.
3. Map GBA A separately from the Wii/GC sender's A (e.g. Z vs Enter).
4. Disable pause-on-focus-loss. Keep the emulated Wii running throughout the test.
5. Configure virtual SD insertion and folder synchronization if the chosen release
   supports it; otherwise build a FAT32 SD image. Populate a deterministic music
   folder before boot and keep synchronization one-directional/controlled.
6. Boot the **GameCube-target** DOL first: the user accepts GC homebrew and this
   exercises the emulator's established GBA-link use case. For real MP3 filesystem
   tests, configure an emulated SD Gecko on a memory-card/EXI slot with a FAT image
   (not an ordinary emulated GC memory card). Wii virtual SD insertion alone does
   NOT provide GC storage. An embedded synthetic fixture is only a first link test.
   Also build/boot the Wii-target DOL when adding Wii SD/USB input and verify its
   mode through guest diagnostics. A GC pass does not prove Wii-mode support.
7. First run a ping/pong homebrew test, then BIOS multiboot, then full track transfer.

Source inspection supports the link path: SI creation is shared, GBACore uses the
system's timer frequency rather than hardcoding GameCube speed, supports no-ROM
BIOS boot, and forwards Joybus commands. HOWEVER Wii-mode integration on the
chosen release remains an explicit smoke-test gate, not an observed pass here.
Likewise SD emulation does not validate physical USB storage or drive behavior.

### Disconnect test: do not destroy the GBA instance

After READY, right-click the integrated GBA screen and toggle **Connected** off.
Current Dolphin source calls `SetForceDisconnect`: it suppresses Joybus responses
but continues scheduling GBA emulation. This preserves RAM and is the correct
software analogue of pulling the cable. Do NOT change port 2 to None, reset,
unload, close Dolphin, or stop Wii emulation: those can destroy/reset the GBA core.

Verify continuous audio, title retention, A pause/resume, end-of-track and restart
for at least two complete passes, while no Joybus data is available. Repeat with
disconnect during transfer and verify a clean error/timeout instead of playback.

To test independence after the *Wii emulator exits*, export a GBA-only state from
the integrated GBA context menu, boot the same no-cartridge BIOS configuration in
standalone mGBA, and import that state without loading a new ROM/resetting RAM.
Dolphin documents mGBA state interchange, but version compatibility and no-ROM
state import must be proven for the pinned builds. A passing in-process Connected
toggle test is not proof that export/import works. If unsupported, use a small
libmgba runner that accepts the exported raw state, or the TCP fallback below.
Never substitute a newly booted ROM with embedded audio and call that retention.

## Secondary emulator: standalone mGBA

- Use first for player audio/UI, frame stepping, memory inspection and GDB.
- Modern mGBA supports **Connect to Dolphin**; select **GBA (TCP)** in Dolphin
  for that lane and connect via loopback, not Wi-Fi. This mode needs BIOS setup.
- Test disconnect while preserving and resuming the same mGBA instance. TCP
  scheduling/disconnect behavior varies; explicitly validate that it resumes
  standalone and is not stuck waiting on the host. Do not claim it is equivalent
  to the synchronized integrated core without tests.
- Lua scripting can drive player buttons and inspect counters in builds that
  enable it. Verify that feature before promising headless scripted mGBA tests.

## What this agent can execute

PowerShell can install approved portable tools, invoke devkitPro's shell, build
both targets, launch Dolphin with an isolated profile and `--exec` DOL, collect
process exit status and parse guest test logs. Future scripts should fail on
missing BIOS/tools rather than bypassing boot or using mocks unnoticed.

Desktop UI automation is NOT supplied by the web browser/Playwright tools: those
control browser pages, not Dolphin's native windows. For repeatable control of
native dialogs, GBA Connected toggle and keyboard input, use Windows UI Automation
where Qt exposes controls, with a local Win32 input/screenshot helper as fallback.
Check active desktop/session and window focus, and save evidence of each action.
Start with supervised setup. Full unattended GUI automation is not implemented.

For robust automated regression, add a narrow test mode to our OWN homebrew:
generated audio fixture, known expected hashes, deterministic protocol transcript,
playback sample counter, underrun counter, and a timeout/PASS record. Expose logs
through a verified Dolphin-compatible channel (e.g. OSReport if enabled by the
selected build); do not assume stderr captures all guest output. Host codec tests
do not replace emulator audio/DMA execution. Capture GBA audio using validated
emulator output or Windows loopback/WAV recording and inspect duration, silence,
discontinuities, and pause/resume. Confirm screen title via screenshot/OCR or RAM.

If native UI automation/state handoff is unreliable, an optional small Dolphin
test-build patch can expose **existing** SetForceDisconnect/log/state operations
to a local harness. Keep actual SI, BIOS, RAM and audio behavior unchanged. This
is a contingency with build cost, not a pre-existing stock Dolphin CLI feature.

## Acceptance matrix and status

| Capability | Execution lane | Present status |
| --- | --- | --- |
| Decode/resample/encode and format validation | Host tests + GC/Wii DOL | Host + real MP3 fixtures passed; DOL runtime pending |
| GBA title, A controls, FIFO/DMA playback | Standalone mGBA + integrated | Source-supported, untested |
| Wii DOL, emulated SD MP3 input | Dolphin | Untested locally |
| GC DOL, emulated SD Gecko MP3 input | Dolphin | Accepted initial target, untested |
| No-cartridge Joybus multiboot | Dolphin integrated, user BIOS | BIOS/toolchain prerequisite |
| Checksummed runtime transfer | GC/Wii DOL + integrated GBA | Implemented/cross-built; integrated run needs BIOS |
| Cable removal retaining same RAM | Integrated Connected toggle | Verified in source, untested locally |
| Playback after Wii process exits | mGBA state handoff / TCP | Version-dependent, unproven |
| Physical unplug, battery and cable timing | Real Wii/GBA | Hardware-only acceptance |
| Actual SD/USB/optical compatibility | Real Wii | Hardware-only acceptance |

The emulator gate is complete only when original test jingle bytes traverse
GC SD Gecko or Wii SD MP3 input -> conversion -> multiboot -> GBA RAM -> unplug simulation ->
audible replay with A controls, with hashes/logs/audio/screenshots preserved.
No end-to-end emulator result is claimed yet, and no emulator proves electrical
or physical-drive behavior.

## Real-Wii rapid iteration

Initial deployment: SD `/apps/tuna/boot.dol` plus metadata, launched with HBC.
Routine deployment: keep Wii in HBC, set `WIILOAD=tcp:<wii-ip>` in the build
shell and send the new DOL with Wiiload. Return cleanly to HBC on exit.
Embed the freshly built GBA multiboot payload in the Wii DOL so the two versions
cannot drift. Keep a version/protocol identifier visible on both screens.
Network logging is optional, bounded and disabled for timing measurements.
Never install a system-menu channel/WAD just for iteration.

For a GC-native DOL on Wii, Wiiload/HBC is NOT a magical GameCube-mode loader.
Use an already configured trusted GameCube homebrew boot path, such as Swiss
via the user's existing compatible loader, and load the GC DOL/data from SD Gecko.
Swiss/network upload availability depends on its actual devices and configuration.
Do not assume Dolphin's ease of directly booting GC DOLs transfers to real HBC.
Prefer the native Wii build for the tightest real-Wii Wiiload iteration loop.
