# Tuna

Tuna is a Wii homebrew application that sends MP3 files to a connected Game Boy Advance to play on the go.
It compresses MP3 files stored on the Wii or a connected storage device into an executable program for the GBA.
After sending the song to the GBA, you can unplug the system and play your song anywhere!

## Supported Hardware

- Wii RVL-001 (has GameCube ports, must be softmodded)
- Game Boy Advance AGB-001 (original)
- Game Boy Advance AGS-001 (SP frontlit)
- Game Boy Advance AGS-101 (SP backlit)
- GameCube - Game Boy Advance link cable (DOL-011, although third-party cables should work)

## How It Works

A Game Boy Advance only has 256 KiB of EWRAM (plus 32 KiB of IWRAM), which a connected device can directly write to.
Tuna fills approximately 9 KiB of this space with code for the music player.
The remaining space is reserved for the song, up to a 192 KiB track capacity.
Because of the low storage constraint, the songs are compressed far, far below typical file sizes.
If we instead connected a GBA with a custom game pack, we would be able to write to that storage.
The current implementation allows users to enjoy the program without buying yet another strange accessory for their gaming setup.

Tracks may be up to 5 minutes long.
Common 3-minute tracks target 2-bit ADPCM at 4096 Hz.
Tracks under this length use higher-profile rates when possible.
Tracks over this length use very low-fidelity 1-bit adaptive delta at 4096 Hz.
Without this level of compression, tracks would need to be shortened like a [HitClip](https://en.wikipedia.org/wiki/HitClips) (which the program can also do).

## Getting Started

### Requirements

- Wii RVL-001 (the original model with GameCube controller ports), softmodded with the Homebrew Channel installed.
- Game Boy Advance (AGB-001) or Game Boy Advance SP (AGS-001/AGS-101), powered on with no cartridge inserted.
- GameCube–Game Boy Advance link cable (DOL-011; third-party cables may also work). An ordinary GBA multiplayer cable is not compatible.
- A FAT32-formatted SD card or USB device with your MP3 files (or `.tuna` files) in a `music` folder at its root.
- A Wii Remote or GameCube controller.
- No flashcart or other accessory is required. A legally dumped GBA BIOS is needed only for emulator testing, not for a real Wii + GBA.

The native GameCube build needs an existing homebrew loader (for example Swiss); it cannot be launched from the Wii's Homebrew Channel in GameCube mode.

### Build Tuna

```sh
make       # tests + all three console targets + dist package
make host  # desktop tests/converter only
```

Requires GNU Make, a host GCC/Clang compiler, and devkitPro gba-dev/gamecube-dev/wii-dev.
Use the devkitPro/MSYS2 UCRT64 shell on Windows.
The built Wii HBC files are saved in `dist/apps/tuna/`.
The GameCube DOL is saved in `dist/tuna-gamecube.dol`.

### Run Tuna

1. Copy the `apps/tuna/` folder from `dist/` onto a FAT32 SD card or USB device as `apps/tuna/`, so the files sit at `apps/tuna/boot.dol` (with `meta.xml` and `LICENSES/`).
2. Create a `music` folder at the root of the same device and copy your MP3 (or `.tuna`) files into it. Tuna searches both `/music/` and `/tuna/`, listing at most 127 files plus its embedded original jingle.
3. Insert the storage device into the Wii, start the Homebrew Channel, and launch Tuna.
4. Connect the GBA (powered on, no cartridge) to the Wii's **second** GameCube controller port with the GCN–GBA link cable. Control Tuna with a GameCube controller in Port 1 or use a Wii remote.
5. (Optional) Use `X` on the GCN controller (or `+` on the Wii remote) to cycle the GBA cable between ports 2, 3, and 4.
6. Use Up/Down to change the selected song.
7. Press `A` to convert and preview a track (the preview shows the actual decoded duration and selected profile), then `A` again to send it to the GBA or `B` to cancel. Completed MP3 conversions are saved as `.tuna` files in `/tuna/` on the **SD card**, before any GBA boot/transfer attempt—even when the MP3 is on USB.
8. (Optional) Press `Y` (or `−`) to trim the song to no more than 30 seconds.
9. (Optional) Cancel by pressing `B`.
10. Wait until the GBA shows `SAFE TO UNPLUG` (the host also reports READY). You may unplug the cable or leave it connected.
11. Press `A` on the GBA to play or pause a track. At the end of a track, press `A` to restart.
12. The track lives in GBA RAM only, so it disappears when the GBA is powered off or reset. Press Start (or Home) to exit Tuna.

## Controls

Tuna is driven from the Wii while you choose and send a track; playback is controlled entirely on the GBA once the song is in RAM.

- **Wii Remote:** `A` opens the conversion preview and sends, `B` cancels a preview or rescans storage, Up/Down browse the list, `+` cycles the GBA cable port (2–4, port 2 by default), `−` toggles the explicit 30-second excerpt, and Home exits Tuna.
- **GameCube controller (port 1):** `A` opens the conversion preview and sends, `B` cancels a preview or rescans, Up/Down browse, `X` cycles the GBA cable port, `Y` toggles the excerpt, and Start exits.
- **GBA:** `A` plays, pauses, resumes, and restarts a track after it ends.

Unplug the cable only after the GBA shows `SAFE TO UNPLUG` and the Wii reports `READY`.
The track is stored in the GBA's RAM, so it disappears when the GBA is powered off or reset.

### Retrying a failed transfer

Select the same MP3 again: Tuna reuses its completed conversion instead of decoding
it again. Saved conversions survive app restarts and appear in the browser after
`B` rescans. Full tracks and explicit excerpts have separate cache files; changing
the source content creates a new conversion. Old conversions are not deleted.
If SD is missing, full or write-protected, Tuna warns and keeps the most recently
prepared track in sender RAM for retry until another track is prepared or Tuna exits.
No `READY` means the GBA transfer did not succeed; follow any reconnect/reset prompt
and wait for both READY indications before unplugging.

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

Copyright © 2026 Scott Ratchford.
Tuna's original code and documentation are available under the MIT License.
See [LICENSE](LICENSE) for the full text.

Tuna incorporates unmodified third-party libraries and runtime components:

- [minimp3](https://github.com/lieff/minimp3) (CC0-1.0) and the FIX94 Joybus key/cipher implementation (MIT)
- devkitPro libgba (LGPL-2.0 with static-linking exceptions) and libogc (zlib/RTEMS notices), including wiiuse (GPLv3 with the libogc independent-module linking exception), BTE/lwBT (BSD-3-Clause), libdvm (ZPL-2.1), and FatFs
- devkitARM GBA startup code (MPL-2.0), newlib, GCC runtime (libgcc, GPLv3 with GCC Runtime Library Exception 3.1), and the MinGW-w64 runtime

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and [LICENSES](LICENSES) for
the audited versions, full license texts, exceptions, and source-availability
details. These notices do not relicense third-party components under Tuna's MIT
license, and Tuna's license does not grant rights to user-supplied music, BIOS
images, or trademarks.
