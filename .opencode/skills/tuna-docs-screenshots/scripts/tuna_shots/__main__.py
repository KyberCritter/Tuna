"""Step-by-step CLI for the Tuna docs screenshot toolkit.

Run as ``python -m tuna_shots <step> ...`` from the ``scripts`` directory (or
add that directory to ``PYTHONPATH``). Every host path is an explicit argument;
nothing machine-specific is hard-coded.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from . import dolphin


def _run_dir(value: str) -> Path:
    return Path(value).expanduser().resolve()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="tuna_shots", description=__doc__)
    sub = parser.add_subparsers(dest="step", required=True)

    stage = sub.add_parser("stage", help="create an isolated Dolphin profile and SD folder")
    stage.add_argument("--run-dir", required=True)
    stage.add_argument("--bios", required=True, help="16 KiB GBA BIOS image")
    stage.add_argument("--mp3", required=True, help="source track to expose to the app")
    stage.add_argument("--name", help="guest filename (controls the GBA title); defaults to the source basename")
    stage.add_argument("--platform", default="wii", choices=["wii", "gamecube"])

    launch = sub.add_parser("launch", help="start the owned Dolphin instance")
    launch.add_argument("--run-dir", required=True)
    launch.add_argument("--dolphin", required=True, help="Dolphin executable")
    launch.add_argument("--dol", required=True, help="Tuna DOL to run")

    opentas = sub.add_parser("open-tas", help="open Movie > TAS Input and make checkboxes authoritative")
    opentas.add_argument("--run-dir", required=True)

    press = sub.add_parser("press", help="toggle one TAS input control")
    press.add_argument("--run-dir", required=True)
    press.add_argument("--target", required=True, choices=["gc", "gbatas", "wiitas"])
    press.add_argument("--button", required=True)
    press.add_argument("--ms", type=int, default=200)

    shot = sub.add_parser("shot", help="capture a cropped PNG")
    shot.add_argument("--run-dir", required=True)
    shot.add_argument("--target", required=True, choices=["main", "sender", "gba"])
    shot.add_argument("--name", required=True)
    shot.add_argument("--out-dir", help="destination directory; defaults to <repo>/docs/images")

    burst = sub.add_parser("burst", help="dense burst capture keeping the sparsest non-blank frame")
    burst.add_argument("--run-dir", required=True)
    burst.add_argument("--target", required=True, choices=["main", "sender", "gba"])
    burst.add_argument("--name", required=True)
    burst.add_argument("--count", type=int, default=160)
    burst.add_argument("--press-target", choices=["gc", "gbatas", "wiitas"], help="TAS window to press while capturing")
    burst.add_argument("--press-button", help="TAS button to hold during the burst")
    burst.add_argument("--out-dir", help="destination directory; defaults to <repo>/docs/images")

    stop = sub.add_parser("stop", help="stop the owned Dolphin instance")
    stop.add_argument("--run-dir", required=True)
    stop.add_argument("--force", action="store_true")

    listing = sub.add_parser("windows", help="list the owned process windows")
    listing.add_argument("--run-dir", required=True)

    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    if args.step == "stage":
        manifest = dolphin.stage(
            run_dir=_run_dir(args.run_dir),
            bios=Path(args.bios).expanduser(),
            mp3=Path(args.mp3).expanduser(),
            mp3_name=args.name,
            platform=args.platform,
        )
        print(json.dumps(manifest, indent=2))
    elif args.step == "launch":
        manifest = dolphin.launch(
            run_dir=_run_dir(args.run_dir),
            dolphin=Path(args.dolphin).expanduser(),
            dol=Path(args.dol).expanduser(),
        )
        print(f"launched pid {manifest['pid']}")
    elif args.step == "open-tas":
        dolphin.open_tas(dolphin.load_manifest(_run_dir(args.run_dir)))
        print("TAS input ready")
    elif args.step == "press":
        dolphin.press(
            dolphin.load_manifest(_run_dir(args.run_dir)), args.target, args.button, args.ms
        )
        print(f"{args.target}:{args.button}")
    elif args.step == "shot":
        out_dir = Path(args.out_dir).expanduser() if args.out_dir else dolphin.default_out_dir()
        path = dolphin.shot(
            dolphin.load_manifest(_run_dir(args.run_dir)), args.target, args.name, out_dir
        )
        print(path)
    elif args.step == "burst":
        out_dir = Path(args.out_dir).expanduser() if args.out_dir else dolphin.default_out_dir()
        path = dolphin.burst(
            dolphin.load_manifest(_run_dir(args.run_dir)),
            args.target,
            args.name,
            args.count,
            out_dir,
            args.press_target,
            args.press_button,
        )
        print(path)
    elif args.step == "stop":
        dolphin.stop(dolphin.load_manifest(_run_dir(args.run_dir)), force=args.force)
        print("stopped")
    elif args.step == "windows":
        manifest = dolphin.load_manifest(_run_dir(args.run_dir))
        for info in dolphin.list_windows(manifest):
            print(f"{info.hwnd}\t{info.pid}\t{info.class_name}\t{info.title}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
