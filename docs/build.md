# Build and run

## Single Windows entry point

Install Visual Studio C++ build tools and devkitPro's `gba-dev`, `gamecube-dev`,
and `wii-dev` groups using the official [getting-started instructions](https://devkitpro.org/wiki/Getting_Started).
Keep devkitPro's managed MSYS2 environment. No BIOS is needed to compile.

```powershell
.\Build.ps1                         # unit tests + GBA + GC + Wii + dist package
.\Build.ps1 -Target Host            # unit tests + desktop converter, no devkitPro
.\Build.ps1 -DevkitPro D:\devkitPro # explicit tool installation
```

Targets `Gba`, `GameCube`, and `Wii` build only the requested console lane plus
its prerequisites. A missing compiler/library is an error, not a skipped pass.
`-MsysBash` selects an existing managed MSYS2 `usr\bin\bash.exe` when necessary.
Nothing is installed or downloaded by the build. Project-local tools are detected
before the default `D:\devkitPro` path.

Outputs:

- `build/host/tests.exe`, `tuna-convert.exe`, `jingle.tuna`
- `build/gba/tuna.mb`, `tuna.elf`, `tuna.map`
- `build/gamecube/tuna.dol` and `build/wii/tuna.dol`, ELF/map files
- `dist/apps/tuna/boot.dol` + HBC metadata (Wii), `dist/tuna-gamecube.dol`
- `dist/tuna.mb`, converter, original jingle, licenses, SHA256 manifest

Each sender embeds the freshly built multiboot image. Linker assertions reserve
192 KiB at EWRAM 0x02010000, constrain the resident image to the preceding 64 KiB,
and retain IWRAM stack/BIOS headroom. The reserved track is NOLOAD: it must not
inflate the multiboot image. GBA code performs no heap allocation.

On Linux/macOS, `make host` builds the portable converter and unit tests with a
system GCC/Clang. Console Makefiles expect the standard devkitPro environment;
the Windows orchestration additionally generates `build/generated/player_image.c`.

## Sender usage

Use a compatible GBA-GCN cable, powered GBA/GBA SP with no cartridge, and Wii
RVL-001 ports or a GameCube. Controller on port 1, GBA on port 2 by default.
X (Wii +) cycles cable ports 2..4. Do not use an ordinary multiplayer cable.

Copy MP3 or `.tuna` files into FAT32 `/music` on Wii SD/USB or native-GC SD Gecko.
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

Only unplug after GBA `SAFE TO UNPLUG` and host verified READY. GBA A plays,
pauses (buffered fade-out, bounded latency), resumes, and restarts at end.
The RAM track vanishes when the GBA is powered off or reset. Start/Home exits
the sender; native GC deployment requires an existing suitable loader, not HBC.

## Desktop conversion

```powershell
.\build\host\tuna-convert.exe song.mp3 song.tuna
.\build\host\tuna-convert.exe song.mp3 excerpt.tuna --excerpt 30
.\build\host\tuna-convert.exe --jingle jingle.tuna
.\scripts\Test-Mp3.ps1 -Ffmpeg 'D:\ffmpeg-8.0.1-full_build\bin\ffmpeg.exe'
```

Decoder input is bounded to 16 MiB, ID3v2 metadata to 1 MiB. Two decode passes use
a fixed 4096-sample mono ring instead of allocating five minutes of PCM, so native
GC memory remains bounded. First pass measures duration; second pass downsamples
and encodes to the highest-quality profile that fits. Variable bitrate is supported;
rate changes, malformed frames, and truncated input are rejected. Stereo is
downmixed in 32-bit arithmetic, then windowed-sinc low-pass filtered/resampled
before block encoding. Valid Xing/LAME delay and padding metadata is honored;
without it, actual decoded duration includes encoder padding and is always shown.
Three-minute defaults are 2-bit ADPCM at 4096 Hz (195,936 bytes); five-minute tracks
use 1-bit adaptive delta at 4096 Hz (172,896 bytes). These custom profiles are
explicitly lo-fi and require representative-music listening acceptance.
Titles are bounded printable ASCII; non-ASCII filename bytes become `?`.

## Regression skill

`.opencode/skills/tuna-regression/` bundles repeatable guidance and a script:

```powershell
.\.opencode\skills\tuna-regression\scripts\Verify.ps1 -Target Host
```

The script preserves build logs and revision/status in ignored `artifacts/`.
Unit tests are not substitutes for the acceptance gates in `docs/validation.md`.
