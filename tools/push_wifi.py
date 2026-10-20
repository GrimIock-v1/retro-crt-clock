#!/usr/bin/env python3
"""
Build and push firmware to the clock using the existing browser OTA endpoint.

One-time setup:
  echo 192.168.1.42 > .clock-host

Then:
  python3 tools/push_wifi.py

You can also pass a host explicitly:
  python3 tools/push_wifi.py 192.168.1.42
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid


ROOT = Path(__file__).resolve().parents[1]
HOST_FILE = ROOT / ".clock-host"
FIRMWARE = ROOT / ".pio" / "build" / "esp32dev" / "firmware.bin"


def normalize_host(value: str) -> str:
    value = value.strip().rstrip("/")
    if value.startswith("http://") or value.startswith("https://"):
        return value
    return "http://" + value


def read_host(cli_host: str | None) -> str:
    if cli_host:
        return normalize_host(cli_host)

    env_host = os.environ.get("CRT_CLOCK_IP", "").strip()
    if env_host:
        return normalize_host(env_host)

    if HOST_FILE.exists():
        value = HOST_FILE.read_text().strip()
        if value:
            return normalize_host(value)

    raise SystemExit(
        "Clock host not set. Pass an IP/hostname, set CRT_CLOCK_IP, or create "
        ".clock-host in the repo root."
    )


def get_json(url: str, timeout: float = 3.0):
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def build_firmware() -> None:
    print("Building esp32dev firmware...")
    subprocess.run(
        ["pio", "run", "-e", "esp32dev"],
        cwd=ROOT,
        check=True,
    )
    if not FIRMWARE.exists():
        raise SystemExit(f"Build succeeded but {FIRMWARE} was not found.")


def multipart_firmware(path: Path):
    boundary = "----RetroClock" + uuid.uuid4().hex
    data = path.read_bytes()

    head = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n"
    ).encode("ascii")
    tail = f"\r\n--{boundary}--\r\n".encode("ascii")
    return boundary, head + data + tail


def upload(host: str) -> None:
    url = host + "/api/firmware"
    boundary, body = multipart_firmware(FIRMWARE)
    request = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(body)),
        },
    )

    print(f"Uploading {FIRMWARE.name} ({FIRMWARE.stat().st_size:,} bytes) to {url}...")
    try:
        with urllib.request.urlopen(request, timeout=90) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise SystemExit(f"OTA upload failed: HTTP {exc.code}\n{detail}") from exc

    if not payload.get("ok", False):
        raise SystemExit("Clock rejected firmware: " + payload.get("message", "Unknown error"))

    print(payload.get("message", "Firmware accepted."))


def wait_for_clock(host: str, old_version: str | None) -> None:
    print("Waiting for clock to reboot...")
    deadline = time.time() + 35
    last_error = None

    while time.time() < deadline:
        time.sleep(1.0)
        try:
            status = get_json(host + "/api/status", timeout=2.0)
            version = str(status.get("firmware", "?"))
            print(f"Clock online. Firmware: {version}")
            if old_version and version == old_version:
                print("Warning: version string did not change. Verify the intended build was uploaded.")
            return
        except Exception as exc:
            last_error = exc

    print("Upload completed, but the clock did not answer within 35 seconds.")
    if last_error:
        print(f"Last connection error: {last_error}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("host", nargs="?", help="Clock IP or hostname")
    parser.add_argument("--no-build", action="store_true", help="Upload existing firmware.bin")
    args = parser.parse_args()

    host = read_host(args.host)
    old_version = None

    try:
        status = get_json(host + "/api/status")
        old_version = str(status.get("firmware", "?"))
        print(f"Clock found at {host}. Current firmware: {old_version}")
    except Exception as exc:
        print(f"Warning: could not read clock status before upload: {exc}")

    if not args.no_build:
        build_firmware()
    elif not FIRMWARE.exists():
        raise SystemExit(f"{FIRMWARE} does not exist. Build first or omit --no-build.")

    upload(host)
    wait_for_clock(host, old_version)


if __name__ == "__main__":
    main()
