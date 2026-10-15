"""Dolphin orchestration for the Tuna docs screenshot toolkit.

The module starts an isolated, single-instance Dolphin, drives it exclusively
through its own TAS input controls, and captures cropped PNGs. Every host path
(emulator, BIOS, DOL, source audio, output directory) is a parameter; the only
paths it derives are relative to the run directory and the supplied arguments.
"""
from __future__ import annotations

import json
import shutil
import subprocess
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

from . import image, uia, win32

SD_IMAGE_BYTES = 134217728  # 128 MiB private SD image, matching the emulator skill


def repo_root() -> Path:
    """Derive the repository root from this file's location (no literal path)."""
    return Path(__file__).resolve().parents[5]


def default_run_dir() -> Path:
    stamp = time.strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:8]
    return repo_root() / ".local" / "docs-screenshots" / stamp


def default_out_dir() -> Path:
    return repo_root() / "docs" / "images"


def _posix(path: Path) -> str:
    return str(path).replace("\\", "/")


# ---------------------------------------------------------------------------
# Manifest
# ---------------------------------------------------------------------------
MANIFEST = "manifest.json"


def _manifest_path(run_dir: Path) -> Path:
    return run_dir / MANIFEST


def load_manifest(run_dir: Path) -> dict:
    path = _manifest_path(run_dir)
    if not path.is_file():
        raise SystemExit(f"No run manifest at {path}; run `stage` first.")
    return json.loads(path.read_text(encoding="utf-8"))


def save_manifest(run_dir: Path, manifest: dict) -> None:
    _manifest_path(run_dir).write_text(json.dumps(manifest, indent=2), encoding="utf-8")


def assert_owned(manifest: dict) -> int:
    """Confirm the recorded process is still the exact one we launched."""
    pid = int(manifest["pid"])
    if not win32.process_alive(pid):
        raise SystemExit("Owned Dolphin has exited.")
    expected = manifest["dolphin"]
    actual = win32.process_image_path(pid)
    if not actual or Path(actual).resolve() != Path(expected).resolve():
        raise SystemExit("PID/executable mismatch: refusing to control an unrelated process.")
    created = win32.process_creation_time(pid)
    if created != manifest.get("pid_created"):
        raise SystemExit("Process start-time mismatch: refusing to control a reused PID.")
    return pid


# ---------------------------------------------------------------------------
# Stage
# ---------------------------------------------------------------------------
def stage(
    run_dir: Path,
    bios: Path,
    mp3: Path,
    mp3_name: Optional[str] = None,
    platform: str = "wii",
) -> dict:
    if win32.find_processes("Dolphin.exe"):
        raise SystemExit("Dolphin is already running. Close it; never start a second instance.")
    run_dir = Path(run_dir)
    profile = run_dir / "user"
    config = profile / "Config"
    sd_folder = run_dir / "sd-folder"
    music = sd_folder / "music"
    for directory in (config, music, sd_folder / "tuna"):
        directory.mkdir(parents=True, exist_ok=True)

    name = mp3_name or Path(mp3).name
    if not name.lower().endswith(".mp3"):
        raise SystemExit(f"Guest track name must end in .mp3: {name!r}")
    shutil.copyfile(mp3, music / name)

    sd_raw = run_dir / "sd.raw"
    ini = "\n".join(
        [
            "[Core]",
            "SIDevice0 = 6",  # standard controller, port 1
            "SIDevice1 = 13",  # integrated GBA, port 2
            "WiiSDCard = True",
            "WiiSDCardEnableFolderSync = True",
            "WiiSDCardAllowWrites = True",
            f"WiiSDCardFilesize = {SD_IMAGE_BYTES}",
            "[General]",
            "ISOPaths = 0",
            f"WiiSDCardPath = {_posix(sd_raw)}",
            f"WiiSDCardSyncFolder = {_posix(sd_folder)}",
            "[Interface]",
            "PauseOnFocusLost = False",
            "ConfirmStop = False",
            "[GBA]",
            f"BIOS = {_posix(Path(bios))}",
            "Rom1 = ",
            "Rom2 = ",
            "Rom3 = ",
            "Rom4 = ",
            "[Analytics]",
            "Enabled = False",
            "PermissionAsked = True",
        ]
    )
    (config / "Dolphin.ini").write_text(ini, encoding="utf-8")
    # Physical input is disabled; TAS checkboxes are the only input source.
    (config / "GCPadNew.ini").write_text("[GCPad1]\nDevice = None/0/Test\n", encoding="utf-8")
    (config / "Hotkeys.ini").write_text("[Hotkeys]\nDevice = None/0/Test\n", encoding="utf-8")

    manifest = {
        "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "platform": platform,
        "run_dir": str(run_dir),
        "profile": str(profile),
        "sd_folder": str(sd_folder),
        "sd_raw": str(sd_raw),
        "mp3": str(Path(mp3)),
        "mp3_name": name,
        "bios": str(Path(bios)),
        "dolphin": None,
        "dol": None,
        "pid": None,
        "pid_created": None,
        "status": "staged",
    }
    save_manifest(run_dir, manifest)
    return manifest


