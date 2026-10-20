#!/usr/bin/env python3
"""Convert one designer-authored 336x240 artwork theme into C++ assets.

This is a build-machine tool. It quantizes RGB artwork, creates the compact
row-RLE backdrop, packs a 1-bit sky mask, detects independent light regions from
an optional window mask, and emits flash-resident C++ data.

Most users should run tools/build_artwork_themes.py rather than calling this
file directly.
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


def label_components(mask_pixels: list[int]) -> tuple[list[int], int]:
    active = [value >= 128 for value in mask_pixels]
    labels = [-1] * (WIDTH * HEIGHT)
    component_count = 0

    for start in range(WIDTH * HEIGHT):
        if not active[start] or labels[start] != -1:
            continue

        if component_count >= 32:
            raise SystemExit("Window mask supports at most 32 separate light regions.")

        stack = [start]
        labels[start] = component_count

        while stack:
            index = stack.pop()
            x = index % WIDTH
            y = index // WIDTH

            if x > 0:
                n = index - 1
                if active[n] and labels[n] == -1:
                    labels[n] = component_count
                    stack.append(n)
            if x + 1 < WIDTH:
                n = index + 1
                if active[n] and labels[n] == -1:
                    labels[n] = component_count
                    stack.append(n)
            if y > 0:
                n = index - WIDTH
                if active[n] and labels[n] == -1:
                    labels[n] = component_count
                    stack.append(n)
            if y + 1 < HEIGHT:
                n = index + WIDTH
                if active[n] and labels[n] == -1:
                    labels[n] = component_count
                    stack.append(n)

        component_count += 1

    return labels, component_count


def component_spans(labels: list[int]) -> list[tuple[int, int, int, int]]:
    spans: list[tuple[int, int, int, int]] = []

    for y in range(HEIGHT):
        x = 0
        while x < WIDTH:
            component = labels[y * WIDTH + x]
            if component < 0:
                x += 1
                continue

            start = x
            while (
                x < WIDTH
                and labels[y * WIDTH + x] == component
                and x - start < 255
            ):
                x += 1
            spans.append((start, y, x - start, component))

    return spans


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--palette", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--namespace", default="moonlit_cabin_art")
    parser.add_argument("--stats-json", type=Path)
    parser.add_argument("--sky-mask", "--mask", dest="sky_mask", type=Path)
    parser.add_argument("--window-mask", type=Path)
    parser.add_argument("--omit-sky-mask", action="store_true")
    parser.add_argument("--omit-windows", action="store_true")
    parser.add_argument("--layout", type=Path)
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
    if args.sky_mask:
        explicit_mask = Image.open(args.sky_mask).convert("L")
        if explicit_mask.size != (WIDTH, HEIGHT):
            raise SystemExit(
                f"Sky mask must be exactly {WIDTH}x{HEIGHT}; got "
                f"{explicit_mask.size[0]}x{explicit_mask.size[1]}"
            )

    explicit_window_mask = None
    if args.window_mask:
        explicit_window_mask = Image.open(args.window_mask).convert("L")
        if explicit_window_mask.size != (WIDTH, HEIGHT):
            raise SystemExit(
                f"Window mask must be exactly {WIDTH}x{HEIGHT}; got "
                f"{explicit_window_mask.size[0]}x{explicit_window_mask.size[1]}"
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
    packed_mask = [] if args.omit_sky_mask else pack_mask(sky_bits)

    namespace = args.namespace.strip()
    if not namespace or not all(c.isalnum() or c == "_" for c in namespace):
        raise SystemExit("--namespace must contain only letters, numbers, and underscore")

    header = f"""#pragma once

#include <stdint.h>
#include "artwork_theme_asset.h"

namespace {namespace} {{

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
{format_bytes(packed_mask if packed_mask else [0])}
}};

static constexpr uint32_t SKY_MASK_SIZE = {len(packed_mask)};

