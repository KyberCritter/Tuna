# Validation and remaining acceptance gates

## Automated host coverage

`make host` runs codec golden vectors, byte-order checks, metadata
CRC/size checks, partial final blocks, exact replay, held-A debounce, paused
sample retention, DATA and COMMIT retransmission, wrong sequences/offsets,
overflow offsets, corrupt final payloads, early-transfer failure, malformed
metadata, unavailable files and 10,000 deterministic malformed-packet cases.
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
