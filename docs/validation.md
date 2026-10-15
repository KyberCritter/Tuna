# Validation and remaining acceptance gates

## Automated host coverage

`make host` runs codec golden vectors, byte-order checks, metadata
CRC/size checks, title/artist/album framing, ID3v1/ID3v2 extraction, sample-accurate
seek, partial final blocks, exact replay, held-A debounce, paused
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
5. Capture GBA output audio and screen. Verify two complete plays, title and the
   artist/album (tagged fixture), codec/rate, progress bar, pause / resume, L/R
   seek and B restart landing on the expected time, exact final sample count, zero
   underruns, no boundary/seek clicks, and no DMA out-of-range reads. Check the
   timer-IRQ FIFO reset/reprime deadline in particular: software simulation does
   not prove its hardware timing.
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
and the 160-byte header leave 1488 blocks: **46.5 seconds maximum for that profile**. A 4-second
original jingle fits at 17,056 bytes. A 30-second explicit excerpt uses 126,880
bytes. No commercial music or BIOS is included.

Three-minute music needs approximately 8.74 kbit/s or less, including overhead;
four minutes needs approximately 6.55 kbit/s. This profile is about 33.8 kbit/s.
Automatic custom 2-bit ADPCM/4096 Hz fits a complete three-minute track in a
196,000-byte container. Adaptive 1-bit delta/4096 Hz fits five minutes in 172,960 bytes. Host tests
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

## Conversion performance follow-up (2026-10-15 UTC)

- The bounded repeating-phase FIR cache passed 28,074 additional exact scalar
  comparison/budget/fallback checks, including track edges and 300-second indices
  at standard MP3 input rates. Total host format/storage/FIR suite: 43,569 checks.
- All generated MP3 regression containers matched their pre-optimization SHA-256
  values, including complete 180/300-second tracks and the explicit excerpt. The
  180-second stereo host benchmark improved from 4.442 s to 0.877 s (5.07x) with
  exactly the same output SHA-256. This is a local single-run host measurement,
  not a claimed real-Wii benchmark.
- Dolphin's native Wii build converted the generated four-second mono MP3 in
  0.869 guest-clock seconds, saved it to SD `/tuna/`, and reached GBA READY with
  the unchanged payload CRC `b315346b` and title/duration. Evidence:
  `artifacts/emulator-20261014-222646-5dd79b7d/` and
  `artifacts/conversion-baseline/`. The state-export helper failed its context-menu
  step in that run; no new retained-byte comparison is claimed for that attempt.
- All console targets and memory/heap/IWRAM IRQ gates pass. This optimization does
  not alter the GBA audio engine or close the audible-output acceptance gates.

## Playback level and continuity follow-up (2026-10-15 UTC)

Reported symptom: GBA output was crackly and extremely quiet on real hardware.
Measured causes and changes:

- Level: a **loud** source is reproduced faithfully - a 0.9-amplitude MP3 decodes to
  peak 32768, so the container/decoder chain does not attenuate. The quiet cases were
  genuinely low-amplitude content (the embedded jingle peaks at 10798/32768 = -9.6 dB;
  FFmpeg's `sine` fixtures peak at -18 dB). Playback now derives a bounded output gain
  from the container's decoded peak (8.8 fixed point, unity at the 31130 target, capped
  at 16x, never attenuating) and applies it at the 16-bit to 8-bit stage with clamping.
  The container and its CRC are unchanged, so existing `/tuna/` files stay valid.
- Measured on the host with `tuna-audio-probe`: jingle +9.20 dB, quiet MP3 fixtures
  +15.3 to +17.7 dB (all reaching 30976/32768 = 94.5% peak), and a full-scale fixture
  exactly 0.00 dB (gain 256, peak unchanged at 32768) - so full-scale material cannot
  be clipped by the change.
- Measured in Dolphin with WASAPI loopback capture of the same jingle: peak
  -11.90 -> **-2.52 dBFS**, active RMS -16.40 -> **-7.11 dBFS**, a **+9.4 dB**
  increase, matching the host prediction. Active span stayed exact (3.997 -> 3.996 s)
  and the GBA reported `ENDED 0:04 / 0:04` with no underrun.
