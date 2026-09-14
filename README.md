# Retro CRT Clock V3.9

ESP32 composite-video clock for a 336x240 NTSC CRT. This release implements the approved tall-city mockup as actual firmware drawing code.

## Hardware

- ESP32 / ESP-WROOM-32 dev board
- GPIO25 (DAC1) -> RCA center pin
- GND -> RCA outer shell
- momentary button between GPIO4 and GND

GPIO4 uses `INPUT_PULLUP`. Avoid GPIO0, GPIO2, GPIO12 and GPIO15 for the button because they are boot-strapping pins.

## Build and flash

Open this directory in PlatformIO, then run:

```bash
pio run
pio run -t upload
pio device monitor
```

The project deliberately pins `espressif32 @ 6.8.1`, which uses Arduino-ESP32 2.0.17. Do not casually update to Arduino-ESP32 3.x because this old composite-video library depends on older low-level APIs.

## Built-in web control panel

After WiFi, NTP, the initial bridge fetch, and composite video have started, V3.9 starts a deliberately small HTTP server on port 80. The serial monitor prints the address, for example:

```text
Clock web UI: http://10.0.0.166/
```

The server is intentionally simple: one static HTML page stored in flash, no WebSockets, no background polling, and no captive portal while video is running. Settings are stored in ESP32 Preferences/NVS.

Available controls:

- Bridge URL
- POSIX timezone rule
- burn-in drift: 0-5 pixels
- drift interval: 1-120 minutes
- night brightness: 10-100%
- main clock brightness: 25-100%
- background brightness: 0-100%
- city animation: off / slow / normal / fast
- 12-hour / 24-hour clock
- Celsius / Fahrenheit
- weather descriptor on/off
- day-progress bar on/off

Display settings can be previewed for 60 seconds without writing flash. If they are not saved, the firmware automatically returns to the last saved configuration. Bridge fetches are queued outside the web request handler so the UI never performs a blocking upstream network request itself.

The status section exposes WiFi RSSI, bridge/time status, free heap, largest contiguous heap block, uptime, current weather, and next event. It also provides buttons for refresh-data, diagnostics, and restart.

## Bridge

Default endpoint:

```text
http://crt-clock-bridge.ultramagnus.ca/status
```

The production firmware understands the compact live payload currently returned by that endpoint, including `next_event_epoch`, `weather.temp_c`, and `weather.condition`. It also retains compatibility with the fuller V3 bridge schema.

The plain ESP32 build intentionally uses HTTP rather than HTTPS. With the video double framebuffer allocated, the largest contiguous heap block is too small to make TLS a sensible reliability target.

## Timezone

The default POSIX timezone rule is:

```text
PST8PDT,M3.2.0,M11.1.0
```

NTP is configured with `configTzTime()`, so Pacific daylight/standard time transitions are automatic. The timezone rule can be changed in the WiFiManager portal.

## Startup architecture

1. Video hardware is not initialized during WiFi provisioning.
2. WiFi connects through WiFiManager.
3. Firmware waits four seconds for the station/AP transition to settle.
4. NTP is started and the first tiny HTTP bridge request runs while the large video framebuffer has not yet been allocated.
5. Composite video is initialized.
6. The color-video library then scans out asynchronously through its interrupt/DMA path.
7. After every completed double-buffer render, the firmware updates the displayed framebuffer pointer once with `sendFrameHalfResolution()`.

There is no continuous FreeRTOS video-sender loop in this release.

## Approved V3.6 visual design

The actual renderer in `include/retro_scene.h` now implements the approved design rather than merely approximating the mockup:

- chunky 8-bit arcade typography for the whole interface
- large vertically centered time as the primary focal point
- date in the upper-left
- weather in the upper-right
- next calendar event below the clock on the left
- moon integrated into the right side of the scene and kept clear of the clock
- three dark parallax skyline layers filling most of the screen height behind the UI
- tall, simplified buildings designed to animate cheaply
- sparse windows with slow state changes
- slowly drifting blocky clouds and sparse stars
- subtle water/reflection band at the bottom
- day-progress line integrated into the horizon
- foreground UI drift is configurable from 0-5 pixels with a configurable interval

The background is intentionally much dimmer than the text. It should read as atmosphere on the CRT, not compete with the time.

## Typeface

`include/arcade_bitmap_font.h` contains only the small 8-pixel ASCII bitmap subset needed by this firmware. Its glyph forms are derived from the Press Start 2P source by Cody "CodeMan38" Boisclair and are embedded as integer bitmap data rather than shipping a font file.

The font-derived data remains subject to the SIL Open Font License 1.1. See:

