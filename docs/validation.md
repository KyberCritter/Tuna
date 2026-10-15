# Validation and remaining acceptance gates

## Automated host coverage

`make host` runs codec golden vectors, byte-order checks, metadata
CRC/size checks, partial final blocks, exact replay, held-A debounce, paused
sample retention, DATA and COMMIT retransmission, wrong sequences/offsets,
overflow offsets, corrupt final payloads, early-transfer failure, malformed
metadata, unavailable files and 10,000 deterministic malformed-packet cases.
Storage tests cover validated cache publication/loading, source-content changes,
separate excerpt keys, failed-transfer retention, corrupt/truncated cached files,
and unavailable output directories. A separate host SI model executes production
`sender/link.c` through boot-to-application handoff, CRC retransmission, replacement,
malformed replies and failed DATA transfers; this model is not emulator evidence.
FIR tests check DC preservation and suppression of a 12 kHz alias at 48 kHz input.
The retained-RAM model completes replay without any receiver/link call.

`scripts/Test-Mp3.ps1` generates original mono/stereo MP3 tones with FFmpeg,
checks deterministic scalar decode/conversion, complete-track fit rejection,
complete 3/5-minute tracks with automatic profiles, an exact explicit 30-second
excerpt, and truncated-input rejection. Run it with
an actual FFmpeg executable. CI also runs GCC/Clang with address/undefined-behavior
sanitizers on Linux/macOS and a Windows MSYS2 GCC lane. CI results are separate
from local results. The MIT C build helper also checks SHA-256 golden vectors
and rejects incorrect GBA track/IRQ addresses and linked heap functions.

## Do not claim emulator/hardware acceptance from host tests

The implementation needs the following measured checks. See the detailed emulator
configuration and disconnect rules in `docs/development.md`.

1. Build all console targets; inspect map files, NOLOAD track address and image
   sizes. Verify a 192 KiB transfer cannot overlap resident code or IWRAM stack.
2. Standalone mGBA can boot the resident player for UI inspection, but it waits for
   a transferred track. This is not a multiboot/disconnect pass.
3. Supply a legally dumped 16,384-byte GBA BIOS locally. Use Dolphin's isolated
   profile, port 2 GBA Integrated, ROM path **empty**, port 1 controller.
4. Boot the GC DOL, select original jingle, verify BIOS boot, HELLO/CAPS and GBA
   READY. Record host CRC and exact GBA retained container bytes.
5. Capture GBA output audio and screen. Verify two complete plays, title, pause /
   resume, exact final sample count, zero underruns, no boundary clicks, and no
   DMA out-of-range reads. Check the timer-IRQ FIFO reset/reprime deadline in
   particular: software simulation does not prove its hardware timing.
6. Toggle **Connected** off without deleting/resetting the integrated GBA core.
   Repeat A controls and replay on the same RAM. Disconnect during DATA and during
   ACK; require a timeout/error, never READY or playback of incomplete bytes.
7. Test filesystem MP3 through emulated GC SD Gecko, then Wii-native SD. An embedded
   jingle is a link test, not proof of storage/MP3 integration. Record matching
   portable/Wii/GC conversion vectors, accounting for floating-point rounding.
8. Validate standalone state handoff separately if playback after Dolphin exits
   is required. Do not use a newly booted ROM as evidence of RAM retention.
9. Repeat on real RVL-001/GBA: electrical cable timing, battery-powered physical
   unplug, actual SD/USB behavior, FIFO/DMA deadlines and pause/end click checks.

## Codec feasibility gate

The short-track profile is mono IMA ADPCM at 8192 Hz. Fixed block framing
and the 96-byte header leave 1488 blocks: **46.5 seconds maximum for that profile**. A 4-second
original jingle fits at 16,992 bytes. A 30-second explicit excerpt uses 126,816
bytes. No commercial music or BIOS is included.

Three-minute music needs approximately 8.74 kbit/s or less, including overhead;
four minutes needs approximately 6.55 kbit/s. This profile is about 33.8 kbit/s.
Automatic custom 2-bit ADPCM/4096 Hz fits a complete three-minute track in 195,936
bytes. Adaptive 1-bit delta/4096 Hz fits five minutes in 172,896 bytes. Host tests
validate encoded fit, exact replay and complete MP3 conversion, including encoder
delay/padding. The typical three-minute planning target is now required; a short
jingle alone is insufficient. These profiles sacrifice bandwidth and fidelity.
The 220 Hz tone diagnostic measures codec error, not music acceptance.

