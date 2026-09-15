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
