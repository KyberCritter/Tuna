# Application design

## Scope and non-negotiable behavior

- Target RVL-001 Wii with physical GameCube ports, GBA/appropriate GBA SP, and
  compatible GBA–GCN cable. An ordinary GBA multiplayer cable is not equivalent.
- Shared libogc sender with Wii-native and GameCube-native targets. User accepts
  GameCube homebrew, so choose that target for initial link integration if simpler.
  The native Wii target needs no GameCube-mode reboot or Nintendont.
- GameCube-native mode on Wii does not inherit Wii SD-slot/USB APIs. Plan SD Gecko
  for native GC storage, or an embedded synthetic fixture for first link tests.
  Treat Nintendont/virtual storage as a separate compatibility lane; it must preserve
  native SI access and is not the default or a substitute for a tested boot path.
- Start with FAT32 SD/USB MP3s. Other devices require explicit filesystem/source
  backends; "any accessible device" is not a promise of universal filesystem support.
- Internal-drive audio CD and data CD support is excluded: there is no dependable
  Wii CD-reading path. DVD compatibility does not imply CD compatibility. Rip CDs
  externally to MP3 files; USB optical access is a separate unproven investigation.
- No flashcart required: use BIOS Joybus multiboot to boot the RAM player.
- Disconnect only after verification and a GBA-side READY indication. Playback,
  A-button control, title, and replay must continue without Wii/link activity.
- RAM contents disappear on power-off/reset. Pausing must not invoke BIOS STOP.
- Do not silently truncate a song. Reject tracks that cannot fit, or offer an
  explicit user-approved excerpt mode with the actual duration shown.
- Required complete-track range: up to five minutes. Choose compression and
  sample rate from decoded duration and framed byte size; use approximately
  three minutes as the default planning target. Show the selected quality profile.
  Source filesize bounds decoder memory; it is not a reliable duration estimate.

## Architecture and stack

Use devkitPPC + libogc/libfat for Wii/GameCube, devkitARM + libgba or libtonc for GBA,
C for shared codec/format code, and a small C++ Wii UI if useful. Use Make and
PowerShell orchestration; avoid additional frameworks until needed.

Wii pipeline: filesystem source -> MP3 decode -> stereo downmix -> anti-aliased
resampling -> encode -> size/quality preview -> multiboot -> runtime transfer.
Use a portable scalar MP3 decoder on Wii, not GBA. Bound allocations, metadata,
duration, and decoder work; handle malformed files and unavailable storage.
Cache converted tracks on Wii storage optionally, not on GBA cartridge storage.

GBA pipeline: receive compressed track into reserved EWRAM -> verify -> decode
incrementally -> signed 8-bit PCM double buffer -> timer/DMA Direct Sound FIFO A,
routed to both outputs. Keep ISR work bounded. Handle DMA prefetch/guard samples,
buffer deadlines, exact final sample count, and click-free mute/pause/resume.

Use fixed allocations and linker assertions, not general-purpose heap allocation
on GBA. Reserve BIOS/IRQ locations and stack headroom. Decoder code, buffers,
and track bytes all count; a multiboot executable is not stored in cartridge ROM.

## Memory and audio feasibility gate

GBA has 256 KiB EWRAM and 32 KiB IWRAM, plus separate video memory. Budget
initially 192 KiB EWRAM for compressed audio and the remaining 64 KiB for the
resident image, state, buffers, metadata, and safety margin. Determine the actual
limit from linker maps and runtime stack/buffer checks, not this estimate.

Retain mono 4-bit IMA ADPCM at 8,192 Hz for short tracks: approximately 4 KiB/s, so 192 KiB
holds slightly less than 48 seconds after block headers. Timer rate divides the
GBA clock exactly; use a sample-counting timer for buffer boundaries rather than
assuming a whole number of samples per display frame.

That profile alone is NOT a solution for complete typical songs. With 192 KiB,
three minutes needs <=8.74 kbit/s INCLUDING framing overhead; four minutes needs
<=6.55 kbit/s. Supporting ordinary-length MP3 music as complete RAM tracks requires
a much lower-rate codec with acceptable sound and real-time ARM7 performance.

Evaluate fixed-point low-bitrate codecs separately. Speech codecs such as GSM or
Codec2 may damage music badly and are not assumed fast enough on a 16.78 MHz
ARM7TDMI. Opus/MP3 are not assumed feasible merely because portable code exists.
Benchmark compiled code, total footprint, worst-case cycles, and listening quality
on representative music. A measured codec decision precedes a quality/performance
acceptance claim. Custom low-bit profiles are not standard IMA compatibility.

