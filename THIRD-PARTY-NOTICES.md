# Third-party notices and license scope

Copyright © 2026 Scott Ratchford. Tuna's original code and documentation are
licensed under the MIT license in `LICENSE`. This does not replace the licenses
of third-party code incorporated into the executables.

Tuna is based in part on the work of the devkitPro project (libgba/libogc).
The libraries and runtime components listed below are used without modification.
Keep this file, `LICENSE`, and the accompanying `LICENSES/` directory with
redistributed binaries. The Wii `apps/tuna/` folder includes its own copies.

| Component | Used by | License / notice |
| --- | --- | --- |
| FIX94 Joybus key/cipher implementation | Desktop converter and console senders | MIT; Copyright (c) 2018 FIX94; `LICENSES/FIX94-MIT.txt` |
| minimp3 and gapless-layout reference | Desktop converter and console senders | CC0-1.0; `LICENSES/minimp3-CC0.txt` in binary packages, `third_party/minimp3/LICENSE` in source |
| libgba | GBA player, also embedded in both senders | LGPL version 2 with static-linking exceptions; `LICENSES/libgba.txt` in binary packages |
| libogc | GameCube/Wii senders | zlib-style license and RTEMS BSD notices; `LICENSES/libogc.txt` in binary packages |
| libogc's wiiuse | Wii sender | GPLv3 with the libogc independent-module linking exception; `LICENSES/wiiuse-GPL-3.0-with-exception.txt` |
| BTE/lwBT | Wii sender | libogc terms and BSD-3-Clause portions; `LICENSES/lwBT-BSD.txt` |
| libdvm (installed as libfat-ogc 2.1.0) | GameCube/Wii senders | ZPL-2.1; Copyright fincs, devkitPro; `LICENSES/libdvm-ZPL-2.1.txt` |
| FatFs | GameCube/Wii senders | ChaN's permissive license; `LICENSES/FatFs.txt` |
| devkitARM GBA startup code | GBA player and embedded copies in senders | MPL-2.0; `LICENSES/MPL-2.0.txt`; source availability below |
| newlib C/math runtime | Console binaries | Multiple upstream permissive licenses; `LICENSES/newlib.txt` |
| GCC runtime (libgcc) | GCC-built binaries | GPLv3 with GCC Runtime Library Exception 3.1; `LICENSES/GPL-3.0.txt` and `LICENSES/GCC-Runtime-Library-Exception-3.1.txt` |
| MinGW-w64 runtime | Windows GCC desktop converter | Upstream runtime notices; `LICENSES/MinGW-w64-runtime.txt` |

The libgba exception permits static linking without imposing LGPL licensing or
application-source/relinking requirements on Tuna. Its required devkitPro
acknowledgement appears above. Changes to libgba itself would retain LGPL
obligations.

The wiiuse exception explicitly permits linking independent modules and
distributing the executable under terms of your choice, subject to each module's
license. Tuna's application code is independent of wiiuse. This exception is
specific to the libogc distribution; do not substitute a different wiiuse build
and assume it has the same permission.

The GCC runtime exception permits distribution of independent application code
under its own license after an eligible compilation process. Running GNU Make
or GCC does not by itself impose GPL licensing on Tuna's source.

## MPL source availability

The GBA image incorporates unmodified `gba_crt0.s` from devkitarm-crtls 1.2.6.
Its Source Code Form is available under MPL-2.0, at no charge:

- [Exact startup source](https://raw.githubusercontent.com/devkitPro/devkitarm-crtls/9788af74f0f3cfd903cffdc903d514fc073a0f95/gba_crt0.s)
- [Corresponding startup package source](https://github.com/devkitPro/devkitarm-crtls/tree/9788af74f0f3cfd903cffdc903d514fc073a0f95)

Recipients may obtain, modify, and redistribute that covered source under
MPL-2.0. Tuna's MIT license does not limit those rights. MPL's file-level copyleft
does not require Tuna's separate application files to be MPL-licensed. A
redistributor must keep the corresponding covered source available and update
these directions if using a different or modified startup component.

## Other material

The bundled jingle and compact GBA glyphs are original Tuna material. No user
music or BIOS dump is included. Tuna's license does not grant rights to music,
BIOS images, trademarks, or other material supplied separately by users.

See `LICENSES/README.md` for license provenance and the release checklist.
