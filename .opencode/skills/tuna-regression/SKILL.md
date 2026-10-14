---
name: Tuna Regression
description: Build and test Tuna changes, record evidence, and checkpoint completed portions without confusing host tests with emulator or hardware acceptance.
---

1. Read `docs/build.md`, `docs/validation.md`, and `git status --short` at the project root.
2. Run `scripts/Verify.ps1` beside this skill. It invokes the repository build/test scripts and saves a report in ignored `artifacts/`.
3. For codec/protocol changes, add golden vectors, boundary/malformed-input tests,
   and idempotent retransmission tests in `tests/test.c`.
4. For GBA audio changes, host tests are insufficient: preserve linker maps and run
   the standalone/integrated emulator acceptance checklist. Never assert READY,
   disconnect, audible output, or cartridge-free multiboot based on a mock alone.
5. Commit only the intended source changes after tests pass. Push only when the
   user authorized it (the original project implementation request did).
6. Do not commit generated media, tools, BIOS, emulator profiles or evidence.

Do not delegate automatically. Use the scripts rather than reconstructing build
commands repeatedly. Missing prerequisites are a failed gate, not permission to
skip the gate or substitute an embedded ROM for multiboot.
