# Moonlit Cabin artwork pipeline

The Moonlit Cabin static backdrop is now authored at the clock's native
**336x240** resolution. The firmware does not need a second image framebuffer:
the generated indexed artwork is decoded directly into the normal backbuffer,
then the live clock elements are drawn on top.

## What the artist should draw

Create `background.png` at exactly **336x240**.

The PNG should contain the static scene:

- sky color / gradient
- mountains and snow
- water / reflections
- trees and foreground
- cabin body / roof
- dark, unlit window openings

Do **not** bake these into the artwork because the clock renders them live:

- stars
- moon
- date
- weather icon / temperature / descriptor
- main clock and AM/PM
- next event
- day-progress bar
- lit window glow

Keeping those pieces live means one piece of artwork works for every date,
weather state, calendar event and moon phase.

## Dynamic sky mask

Stars are rendered only where the generated `SKY_MASK` permits them. This is
what prevents stars from appearing in front of mountains.

There are three ways to define sky:

1. **Transparent pixels** in `background.png` are automatically considered
   dynamic sky and are filled with the palette's `transparent_fill` color.
2. Palette colors can set `"dynamic_sky": true` in `palette.json`.
3. For exact control, provide a separate 336x240 grayscale `sky_mask.png` and
   pass it with `--mask`. White permits stars; black blocks them.

The explicit mask is best for artwork with gradients, clouds, or complicated
silhouettes.

## Dynamic positions and light mask

`layout.json` defines the moon anchor.

For new artwork, animated window/light shapes should be supplied as a separate
336x240 `window_mask.png`:

- black = static artwork
- white = firmware-controlled glow

Each disconnected white region becomes an independently flickering light. The
converter stores compact horizontal spans in flash, not a 336x240 runtime mask.

The rectangle entries still present in the compatibility `layout.json` are a
legacy fallback for the current Cabin asset and can be removed once the new
authored artwork supplies `window_mask.png`.

## Palette

`palette.json` is the designer-facing palette. The converter quantizes source
RGB pixels to these logical colors. The ESP32 renderer then maps those logical
colors to the Sony-calibrated composite hue codes.

This separation is intentional: artwork can stay visually understandable while
the hardware mapping can be tuned independently for the real CRT.

Avoid very fine 1-pixel alternating color patterns. They are legal, but NTSC
composite will produce more chroma artifacts than broader color shapes.

## Convert an image

One-time Python setup:

```bash
python3 -m venv .venv-art
source .venv-art/bin/activate
pip install -r tools/requirements-art.txt
```

Convert:

```bash
python3 tools/convert_background.py \
  artwork/moonlit_cabin/background.png \
  --palette artwork/moonlit_cabin/palette.json \
  --layout artwork/moonlit_cabin/layout.json \
  --sky-mask artwork/moonlit_cabin/sky_mask.png \
  --window-mask artwork/moonlit_cabin/window_mask.png \
  --output include/moonlit_cabin_art.h \
  --preview-dir artwork/moonlit_cabin/build
```

Both mask arguments are optional. If the sky mask is omitted, transparent or
palette-marked sky pixels are used. If the window mask is omitted, the legacy
window rectangles in `layout.json` are used.

The preview directory contains:

- `quantized.png` – what colors were accepted by the designer palette
- `sky-mask.png` – exactly where live stars/moon are allowed
- `window-mask.png` – exactly where animated light is allowed

Commit the generated `include/moonlit_cabin_art.h` with the artwork change.

## Current compatibility artwork

The current generated asset is already **336x240**, but it is a 2x compatibility
conversion of the previous 168x120 art. It proves the full-resolution renderer
and mask pipeline. A genuinely authored 336x240 PNG is what will add the extra
detail we actually want.
