# Build and run

## Single cross-platform entry point

Use GNU Make, a host GCC/Clang compiler, and devkitPro's `gba-dev`, `gamecube-dev`,
and `wii-dev` groups from the official [getting-started instructions](https://devkitpro.org/wiki/Getting_Started).
No Python, PowerShell, CMake or BIOS is needed to build.

```sh
make -j4     # unit tests + GBA + GC + Wii + dist package
make host    # desktop converter + unit tests, no devkitPro required
make gba     # GBA + memory/heap/IRQ checks
make gamecube
make wii
```

Linux/macOS: install GNU Make and GCC/Clang (Apple Clang works), then source the
devkitPro environment. Set `DEVKITPRO` if your installation is not `/opt/devkitpro`.
`DEVKITARM`/`DEVKITPPC` default to its toolchain subdirectories. Select host compiler
and flags with standard `CC`/`CFLAGS`, e.g. `make host CC=clang`.

Windows: run the same commands in devkitPro's MSYS2 **UCRT64 shell**. Install host
tools with `pacman -S --needed make mingw-w64-ucrt-x86_64-gcc`. Keep the managed
devkitPro/MSYS2 environment; do not run a Unix Makefile directly in cmd/PowerShell.
The required POSIX utilities (`mkdir`, `cp`) come with MSYS2 and Unix systems.
Console Make rules, including upstream devkitPro rules, require project/toolchain
paths without whitespace. Host targets do not need console toolchains.

Missing compilers/libraries fail the build instead of silently skipping a target.
Nothing is installed or downloaded by Make. `Build.ps1` is only an optional legacy
Windows convenience wrapper; it delegates to this Makefile, not a second build.

Outputs:

- `build/host/tests`, `tuna-convert`, `tuna-build`, `jingle.tuna` (`.exe` on Windows)
- `build/gba/tuna.mb`, `tuna.elf`, `tuna.map`
- `build/gamecube/tuna.dol` and `build/wii/tuna.dol`, ELF/map files
- `dist/apps/tuna/boot.dol` + HBC metadata (Wii), `dist/tuna-gamecube.dol`
- `dist/tuna.mb`, converter, original jingle, licenses, SHA256 manifest

Each sender embeds the freshly built multiboot image. Linker assertions reserve
192 KiB at EWRAM 0x02010000, constrain the resident image to the preceding 64 KiB,
and retain IWRAM stack/BIOS headroom. The reserved track is NOLOAD: it must not
inflate the multiboot image. GBA code performs no heap allocation.

Make compiles `tools/build_tool.c` and `tools/sha256.c` with the host C compiler.
This MIT-licensed helper embeds `build/generated/player_image.c/.h`, validates
GBA symbols/image size, and generates the package SHA256 manifest. SHA-256 golden
vectors and memory-gate tests run alongside the application tests. Generated
embedding files retain timestamps when unchanged, enabling incremental builds.
GNU Make/GCC are build tools; their GPL licenses do not relicense Tuna's source.
Linked libraries keep their own notices/exceptions (see `third_party/README.md`).

## Sender usage

Use a compatible GBA-GCN cable, powered GBA/GBA SP with no cartridge, and Wii
RVL-001 ports or a GameCube. Controller on port 1, GBA on port 2 by default.
X (Wii +) cycles cable ports 2..4. Do not use an ordinary multiplayer cable.

Copy MP3 or `.tuna` files into FAT32 `/music/` or `/tuna/` on Wii SD/USB or native-GC SD Gecko.
The shallow browser bounds paths to 511 bytes and lists at most 127 files plus
an embedded original complete jingle. B rescans after device changes. Unsupported
filesystems and storage devices are not promised. No internal optical/CD support.

A opens conversion/size preview. Complete decoded tracks that exceed RAM are
rejected. Automatic compression supports complete tracks up to five minutes,
with a three-minute default planning target. Y (Wii -) enables an explicit
30-second MP3 excerpt for over-limit files, never silently.
The preview shows actual decoded duration and requires another A to send/replace
GBA RAM contents. B cancels. A replacement invalidates the old track before transfer.
Port changes or link failures can require a no-cartridge GBA reset before boot.

Completed MP3 conversions are saved to SD `/tuna/` **before** confirmation/boot/
transfer, including MP3s read from USB. The directory is created automatically.
Native GC uses its SD Gecko/default FAT mount. Cache filenames include source-path
and source-content CRCs, a conversion revision and excerpt duration (`0s` means full
track), so full/excerpt modes and changed sources do not share a cache entry.
The cache tag is `f2` for the artist/album container (format revision 2); older
tagged files are left untouched and never matched, so an upgrade re-converts each
track once. This is separate from the release version in `include/version.h`.
Retries validate/reuse the saved container without another MP3 conversion; B rescans
to list the saved files for direct selection. Cancelling or failing a transfer does
not delete the conversion. Existing invalid cache files are left untouched with a
warning. SD write failures warn but retain the most recent conversion in sender RAM
until a different track is prepared or the app exits. Delete unwanted old `.tuna`
files manually; interrupted `.tmp` files are not listed as tracks.

Only unplug after GBA `SAFE TO UNPLUG` and host verified READY. GBA A plays,
pauses (buffered fade-out, bounded latency), resumes, and restarts at end;
L/R seek five seconds backward/forward (hold to repeat) and B returns to 0:00.
The screen shows the track title, artist, album, codec, sample rate, encoded size,
a progress bar, and elapsed/total time; artist/album appear only when the ID3 tags
carry them. The RAM track vanishes when the GBA is powered off or reset.
Start/Home exits the sender; native GC deployment requires an existing suitable
loader, not HBC.

## Desktop conversion

```sh
./build/host/tuna-convert song.mp3 song.tuna
./build/host/tuna-convert song.mp3 excerpt.tuna --excerpt 30
./build/host/tuna-convert --jingle jingle.tuna
```

Add `.exe` when invoking directly from a Windows shell. The optional Windows
fixture/emulator helpers in `scripts/*.ps1` remain platform-specific; they are
not prerequisites for `make`. `scripts/Test-Mp3.ps1` can generate longer MP3
fixtures with FFmpeg for the separate media regression lane.

Decoder input is bounded to 16 MiB, ID3v2 metadata to 1 MiB. Two decode passes use
a fixed 4096-sample mono ring instead of allocating five minutes of PCM, so native
GC memory remains bounded. First pass measures duration; second pass downsamples
and encodes using a fixed profile preference, not an objective music-quality
score. Variable bitrate is supported;
rate changes, malformed frames, and truncated input are rejected. Stereo is
downmixed in 32-bit arithmetic, then windowed-sinc low-pass filtered/resampled
before block encoding. Valid Xing/LAME delay and padding metadata is honored;
without it, actual decoded duration includes encoder padding and is always shown.
Three-minute defaults are 2-bit ADPCM at 4096 Hz (196,000-byte container); five-minute
tracks use 1-bit adaptive delta at 4096 Hz (172,960 bytes). These custom profiles are
explicitly lo-fi and require representative-music listening acceptance.
Titles, artists and albums are bounded printable ASCII; non-ASCII bytes become `?`,
and artist/album remain empty when no ID3 tag supplies them.

Conversion caches repeating sinc/window FIR coefficients for 4096/8192 Hz output
instead of recalculating trigonometric functions for every sample. The optional
cache is bounded to 4 MiB and falls back to the original scalar resampler if its
allocation/budget is unavailable. Tap accumulation and final normalization retain
their original double-precision order: encoded bytes, profiles, duration and
quality are unchanged, so existing `/tuna/` conversions remain valid. Sender
previews report measured MP3 conversion time; loading a cached track avoids this
work altogether.

Playback applies a bounded output gain derived from the container's decoded peak
(8.8 fixed point, unity at a 31130 target, capped at 16x, never attenuating) so
low-amplitude tracks are not inaudibly quiet on the GBA's Direct Sound DAC. The
container format and CRC are unchanged, so previously saved `/tuna/` conversions
remain valid and are not re-gained on disk. Full-scale material keeps unity gain
and is not clipped; the trade-off is that relative loudness between tracks is
normalised rather than preserved. The Direct Sound bias (`SOUNDBIAS` 0x200) is
set explicitly at startup.

## Regression skill

`.opencode/skills/tuna-regression/` bundles repeatable guidance and a script:

```sh
sh .opencode/skills/tuna-regression/scripts/Verify.sh host
```

The script preserves build logs and revision/status in ignored `artifacts/`.
Unit tests are not substitutes for the acceptance gates in `docs/validation.md`.

`scripts/Test-Gba.ps1` additionally builds a **separate, embedded-fixture diagnostic
ROM** and attempts standalone mGBA/GDB counter tests. It is not in the production
package and is never a substitute for no-cartridge BIOS multiboot. It uses isolated
child configuration paths and dummy SDL audio/video drivers; it does not capture
audible output. The installed mGBA 0.10.5 SDL build did not expose its GDB listener
locally, so runtime acceptance for this optional lane remains blocked. Supply a
verified compatible `-Mgba` executable to retry. The script fails rather than
silently treating a missing listener as a pass.
