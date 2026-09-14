import copy
import json
import os
import threading
import time
from datetime import date, datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

import recurring_ical_events
import requests
from flask import Flask, jsonify
from icalendar import Calendar

app = Flask(__name__)

API_VERSION = 3
LATITUDE = float(os.environ.get("LATITUDE", "49.058"))
LONGITUDE = float(os.environ.get("LONGITUDE", "-122.470"))
TIMEZONE = os.environ.get("TIMEZONE", "America/Vancouver")
CALENDAR_URLS = [u.strip() for u in os.environ.get("CALENDAR_URLS", "").split(",") if u.strip()]
REFRESH_SECONDS = int(os.environ.get("REFRESH_SECONDS", os.environ.get("CACHE_SECONDS", "180")))
RETRY_SECONDS = int(os.environ.get("RETRY_SECONDS", "60"))
STALE_AFTER_SECONDS = int(os.environ.get("STALE_AFTER_SECONDS", "900"))
STATE_FILE = Path(os.environ.get("STATE_FILE", "/data/last_status.json"))
DISABLE_WORKERS = os.environ.get("RETRO_BRIDGE_DISABLE_WORKERS", "0") == "1"

tz = ZoneInfo(TIMEZONE)
state_lock = threading.Lock()
persist_lock = threading.Lock()
workers_started = False


def default_state():
    return {
        "weather": {
            "valid": False,
            "temp_c10": 0,
            "condition": "unknown",
            "updated_epoch": 0,
            "last_attempt_ok": False,
        },
        "sun": {
            "valid": False,
            "sunrise_min": 8 * 60,
            "sunset_min": 18 * 60,
            "updated_epoch": 0,
            "last_attempt_ok": False,
        },
        "calendar": {
            "configured": bool(CALENDAR_URLS),
            "valid": not bool(CALENDAR_URLS),
            "updated_epoch": int(time.time()) if not CALENDAR_URLS else 0,
            "last_attempt_ok": not bool(CALENDAR_URLS),
            "event": {
                "valid": False,
                "start_epoch": 0,
                "all_day": False,
            },
        },
    }


def load_state():
    base = default_state()
    try:
        raw = json.loads(STATE_FILE.read_text())
        for section in ("weather", "sun", "calendar"):
            if isinstance(raw.get(section), dict):
                base[section].update(raw[section])
        if isinstance(raw.get("calendar", {}).get("event"), dict):
            base["calendar"]["event"].update(raw["calendar"]["event"])
    except FileNotFoundError:
        pass
    except Exception as exc:
        app.logger.warning("Persistent state load failed: %s", type(exc).__name__)

    # A process restart means the persisted snapshot is useful but explicitly
    # stale until each upstream worker proves it can refresh again.
    base["weather"]["last_attempt_ok"] = False
    base["sun"]["last_attempt_ok"] = False
    if CALENDAR_URLS:
        base["calendar"]["last_attempt_ok"] = False
    else:
        base["calendar"]["configured"] = False
        base["calendar"]["valid"] = True
        base["calendar"]["last_attempt_ok"] = True
        base["calendar"]["updated_epoch"] = int(time.time())
        base["calendar"]["event"] = {"valid": False, "start_epoch": 0, "all_day": False}
    return base


state = load_state()


def persistent_snapshot():
    with state_lock:
        return copy.deepcopy(state)


def persist_state():
    snapshot = persistent_snapshot()
    try:
        with persist_lock:
            STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
            tmp = STATE_FILE.with_suffix(STATE_FILE.suffix + ".tmp")
            tmp.write_text(json.dumps(snapshot, separators=(",", ":")))
            os.replace(tmp, STATE_FILE)
    except Exception as exc:
        app.logger.warning("Persistent state save failed: %s", type(exc).__name__)


def weather_condition(code: int) -> str:
    if code == 0:
        return "sun"
    if code in (1, 2, 3, 45, 48):
        return "cloud"
    if code in (71, 73, 75, 77, 85, 86):
        return "snow"
    if code in (51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82, 95, 96, 99):
        return "rain"
    return "cloud"


def local_minutes(iso_value: str) -> int:
    value = datetime.fromisoformat(iso_value)
    if value.tzinfo is None:
        value = value.replace(tzinfo=tz)
    else:
        value = value.astimezone(tz)
    return value.hour * 60 + value.minute


