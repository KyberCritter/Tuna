# Tuna

Wii/GameCube-to-GBA RAM music player: browse MP3s, convert on the host
console, boot a cartridge-free GBA over the GBA–GameCube link cable, and transfer
a track that plays after unplugging. A toggles play/pause; the GBA shows the title
using a two-color tile UI. The GBA must remain powered on.

GameCube homebrew is an acceptable primary integration target. Retain a native
Wii build for convenient Wii SD/USB sources; native GameCube mode instead needs
a suitable source such as SD Gecko. Both share the codec, protocol and GBA player.

The application sources and single build script are implemented. Complete tracks
up to **five minutes** use automatic compression selection; the three-minute
default target uses custom 2-bit ADPCM at 4096 Hz. Five-minute tracks use very
lo-fi 1-bit adaptive delta at 4096 Hz. Short tracks use higher-rate profiles when
they fit. Compression is previewed; songs are never silently cropped.

Host tests, actual 3/5-minute MP3 conversion, and GBA/GameCube/Wii cross-builds
have passed locally. **BIOS multiboot, audible GBA DMA playback, cable disconnect,
and representative music quality are not yet validated.** A legal user-supplied
GBA BIOS and hardware/emulator acceptance runs are still required.

```sh
make       # tests + all three console targets + dist package
make host  # desktop tests/converter only
```

Requires GNU Make, a host GCC/Clang compiler, and devkitPro
gba-dev/gamecube-dev/wii-dev. Use the devkitPro/MSYS2 UCRT64 shell on Windows.
No Python or PowerShell is required for the build; embedding, memory validation
and SHA256 manifests use a small MIT-licensed C helper.
Built Wii HBC files are in `dist/apps/tuna/`; GC DOL in `dist/tuna-gamecube.dol`.
Tuna is based in part on the work of the devkitPro project (libgba/libogc).

- [Application design](docs/design.md)
- [Build, controls and conversion](docs/build.md)
- [Wire format](docs/wire-format.md)
- [Validation and acceptance gates](docs/validation.md)
- [Verified CI action pins](docs/ci-dependencies.md)
- [Windows development and emulator setup](docs/development.md)
- [Research sources](docs/sources.md)

Run the read-only prerequisite check from PowerShell:

```powershell
.\scripts\Test-Environment.ps1
```

Downloaded references belong only in `.agents/docs/` (Git-ignored). Local BIOS
dumps, tools, and emulator profiles belong in `.local/` (also Git-ignored).
Never commit copyrighted BIOS images, music, or private device information.

Copyright © 2026 Scott Ratchford. Tuna's original code is MIT-licensed; third-party
components retain their respective licenses (see `LICENSES/` and `third_party/README.md`).
