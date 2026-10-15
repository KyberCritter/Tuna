#ifndef TUNA_VERSION_H
#define TUNA_VERSION_H

/* Canonical Tuna release version. This is the ONLY place the release number is
   defined; everything user-visible (Homebrew Channel metadata, the GBA player
   header, the Wii/GameCube sender heading, the README and docs) must use it.
   The matching GitHub release tag is "v" + TUNA_VERSION_STRING (e.g. v0.1.3).
   Do not hardcode a version anywhere else; see .opencode/AGENTS.md. */
#define TUNA_VERSION_MAJOR 0
#define TUNA_VERSION_MINOR 1
#define TUNA_VERSION_PATCH 3
#define TUNA_VERSION_STRING "0.1.3"

#endif
