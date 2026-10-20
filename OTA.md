# Browser OTA firmware updates

## First install

V3.11 must be flashed once over USB because older firmware does not contain the OTA web endpoint. The project pins `board_build.partitions = default.csv`, which provides two application slots.

## Future updates

1. Build normally with PlatformIO: `pio run`.
2. Open the clock web UI.
3. In **Firmware Update**, choose `.pio/build/esp32dev/firmware.bin`.
4. Upload and keep power connected.
5. The current CRT image intentionally freezes while flash is written.
6. After verification, the ESP32 reboots into the new image.

WiFiManager credentials and the `retroclock` Preferences namespace are in NVS and are preserved.

## Important

OTA only replaces an application partition. It does not update the partition table. Keep future OTA builds on an OTA-compatible partition layout unless you intentionally return to USB for a partition-table change.


## One-command WiFi update

The existing browser OTA endpoint can also be driven directly from the repo, so
you do not need to connect USB or manually choose `firmware.bin`.

Set the clock IP once:

```bash
echo 192.168.1.42 > .clock-host
```

Use the IP shown on the CRT diagnostics screen or in the web UI.

Then every future update is:

```bash
git pull
python3 tools/push_wifi.py
```

The script:

1. reads the current firmware version and OTA-slot capacity from the clock,
2. regenerates all enabled artwork themes and applies the artwork flash budgets,
3. runs `pio run -e esp32dev`,
4. checks the final `firmware.bin` against the real OTA slot with a safety margin,
5. uploads it directly to `/api/firmware` over WiFi,
6. waits for the ESP32 to reboot,
7. reports the firmware version that came back online.

You can override the saved host without editing anything:

```bash
python3 tools/push_wifi.py 192.168.1.99
```

Or use the `CRT_CLOCK_IP` environment variable.

To resend an already-built binary without rebuilding:

```bash
python3 tools/push_wifi.py --no-build
```

This intentionally reuses the firmware's existing HTTP OTA implementation
rather than adding ArduinoOTA/mDNS services to the running clock. That keeps the
normal video/WiFi runtime unchanged and avoids adding another network service
while composite scanout is active.
