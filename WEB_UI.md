# V3.9 Web UI

Open `http://<clock-ip>/` after the clock has completed normal startup. The exact URL is printed to the serial monitor.

## Endpoints

- `GET /` - configuration page
- `GET /api/settings` - active settings
- `GET /api/status` - current clock/network/heap status
- `POST /api/preview` - temporarily apply display settings for 60 seconds
- `POST /api/revert` - restore saved settings
- `POST /api/save` - validate, persist, and apply settings
- `POST /api/refresh` - queue a bridge refresh
- `POST /api/diagnostics` - toggle the CRT diagnostic screen
- `POST /api/restart` - schedule an ESP32 restart

The browser does not poll automatically. Status is fetched once on page load and when the user presses **Refresh status**.

## Safety / timing choices

The web server starts only after the provisioning flow has finished and video is active. It never opens WiFiManager or AP/captive-portal mode while composite video is running. The page is stored in flash and served directly; there are no external libraries or assets. Upstream bridge requests are never executed from a web handler.

## Default settings

| Setting | Default | Range |
|---|---:|---:|
| Drift pixels | 1 px | 0-5 |
| Drift interval | 30 min | 1-120 min |
| Night brightness | 75% | 10-100% |
| Clock brightness | 100% | 25-100% |
| Background brightness | 100% | 0-100% |
| City animation | Normal | Off/Slow/Normal/Fast |
| Time format | 12-hour | 12/24 |
| Temperature | Celsius | C/F |
| Weather descriptor | On | On/Off |
| Day progress bar | On | On/Off |

## Network exposure

The control panel has no authentication. It is intended for a trusted home LAN. Do not expose the ESP32's port 80 directly to the public Internet.


## V3.9.1 sync / network diagnostic update

- Framebuffer handoff is now synchronized to the NTSC vertical blanking interval instead of occurring mid-raster.
- Bridge HTTP read timeout increased to 5 seconds.
- A read timeout is retried once after 250 ms.
- HTTP failures log both the numeric code and its readable description.
- Added a 30-second "Freeze display" test in the web UI. During the test the already-selected framebuffer remains untouched while WiFi/video scanout continue.
- Added a five-minute heap/RSSI/video health log.

If the CRT still jumps during the 30-second freeze, the issue is below the scene renderer/framebuffer-swap layer and should be investigated as interrupt/signal/power timing. If the jump stops, the prior unsynchronized buffer handoff was the likely cause.
