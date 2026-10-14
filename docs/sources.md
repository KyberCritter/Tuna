# Research sources

Reviewed 2026-10-14. Source support is not the same as a completed local test.
Downloaded reference copies belong in `.agents/docs/`, not tracked source folders.

Local source snapshots (with retrieval times and SHA256 in the ignored manifest):

- Dolphin: `8d2ad1a662472955497e5bb79d7b9ec6999e1f0d`
- FIX94 sender: `615f39ae6674df1736ae8d92fa7f655fff65386e`
- afska tester: `7faea95a37d78cecd3d832c5a5fe64d551eec117`

These pin the inspected sources, not an installed/tested emulator binary.

- [Dolphin integrated GBA announcement](https://dolphin-emu.org/blog/2021/07/21/integrated-gba/): synchronized mGBA, BIOS configuration, state interchange, integrated core lifetime limitations.
- [Dolphin GBACore.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/GBACore.cpp): no-ROM boot, BIOS load, Joybus routing, `m_force_disconnect`, raw state import/export.
- [Dolphin GBAWidget.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DolphinQt/GBAWidget.cpp): Connected toggle calls SetForceDisconnect without destroying core.
- [Dolphin SI_DeviceGBAEmu.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/SI/SI_DeviceGBAEmu.cpp): integrated SI command path and continued synchronization.
- [Dolphin SI.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/SI/SI.cpp): shared SI initialization and device removal lifecycle.
- [mGBA Window.cpp](https://github.com/mgba-emu/mgba/blob/master/src/platform/qt/Window.cpp): Boot BIOS, Connect to Dolphin, state-loading UI.
- [FIX94 GBA Link Cable ROM Sender](https://github.com/FIX94/gba-link-cable-rom-sender): existing Wii-native `SI_Transfer`, Joybus cipher/CRC and 256 KiB image envelope; MIT licensed.
- [afska gba-joybus-tester](https://github.com/afska/gba-joybus-tester): Wii runtime 0x14/0x15 read/write test; GPL licensing must be respected.
- [afska gba-link-connection / LinkCube](https://github.com/afska/gba-link-connection): GBA-side Joybus implementation and examples.
- [libogc](https://github.com/devkitPro/libogc): Wii/GameCube library and SI API.
- [GBATEK](https://mgba-emu.github.io/gbatek/): memory map, BIOS/multiboot, Joybus registers, audio FIFO/timers/DMA. Prefer primary register details over secondary articles.
- [GBA memory layout](https://gbadev.net/gbadoc/memory.html): 256 KiB EWRAM and 32 KiB IWRAM.
- [GBATEK DMA Sound](https://www.problemkaputt.de/gbatek-gba-sound-channel-a-and-b-dma-sound.htm): signed 8-bit samples, 32-byte FIFO, sample counting and hardware rates.
- [devkitPro Getting Started](https://devkitpro.org/wiki/Getting_Started): Windows installer/MSYS2 and console-dev package groups.
- [Dolphin downloads](https://dolphin-emu.org/download/): official Windows builds.
- [mGBA downloads](https://mgba.io/downloads.html): standalone builds.
- [Wiiload](https://wiibrew.org/wiki/Wiiload): HBC network deployment.

## Corrections to the initial conversational sketch

- Do not promise complete typical songs with 4-bit ADPCM and 192 KiB storage.
- Do not assert 16 KiB as the general GBA multiboot size limit.
- Monochrome UI is not a native 1bpp GBA framebuffer mode.
- "Disconnect" must preserve the emulated GBA instance, not remove its SI device.
- Do not equate a GameCube-mode link test with a Wii-mode link test.
- An actual user-supplied GBA BIOS is a prerequisite for faithful cartridge-free
  multiboot verification, not an incidental optional asset.
- Internal CD support is excluded; no untested USB optical support is promised.
