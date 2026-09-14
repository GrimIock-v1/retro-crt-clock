# V3.9.1a build fix

- Fixed the malformed newline escape in the periodic health `Serial.printf()` that prevented PlatformIO compilation.
- No functional changes to the VBlank synchronization, HTTP retry, or freeze diagnostic.

# V3.9.1

## V3.9.1 sync / network diagnostic update

- Framebuffer handoff is now synchronized to the NTSC vertical blanking interval instead of occurring mid-raster.
- Bridge HTTP read timeout increased to 5 seconds.
- A read timeout is retried once after 250 ms.
- HTTP failures log both the numeric code and its readable description.
- Added a 30-second "Freeze display" test in the web UI. During the test the already-selected framebuffer remains untouched while WiFi/video scanout continue.
- Added a five-minute heap/RSSI/video health log.

If the CRT still jumps during the 30-second freeze, the issue is below the scene renderer/framebuffer-swap layer and should be investigated as interrupt/signal/power timing. If the jump stops, the prior unsynchronized buffer handoff was the likely cause.

# V3.9 Web Control Panel

- Added a lightweight built-in HTTP configuration interface on port 80.
- Added persistent NVS settings for bridge URL, timezone, drift pixels/interval, night brightness, clock brightness, background brightness, time format, temperature units, weather text, day bar and city animation speed.
- Added 60-second non-persistent display preview with automatic revert.
- Added `/api/settings` and `/api/status` JSON endpoints.
- Added web actions for bridge refresh, diagnostics and restart.
- Bridge refresh is queued and executed by the normal loop, never inside the HTTP handler.
- Parameterized the existing renderer instead of adding a second web-specific drawing path.
- Added 24-hour clock and Fahrenheit support.

# V3.8.3 moon size tweak


- Increased the moon size by about 10 percent for better visibility near thin phases.

# V3.8.2 alignment correction


- Nudged the AM/PM marker back up for better baseline alignment.
- Moved the weather descriptor higher so it tucks more tightly under the temperature.

# V3.8.1 alignment tweak


- Aligned the weather descriptor directly under the temperature block.
- Lowered the AM/PM marker so it aligns better with the bottom of the main clock digits.

# V3.8 UI polish


## V3.8 polish update

- Added a weather descriptor label such as SUNNY, CLOUDY, or WINDY.
- Increased the size and contrast of the NEXT label.
- Reduced burn-in drift so the screen no longer looks like it is constantly bumping around.
- Brightened the skyline and windows so the city scene is more noticeable.
- Moved the moon a bit higher.

# V3.7 Minor visual tweaks

- Moved the moon higher and rendered it behind the skyline layers.
- Made the bottom day-progress horizon thicker for better CRT readability.
- Replaced procedural weather symbols with bitmap icons derived from the user-provided icon set.
- Expanded weather condition mapping to support thunder, storm, fog, wind, drizzle, partly cloudy, and clear night icons.

# V3.6 - Mockup implementation

- Rebuilt the on-device renderer around the approved tall-city mockup.
- Embedded the required 8px arcade glyph subset derived from Press Start 2P under OFL 1.1; no font file is distributed.
- Enlarged and vertically centered the hero clock.
- Added three dim parallax skyline layers behind the clock, plus sparse windows, clouds, stars and water shimmer.
- Moved the computed moon into the right side of the city scene.
- Moved the calendar block below the clock and retained date/weather placement.
- Added slow +/-2 px foreground drift as an additional CRT burn-in mitigation measure.
- Removed the V3.4 HTTP stress-test loop from the production build while keeping interrupt/DMA scanout with one framebuffer-pointer update after each render.

# V3.4 WiFi Validation

- Removed the redundant continuous core-0 video sender task.
- After each `graphics.end()` framebuffer swap, repoint the interrupt/DMA video
  engine exactly once to the new front buffer.
- Added a 50-request / 30-second-interval station-mode HTTP coexistence test
  after video startup.
- Added GET success/failure counts, HTTP latency, RSSI, heap, and largest-block
  statistics to the diagnostic screen and serial log.
- Captive-portal setup remains video-off; this build tests ordinary connected
  WiFi rather than changing provisioning behavior.

# V3.3