# ---------------------------------------------------------------------------
# Launch / attach
# ---------------------------------------------------------------------------
def _wait_for_window(pid: int, predicate, timeout: float = 25.0) -> win32.WindowInfo:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for window in win32.windows_for_pid(pid):
            if predicate(window.title):
                return window
        time.sleep(0.1)
    raise SystemExit("Timed out waiting for the Dolphin render window.")


def _detached_flags() -> int:
    """Flags that let Dolphin outlive this short-lived launcher process.

    Capture steps run as separate short processes; if Dolphin stayed inside the
    launcher's job object it would be torn down the moment the step returned.
    """
    flags = 0
    for name in ("DETACHED_PROCESS", "CREATE_NEW_PROCESS_GROUP", "CREATE_BREAKAWAY_FROM_JOB"):
        flags |= int(getattr(subprocess, name, 0))
    return flags


def launch(run_dir: Path, dolphin: Path, dol: Path) -> dict:
    manifest = load_manifest(run_dir)
    if win32.find_processes("Dolphin.exe"):
        raise SystemExit("Dolphin is already running. Close it; never start a second instance.")
    dolphin = Path(dolphin).resolve()
    dol = Path(dol).resolve()
    if not dolphin.is_file():
        raise SystemExit(f"Dolphin executable not found: {dolphin}")
    if not dol.is_file():
        raise SystemExit(f"DOL not found: {dol}")

    command = [str(dolphin), "--user", manifest["profile"], "--exec", str(dol)]
    try:
        proc = subprocess.Popen(command, creationflags=_detached_flags())
    except OSError:
        # Some job objects forbid breakaway; keep the process group/detach flags.
        flags = int(getattr(subprocess, "DETACHED_PROCESS", 0)) | int(
            getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
        )
        proc = subprocess.Popen(command, creationflags=flags)
    try:
        _wait_for_window(proc.pid, lambda title: "|" in title)
    except SystemExit:
        proc.terminate()
        raise

    manifest.update(
        {
            "dolphin": str(dolphin),
            "dol": str(dol),
            "pid": proc.pid,
            "pid_created": win32.process_creation_time(proc.pid),
            "status": "running",
        }
    )
    save_manifest(run_dir, manifest)
    return manifest


# ---------------------------------------------------------------------------
# Windows
# ---------------------------------------------------------------------------
_TARGETS = {
    "main": lambda title: title.startswith("Dolphin ") and "|" not in title,
    "sender": lambda title: title.startswith("Dolphin ") and "|" in title,
    "gba": lambda title: title.startswith("GBA2 |"),
    "gc": lambda title: title == "GameCube TAS Input 1",
    "gbatas": lambda title: title == "GBA TAS Input 2",
    "wiitas": lambda title: title.startswith("Wii TAS Input 1"),
}


def window(manifest: dict, target: str) -> win32.WindowInfo:
    assert_owned(manifest)
    if target not in _TARGETS:
        raise SystemExit(f"Unknown target {target!r}; expected one of {sorted(_TARGETS)}")
    matches = [w for w in win32.windows_for_pid(int(manifest["pid"])) if _TARGETS[target](w.title)]
    if len(matches) != 1:
        # The sender render window owns the menus in some builds; fall back for
        # 'main' so menu actions still find a home.
        if target == "main":
            render = [w for w in win32.windows_for_pid(int(manifest["pid"])) if "|" in w.title]
            if len(render) == 1:
                return render[0]
        raise SystemExit(f"Expected exactly one {target} window; found {len(matches)}.")
    return matches[0]