inline bool skyAllows(uint16_t x, uint16_t y) {{
    if (x >= WIDTH || y >= HEIGHT) return false;
    const uint32_t index = (uint32_t)y * WIDTH + x;
    return (SKY_MASK[index >> 3] & (uint8_t)(1U << (index & 7))) != 0;
}}

}} // namespace {namespace}
"""

    layout = {
        "moon": {"x": 65, "y": 65},
    }
    if args.layout:
        layout = json.loads(args.layout.read_text())

    moon = layout.get("moon", {})
    moon_x = int(moon.get("x", 65))
    moon_y = int(moon.get("y", 65))

    # Preferred path: an artist supplies an exact 336x240 mask. Every
    # disconnected white region becomes an independently animated light.
    # Legacy layout rectangles remain accepted as a fallback so older artwork
    # can be converted without first painting a mask.
    if args.omit_windows:
        window_pixels = [0] * (WIDTH * HEIGHT)
    elif explicit_window_mask is not None:
        window_pixels = list(explicit_window_mask.getdata())
    else:
        window_pixels = [0] * (WIDTH * HEIGHT)
        for item in layout.get("windows", []):
            wx = int(item["x"])
            wy = int(item["y"])
            ww = int(item["w"])
            wh = int(item["h"])
            if wx < 0 or wy < 0 or ww <= 0 or wh <= 0 or wx + ww > WIDTH or wy + wh > HEIGHT:
                raise SystemExit(f"Invalid legacy window rectangle: {item}")
            for yy in range(wy, wy + wh):
                for xx in range(wx, wx + ww):
                    window_pixels[yy * WIDTH + xx] = 255

    window_labels, window_component_count = label_components(window_pixels)
    window_spans = component_spans(window_labels)

    span_rows = [
        f"    {{{x}, {y}, {length}, {component}}},"
        for x, y, length, component in window_spans
    ]
    emitted_spans = span_rows or ["    {0, 0, 0, 0},"]

    metadata = f"""
static constexpr uint16_t MOON_X = {moon_x};
static constexpr uint16_t MOON_Y = {moon_y};

static const artwork_theme::WindowSpan WINDOW_SPANS[] = {{
{chr(10).join(emitted_spans)}
}};
static constexpr uint16_t WINDOW_SPAN_COUNT = {len(window_spans)};
static constexpr uint8_t WINDOW_COMPONENT_COUNT = {window_component_count};
"""

    closing = f"}} // namespace {namespace}"
    header = header.replace(
        closing,
        metadata + "\n" + closing,
    )

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

        window_preview = Image.new("L", (WIDTH, HEIGHT))
        window_preview.putdata(window_pixels)
        window_preview.save(args.preview_dir / "window-mask.png")

    print(f"Wrote {args.output}")
    print(f"Resolution: {WIDTH}x{HEIGHT}")
    print(f"RLE bytes: {len(rle)}")
    print(f"Sky mask bytes: {len(packed_mask)}")
    print(f"Sky pixels: {sum(1 for value in sky_bits if value)}")
    print(f"Window components: {window_component_count}")
    binary_bytes = len(rle) + len(packed_mask) + len(window_spans) * 6
    if args.stats_json:
        args.stats_json.parent.mkdir(parents=True, exist_ok=True)
        args.stats_json.write_text(json.dumps({
            "width": WIDTH,
            "height": HEIGHT,
            "rle_bytes": len(rle),
            "sky_mask_bytes": len(packed_mask),
            "sky_pixel_count": sum(1 for value in sky_bits if value),
            "window_span_count": len(window_spans),
            "window_component_count": window_component_count,
            "estimated_flash_bytes": binary_bytes,
        }, indent=2) + "\n")

    print(f"Window spans: {len(window_spans)}")
    print(f"Estimated asset flash: {binary_bytes} bytes")
    print("Palette usage:")
    for name, count in sorted(counts.items(), key=lambda kv: -kv[1]):
        if count:
            print(f"  {name:18s} {count:6d}")


if __name__ == "__main__":
    main()