```text
LICENSES/PressStart2P-OFL.txt
```

## Important drawing constraint

Do not move drawing helpers out of the headers.

Every helper that eventually writes pixels or calls `fillRect()` is intentionally `inline` and defined in the header. This project previously demonstrated a real CRT brightness difference when identical drawing calls were routed through separately compiled non-inline helpers, even when the framebuffer bytes matched.

## WiFi reset / diagnostics button

- short press GPIO4: toggle CRT diagnostic screen
- hold GPIO4 for 3 seconds: erase WiFi credentials and restart into provisioning

The captive portal is never opened while video is running.

## Desktop simulator

The simulator compiles the exact same `retro_scene.h` drawing/layout code against an in-memory backend:

```bash
cd simulator
make run
```

It writes the preview PPM files. The PNG previews in the project root were generated from that exact renderer with nearest-neighbor scaling.

This matters: `preview-evening.png` is not an AI concept image. It is the output of the same drawing code that will run on the ESP32.

## CRT notes

The output is grayscale. If a color CRT shows colored fringes around sharp white pixel edges, turn down the set's physical CHROMA/COLOR control. Composite luma/chroma crosstalk can create false color even when the generated image is grayscale.

## What has been validated here

The native preview renderer and scene tests compile with `-Wall -Wextra -Werror` and run successfully. The Python bridge is syntax checked.

The ESP32 PlatformIO toolchain is not installed in the environment used to package this release, so the final embedded compile and analog-output validation still happen when you run `pio run` / flash it on your hardware.


## V3.7 tweak update

This build applies the latest requested polish pass: the moon now sits higher in the scene and behind the buildings, the integrated bottom day bar is thicker, and the weather indicator uses the provided pixel icon set.


## V3.8 polish update

- Added a weather descriptor label such as SUNNY, CLOUDY, or WINDY.
- Increased the size and contrast of the NEXT label.
- Reduced burn-in drift so the screen no longer looks like it is constantly bumping around.
- Brightened the skyline and windows so the city scene is more noticeable.
- Moved the moon a bit higher.


## V3.8.1 alignment tweak

- Aligned the weather descriptor directly under the temperature block.
- Lowered the AM/PM marker so it aligns better with the bottom of the main clock digits.


## V3.8.2 alignment correction

- Nudged the AM/PM marker back up for better baseline alignment.
- Moved the weather descriptor higher so it tucks more tightly under the temperature.


## V3.8.3 moon size tweak

- Increased the moon size by about 10 percent for better visibility near thin phases.


## V3.9.1 sync / network diagnostic update

- Framebuffer handoff is now synchronized to the NTSC vertical blanking interval instead of occurring mid-raster.
- Bridge HTTP read timeout increased to 5 seconds.
- A read timeout is retried once after 250 ms.
- HTTP failures log both the numeric code and its readable description.
- Added a 30-second "Freeze display" test in the web UI. During the test the already-selected framebuffer remains untouched while WiFi/video scanout continue.
- Added a five-minute heap/RSSI/video health log.

If the CRT still jumps during the 30-second freeze, the issue is below the scene renderer/framebuffer-swap layer and should be investigated as interrupt/signal/power timing. If the jump stops, the prior unsynchronized buffer handoff was the likely cause.


## V3.9.2 video isolation tests

The web UI now includes three 30-second CRT timing tests:

1. **Freeze** - leaves the currently displayed framebuffer untouched. No pixel rendering or framebuffer swaps occur.
2. **Render-only** - redraws the same frozen scene into the hidden backbuffer every 500 ms, but never swaps it onto the CRT. This isolates CPU/RAM/pixel-writing load.
3. **Swap-only** - prepares both framebuffers with identical pixels, then swaps/hands them off every 500 ms without drawing pixels during the active test. This isolates the swap/VBlank/handoff path.

Run one test at a time and watch only the 30-second ACTIVE period. Normal bridge refresh, periodic heap/RSSI cache refresh, and ordinary scene rendering are paused while a test is active. Serial logs announce when preparation ends and the clean observation period begins.

Interpretation:

- Freeze stable + Render-only jitters -> rendering/memory traffic is disturbing composite timing.
- Freeze stable + Render-only stable + Swap-only jitters -> swap/VBlank/framebuffer handoff is the likely fault.
- All three stable + normal mode jitters -> investigate dynamic scene preparation, clock/system calls, bridge/web activity, or interaction between rendering and swapping.

V3.9.2 also moves WiFi/RSSI/heap diagnostic queries out of `renderFrame()` and caches those values every five seconds during normal operation.