def list_windows(manifest: dict) -> List[win32.WindowInfo]:
    assert_owned(manifest)
    return win32.windows_for_pid(int(manifest["pid"]))


# ---------------------------------------------------------------------------
# TAS input
# ---------------------------------------------------------------------------
def _find_in_all_windows(auto: uia.Automation, manifest: dict, name: str) -> Optional[int]:
    for window_info in list_windows(manifest):
        root = auto.element_from_handle(window_info.hwnd)
        try:
            found = auto.find_first(root, name)
        finally:
            auto.release(root)
        if found:
            return found
    return None


def open_tas(manifest: dict) -> None:
    """Open the Movie > TAS Input windows and make the TAS checkboxes authoritative."""
    auto = uia.Automation()
    try:
        main = window(manifest, "main")
        root = auto.element_from_handle(main.hwnd)
        try:
            menu = auto.find_first(root, "Movie")
            if not menu:
                raise SystemExit("No 'Movie' menu found in the Dolphin window.")
            pattern = auto.get_pattern(menu, uia.UIA_ExpandCollapsePatternId)
            if not pattern:
                raise SystemExit("The 'Movie' menu does not expose ExpandCollapse.")
            auto.expand(pattern)
            auto.release(pattern)
            auto.release(menu)
            time.sleep(0.4)

            item = _find_in_all_windows(auto, manifest, "TAS Input")
            if not item:
                raise SystemExit("No 'TAS Input' menu item found after expanding Movie.")
            pattern = auto.get_pattern(item, uia.UIA_InvokePatternId)
            if not pattern:
                raise SystemExit("The 'TAS Input' item does not expose Invoke.")
            auto.invoke(pattern)
            auto.release(pattern)
            auto.release(item)
        finally:
            auto.release(root)

        for target in ("gc", "gbatas"):
            info = _wait_for_window_any(manifest, target)
            root = auto.element_from_handle(info.hwnd)
            try:
                toggle = auto.find_first(root, "Enable Controller Input")
                if toggle:
                    pattern = auto.get_pattern(toggle, uia.UIA_TogglePatternId)
                    if pattern:
                        # This must be On: it is what makes the TAS checkboxes
                        # (rather than any physical device) drive the controller.
                        if auto.toggle_state(pattern) != uia.ToggleState_On:
                            auto.set_toggle_state(pattern, uia.ToggleState_On)
                        auto.release(pattern)
                    auto.release(toggle)
            finally:
                auto.release(root)
    finally:
        auto.close()


def _wait_for_window_any(manifest: dict, target: str, timeout: float = 10.0) -> win32.WindowInfo:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            return window(manifest, target)
        except SystemExit:
            time.sleep(0.1)
    raise SystemExit(f"Timed out waiting for the {target} TAS window.")


_NAME_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"


def _valid_name(name: str) -> None:
    if not name or any(ch not in _NAME_CHARS for ch in name):
        raise SystemExit("Screenshot name must be alphanumeric, dash or underscore.")


def _engage_button(auto, manifest: dict, target: str, button: str):
    """Return ``(root, element, pattern)`` for a TAS button and assert it is idle."""
    info = window(manifest, target)
    root = auto.element_from_handle(info.hwnd)
    element = auto.find_first(root, button)
    if not element:
        auto.release(root)
        raise SystemExit(f"No TAS control named {button!r} in the {target} window.")
    pattern = auto.get_pattern(element, uia.UIA_TogglePatternId)
    if not pattern:
        auto.release(element)
        auto.release(root)
        raise SystemExit(f"The {button!r} control does not expose Toggle.")
    if auto.toggle_state(pattern) != uia.ToggleState_Off:
        auto.release(pattern)
        auto.release(element)
        auto.release(root)
        raise SystemExit(f"TAS control {button!r} is already engaged.")
    return root, element, pattern


