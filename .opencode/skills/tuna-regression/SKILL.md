---
name: Tuna Regression
description: Build and test Tuna changes, record evidence, and checkpoint completed portions without confusing host tests with emulator or hardware acceptance.
---

1. Read `docs/build.md`, `docs/validation.md`, and `git status --short` at the project root.
2. Run `sh scripts/Verify.sh host` beside this skill (or omit `host` for all targets).
   It invokes the root GNU Makefile and saves a report in ignored `artifacts/`.
   `Verify.ps1` remains an optional Windows wrapper, not the portable build path.
   For real MP3 fixtures run root `scripts/Test-Mp3.ps1` with a verified FFmpeg.
   Root `scripts/Test-Gba.ps1` is a separate embedded-fixture/HLE/GDB lane; it is
   currently blocked on the installed SDL build's missing GDB listener and must
   never be reported as a multiboot, captured-audio or disconnect pass.
3. For codec/protocol changes, add golden vectors, boundary/malformed-input tests,
   and idempotent retransmission tests in `tests/test.c`.
4. For GBA audio changes, host tests are insufficient: preserve linker maps and run
   the standalone/integrated emulator acceptance checklist. Never assert READY,
   disconnect, audible output, or cartridge-free multiboot based on a mock alone.
   Use `tuna-emulator` for the concrete local Windows Dolphin procedure, scripts,
   process-owned TAS/screenshot/state handling and mandatory single-instance cleanup.
5. Commit only the intended source changes after tests pass. Push only when the
   user authorized it (the original project implementation request did).
6. Do not commit generated media, tools, BIOS, emulator profiles or evidence.

Do not delegate automatically. Use the scripts rather than reconstructing build
commands repeatedly. Missing prerequisites are a failed gate, not permission to
skip the gate or substitute an embedded ROM for multiboot.
