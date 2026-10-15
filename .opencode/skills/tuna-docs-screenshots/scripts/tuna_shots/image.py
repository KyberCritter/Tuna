"""Tiny PNG writer built on the standard library only.

Dolphin captures arrive as raw BGRA DIB bytes; this encodes them as a PNG
without Pillow or any other third-party package.
"""
from __future__ import annotations

import struct
import zlib
from typing import Optional, Tuple


def _chunk(tag: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + tag
        + data
        + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    )


def bgra_to_rgba(bgra: bytes, width: int, height: int) -> bytearray:
    row_bytes = width * 4
    if len(bgra) != row_bytes * height:
        raise ValueError("pixel buffer does not match dimensions")
    out = bytearray(len(bgra))
    for y in range(height):
        start = y * row_bytes
        row = bytearray(bgra[start : start + row_bytes])
        row[0::4], row[2::4] = row[2::4], row[0::4]  # BGRA -> RGBA
        out[start : start + row_bytes] = row
    return out


def write_png(
    path: str,
    width: int,
    height: int,
    bgra: bytes,
    crop: Optional[Tuple[int, int, int, int]] = None,
) -> Tuple[int, int]:
    """Write a PNG. ``crop`` is ``(left, top, right, bottom)`` in pixels."""
    if crop is not None:
        left, top, right, bottom = crop
        left = max(0, left)
        top = max(0, top)
        right = min(width, right)
        bottom = min(height, bottom)
        if right <= left or bottom <= top:
            raise ValueError("empty crop rectangle")
        cropped = bytearray()
        for y in range(top, bottom):
            cropped += bgra[(y * width + left) * 4 : (y * width + right) * 4]
        bgra = bytes(cropped)
        width = right - left
        height = bottom - top

    rgba = bgra_to_rgba(bgra, width, height)
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type 0 (None)
        raw += rgba[y * stride : (y + 1) * stride]

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + _chunk(b"IEND", b"")
    )
    with open(path, "wb") as handle:
        handle.write(data)
    return width, height