The implementation selects the highest-quality fitting profile from 4-bit IMA at
8192 Hz, custom 2-bit ADPCM at 8192 Hz, 2-bit ADPCM at 4096 Hz, and 1-bit adaptive
delta at 4096 Hz. Three-minute tracks use 2-bit/4096 (195,936 framed bytes);
five-minute tracks use 1-bit/4096 (172,896 bytes). Low-rate profiles are explicitly
lo-fi; 4096 Hz retains only a narrow music bandwidth. Host conversion/replay proves
size and sample-count feasibility, not representative listening quality or ARM7
worst-case timing. Preserve those acceptance gates rather than reducing the
five-minute requirement to a short-jingle-only deliverable.

For the guaranteed first acceptance track, use an original complete short jingle
that fits, not a silently cropped commercial song. If typical full-length songs
are mandatory, measured codec quality/performance remains a go/no-go gate. A flashcart/extra
storage would change the requested RAM-only model and needs separate approval.

## Link and container

Joybus is not GBA multiplayer SIO: do not infer its throughput from 115,200-baud
multiplayer or 2 MHz normal mode figures. On Wii use libogc `SI_Transfer`, with
exclusive ownership of the selected SI channel so PAD polling cannot interfere.
Choose port 2 (SI index 1) by default, configurable on both emulator and hardware.

Bootstrap a small correctly linked multiboot image using the existing documented
Joybus handshake/cipher/checksum algorithm. Multiboot can use up to the GBA EWRAM
image envelope (~256 KiB); it is NOT generally limited to 16 KiB. The compact
player is transferred first, the audio later into a disjoint reserved buffer.

Runtime link uses 0x15 WRITE (command plus four data bytes, one status reply) and
0x14 READ (command, four data bytes plus status reply). Application packets span
multiple such transactions. Respect receiver-ready/status flags; avoid overwriting
the single-word receive register. Define byte serialization explicitly because Wii
PowerPC is big-endian and GBA ARM is little-endian. Avoid packed-struct casts.

Proposed versioned container: magic, version, header length, codec/profile,
sample rate, sample count, encoded length, bounded title length/title, payload
CRC32. Define byte order, padding, title encoding, and ADPCM block predictor/index
layout before coding. Use independent blocks for decoder recovery/replay.

Proposed messages: HELLO/CAPABILITIES, BEGIN, DATA, ACK/NACK, COMMIT, READY,
ABORT, STATUS. Logical data chunks ~256 bytes initially; sequence number,
offset, length, per-chunk CRC and final CRC. Bounded timeouts/retries for every SI
operation, including boot. Retransmission must be idempotent. Validate all lengths
and offsets before writes. READY follows GBA validation, never just Wii send completion.

State machine: BOOT -> WAIT -> RECEIVE -> VERIFY -> READY/PAUSED <-> PLAYING ->
ENDED. A toggles on key-down, not every held frame. A at ENDED restarts the track.
Failure/early removal leaves an error state, not partially playable data. Transfer
of a replacement track is allowed only through an explicit user flow; a second
full-size staging buffer will not fit. No link polling is required for playback.

## Display

Two colors, a tiny font, title, transfer status, elapsed/total time, play/pause
state, and "Safe to unplug". Store glyphs compactly and expand into 4bpp tiles in
VRAM. The GBA has no native 1bpp framebuffer mode: monochrome reduces asset size
but does not magically reclaim ordinary EWRAM or video memory. No album art.

## Build-out sequence

1. Establish local toolchains, BIOS prerequisite, and emulator smoke tests first.
2. GBA signed PCM tone, then ADPCM test jingle, title, A controls, exact replay.
3. Wii storage/MP3 conversion with host golden-vector comparisons.
4. GameCube-mode emulated SI ping/pong, then cartridge-free BIOS multiboot;
   repeat in Wii mode when implementing the Wii-storage backend.
5. Checksummed runtime track transfer and simulated disconnect acceptance.
6. Codec duration/quality feasibility and explicit fit policy.
7. Real Wii/GBA validation: physical unplug, power independence, cable timing,
   SD/USB device behavior, and worst-case playback performance.

License-audit borrowed implementations. FIX94 sender is MIT; the Joybus tester
and Dolphin have GPL licensing constraints. Consult their actual licenses before
copying code; source inspection does not grant permission to ignore those terms.
