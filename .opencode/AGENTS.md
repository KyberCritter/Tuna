# Project instructions

- Downloaded references belong only in `.agents/docs/` (Git-ignored).
- Local BIOS dumps, tools, and emulator profiles belong in `.local/` (also Git-ignored).
- Never commit copyrighted BIOS images, music, or private device information.
- As you finish tasks given by the user, commit and push.
- Verify your work with the skill `.opencode/skills/tuna-emulator` before committing unless otherwise ordered.

## Version number

- The release version is defined in exactly one place: `include/version.h`
  (`TUNA_VERSION_STRING`, with `TUNA_VERSION_MAJOR/MINOR/PATCH`). GitHub releases
  are tagged `v` + that string (for example `v0.1.3`). Tuna starts at `v0.1.0`.
- Every user-visible version reference must use that value. Do not hardcode a
  version string anywhere else. Current users of it:
  - Homebrew Channel metadata: `assets/meta.xml` `<version>` (the `<app version>`
    attribute is the HBC schema version and stays `1`).
  - GBA player header (`gba/main.c`, from `TUNA_VERSION_STRING`).
  - Wii/GameCube sender heading (`sender/main.c`).
  - `README.md` and the docs.
- `TUNA_FORMAT_VERSION` in `include/tuna.h` is the **wire/container format
  revision**, not a release version. It only changes when the on-the-wire layout
  changes, and docs call it the "format revision" (never "v1"/"v2"). The sender's
  conversion cache tag (`host/track_store.c`, currently `f2`) is likewise a
  format/cache revision, independent of the release version.
- To cut a release: bump the three numbers and `TUNA_VERSION_STRING` in
  `include/version.h`, update the `assets/meta.xml` `<version>`, update the
  `README.md` "Current release" line, then tag `v<version>` on GitHub. Do not
  reuse or renumber format/cache revisions for a release-only change.
