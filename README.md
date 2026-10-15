# Tuna

Tuna is a Wii homebrew application that sends MP3 files to a connected Game Boy Advance to play on the go.
It compresses MP3 files stored on the Wii or a connected storage device into an executable program for the GBA.
After sending the song to the GBA, you can unplug the system and play your song anywhere!

## Supported Hardware

- Wii RVL-001 (has GameCube ports)
- Game Boy Advance AGB-001 (original)
- Game Boy Advance AGS-001 (SP frontlit)
- Game Boy Advance AGS-101 (SP backlit)
- GameCube - Game Boy Advance link cable (DOL-011, although third-party cables should work)

## How It Works

A Game Boy Advance only has GBA_RAM_AND_UNITS of RAM, which a connected device can directly write to (FACT CHECK AND UPDATE SENTENCE).
Tuna fills PLAYER_SIZE_AND_UNITS of this space with code for the music player.
The remaining space is allocated for the song.
Because of the low storage constraint, the songs are compressed far, far below typical file sizes.
If we instead connected a GBA with a custom game pack, we would be able to write to that storage.
The current implementation allows users enjoy the program without buying yet another strange accessory for their gaming setup.

Tracks may be up to 5 minutes long.
Common 3-minute tracks target 2-bit ADPCM at 4096 Hz.
Tracks under this length use higher-profile rates when possible.
Tracks over this length use very low-fidelity 1-bit adaptive delta at 4096 Hz.
Without this level of compression, tracks would need to be shortened like a [HitClip](INSERT_LINK_TO_WIKIPEDIA).

## Getting Started

### Build the App

```sh
make       # tests + all three console targets + dist package
make host  # desktop tests/converter only
```

Requires GNU Make, a host GCC/Clang compiler, and devkitPro
gba-dev/gamecube-dev/wii-dev. Use the devkitPro/MSYS2 UCRT64 shell on Windows.
The built Wii HBC files are saved in `dist/apps/tuna/`.
The GameCube DOL is saved in `dist/tuna-gamecube.dol`.

## Acknowledgements

Tuna is based in part on the work of the devkitPro project (libgba/libogc).

- [Application design](docs/design.md)
- [Build, controls and conversion](docs/build.md)
- [Wire format](docs/wire-format.md)
- [Validation and acceptance gates](docs/validation.md)
- [Verified CI action pins](docs/ci-dependencies.md)
- [Windows development and emulator setup](docs/development.md)
- [Research sources](docs/sources.md)

## License

Copyright © 2026 Scott Ratchford. Tuna's original code is MIT-licensed; third-party
components retain their respective licenses (see `LICENSES/` and `third_party/README.md`).
