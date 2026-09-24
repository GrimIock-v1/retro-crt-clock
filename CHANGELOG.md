# V3.11.0 - Browser OTA firmware updates

## V3.12.3 - WiFi isolation diagnostic

- Added a 30-second WiFi-off video isolation test.
- The test freezes the displayed framebuffer, stops the local web listener, disables the ESP32 WiFi radio, and leaves only composite scanout running.
- WiFi station mode and the web listener restart automatically after the test.
- This helps distinguish renderer/framebuffer issues from WiFi interrupt, RF, or power-noise effects on composite output.


## V3.12.2 - Video bootstrap fix

- Fixed a startup deadlock introduced by V3.12.1 fresh-VBlank synchronization.
- The first framebuffer handoff now occurs immediately while the composite library has no active framebuffer attached.
- Fresh-VBlank synchronization is used only after scanout has been bootstrapped.


## V3.12.1 - Fresh VBlank presentation

- Changed framebuffer presentation to wait for the start of a fresh NTSC blanking interval.
- If rendering finishes while already in VBlank, the clock now waits through the next active frame instead of swapping near the end of blanking.
- A VBlank timeout now drops that presentation rather than risking a framebuffer handoff during active scanout.
- This specifically targets brief horizontal tears/blips that became easier to expose with faster alternate theme renderers.


## V3.12.0 - Selectable display themes

- Added three selectable clock designs: Rich Cityscape, Daylight, and Retro RPG.
- Preserved the existing Rich Cityscape renderer as the default/fallback theme.
- Added a lightweight Daylight scene with sun, clouds, birds, and restrained two-layer urban silhouettes.
- Added a Retro RPG layout inspired by classic framed status screens: ornamental information panels, large hero time, framed next-meeting panel, and RPG-style day meter.
- Kept the RPG background deliberately simple so the information framing, not a complex landscape, carries the design.
- Theme selection is persisted in NVS and participates in the existing 60-second web preview/revert flow.
- Added theme name to the web status panel and API.
- Added native simulator scenes for the Daylight and Retro RPG themes.


- Added streamed `.bin` firmware upload to the existing local web UI.
- Added upload progress, update-slot reporting, validation/error responses, and automatic reboot after a successful update.
- Pauses scene rendering, bridge refreshes, diagnostics sampling, and video-isolation work during flash writes while leaving the current CRT framebuffer displayed.
- Explicitly pins PlatformIO to the OTA-capable Arduino `default.csv` partition table.
- Preserves NVS clock settings and WiFi credentials across updates.
- Corrected the runtime firmware version string to `3.11.0`.

# V3.10.2 header and weather spacing


## V3.10.2 header/weather spacing

- Brought the top date/weather block slightly lower on the screen.
- Increased the weather icon by about 10 percent.
- Moved the icon closer to the temperature and weather descriptor.

# V3.10.1 HP-style day bar


## V3.10.1 HP-style day bar

- Raised the day-progress bar back into the lower city area.
- Reworked it as a compact retro HP/status bar with a dim shell, dark empty track, brighter fill, and leading marker.
- Kept the bar inset from the screen edges so it no longer reads as a city/horizon separator.

# V3.10 rich skyline


## V3.10 rich skyline

- Reworked the skyline into a richer two-layer city rather than three simpler layers.
- Added more varied building silhouettes, rooftop props, signs, beacons, and window patterns.
- Preserved the bright clock-first layout while making the city feel more alive in the background.

# V3.9.6 - Full-depth city base

- Extends far, mid, and near skyline layers to the bottom edge.
- Continues window lights through the lower extended facades.
- Moves the day-progress indicator to a thin bottom-edge HUD treatment so it no longer reads as a separator through the skyline.
- Keeps V3.9.2+ video-stability work and V3.9.3+ brightness controls unchanged.

# V3.9.5 - Buildings to bottom

- Extended the foreground skyline silhouettes to the bottom edge of the CRT frame.
- Preserved the far and mid skyline layers for depth/parallax.
- The day-progress bar remains overlaid on top of the foreground buildings.

# V3.9.4 - Remove water reflections

- Removed the animated water/reflection effect from the bottom of the city scene.
- The lower raster is now clean/black beneath the skyline, with the day-progress bar remaining as the visual horizon.
- No changes to video timing, web controls, or the V3.9.2 stability work.

# V3.9.3 - Brighter skyline + restore defaults

- Increased the three skyline body luminance levels substantially for real-CRT visibility.
- Increased city-window luminance with the skyline while keeping the main clock brighter.
- The Background brightness slider remains 0-100%; roughly 50% now approximates the old maximum city-body brightness, leaving useful headroom up to 100%.
- Added **Restore defaults** to the web UI. It resets clock/network-display settings to firmware defaults, persists them immediately, reapplies the timezone/scene, and queues a bridge refresh.
- Restore defaults intentionally keeps WiFi credentials. WiFi reset remains on the physical GPIO4 long-press flow.

# V3.9.2 video isolation diagnostics

- Added three 30-second video isolation tests to separate rendering load from framebuffer handoff behavior.
- **Freeze**: no rendering and no swaps. Composite ISR/DMA scanout and WiFi remain active.
- **Render-only**: repeatedly redraws one frozen scene into the hidden backbuffer every 500 ms without changing the displayed framebuffer.
- **Swap-only**: pre-renders the same frozen scene into both framebuffers, then only swaps/hands off the identical buffers every 500 ms.
- Normal bridge refreshes, periodic heap/RSSI sampling, and normal scene rendering are paused during an isolation test so each test changes one main variable.
- Removed WiFi status, RSSI, free-heap, and largest-block queries from the twice-per-second render function. The renderer now consumes a diagnostic cache refreshed every 5 seconds outside the render path.
- `/api/status` now reports the active video-test mode, remaining time, step count, and VBlank wait timeout count.
- Retains the V3.9.1 HTTP 5-second read timeout, one timeout retry, readable HTTP error logging, and VBlank-aware framebuffer handoff.

**Interpretation:** if Render-only jitters, full framebuffer drawing/memory traffic is disturbing scanout timing. If Render-only is stable but Swap-only jitters, the framebuffer swap/handoff/VBlank path is the primary suspect. If both are stable but normal mode jitters, investigate the combination of dynamic rendering, system calls, or other periodic work.

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
