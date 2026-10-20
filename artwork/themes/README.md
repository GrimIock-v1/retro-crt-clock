# Optional artwork themes

This folder is the source for additional **336x240 artwork-driven themes**.

The ESP32 does not load PNGs at runtime. All PNG parsing, palette quantization,
mask analysis, connected-component detection, and compression happen on the
development machine. The generated firmware contains only compact flash-resident
arrays plus a tiny registry entry.

That keeps runtime RAM usage effectively unchanged when another artwork theme is
added.

## Add a theme

Copy the template folder:

```bash
cp -R artwork/themes/_template artwork/themes/my_theme
```

Then provide:

- `theme.json` – theme identity, dynamic features and CRT color calibration
- `background.png` – exactly 336x240
- `palette.json` – designer-facing RGB -> logical composite palette
- `layout.json` – positions for live clock information
- `sky_mask.png` – optional 336x240 white/black mask
- `window_mask.png` – optional 336x240 white/black mask

Generate all optional artwork themes:

```bash
python3 tools/build_artwork_themes.py
```

The script writes:

- `include/generated/<theme>_art.h`
- `include/artwork_theme_registry.h`

Then build/flash normally, or use:

```bash
python3 tools/push_wifi.py
```

The web UI reads the generated registry from the clock and automatically adds
registered artwork themes to the theme selector.

## Runtime model

Every artwork theme shares one renderer:

1. decode row-RLE backdrop directly into the existing 336x240 backbuffer,
2. draw stars only where the sky mask permits them,
3. draw the real lunar phase only where the sky mask permits it,
4. draw animated light regions from window-mask spans,
5. draw live date/weather/time/calendar/day-progress overlays.

No PNG decoder, filesystem image loader, full-size mask, or extra image
framebuffer is created on the ESP32.

## ESP32 guardrails

The generator intentionally rejects artwork that gets too expensive:

- optional artwork themes: maximum 8
- one generated theme: maximum ~96 KB estimated flash data
- optional artwork total: maximum ~384 KB estimated flash data
- theme IDs 0..15 are reserved for firmware themes; optional artwork uses 16..63

Those are conservative build-time limits, not promises that the application slot
can never be exceeded. The WiFi updater also checks the final compiled
`firmware.bin` against the actual OTA slot reported by the clock before upload.

Very noisy single-pixel artwork compresses poorly and also looks worse over NTSC
composite, so the flash limit reinforces the visual rules we want anyway.

## Masks

### sky_mask.png

- white: stars/moon may be drawn here
- black: dynamic sky elements are blocked

This guarantees stars and moon can remain behind mountains, trees and buildings.

### window_mask.png

- black: no animated light
- white: animated light region

Each disconnected white region is detected at build time and becomes an
independent flicker component. Only compact horizontal spans are stored in
firmware.

## Theme IDs

Theme IDs are persisted in NVS. Once a theme is shipped, keep its ID stable.
Do not reuse an old ID for a different visual design.