def fetch_weather_and_sun():
    # One Open-Meteo request supplies both current weather and today's
    # astronomical sunrise/sunset values. Daily sunrise/sunset require timezone.
    r = requests.get(
        "https://api.open-meteo.com/v1/forecast",
        params={
            "latitude": LATITUDE,
            "longitude": LONGITUDE,
            "current": "temperature_2m,weather_code",
            "daily": "sunrise,sunset",
            "timezone": TIMEZONE,
            "forecast_days": 1,
        },
        timeout=8,
    )
    r.raise_for_status()
    payload = r.json()

    current = payload["current"]
    weather = {
        "temp_c10": int(round(float(current["temperature_2m"]) * 10.0)),
        "condition": weather_condition(int(current["weather_code"])),
    }

    daily = payload.get("daily", {})
    sunrise_values = daily.get("sunrise") or []
    sunset_values = daily.get("sunset") or []
    sun = None
    if sunrise_values and sunset_values:
        sun = {
            "sunrise_min": local_minutes(sunrise_values[0]),
            "sunset_min": local_minutes(sunset_values[0]),
        }

    return weather, sun


def normalize_start(component):
    value = component["DTSTART"].dt
    all_day = isinstance(value, date) and not isinstance(value, datetime)

    if all_day:
        value = datetime(value.year, value.month, value.day, tzinfo=tz)
    elif value.tzinfo is None:
        value = value.replace(tzinfo=tz)
    else:
        value = value.astimezone(tz)

    return value, all_day


def fetch_next_event(now):
    horizon = now + timedelta(days=45)
    candidates = []

    for url in CALENDAR_URLS:
        try:
            r = requests.get(url, timeout=10)
            r.raise_for_status()
            cal = Calendar.from_ical(r.content)
            occurrences = recurring_ical_events.of(cal).between(now, horizon)
            for item in occurrences:
                try:
                    start, all_day = normalize_start(item)
                except Exception:
                    continue
                if start >= now:
                    candidates.append((start, all_day))
        except Exception as exc:
            # A missing feed means the merged result is incomplete. Keep the
            # previous merged event rather than silently replacing it with a
            # potentially-later event from only the feeds that happened to work.
            app.logger.warning("Calendar fetch/parse failed: %s", type(exc).__name__)
            raise

    if not candidates:
        return {"valid": False, "start_epoch": 0, "all_day": False}

    start, all_day = min(candidates, key=lambda pair: pair[0])
    return {
        "valid": True,
        "start_epoch": int(start.timestamp()),
        "all_day": bool(all_day),
    }


def weather_worker():
    while True:
        sleep_for = REFRESH_SECONDS
        try:
            weather, sun = fetch_weather_and_sun()
            now_epoch = int(time.time())
            with state_lock:
                state["weather"].update(
                    valid=True,
                    temp_c10=weather["temp_c10"],
                    condition=weather["condition"],
                    updated_epoch=now_epoch,
                    last_attempt_ok=True,
                )
                if sun is not None:
                    state["sun"].update(
                        valid=True,
                        sunrise_min=sun["sunrise_min"],
                        sunset_min=sun["sunset_min"],
                        updated_epoch=now_epoch,
                        last_attempt_ok=True,
                    )
                else:
                    state["sun"]["last_attempt_ok"] = False
            persist_state()
        except Exception as exc:
            sleep_for = RETRY_SECONDS
            with state_lock:
                state["weather"]["last_attempt_ok"] = False
                state["sun"]["last_attempt_ok"] = False
            app.logger.warning("Weather/sun refresh failed: %s", type(exc).__name__)

        time.sleep(max(5, sleep_for))


def calendar_worker():
    if not CALENDAR_URLS:
        return

    while True:
        sleep_for = REFRESH_SECONDS
        try:
            event = fetch_next_event(datetime.now(tz))
            now_epoch = int(time.time())
            with state_lock:
                state["calendar"].update(
                    configured=True,
                    valid=True,
                    updated_epoch=now_epoch,
                    last_attempt_ok=True,
                    event=event,
                )
            persist_state()
        except Exception as exc:
            sleep_for = RETRY_SECONDS
            with state_lock:
                state["calendar"]["last_attempt_ok"] = False
            app.logger.warning("Calendar refresh failed: %s", type(exc).__name__)

        time.sleep(max(5, sleep_for))


