# Tuna

Planned Wii/GameCube-to-GBA RAM music player: browse MP3s, convert on the host
console, boot a cartridge-free GBA over the GBA–GameCube link cable, and transfer
a track that plays after unplugging. A toggles play/pause; the GBA shows the title
using a two-color tile UI. The GBA must remain powered on.

GameCube homebrew is an acceptable primary integration target. Retain a native
Wii build for convenient Wii SD/USB sources; native GameCube mode instead needs
a suitable source such as SD Gecko. Both share the codec, protocol and GBA player.

This repository currently contains design and environment preparation only, not
a working sender/player or a validated emulator installation.

- [Application design](docs/design.md)
- [Windows development and emulator setup](docs/development.md)
- [Research sources](docs/sources.md)

Run the read-only prerequisite check from PowerShell:

```powershell
.\scripts\Test-Environment.ps1
```

Downloaded references belong only in `.agents/docs/` (Git-ignored). Local BIOS
dumps, tools, and emulator profiles belong in `.local/` (also Git-ignored).
Never commit copyrighted BIOS images, music, or private device information.
