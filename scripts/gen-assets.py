#!/usr/bin/env python3
"""Generate placeholder DHB app icons without external image libraries.

HarmonyOS expects a raster icon; this repo intentionally does not commit binary
files. Run this once before opening/building the HAP:

    python3 scripts/gen-assets.py
"""

from __future__ import annotations

import binascii
import math
import os
import struct
import zlib

WIDTH = 512
HEIGHT = 512


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return (
        struct.pack(">I", len(data))
        + kind
        + data
        + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
    )


def render() -> bytes:
    rows = bytearray()
    for y in range(HEIGHT):
        rows.append(0)  # filter type: none
        for x in range(WIDTH):
            # Deep navy background with a soft top-left glow.
            t = max(0.0, 1.0 - math.hypot(x - 140, y - 120) / 520.0)
            r = int(8 + 10 * t)
            g = int(16 + 18 * t)
            b = int(28 + 34 * t)

            # Bridge deck: a shallow arc from x=84 to x=428.
            if 84 <= x <= 428:
                u = (x - 84) / (428 - 84)
                deck = 330 - 76 * math.sin(math.pi * u)
                if abs(y - deck) < 7:
                    r, g, b = 47, 124, 246
                elif abs(y - deck) < 12:
                    r, g, b = 24, 62, 110

            # Two towers.
            for tower_x in (150, 362):
                if abs(x - tower_x) < 8 and 236 <= y <= 356:
                    r, g, b = 120, 210, 255

            # Connect dot.
            if math.hypot(x - 256, y - 176) < 11:
                r, g, b = 56, 211, 159

            rows.extend((r, g, b))
    compressed = zlib.compress(bytes(rows), 9)
    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", compressed)
        + png_chunk(b"IEND", b"")
    )


def main() -> None:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data = render()
    targets = [
        os.path.join(root, "ohos", "AppScope", "resources", "base", "media", "app_icon.png"),
        os.path.join(root, "ohos", "entry", "src", "main", "resources", "base", "media", "app_icon.png"),
    ]
    for path in targets:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as handle:
            handle.write(data)
        print("wrote", path, len(data), "bytes")


if __name__ == "__main__":
    main()