def is_fresh(section, now_epoch):
    if not section.get("valid", False):
        return False
    age = max(0, now_epoch - int(section.get("updated_epoch", 0)))
    return bool(section.get("last_attempt_ok", False)) and age <= STALE_AFTER_SECONDS


def day_label(start: datetime, today: date) -> str:
    days = (start.date() - today).days
    if days == 0:
        return "TODAY"
    if days == 1:
        return "TMR"
    if 2 <= days <= 6:
        return start.strftime("%a").upper()
    return start.strftime("%b %d").upper().replace(" 0", " ")


def event_for_response(calendar, now):
    event = calendar.get("event") or {}
    if not calendar.get("valid", False) or not event.get("valid", False):
        return {"valid": False, "day": "", "time": "--:--"}

    try:
        start = datetime.fromtimestamp(int(event["start_epoch"]), tz)
    except Exception:
        return {"valid": False, "day": "", "time": "--:--"}

    all_day = bool(event.get("all_day", False))
    if all_day:
        if start.date() < now.date():
            return {"valid": False, "day": "", "time": "--:--"}
        time_text = "ALL DAY"
    else:
        if start < now:
            return {"valid": False, "day": "", "time": "--:--"}
        hour = start.hour % 12 or 12
        time_text = f"{hour}:{start.minute:02d} {'PM' if start.hour >= 12 else 'AM'}"

    return {
        "valid": True,
        "day": day_label(start, now.date()),
        "time": time_text,
    }


def status_payload():
    now = datetime.now(tz)
    now_epoch = int(time.time())
    with state_lock:
        snapshot = copy.deepcopy(state)

    weather = snapshot["weather"]
    sun = snapshot["sun"]
    calendar = snapshot["calendar"]

    weather_age = max(0, now_epoch - int(weather.get("updated_epoch", 0))) if weather.get("valid") else 0
    calendar_age = max(0, now_epoch - int(calendar.get("updated_epoch", 0))) if calendar.get("valid") else 0

    calendar_fresh = True if not CALENDAR_URLS else is_fresh(calendar, now_epoch)

    return {
        "v": API_VERSION,
        "generated_epoch": now_epoch,
        "utc_offset_seconds": int(now.utcoffset().total_seconds()),
        "weather": {
            "valid": bool(weather.get("valid", False)),
            "fresh": is_fresh(weather, now_epoch),
            "age_s": weather_age,
            "temp_c10": int(weather.get("temp_c10", 0)),
            "condition": weather.get("condition", "unknown"),
        },
        "sun": {
            "valid": bool(sun.get("valid", False)),
            "fresh": is_fresh(sun, now_epoch),
            "sunrise_min": int(sun.get("sunrise_min", 8 * 60)),
            "sunset_min": int(sun.get("sunset_min", 18 * 60)),
        },
        "calendar": {
            "configured": bool(CALENDAR_URLS),
            "valid": bool(calendar.get("valid", False)),
            "fresh": calendar_fresh,
            "age_s": calendar_age,
        },
        "next_event": event_for_response(calendar, now),
    }


def start_workers():
    global workers_started
    if DISABLE_WORKERS or workers_started:
        return
    workers_started = True

    threading.Thread(target=weather_worker, name="weather-refresh", daemon=True).start()
    if CALENDAR_URLS:
        threading.Thread(target=calendar_worker, name="calendar-refresh", daemon=True).start()


start_workers()


@app.get("/api/status")
def status():
    # No network I/O occurs on this request path. Upstreams are refreshed by
    # background workers, so the ESP32 always receives an immediate memory read.
    return jsonify(status_payload())


@app.get("/health")
def health():
    payload = status_payload()
    return {
        "ok": True,
        "api_version": API_VERSION,
        "weather_valid": payload["weather"]["valid"],
        "weather_fresh": payload["weather"]["fresh"],
        "calendar_valid": payload["calendar"]["valid"],
        "calendar_fresh": payload["calendar"]["fresh"],
    }


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)
