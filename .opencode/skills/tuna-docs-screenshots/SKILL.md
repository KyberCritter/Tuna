---
name: Tuna Documentation Screenshots
description: Capture the README/documentation screenshots of the Wii sender and the integrated GBA with a self-contained Python 3 toolkit that drives Dolphin itself (no PowerShell, no Bash, no manual input, no hard-coded paths).
---

# Documentation screenshot capture

Use this when the user wants fresh screenshots of Tuna's sender (Wii/GameCube)
and its resident GBA player for `README.md` or other documentation. It is a
**step-by-step toolkit**, not one monolith: you run the steps in order and the
script owns the emulator. No keystrokes are sent to the focused window, and no
shell script (PowerShell or Bash) is involved.

This complements `tuna-emulator`: that skill *verifies behaviour* and records
evidence; this one *produces presentation images*. Do not confuse the two.

## Hard rules

- **Python 3 standard library only.** No third-party packages, no `.ps1`, no
  `.sh`. The toolkit talks to Win32 directly through `ctypes`.
- **No hard-coded paths.** The emulator, BIOS, DOL, source audio, run directory
  and output directory are all arguments. The repository root is derived from
  the script location.
- **One Dolphin at a time.** `stage`/`launch` refuse to run if any `Dolphin.exe`
  is already running. Only the process recorded in the run manifest is ever
  controlled or stopped, and its PID, executable and start time are rechecked.
- **Never commit BIOS, music, `.tuna` files, SD images or run evidence.** Only
  the final PNGs you intentionally choose are meant to be tracked
  (normally under `docs/images/`). Run state lives in the Git-ignored
  `.local/docs-screenshots/`.
- **Input only through Dolphin's own TAS controls,** toggled via UI Automation —
  never foreground keyboard injection or desktop clicks.

## Prerequisites

- Windows (the capture/drive layer is Win32; the file layout is otherwise
  portable).
- Python 3 available as `python`.
- A Dolphin build with the GBA (integrated) SI device and TAS input windows.
- A built Tuna Wii DOL (and the matching player image is inside it).
- A legally obtained 16 KiB GBA BIOS. Never download one and never commit it.
- A source track (MP3). Its **guest filename** decides the GBA title, so pass
  `--name` when the file's own name is not what you want shown.

## Steps

Run everything from the skill's `scripts` directory (or add it to `PYTHONPATH`):

```sh
cd <repo>/.opencode/skills/tuna-docs-screenshots/scripts
```

1. **Stage** an isolated profile, a private SD folder and the source track. The
   folder holds only the track you pass, so the browser's selection order is
   deterministic.

   ```sh
   python -m tuna_shots stage --run-dir <run> \
       --bios <path-to-gba_bios.bin> \
       --mp3 <path-to-without-id3-or-not.mp3> \
       --name Animal.mp3
   ```

2. **Launch** the owned emulator (detached, so it survives this short process):

   ```sh
   python -m tuna_shots launch --run-dir <run> \
       --dolphin <path-to-Dolphin.exe> \
       --dol <path-to-build/wii/tuna.dol>
   ```

3. **Open the TAS input windows** and make the checkboxes (not any physical
   device) authoritative:

   ```sh
   python -m tuna_shots open-tas --run-dir <run>
   ```

4. **Drive and capture.** `press --target gc` is the port-1 controller;
   `press --target gbatas` is the integrated GBA's buttons. `shot` crops to the
   window's client area (no Dolphin title bar). `burst` captures a dense series
   of frames while holding a button and keeps the sparsest non-blank frame —
   the only reliable way to catch a screen that lasts a couple of frames.

   ```sh
   python -m tuna_shots press --run-dir <run> --target gc --button Down
   python -m tuna_shots shot  --run-dir <run> --target sender --name wii-startup --out-dir <docs/images>
   python -m tuna_shots press --run-dir <run> --target gc --button A
   # wait for the conversion preview to appear, then:
   python -m tuna_shots shot  --run-dir <run> --target sender --name wii-preview --out-dir <docs/images>
   python -m tuna_shots press --run-dir <run> --target gc --button A        # confirm the send
   # wait for "GBA verified CRC ... READY. Safe to unplug." then:
   python -m tuna_shots shot  --run-dir <run> --target gba --name gba-player --out-dir <docs/images>
   python -m tuna_shots press --run-dir <run> --target gbatas --button A
   python -m tuna_shots shot  --run-dir <run> --target gba --name gba-playing --out-dir <docs/images>
   # SECOND send, now to an already-booted GBA, so WAITING FOR TRACK appears immediately:
   python -m tuna_shots press --run-dir <run> --target gc --button B        # back to the browser
   python -m tuna_shots press --run-dir <run> --target gc --button A        # cached preview
   python -m tuna_shots burst --run-dir <run> --target gba --name gba-startup \
       --press-target gc --press-button A --out-dir <docs/images>
   ```

   Capture `gba-startup` on a **second** send. The first send spends seconds in
   multiboot, so a burst there can catch only the GBA BIOS logo; once the GBA is
   booted, `WAITING FOR TRACK` shows right after the link reset that opens every
   send.

5. **Stop** only the owned instance:

   ```sh
   python -m tuna_shots stop --run-dir <run>        # add --force only if graceful close stalls
   ```

`windows` lists the owned process's windows when something cannot be found.

## Full README recipe

The set used in `README.md` (see it for placement) is:

| Image | Step | Screen |
| --- | --- | --- |
| `gba-player`  | intro        | GBA player with the track loaded (`PAUSED`, `SAFE TO UNPLUG`) |
| `wii-startup` | Run Tuna 3   | Wii browser with the track selected |
| `wii-preview` | Run Tuna 7   | post-conversion preview (duration + chosen profile) |
| `gba-startup` | Run Tuna 9–10| GBA `WAITING FOR TRACK` (via `burst` on a second send) |
| `gba-playing` | Run Tuna 11  | GBA `PLAYING` |

The conversion wait is not fixed: an emulated Wii can take tens of seconds per
track. Poll a `shot` until the preview text stops changing rather than sleeping
a constant. A second pass over an already-converted track reuses the cached
`.tuna` and shows `Using completed conversion ...` instead of converting again;
use a fresh `--run-dir` (fresh SD image) when you specifically want the
conversion screen.

## Troubleshooting

- **The render/GBA windows vanish shortly after launch.** Dolphin was killed
  with its launcher's job object. The toolkit launches it detached; if it still
  dies, run the launcher from a session that stays alive.
- **TAS toggles have no effect on the game.** `Enable Controller Input` must be
  **On** in each TAS window; `open-tas` enforces this.
- **`burst` picks the wrong frame.** Inspect the numbered frames under
  `<run>/burst/`; the waiting screen is normally the sparsest one. Tighten
  `--count` or re-run so the burst starts closer to the button press.
