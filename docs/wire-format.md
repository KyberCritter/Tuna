# Tuna wire format

This document describes the on-the-wire container and packet layout. It is
independent of the app's release version, which is defined in `include/version.h`
(`TUNA_VERSION_STRING`, tagged `v` + version, e.g. `v0.1.3`). The integer in the
header below is the **format revision** and only changes when this layout changes.

All integers are unsigned little-endian, except signed 16-bit ADPCM predictors.
No native structure is serialized. Unknown format revisions/profiles are rejected.

## Track

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | ASCII `TUNA` |
| 4 | 2 | format revision = 2 |
| 6 | 2 | header length = 160 |
| 8 | 1 | codec = 1 (IMA4), 2 (custom ADPCM2), 3 (adaptive DELTA1) |
| 9 | 3 | zero |
| 12 | 4 | rate = 8192 or 4096, constrained by codec |
| 16 | 4 | exact decoded sample count, nonzero |
| 20 | 4 | encoded payload bytes |
| 24 | 1 | title length, 0..60 |
| 25 | 1 | artist length, 0..30 (0 when absent) |
| 26 | 1 | album length, 0..30 (0 when absent) |
| 27 | 1 | zero |
| 28 | 4 | header CRC32 with this field zeroed |
| 32 | 60 | printable ASCII title, zero padded, no terminator required |
| 92 | 30 | printable ASCII artist, zero padded |
| 122 | 30 | printable ASCII album, zero padded |
| 152 | 4 | payload CRC32 |
| 156 | 4 | zero |
| 160 | variable | independent ADPCM blocks |

The header CRC covers bytes 0..27 and 32..159 with the CRC field itself zeroed.
Artist and album come from the MP3's ID3 tags (ID3v2 `TIT2`/`TPE1`/`TALB`, falling
back to the ID3v1 trailer and then the filename for the title); unsupported or
absent tags leave the field empty. Non-ASCII bytes become `?`.

CRC32 uses reflected IEEE polynomial 0xedb88320, initial/final XOR 0xffffffff.
Every independent block represents 256 samples: predictor (2), index (1, 0..88),
reserved zero (1), then 255 codes packed least-significant bits first. Predictor
is the first decoded sample. Unused high bits of the last byte are zero. The last
block is padded; only the exact sample count is played. Container fits 192 KiB;
decoded duration is bounded to 300 seconds. The IMA step table is in `shared/adpcm.c`.

| Profile | Block bytes | Allowed rate | Adaptation |
| --- | --- | --- | --- |
| IMA4 | 132 | 8192 | standard 4-bit IMA predictor/index |
| ADPCM2 | 68 | 8192, 4096 | bit 1 sign, bit 0 magnitude: step/2 or 3*step/2; index -1 or +4 |
| DELTA1 | 36 | 4096 | 1 positive/0 negative, delta=step; index +2 for repeated sign, -2 for changed sign |

All predictor updates saturate to signed 16-bit, indices clamp to 0..88.
DELTA1 resets previous sign to 0 at every block; ADPCM2/DELTA1 initial index is
the first step >= abs(sample1-sample0), or 88. Integer divisions truncate toward
zero. Custom profiles are **not** standard IMA. Three-minute complete tracks fit
ADPCM2/4096 in a 196,000-byte container; five-minute DELTA1/4096 in 172,960 bytes.

## Packets

20-byte header: `TNPK` (4), format revision (2), message type (1), body length (2),
sequence (4), offset (4), CRC32 (4). CRC covers first 16 bytes followed by body.
Maximum body 256 bytes. Joybus words carry four consecutive packet bytes,
with zero padding in the last word (not included in packet length or CRC).

Types are enumerated in `include/tuna.h`. HELLO returns CAPS (12 bytes:
capacity, maximum seconds=300, supported codec bitmask=14, all u32).
BEGIN carries the full track header, sequence/offset zero. DATA sequences start
at 1 with contiguous payload offsets. ACK reports accepted payload offset.
The immediately previous DATA is idempotent if its sequence/offset/content match.
Duplicate BEGIN does not clear progress. COMMIT uses next sequence and full payload
length as offset; READY is sent only after complete format and CRC validation.
Duplicate COMMIT returns READY. NACK body is a 32-bit TunaError.
STATUS returns receive state/error. ABORT invalidates the track and enables an
explicit replacement; there is no second staging buffer. Playback is independent
of link polling. Host transport failure must not imply READY.

The application transport uses bounded stop-and-wait word transactions, checking
Joybus receive/transmit status before register access. BIOS multiboot uses its
separate key/cipher/CRC, not these runtime packets.

JOYSTAT bit 1 (`0x02`) means GBA receive register full; bit 3 (`0x08`)
means GBA transmit register full. These flags cannot be cleared by writing zero
to JOYSTAT. On entry the player reads any remaining BIOS receive word before
assembling packets, then sets general-purpose bit 5 (`0x20`) to announce application
startup. After BIOS boot the sender waits for that bit and consumes any pending
BIOS transmit word before sending HELLO. Runtime packets still require CAPS/READY
validation; the startup bit is not proof of a transferred track.