No low-rate profile has passed measured ARM7 worst-case cycle or representative
music-listening tests yet. Full-length **quality/performance acceptance** remains
open despite implemented duration/size support. Do not advertise speech codecs,
Opus, or 3-bit ADPCM as alternatives without measurements or change RAM-only
storage silently.

## Evidence

Keep BIOS, generated fixtures, emulator profiles, audio, logs and screenshots
under ignored `.local/`, `build/`, or `artifacts/`. Record tool/compiler versions,
package inventory, binary hashes, ROM-empty profile and SI port with every run.
`scripts/Start-Dolphin.ps1` creates a fresh isolated profile and diagnostic manifest;
it requires BIOS and built DOL rather than bypassing boot. It does not automate
native Dolphin dialogs, controller input, audio capture or Connected toggles.

## Local execution record (2026-10-14)

- MSVC host regression: 15,463 checks passed, including a complete 300-second
  low-rate track and exact sample-count replay. Codec tone SNR is diagnostic only.
- FFmpeg 8.0.1 generated original mono/stereo and complete 180-/300-second MP3
  fixtures. Conversion, gapless duration, selected profiles, 301-second rejection,
  deterministic output, explicit excerpts and truncated-input rejection passed.
- devkitARM r68 / GCC 16.1.0, devkitPPC r51 / GCC 16.2.0, libgba 0.5.4,
  libogc 3.1.0: all three targets cross-built. Production player: 9316 bytes;
  track 0x02010000..0x02040000 NOLOAD, audio boundary IRQ in IWRAM, no heap symbols.
- GitHub host CI passed for GCC/Clang sanitizers and Windows MSVC.
- Build portability follow-up: the root GNU Makefile now drives every target
  without Python/PowerShell. Windows MSYS2 UCRT64 GCC 16.2 built/tested/packaged all
  targets; 15 additional C build-tool checks passed, and every package digest
  matched Windows' independent `Get-FileHash` implementation. CI now exercises
  Make on Linux, macOS and Windows, using verified current action SHA pins.
- Optional embedded-fixture standalone runtime harness compiled, but installed
  mGBA SDL 0.10.5 (`26b7884b`) did not expose port 2345. That runtime lane failed
  its prerequisite gate. No audio/counter/disconnect pass is asserted.
- User BIOS is absent. Dolphin no-cartridge multiboot/runtime transfer and real
  cable/audio/storage/listening-quality acceptance remain unexecuted.

## Transfer/cache follow-up (2026-10-14 local / 2026-10-15 UTC)

- All console targets rebuilt; host format/storage suite passed 15,495 checks,
  production sender SI-model suite passed 160,613 checks, and build-tool suite
  passed 15 checks. FFmpeg MP3 regressions passed again. Resident player is 9332
  bytes; the existing memory/heap/IWRAM IRQ gates pass.
- A user-supplied BIOS matched the known GBA dump. Dolphin 2609a, isolated profiles,
  port 2 integrated GBA and **empty GBA ROM paths** booted the production GameCube
  sender/player and transferred the original jingle to READY (CRC `1089c2b8`).
- The native Wii build read a generated four-second MP3 from an emulated FAT SD
  image and saved its completed container under `sd:/tuna/` before confirmation.
  An intentional wrong-port attempt timed out without READY. Retrying on port 2
  reused sender RAM rather than converting again and reached READY (`b315346b`).
- An exported integrated GBA state contained an exact match of all 16,992 track
  bytes against the desktop converter output, SHA-256
  `779ce28e87e6039a64108f4c814bfbafe3dcc854310ac05d318a3418a2a48450`.
  After disabling Connected on the same GBA core, A played the retained track to
  the displayed four-second end. No captured-audio, exact runtime sample counter,
  underrun, pause/resume or two-play acceptance is claimed from this screen check.
- The final rebuilt Wii binary reused the saved conversion after a cold app restart
  against the same SD image, displayed the saved `/tuna/` file in its browser, and
  waited for a separate A confirmation even when the preview-opening A was held.
  The subsequent confirmed send reached READY. Dolphin instances were closed.
- Evidence is ignored under `artifacts/dolphin-20261014-213745/` and
  `artifacts/dolphin-cache-20261014-2200/`, with screenshots, BIOS/binary hashes and
  retained-byte comparison. Final full-build evidence is
  `artifacts/20261015T015235Z-4041/`; MP3 output is `artifacts/mp3-cache-followup.log`.
  GC SD Gecko, real SD/USB/cable hardware, captured audio, disconnect during DATA/ACK,
  and low-rate profile quality/performance gates remain open. The historical BIOS
  absence above no longer blocks the locally tested BIOS/link lanes.
