#!/usr/bin/env python3
"""
Convert a designer-authored 336x240 PNG into the Moonlit Cabin firmware asset.

The converter maps RGB artwork to a small logical palette, emits row-RLE indexed
pixels, and creates a 1-bit dynamic-sky mask used by stars/moon overlays.

Usage:
  python3 tools/convert_background.py \
      artwork/moonlit_cabin/background.png \
      --palette artwork/moonlit_cabin/palette.json \
      --output include/moonlit_cabin_art.h

Optional:
  --mask artwork/moonlit_cabin/sky_mask.png
      White/non-zero pixels explicitly allow dynamic stars/moon.
      If omitted, palette entries marked "dynamic_sky": true are used.
      Transparent source pixels are also treated as dynamic sky.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Iterable

from PIL import Image

WIDTH = 336
HEIGHT = 240


def parse_hex_rgb(value: str) -> tuple[int, int, int]:
    value = value.strip().lstrip("#")
    if len(value) != 6:
        raise ValueError(f"Expected #RRGGBB, got {value!r}")
    return tuple(int(value[i:i+2], 16) for i in (0, 2, 4))


def parse_byte(value) -> int:
    if isinstance(value, int):
        result = value
    else:
        result = int(str(value), 0)
    if not 0 <= result <= 0xFF:
        raise ValueError(f"Palette byte out of range: {value!r}")
    return result


def nearest_entry(rgb: tuple[int, int, int], entries):
    r, g, b = rgb
    return min(
        entries,
        key=lambda e: (
            (r - e["rgb_tuple"][0]) ** 2
            + (g - e["rgb_tuple"][1]) ** 2
            + (b - e["rgb_tuple"][2]) ** 2
        ),
    )


def format_bytes(data: Iterable[int], per_line: int = 24) -> str:
    values = list(data)
    lines = []
    for i in range(0, len(values), per_line):
        chunk = values[i:i+per_line]
        lines.append(
            "    " + ", ".join(f"0x{v:02X}" for v in chunk) + ","
        )
    return "\n".join(lines)


def row_rle(indexed: list[int]) -> list[int]:
    out: list[int] = []
    for y in range(HEIGHT):
        x = 0
        row = indexed[y * WIDTH:(y + 1) * WIDTH]
        while x < WIDTH:
            value = row[x]
            count = 1
            while x + count < WIDTH and row[x + count] == value and count < 255:
                count += 1
            out.extend((count, value))
            x += count
        out.extend((0, 0))
    return out


def pack_mask(bits: list[bool]) -> list[int]:
    packed = [0] * ((len(bits) + 7) // 8)
    for i, enabled in enumerate(bits):
        if enabled:
            packed[i >> 3] |= 1 << (i & 7)
    return packed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--palette", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mask", type=Path)
    parser.add_argument("--preview-dir", type=Path)
    args = parser.parse_args()

    config = json.loads(args.palette.read_text())
    entries = config.get("colors", [])
    if not entries:
        raise SystemExit("Palette has no colors.")

    by_name = {}
    for entry in entries:
        item = dict(entry)
        item["rgb_tuple"] = parse_hex_rgb(item["rgb"])
        item["byte_value"] = parse_byte(item["byte"])
        item["dynamic_sky"] = bool(item.get("dynamic_sky", False))
        by_name[item["name"]] = item
    entries = list(by_name.values())

    transparent_fill_name = config.get("transparent_fill")
    if transparent_fill_name not in by_name:
        raise SystemExit(
            "palette.json must define transparent_fill using one of the color names"
        )
    transparent_fill = by_name[transparent_fill_name]

    image = Image.open(args.image).convert("RGBA")
    if image.size != (WIDTH, HEIGHT):
        raise SystemExit(
            f"Artwork must be exactly {WIDTH}x{HEIGHT}; got {image.size[0]}x{image.size[1]}"
        )

    explicit_mask = None
    if args.mask:
        explicit_mask = Image.open(args.mask).convert("L")
        if explicit_mask.size != (WIDTH, HEIGHT):
            raise SystemExit(
                f"Mask must be exactly {WIDTH}x{HEIGHT}; got "
                f"{explicit_mask.size[0]}x{explicit_mask.size[1]}"
            )

    indexed: list[int] = []
    sky_bits: list[bool] = []
    quantized_rgb: list[tuple[int, int, int]] = []
    counts = {entry["name"]: 0 for entry in entries}

    pixels = list(image.getdata())
    mask_pixels = list(explicit_mask.getdata()) if explicit_mask else None

    for i, (r, g, b, a) in enumerate(pixels):
        transparent = a < 128
        entry = transparent_fill if transparent else nearest_entry((r, g, b), entries)

        indexed.append(entry["byte_value"])
        quantized_rgb.append(entry["rgb_tuple"])
        counts[entry["name"]] += 1

        if mask_pixels is not None:
            sky = mask_pixels[i] >= 128
        else:
            sky = transparent or entry["dynamic_sky"]
        sky_bits.append(sky)

    rle = row_rle(indexed)
    packed_mask = pack_mask(sky_bits)

    header = f"""#pragma once

#include <stdint.h>

namespace moonlit_cabin_art {{

static constexpr uint16_t WIDTH = {WIDTH};
static constexpr uint16_t HEIGHT = {HEIGHT};

// GENERATED FILE. Do not hand edit.
// Source: {args.image.as_posix()}
// Row-wise RLE: {{count, logical composite palette byte}}; {{0,0}} ends a row.
static const uint8_t RLE[] = {{
{format_bytes(rle)}
}};

static constexpr uint32_t RLE_SIZE = sizeof(RLE);

// One bit per output pixel. Dynamic stars/moon may only draw where this mask
// is set, preventing them from appearing over mountains, trees, or buildings.
static const uint8_t SKY_MASK[] = {{
{format_bytes(packed_mask)}
}};

static constexpr uint32_t SKY_MASK_SIZE = sizeof(SKY_MASK);

inline bool skyAllows(uint16_t x, uint16_t y) {{
    if (x >= WIDTH || y >= HEIGHT) return false;
    const uint32_t index = (uint32_t)y * WIDTH + x;
    return (SKY_MASK[index >> 3] & (uint8_t)(1U << (index & 7))) != 0;
}}

}} // namespace moonlit_cabin_art
"""

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header)

    if args.preview_dir:
        args.preview_dir.mkdir(parents=True, exist_ok=True)

        quantized = Image.new("RGB", (WIDTH, HEIGHT))
        quantized.putdata(quantized_rgb)
        quantized.save(args.preview_dir / "quantized.png")

        mask_preview = Image.new("L", (WIDTH, HEIGHT))
        mask_preview.putdata([255 if v else 0 for v in sky_bits])
        mask_preview.save(args.preview_dir / "sky-mask.png")

    print(f"Wrote {args.output}")
    print(f"Resolution: {WIDTH}x{HEIGHT}")
    print(f"RLE bytes: {len(rle)}")
    print(f"Sky mask bytes: {len(packed_mask)}")
    print("Palette usage:")
    for name, count in sorted(counts.items(), key=lambda kv: -kv[1]):
        if count:
            print(f"  {name:18s} {count:6d}")


if __name__ == "__main__":
    main()
