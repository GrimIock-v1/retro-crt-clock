#!/usr/bin/env python3
"""
Build every optional artwork theme under artwork/themes/.

Each theme directory contains:
  theme.json
  background.png
  palette.json
  layout.json              (optional)
  sky_mask.png             (optional)
  window_mask.png          (optional)

Generated C++ stays in include/generated/ and a tiny registry is emitted at
include/artwork_theme_registry.h.

The ESP32 never decodes PNGs or allocates image buffers. All expensive image
work happens here on the development machine.
"""

from __future__ import annotations

import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
THEMES_ROOT = ROOT / "artwork" / "themes"
GENERATED = ROOT / "include" / "generated"
REGISTRY = ROOT / "include" / "artwork_theme_registry.h"
CONVERTER = ROOT / "tools" / "convert_background.py"

MAX_THEME_COUNT = 8
MAX_THEME_FLASH_BYTES = 96 * 1024
MAX_TOTAL_ARTWORK_FLASH_BYTES = 384 * 1024
WARN_RLE_BYTES = 32 * 1024
MAX_RLE_BYTES = 64 * 1024
MAX_WINDOW_SPANS = 512

DEFAULT_HUE_MAP = [
    0, 1, 2, 10,
    4, 5, 6, 7,
    2, 3, 15, 7,
    11, 13, 14, 15,
]

DEFAULT_LAYOUT = {
    "moon": {"x": 65, "y": 65},
    "ui": {
        "date": {"x": 13, "y": 17, "scale": 1.15},
        "weather_icon": {"x": 244, "y": 14},
        "temperature": {"right_x": 322, "y": 17},
        "weather_descriptor": {"y": 30},
        "clock": {
            "y": 115,
            "scale": 5.25,
            "suffix_scale": 1.35,
            "suffix_dy": 26,
        },
        "next_label": {"x": 35, "y": 169, "scale": 0.90},
        "next_event": {"x": 35, "y": 183, "scale": 1.35, "max_x": 238},
        "day_bar": {"y": 218},
    },
}