- Continuity: the engine still uses the proven stop-and-wait double buffer because
  mGBA's sound-FIFO DMA **streams forward and does not wrap**, so no ring/looping
  design is valid. Instead the 32-byte guard region now holds the decoded
  **continuation** (a rewind-safe peek) instead of zeros, so a boundary IRQ that runs
  late plays real audio rather than injecting silence; the peek is discarded, so
  sample counting and the exact final count are unchanged. `SOUNDBIAS` is now set
  explicitly to 0x200 instead of inheriting an arbitrary post-BIOS value, which could
  otherwise offset/clip Direct Sound on hardware. Worst-case single-sample
  discontinuity normalised to the signal improved from 0.488 to 0.368.
- Host suite grew to 76,359 checks, including gain/scale/saturation/peak-scan
  boundaries, unity-gain equality with the previous truncation, and a full-scale
  container staying at unity. All targets build; resident player is 9,676 bytes.
- Evidence: `artifacts/emulator-20261014-225211-7d1a80f2/` (screenshots, capture,
  analysis) against the pre-fix baseline in
  `artifacts/emulator-20261014-221739-e3022abd/`. Remaining open gates: captured-audio
  listening on real hardware, exact GBA sample counter/underrun instrumentation, and
  whether the residual discontinuity is Dolphin's resampler or the codec's 256-sample
  block seams. The automatic gain also changes relative loudness between tracks, which
  is a deliberate trade-off for audibility on the GBA's Direct Sound output.

## Seek, metadata and release versioning follow-up (2026-10-15 UTC)

- GBA playback now seeks: `L`/`R` step five seconds backward/forward (hold repeats)
  and `B` returns to 0:00. Blocks are independent, so seeking only moves the decode
  cursor and re-primes the existing double buffer; there is no keyframe table or
  extra buffer. Host tests prove sample-accurate seeks (mid-block, clamped-forward,
  backward-from-end, replay-from-zero) against a full reference decode.
- The container gained artist/album (format revision 2, 160-byte header), extracted
  from ID3v2 `TIT2`/`TPE1`/`TALB` with an ID3v1 fallback. The GBA screen shows
  title/artist/album (artist/album only when tagged), codec, sample rate, encoded
  size, a progress bar, and elapsed/total time, with an inverted two-colour header
  and footer bar. The release version is `0.1.3` from `include/version.h` and is
  shown on the GBA header, the sender heading, and HBC `meta.xml`.
- Host suite: 76,886 checks; sender SI model: 161,317; build tool: 15. All three
  console targets build; resident player is 11,640 bytes (well under the 64 KiB
  gateway). A 60-second tagged MP3 extracted `title='Seek Test Song'`,
  `artist='Tuna Artist'`, `album='Tuna Album'` into a 130,720-byte format-revision-2
  container.
- Dolphin (Wii mode, tagged 60 s `mono.mp3`): the GBA displayed the title, artist,
  album, codec/rate, progress bar and controls, and A/R/L/B moved playback forward
  to the end, back to 0:00 and restarted. Evidence:
  `artifacts/emulator-20261014-230958-c7c98eba/`. In that run the sender reported a
  late DATA-phase link timeout at 122,624/130,560 bytes; this is recorded as an
  emulator-link failure, not a pass, and the 130 KiB transfer is not claimed READY.
- Dolphin (GameCube mode, embedded 4 s jingle): BIOS boot, v2 transfer, READY and the
  new UI including `TUNA 0.1.3 MUSIC PLAYER`, progress fill, and the sender heading
  `TUNA 0.1.3 - native GameCube SD Gecko`. Evidence:
  `artifacts/emulator-20261014-230821-1f67e790/` (READY screenshots) and
  `artifacts/emulator-20261014-231602-e9697f11/` (version + playing). The state-export
  context menu failed in both attempts, so no new retained-byte comparison is claimed.
- Still open: captured audible output on hardware, real-cable/long-transfer
  reliability, and the low-rate quality/performance gates. No audio or hardware
  acceptance is claimed here.
