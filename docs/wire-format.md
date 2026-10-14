# Tuna v1 wire format

All integers are unsigned little-endian, except signed 16-bit ADPCM predictors.
No native structure is serialized. Unknown versions/profiles are rejected.

## Track

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | ASCII `TUNA` |
| 4 | 2 | version = 1 |
| 6 | 2 | header length = 96 |
| 8 | 1 | codec = 1 (mono IMA ADPCM) |
| 9 | 3 | zero |
| 12 | 4 | rate = 8192 |
| 16 | 4 | exact decoded sample count, nonzero |
| 20 | 4 | encoded payload bytes |
| 24 | 1 | title length, 0..60 |
| 25 | 3 | zero |
| 28 | 4 | header CRC32 with this field zeroed |
| 32 | 60 | printable ASCII title, zero padded, no terminator required |
| 92 | 4 | payload CRC32 |
| 96 | variable | independent ADPCM blocks |

CRC32 uses reflected IEEE polynomial 0xedb88320, initial/final XOR 0xffffffff.
Each 132-byte block represents 256 samples: predictor (2), index (1, 0..88),
reserved zero (1), 255 nibbles (128 bytes, low nibble first). Predictor is the
first decoded sample. Final unused high nibble is zero. The last block is padded;
only the header's exact sample count is played. Total container must fit 192 KiB.
Maximum 380,928 samples / 46.5 seconds; no ordinary full-song guarantee.

## Packets

20-byte header: `TNPK` (4), version (1), message type (1), body length (2),
sequence (4), offset (4), CRC32 (4). CRC covers first 16 bytes followed by body.
Maximum body 256 bytes. Joybus words carry four consecutive packet bytes,
with zero padding in the last word (not included in packet length or CRC).

Types are enumerated in `include/tuna.h`. HELLO returns CAPS (capacity/rate).
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