- Removed the unsupported ESP-IDF power-management lock. The firmware now logs
  the actual CPU frequency immediately before composite-video initialization and
  warns if it is not 240 MHz.
- Suppressed the pinned composite library's unused GPIO18 PWM-audio setup. This
  removes the `ledcSetup()` startup error without changing its DAC/I2S/APLL/DMA
  video path or unpinning the library.
- No changes to framebuffer allocation, video-task scheduling, drawing cadence,
  WiFi sequencing, bridge protocol, or CRT scene rendering.

# V3.2

- Fixed timezone handling by replacing `configTime(0, 0, ...)` with
  `configTzTime(timezoneRule, ...)`.
- Added a serial log showing resolved local time and DST/standard state after
  NTP synchronization.

# V3.1 - Ultramagnus bridge integration

- Default bridge URL is now `http://crt-clock-bridge.ultramagnus.ca/status`.
- Added compatibility with the deployed compact JSON schema (`next_event_epoch`, `weather.temp_c`, `weather.condition`).
- Retained compatibility with the full V3 bridge API.
- Added case-insensitive/word-based weather condition parsing (`CLOUDY`, `CLEAR`, etc.).
- Compact event epochs are formatted locally as `TODAY`, `TMR`, or weekday plus 12-hour time.
- Added explicit serial diagnostics for HTTP redirects, especially HTTP-to-HTTPS redirects that this no-TLS WROOM build cannot follow safely.
- Automatically migrates only the old generated-project placeholder bridge URLs while preserving genuine user-configured URLs.

# Revision notes

## V3 reliability / appliance pass

- Rebuilt the bridge so `/api/status` is an immediate in-memory read and never waits for Open-Meteo or ICS servers.
- Split weather/sun and calendar refreshes into independent background worker threads.
- Added persistent last-known-good bridge state at `/data/last_status.json` with atomic writes.
- Added Docker volume persistence for bridge state.
- Added bridge API version `v: 3` and firmware compatibility checking.
- Added independent weather and calendar `valid` / `fresh` states.
- Added 1-minute ESP retry cadence for stale or failed bridge results; fresh results remain on the 5-minute cadence.
- Added real Open-Meteo sunrise/sunset data and local seasonal ambient transitions.
- Added next-event context: `TODAY`, `TMR`, weekday, or month/day for more distant events.
- Continued to omit event titles and calendar contents from the device payload.
- Added explicit invalid-time screen (`--:--` / `SYNC TIME`) instead of displaying plausible fake defaults.
- Added one-pixel 3x3 foreground drift every 15 minutes for CRT phosphor-wear mitigation.
- Added foreground nighttime luma reduction.
- Added a debounced short-press GPIO4 diagnostic mode while preserving the 3-second WiFi-reset hold.
- Added CRT diagnostic screen with safe area, luma ramp, pixel-frequency pattern, heap stats, time source, connectivity, and equal-luma inline path comparison blocks.
- Added task-creation failure checking for the core-0 composite sender.
- Cached date, rounded temperature, and formatted event strings.
- Fixed temperature rounding around half degrees and negative zero.
- Expanded native scene tests and compile them with warnings as errors.

## V2 design / optimization pass

- Replaced the oversized 5x7 time with a dedicated blocky segmented numeral set.
- Removed the permanent outer software border and redundant status labels.
- Added a compact top band for date, moon, weather icon, and temperature.
- Integrated day progress into the city horizon.
- Added two-layer skyline parallax with calmer windows.
- Added explicit CRT safe-area constants.
- Reduced normal framebuffer regeneration from 10 fps to 2 fps while keeping composite transmission continuous.
- Converted general font placement to fixed-point internally while preserving float scale inputs.
- Cached building geometry and moon scanlines.
- Moved primary timekeeping to NTP with bridge time as fallback.
- Kept the first bridge request before framebuffer allocation.

## Intentionally unchanged

- Pixel drawing remains header-only and forced inline.
- Video starts only after WiFi setup plus a grace period.
- Video sending remains a simple core-0 FreeRTOS loop with `vTaskDelay(1)`.
- No video-task suspension or dynamic video teardown was introduced.
- No dirty-rectangle or complex task synchronization scheme was introduced.
- The plain ESP32 build still refuses HTTPS bridge URLs.
- Platform/core/video-library versions remain pinned.
