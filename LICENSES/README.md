# License provenance and release checklist

Reviewed 2026-10-14 against the local linker maps and installed packages.
These notices do not relicense third-party components under Tuna's MIT license.
The full upstream newlib and MinGW-w64 notice collections are retained; inclusion
of a notice does not mean every module in those collections is linked.

## Provenance

- `FIX94-MIT.txt`: FIX94/gba-link-cable-rom-sender revision
  `615f39ae6674df1736ae8d92fa7f655fff65386e`, `LICENSE`.
- `minimp3-CC0.txt` (packaged): copied from `third_party/minimp3/LICENSE`,
  lieff/minimp3 revision `ea99364f61c14656440e8d77e9c233ccf3124633`.
- `libgba.txt` (packaged): copied from the installed libgba 0.5.4
  `libgba_license.txt`, including the LGPL static-linking exceptions.
- `libogc.txt` (packaged): copied from the installed libogc 3.1.0 `LICENSE`.
- `wiiuse-GPL-3.0-with-exception.txt`: verbatim libogc 3.1.0
  [`wiiuse/license_libogc.txt`](https://github.com/devkitPro/libogc/blob/c70bdf2fc36bf6c6c12d946ae3ad70d604366d1b/wiiuse/license_libogc.txt),
  including section 18, not the license of a separately installed wiiuse.
- `lwBT-BSD.txt`: notice in libogc 3.1.0
  [`lwbt/hci.c`](https://github.com/devkitPro/libogc/blob/c70bdf2fc36bf6c6c12d946ae3ad70d604366d1b/lwbt/hci.c).
- `libdvm-ZPL-2.1.txt`: libdvm 2.1.0
  [`COPYING`](https://github.com/devkitPro/libdvm/blob/3006b93714b2cac7e052dbc44903b3ef29d2965a/COPYING)
  with the fincs/devkitPro copyright notice from its source headers. Installed
  package: `libfat-ogc` 2.1.0-4; the local maps show libdvm/FatFs objects.
- `FatFs.txt`: notices in devkitPro/fatfs-mod
  [`source/ff.c`](https://github.com/devkitPro/fatfs-mod/blob/6d3e8f89202223bf70e957357dd31405b17d1f95/source/ff.c)
  and `source/ffunicode.c` at that revision (R0.15a w/patch 1).
- `MPL-2.0.txt`: Mozilla's verbatim MPL-2.0 text. Applies to unmodified
  devkitarm-crtls 1.2.6 `gba_crt0.s`; exact source directions are in
  `THIRD-PARTY-NOTICES.md`, which is supplied with each binary package.
- `newlib.txt`: verbatim devkitPro/newlib
  [`COPYING.NEWLIB`](https://github.com/devkitPro/newlib/blob/57bd8e1ade3cf46d2e61a72516ac8bfaf8fdfd62/COPYING.NEWLIB)
  from the 4.6.0 devkitPro branch. Installed ARM/PPC packages:
  4.6.0.20260123-5 and 4.6.0.20260123-4 respectively.
- `GPL-3.0.txt` and `GCC-Runtime-Library-Exception-3.1.txt`: verbatim
  `COPYING3`/`COPYING.RUNTIME` supplied by MSYS2's UCRT64 libgcc package.
  The console toolchains use the same GCC runtime license/exception.
- `MinGW-w64-runtime.txt`: verbatim
  `ucrt64/share/licenses/crt/COPYING.MinGW-w64-runtime.txt` from the local MSYS2
  toolchain used for the Windows converter.

## Before distributing

1. Ship `LICENSE`, `THIRD-PARTY-NOTICES.md`, and the entire packaged `LICENSES/`
   directory. `make package` also includes them in `apps/tuna/` for HBC-only
   redistribution and covers them in `sha256.txt`.
2. Keep MPL-covered startup source available at the exact locations described in
   the notices, or provide an equivalent source archive with the release. Update
   the source directions for any modified or upgraded startup code. Do not
   assume a generic upstream homepage is corresponding source for every build.
3. Re-audit the actual linked components after library/toolchain/compiler
   changes, including platform-specific runtimes. The build does not download
   licenses, and the pinned notices are not an automatic license inventory for
   arbitrary future toolchains.
4. Do not import GPL reference implementations (e.g. Dolphin or the Joybus tester)
   into MIT application files without checking their actual terms. Using them as
   research references or separate testing tools is not the same as copying code.
5. Do not distribute BIOS dumps, user music, downloaded emulator/toolchain
   bundles, private profiles, or ignored research snapshots with Tuna.