def press(manifest: dict, target: str, button: str, milliseconds: int = 200) -> None:
    auto = uia.Automation()
    try:
        root, element, pattern = _engage_button(auto, manifest, target, button)
        try:
            auto.set_toggle_state(pattern, uia.ToggleState_On)
            time.sleep(milliseconds / 1000.0)
        finally:
            auto.set_toggle_state(pattern, uia.ToggleState_Off)
            auto.release(pattern)
            auto.release(element)
            auto.release(root)
    finally:
        auto.close()
    time.sleep(0.1)


def _lit_pixels(bgra: bytes) -> int:
    lit = 0
    for index in range(0, len(bgra), 4):
        if bgra[index] > 8 or bgra[index + 1] > 8 or bgra[index + 2] > 8:
            lit += 1
    return lit


def burst(
    manifest: dict,
    target: str,
    name: str,
    count: int,
    out_dir: Path,
    press_target: Optional[str] = None,
    press_button: Optional[str] = None,
) -> Path:
    """Capture ``count`` dense frames and keep the sparsest non-blank one.

    A short-lived screen (the GBA's ``WAITING FOR TRACK`` between multiboot and
    the first data packet) only lasts a couple of frames, so a single capture
    misses it. This grabs a dense burst and picks the frame with the fewest lit
    pixels that is not blank -- the waiting screen is the sparsest screen drawn
    during a transfer. All frames are kept under the run directory for review.
    """
    _valid_name(name)
    auto = uia.Automation()
    held = None
    frames = []
    try:
        if press_target and press_button:
            root, element, pattern = _engage_button(auto, manifest, press_target, press_button)
            held = (root, element, pattern)
            auto.set_toggle_state(pattern, uia.ToggleState_On)
        info = window(manifest, target)
        for _ in range(count):
            frames.append(win32.capture_client_bgra(info.hwnd))
    finally:
        if held:
            root, element, pattern = held
            auto.set_toggle_state(pattern, uia.ToggleState_Off)
            auto.release(pattern)
            auto.release(element)
            auto.release(root)
        auto.close()

    scored = [
        (_lit_pixels(bgra), index, width, height, bgra)
        for index, (width, height, bgra) in enumerate(frames)
    ]
    candidates = [item for item in scored if item[0] > 0]
    if not candidates:
        raise SystemExit("Burst captured only blank frames.")
    candidates.sort(key=lambda item: (item[0], item[1]))
    lit, chosen, width, height, bgra = candidates[0]

    evidence = Path(manifest["run_dir"]) / "burst"
    evidence.mkdir(parents=True, exist_ok=True)
    for index, (w, h, data) in enumerate(frames):
        image.write_png(str(evidence / f"{name}-{index:03d}.png"), w, h, data)

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"{name}.png"
    image.write_png(str(path), width, height, bgra)
    print(f"burst: chose frame {chosen} ({lit} lit pixels) of {len(frames)}")
    return path


# ---------------------------------------------------------------------------
# Capture
# ---------------------------------------------------------------------------
def shot(manifest: dict, target: str, name: str, out_dir: Path) -> Path:
    _valid_name(name)
    info = window(manifest, target)
    width, height, bgra = win32.capture_client_bgra(info.hwnd)
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / f"{name}.png"
    image.write_png(str(path), width, height, bgra)
    return path


# ---------------------------------------------------------------------------
# Stop
# ---------------------------------------------------------------------------
def stop(manifest: dict, force: bool = False, timeout: float = 10.0) -> None:
    run_dir = Path(manifest["run_dir"])
    if win32.find_processes("Dolphin.exe"):
        try:
            assert_owned(manifest)
        except SystemExit:
            return
        try:
            main = window(manifest, "main")
        except SystemExit:
            try:
                main = window(manifest, "sender")
            except SystemExit:
                main = None
        if main:
            win32.post_close(main.hwnd)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if not win32.process_alive(int(manifest["pid"])):
                break
            time.sleep(0.2)
        if win32.process_alive(int(manifest["pid"])):
            if not force:
                raise SystemExit("Graceful shutdown timed out; retry with --force.")
            win32.terminate_process(int(manifest["pid"]))
    manifest = load_manifest(run_dir)
    manifest["status"] = "closed"
    save_manifest(run_dir, manifest)