def ident(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not value or value[0].isdigit():
        value = "_" + value
    return value


def scale256(value) -> int:
    number = float(value)
    if number < 0.25 or number > 8.0:
        raise ValueError(f"Text scale {number} is outside 0.25..8.0")
    return int(round(number * 256.0))


def merged_layout(theme_dir: Path) -> dict:
    layout = json.loads(json.dumps(DEFAULT_LAYOUT))
    path = theme_dir / "layout.json"
    if not path.exists():
        return layout

    user = json.loads(path.read_text())
    if "moon" in user:
        layout["moon"].update(user["moon"])
    if "ui" in user:
        for key, value in user["ui"].items():
            if key not in layout["ui"]:
                raise ValueError(f"Unknown UI layout key: {key}")
            if isinstance(value, dict):
                layout["ui"][key].update(value)
            else:
                layout["ui"][key] = value
    return layout


def validate_layout(theme_name: str, layout: dict) -> None:
    moon = layout["moon"]
    mx = int(moon["x"])
    my = int(moon["y"])
    if not (0 <= mx < 336 and 0 <= my < 240):
        raise SystemExit(f"{theme_name}: moon anchor must be inside 336x240")

    ui = layout["ui"]
    point_checks = [
        ("date", int(ui["date"]["x"]), int(ui["date"]["y"])),
        ("weather_icon", int(ui["weather_icon"]["x"]), int(ui["weather_icon"]["y"])),
        ("temperature", int(ui["temperature"]["right_x"]), int(ui["temperature"]["y"])),
        ("next_label", int(ui["next_label"]["x"]), int(ui["next_label"]["y"])),
        ("next_event", int(ui["next_event"]["x"]), int(ui["next_event"]["y"])),
    ]
    for label, x, y in point_checks:
        if not (0 <= x < 336 and 0 <= y < 240):
            raise SystemExit(f"{theme_name}: {label} anchor must be inside 336x240")

    desc_y = int(ui["weather_descriptor"]["y"])
    if not 0 <= desc_y < 240:
        raise SystemExit(f"{theme_name}: weather descriptor y must be inside 0..239")

    clock_y = int(ui["clock"]["y"])
    clock_scale = float(ui["clock"]["scale"])
    suffix_scale = float(ui["clock"]["suffix_scale"])
    scale256(clock_scale)
    scale256(suffix_scale)
    if clock_y < 0 or clock_y + int(round(8 * clock_scale)) > 240:
        raise SystemExit(f"{theme_name}: clock does not fit vertically in 336x240")

    for key in ("date", "next_label", "next_event"):
        scale256(ui[key]["scale"])

    max_x = int(ui["next_event"]["max_x"])
    if not 0 <= max_x < 336:
        raise SystemExit(f"{theme_name}: next_event.max_x must be inside 0..335")

    day_y = int(ui["day_bar"]["y"])
    if day_y < -1 or day_y > 234:
        raise SystemExit(f"{theme_name}: day_bar.y must be -1 or within 0..234")


def feature_bits(config: dict) -> int:
    dynamic = config.get("dynamic", {})
    bits = 0
    if dynamic.get("stars", False):
        bits |= 1
    if dynamic.get("moon", False):
        bits |= 2
    if dynamic.get("lights", False):
        bits |= 4
    return bits


def registry_entry(item: dict) -> str:
    ns = item["namespace"]
    cfg = item["config"]
    layout = item["layout"]
    ui = layout["ui"]

    hue_map = cfg.get("hue_map", DEFAULT_HUE_MAP)
    if len(hue_map) != 16 or any(not 0 <= int(v) <= 15 for v in hue_map):
        raise ValueError(f"{item['slug']}: hue_map must contain 16 values in 0..15")
    hue_map_text = ", ".join(str(int(v)) for v in hue_map)

    hues = cfg.get("hues", {})
    neutral = int(hues.get("neutral", 0))
    ice = int(hues.get("ice", 15))
    warm = int(hues.get("warm", 9))
    for label, value in (("neutral", neutral), ("ice", ice), ("warm", warm)):
        if not 0 <= value <= 15:
            raise ValueError(f"{item['slug']}: {label} hue must be 0..15")

    burst = int(cfg.get("burst_phase_quarters", 2))
    if not 0 <= burst <= 3:
        raise ValueError(f"{item['slug']}: burst_phase_quarters must be 0..3")

    star_seed = int(cfg.get("star_seed", item["id"] * 2654435761)) & 0xFFFFFFFF

    date = ui["date"]
    wx = ui["weather_icon"]
    temp = ui["temperature"]
    desc = ui["weather_descriptor"]
    clock = ui["clock"]
    next_label = ui["next_label"]
    next_event = ui["next_event"]
    day_bar = ui["day_bar"]

    return f"""    {{
        {item['id']}, {json.dumps(item['name'])},
        {ns}::RLE, {ns}::RLE_SIZE,
        {ns}::SKY_MASK, {ns}::SKY_MASK_SIZE,
        {ns}::WINDOW_SPANS, {ns}::WINDOW_SPAN_COUNT, {ns}::WINDOW_COMPONENT_COUNT,
        {ns}::MOON_X, {ns}::MOON_Y,
        {feature_bits(cfg)}, {1 if cfg.get("color", True) else 0}, {burst}, {neutral}, {ice}, {warm}, 0x{star_seed:08X}U,
        {{{hue_map_text}}},
        {{
            {int(date['x'])}, {int(date['y'])}, {scale256(date['scale'])},
            {int(wx['x'])}, {int(wx['y'])}, {int(temp['right_x'])}, {int(temp['y'])}, {int(desc['y'])},
            {int(clock['y'])}, {scale256(clock['scale'])}, {scale256(clock['suffix_scale'])}, {int(clock['suffix_dy'])},
            {int(next_label['x'])}, {int(next_label['y'])}, {scale256(next_label['scale'])},
            {int(next_event['x'])}, {int(next_event['y'])}, {scale256(next_event['scale'])}, {int(next_event['max_x'])},
            {int(day_bar['y'])}
        }}
    }}"""


def main() -> None:
    GENERATED.mkdir(parents=True, exist_ok=True)
    themes = []

    if THEMES_ROOT.exists():
        for theme_dir in sorted(p for p in THEMES_ROOT.iterdir() if p.is_dir()):
            if theme_dir.name.startswith("_"):
                continue
            config_path = theme_dir / "theme.json"
            if not config_path.exists():
                continue

            cfg = json.loads(config_path.read_text())
            if not cfg.get("enabled", True):
                continue

            theme_id = int(cfg["id"])
            if not 16 <= theme_id <= 63:
                raise SystemExit(
                    f"{theme_dir.name}: artwork theme IDs must be 16..63 "
                    "(0..15 are reserved for firmware themes)."
                )

            name = str(cfg["name"]).strip()
            if not name:
                raise SystemExit(f"{theme_dir.name}: theme name cannot be empty")
            if len(name) > 32:
                raise SystemExit(f"{theme_dir.name}: theme name must be 32 characters or fewer")

            background = theme_dir / "background.png"
            palette = theme_dir / "palette.json"
            if not background.exists() or not palette.exists():
                raise SystemExit(
                    f"{theme_dir.name}: background.png and palette.json are required"
                )

            slug = ident(theme_dir.name.lower())
            namespace = f"art_{slug}"
            output = GENERATED / f"{slug}_art.h"
            stats_path = GENERATED / f".{slug}_stats.json"
            layout_path = theme_dir / "layout.json"

            command = [
                sys.executable,
                str(CONVERTER),
                str(background),
                "--palette", str(palette),
                "--output", str(output),
                "--namespace", namespace,
                "--stats-json", str(stats_path),
            ]
            if layout_path.exists():
                command += ["--layout", str(layout_path)]

            dynamic = cfg.get("dynamic", {})
            needs_sky = bool(dynamic.get("stars", False) or dynamic.get("moon", False))
            if needs_sky:
                sky = theme_dir / "sky_mask.png"
                if sky.exists():
                    command += ["--sky-mask", str(sky)]
            else:
                command += ["--omit-sky-mask"]

            if dynamic.get("lights", False):
                windows = theme_dir / "window_mask.png"
                if windows.exists():
                    command += ["--window-mask", str(windows)]
            else:
                command += ["--omit-windows"]

            command += ["--preview-dir", str(theme_dir / "build")]

            print(f"\n=== Building artwork theme: {name} ({theme_id}) ===")
            subprocess.run(command, cwd=ROOT, check=True)

            stats = json.loads(stats_path.read_text())
            stats_path.unlink(missing_ok=True)
            estimated = int(stats["estimated_flash_bytes"])
            rle_bytes = int(stats["rle_bytes"])
            sky_pixel_count = int(stats.get("sky_pixel_count", 0))
            window_span_count = int(stats["window_span_count"])
            window_component_count = int(stats["window_component_count"])

            dynamic = cfg.get("dynamic", {})
            if (dynamic.get("stars", False) or dynamic.get("moon", False)) and sky_pixel_count == 0:
                raise SystemExit(
                    f"{name}: stars/moon are enabled but the generated sky mask has no white pixels."
                )
            if dynamic.get("lights", False) and window_component_count == 0:
                raise SystemExit(
                    f"{name}: lights are enabled but no window/light regions were generated."
                )

            if rle_bytes > MAX_RLE_BYTES:
                raise SystemExit(
                    f"{name}: backdrop RLE is {rle_bytes:,} bytes; hard render-complexity "
                    f"limit is {MAX_RLE_BYTES:,}. Reduce single-pixel/noisy detail."
                )
            if rle_bytes > WARN_RLE_BYTES:
                print(
                    f"WARNING: {name} backdrop RLE is {rle_bytes:,} bytes. "
                    "This is within the build limit but should be CRT-tested for render time."
                )
            if window_span_count > MAX_WINDOW_SPANS:
                raise SystemExit(
                    f"{name}: window mask generated {window_span_count} horizontal spans; "
                    f"limit is {MAX_WINDOW_SPANS}. Simplify the light mask."
                )

            if estimated > MAX_THEME_FLASH_BYTES:
                raise SystemExit(
                    f"{name}: estimated generated asset is {estimated:,} bytes; "
                    f"limit is {MAX_THEME_FLASH_BYTES:,}. Simplify the artwork "
                    "or reduce high-frequency pixel detail."
                )

            layout = merged_layout(theme_dir)
            validate_layout(name, layout)

            themes.append({
                "id": theme_id,
                "name": name,
                "slug": slug,
                "namespace": namespace,
                "config": cfg,
                "layout": layout,
                "estimated": estimated,
            })

    ids = [item["id"] for item in themes]
    if len(ids) != len(set(ids)):
        raise SystemExit("Artwork theme IDs must be unique.")
    if len(themes) > MAX_THEME_COUNT:
        raise SystemExit(
            f"At most {MAX_THEME_COUNT} optional artwork themes are allowed."
        )

    total = sum(item["estimated"] for item in themes)
    if total > MAX_TOTAL_ARTWORK_FLASH_BYTES:
        raise SystemExit(
            f"Artwork assets total {total:,} bytes; build budget is "
            f"{MAX_TOTAL_ARTWORK_FLASH_BYTES:,}. Remove/simplify themes."
        )

    includes = "\n".join(
        f'#include "generated/{item["slug"]}_art.h"' for item in themes
    )
    entries = ",\n".join(registry_entry(item) for item in themes)

    if entries:
        registry_array = f"""static const artwork_theme::Asset THEMES[] = {{
{entries}
}};
static constexpr uint8_t COUNT = sizeof(THEMES) / sizeof(THEMES[0]);"""
    else:
        registry_array = """static const artwork_theme::Asset* const THEMES = nullptr;
static constexpr uint8_t COUNT = 0;"""

    registry = f"""#pragma once

#include <stdint.h>
#include "artwork_theme_asset.h"
{includes}

namespace artwork_registry {{

// GENERATED by tools/build_artwork_themes.py. Do not hand edit.
{registry_array}

inline const artwork_theme::Asset* find(uint8_t id) {{
    for (uint8_t i = 0; i < COUNT; ++i) {{
        if (THEMES[i].id == id) return &THEMES[i];
    }}
    return nullptr;
}}

}} // namespace artwork_registry
"""
    REGISTRY.write_text(registry)

    print("\nArtwork theme registry generated.")
    print(f"Optional themes: {len(themes)}")
    print(f"Estimated generated artwork flash: {total:,} bytes")
    for item in themes:
        print(f"  {item['id']:2d}  {item['name']:<24} {item['estimated']:>7,} bytes")


if __name__ == "__main__":
    main()
