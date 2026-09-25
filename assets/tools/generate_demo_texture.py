#!/usr/bin/env python3
"""
generate_demo_texture.py — writes demo_checker.png using Python stdlib only.
Deterministic: same invocation → byte-identical PNG output across runs.
No timestamps, no random data, no third-party packages (Python stdlib: struct, zlib, os).

Output: assets/geometry/placeholders/demo_checker/textures/demo_checker.png
"""
import os
import struct
import zlib


# ---------------------------------------------------------------------------
# Image parameters — fixed; changing these alters the output file
# ---------------------------------------------------------------------------
WIDTH  = 16
HEIGHT = 16
BLOCK  = 4        # checker-square edge length in pixels

# Orange (#DC641E) and white — visually distinct checker on a dark scene background
COLOR_A = (220, 100,  30, 255)   # orange
COLOR_B = (255, 255, 255, 255)   # white


# ---------------------------------------------------------------------------
# Minimal PNG writer (IHDR + IDAT + IEND, no ancillary chunks)
# ---------------------------------------------------------------------------

def _chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFF_FFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def write_rgba_png(path: str, width: int, height: int, rgba_rows: list) -> None:
    """Write a minimal RGBA PNG file from a list of row byte objects."""
    # IHDR: width(4) height(4) bitdepth(1) colortype(1=RGBA→6) compress(1) filter(1) interlace(1)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)

    # Scanlines: each row preceded by filter-byte 0 (None)
    raw = b"".join(b"\x00" + bytes(row) for row in rgba_rows)
    idat = zlib.compress(raw, level=9)   # level fixed for determinism

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")  # PNG signature
        f.write(_chunk(b"IHDR", ihdr))
        f.write(_chunk(b"IDAT", idat))
        f.write(_chunk(b"IEND", b""))


# ---------------------------------------------------------------------------
# Checker pattern
# ---------------------------------------------------------------------------

def _checker_rows(width: int, height: int, block: int,
                  ca: tuple, cb: tuple) -> list:
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            color = ca if ((x // block) + (y // block)) % 2 == 0 else cb
            row.extend(color)
        rows.append(row)
    return rows


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.normpath(os.path.join(
        script_dir, "..", "geometry", "placeholders",
        "demo_checker", "textures", "demo_checker.png"))

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    rows = _checker_rows(WIDTH, HEIGHT, BLOCK, COLOR_A, COLOR_B)
    write_rgba_png(out_path, WIDTH, HEIGHT, rows)
    print(f"wrote {out_path!r}  ({WIDTH}x{HEIGHT} RGBA checker)")


if __name__ == "__main__":
    main()
