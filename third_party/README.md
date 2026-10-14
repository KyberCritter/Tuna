# Third-party code

`minimp3/minimp3.h` and `minimp3/LICENSE` are unmodified from
lieff/minimp3 revision `ea99364f61c14656440e8d77e9c233ccf3124633` (CC0).
Only the scalar Layer III decoder is compiled, on host/PowerPC, never GBA.
The Xing/LAME gapless layout and 529-sample decoder-latency handling in
`host/convert.c` follow `minimp3_ex.h` at that same CC0 revision.

`shared/boot.c` derives the BIOS Joybus key/cipher algorithm from FIX94's
MIT sender revision `615f39ae6674df1736ae8d92fa7f655fff65386e`.
Its attribution/license is retained in `LICENSES/FIX94-MIT.txt`.
No Dolphin or GPL Joybus tester implementation is copied.

Tuna is based in part on the work of the devkitPro project, including unmodified
libgba (LGPL with its static-linking exception) and libogc. Console packages
include their installed license notices. Toolchain/runtime components retain
their original licenses; the repository's MIT license applies to Tuna's own code.

The C build helper and SHA-256 implementation in `tools/` are Tuna's original
MIT code (SHA-256 follows the published FIPS 180-4 specification). GNU Make and
GCC are separately licensed build tools, not code incorporated into Tuna by
running the build. Their use does not relicense Tuna's source under the GPL.
