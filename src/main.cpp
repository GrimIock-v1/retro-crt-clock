#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <time.h>

#include "CompositeGraphics.h"
#include "retro_composite_output.h"
#include "retro_scene.h"
#include "esp_backend.h"
#include "web_ui.h"

static constexpr int WIFI_RESET_PIN = 4;
static constexpr uint32_t WIFI_RESET_HOLD_MS = 3000;
static constexpr uint32_t SHORT_PRESS_MAX_MS = 700;
static constexpr uint32_t BUTTON_DEBOUNCE_MS = 35;
static constexpr uint32_t WIFI_GRACE_MS = 4000;
static constexpr uint32_t NTP_WAIT_MS = 2500;

static constexpr uint32_t NORMAL_RENDER_INTERVAL_MS = 500;
static constexpr uint32_t BUTTON_RENDER_INTERVAL_MS = 100;

static constexpr uint32_t FETCH_FRESH_INTERVAL_MS = 5UL * 60UL * 1000UL;
static constexpr uint32_t FETCH_STALE_INTERVAL_MS = 60UL * 1000UL;
static constexpr uint32_t BRIDGE_READ_TIMEOUT_MS = 5000;
static constexpr uint32_t BRIDGE_RETRY_DELAY_MS = 250;
static constexpr uint32_t VIDEO_ISOLATION_TEST_MS = 30000;
static constexpr uint32_t VIDEO_ISOLATION_STEP_MS = 500;
static constexpr uint32_t RENDER_DIAG_CACHE_INTERVAL_MS = 5000;
static constexpr uint32_t HEALTH_LOG_INTERVAL_MS = 5UL * 60UL * 1000UL;

static constexpr int BRIDGE_API_VERSION = 3;
static constexpr int SETTINGS_VERSION = 1;
static constexpr const char* FIRMWARE_VERSION = "3.9.6";
static constexpr const char* DEFAULT_BRIDGE_URL =
    "http://crt-clock-bridge.ultramagnus.ca/status";

CompositeGraphics graphics(CompositeColorOutput::XRES, CompositeColorOutput::YRES);
CompositeColorOutput composite(CompositeColorOutput::NTSC);
EspCompositeBackend backend(graphics);

WiFiManager wm;
Preferences prefs;
WebServer configServer(80);
WiFiManagerParameter* bridgeParam = nullptr;
WiFiManagerParameter* timezoneParam = nullptr;

struct ClockSettings {
    uint8_t version = SETTINGS_VERSION;
    char bridgeUrl[160] = "http://crt-clock-bridge.ultramagnus.ca/status";
    char timezoneRule[96] = "PST8PDT,M3.2.0,M11.1.0";
    uint8_t driftPixels = 1;
    uint16_t driftMinutes = 30;
    uint8_t nightBrightness = 75;
    uint8_t clockBrightness = 100;
    uint8_t backgroundBrightness = 100;
    bool use24Hour = false;
    bool fahrenheit = false;
    bool weatherText = true;
    bool dayBar = true;
    uint8_t citySpeed = 2;
};

ClockSettings settings;
ClockSettings savedSettings;
bool previewActive = false;
uint32_t previewExpiresAt = 0;
bool webServerStarted = false;
bool forceBridgeRefresh = false;
bool restartRequested = false;
uint32_t restartRequestedAt = 0;
enum VideoIsolationMode : uint8_t {
    VIDEO_TEST_NONE = 0,
    VIDEO_TEST_FREEZE = 1,
    VIDEO_TEST_RENDER_ONLY = 2,
    VIDEO_TEST_SWAP_ONLY = 3
};

VideoIsolationMode videoTestMode = VIDEO_TEST_NONE;
VideoIsolationMode requestedVideoTestMode = VIDEO_TEST_NONE;
uint32_t videoTestUntil = 0;
uint32_t videoTestLastStepAt = 0;
uint32_t videoTestSteps = 0;
retro::SceneData videoTestScene;

uint32_t lastHealthLogAt = 0;
uint32_t videoBlankWaitTimeouts = 0;
uint32_t lastRenderDiagCacheAt = 0;
bool cachedDiagWifiConnected = false;
int cachedDiagRssi = -127;
uint32_t cachedDiagFreeHeap = 0;
uint32_t cachedDiagLargestHeap = 0;

bool portalActive = false;
bool videoStarted = false;
bool videoHardwareInitialized = false;
bool ntpConfigured = false;
uint32_t wifiConnectedAt = 0;
uint32_t lastDrawAt = 0;
uint32_t lastFetchAttemptAt = 0;
uint32_t nextFetchDelay = FETCH_STALE_INTERVAL_MS;


char bridgeUrl[160] = "http://crt-clock-bridge.ultramagnus.ca/status";
char timezoneRule[96] = "PST8PDT,M3.2.0,M11.1.0";

retro::SceneData scene;
retro::SceneCache sceneCache;

// Bridge time is retained only as a fallback if NTP has not yet synchronized.
uint32_t bridgeClockBaseEpoch = 0;
uint32_t bridgeClockBaseMillis = 0;
int bridgeUtcOffsetSeconds = 0;
bool bridgeClockValid = false;

bool buttonRawDown = false;
bool buttonStableDown = false;
uint32_t buttonRawChangedAt = 0;
uint32_t buttonPressedAt = 0;

enum BridgeFetchResult : uint8_t {
    BRIDGE_FETCH_FAILED = 0,
    BRIDGE_FETCH_STALE = 1,
    BRIDGE_FETCH_FRESH = 2
};

static void renderFrame();
static void startConfigWebServer();
static void serviceVideoIsolationTest(uint32_t now);


static void logHeap(const char* tag) {
    Serial.printf("[%s] free=%u largest8=%u\n",
                  tag,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}


static int clampValue(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void clampSettings(ClockSettings& cfg) {
    cfg.version = SETTINGS_VERSION;
    cfg.driftPixels = (uint8_t)clampValue(cfg.driftPixels, 0, 5);
    cfg.driftMinutes = (uint16_t)clampValue(cfg.driftMinutes, 1, 120);
    cfg.nightBrightness = (uint8_t)clampValue(cfg.nightBrightness, 10, 100);
    cfg.clockBrightness = (uint8_t)clampValue(cfg.clockBrightness, 25, 100);
    cfg.backgroundBrightness = (uint8_t)clampValue(cfg.backgroundBrightness, 0, 100);
    cfg.citySpeed = (uint8_t)clampValue(cfg.citySpeed, 0, 3);
    if (!cfg.bridgeUrl[0]) strlcpy(cfg.bridgeUrl, DEFAULT_BRIDGE_URL, sizeof(cfg.bridgeUrl));
    if (!cfg.timezoneRule[0]) strlcpy(cfg.timezoneRule, "PST8PDT,M3.2.0,M11.1.0", sizeof(cfg.timezoneRule));
}

static void copySettingsToRuntime() {
    strlcpy(bridgeUrl, settings.bridgeUrl, sizeof(bridgeUrl));
    strlcpy(timezoneRule, settings.timezoneRule, sizeof(timezoneRule));
}

static void loadSettings() {
    settings = ClockSettings();
    prefs.begin("retroclock", true);
    String storedBridge = prefs.getString("bridge_url", settings.bridgeUrl);
    String storedTz = prefs.getString("tz_rule", settings.timezoneRule);
    settings.version = prefs.getUChar("cfg_ver", SETTINGS_VERSION);
    settings.driftPixels = prefs.getUChar("drift_px", settings.driftPixels);
    settings.driftMinutes = prefs.getUShort("drift_min", settings.driftMinutes);
    settings.nightBrightness = prefs.getUChar("night_pct", settings.nightBrightness);
    settings.clockBrightness = prefs.getUChar("clock_pct", settings.clockBrightness);
    settings.backgroundBrightness = prefs.getUChar("bg_pct", settings.backgroundBrightness);
    settings.use24Hour = prefs.getBool("use_24h", settings.use24Hour);
    settings.fahrenheit = prefs.getBool("fahrenheit", settings.fahrenheit);
    settings.weatherText = prefs.getBool("wx_text", settings.weatherText);
    settings.dayBar = prefs.getBool("day_bar", settings.dayBar);
    settings.citySpeed = prefs.getUChar("city_speed", settings.citySpeed);
    prefs.end();

    if (storedBridge == "http://192.168.1.50:8080/api/status" ||
        storedBridge == "http://retroclock.local:8080/api/status") {
        storedBridge = DEFAULT_BRIDGE_URL;
    }
    storedBridge.toCharArray(settings.bridgeUrl, sizeof(settings.bridgeUrl));
    storedTz.toCharArray(settings.timezoneRule, sizeof(settings.timezoneRule));
    clampSettings(settings);
    savedSettings = settings;
    copySettingsToRuntime();
}

static void persistSettings(const ClockSettings& cfg) {
    prefs.begin("retroclock", false);
    prefs.putUChar("cfg_ver", SETTINGS_VERSION);
    prefs.putString("bridge_url", cfg.bridgeUrl);
    prefs.putString("tz_rule", cfg.timezoneRule);
    prefs.putUChar("drift_px", cfg.driftPixels);
    prefs.putUShort("drift_min", cfg.driftMinutes);
    prefs.putUChar("night_pct", cfg.nightBrightness);
    prefs.putUChar("clock_pct", cfg.clockBrightness);
    prefs.putUChar("bg_pct", cfg.backgroundBrightness);
    prefs.putBool("use_24h", cfg.use24Hour);
    prefs.putBool("fahrenheit", cfg.fahrenheit);
    prefs.putBool("wx_text", cfg.weatherText);
    prefs.putBool("day_bar", cfg.dayBar);
    prefs.putUChar("city_speed", cfg.citySpeed);
    prefs.end();
}

static void applySettingsToScene() {
    scene.driftPixels = settings.driftPixels;
    scene.driftIntervalMinutes = settings.driftMinutes;
    scene.nightBrightnessPct = settings.nightBrightness;
    scene.clockBrightnessPct = settings.clockBrightness;
    scene.backgroundBrightnessPct = settings.backgroundBrightness;
    scene.use24Hour = settings.use24Hour;
    scene.fahrenheit = settings.fahrenheit;
    scene.showWeatherText = settings.weatherText;
    scene.showDayBar = settings.dayBar;
    scene.citySpeed = settings.citySpeed;
}

static void applyTimezoneLive() {
    setenv("TZ", timezoneRule, 1);
    tzset();
    Serial.printf("Applied timezone rule: %s\n", timezoneRule);
}

static int serverIntArg(const char* name, int current, int lo, int hi) {
    if (!configServer.hasArg(name)) return current;
    return clampValue(configServer.arg(name).toInt(), lo, hi);
}

static bool serverBoolArg(const char* name, bool current) {
    if (!configServer.hasArg(name)) return current;
    const String v = configServer.arg(name);
    return v == "1" || v == "true" || v == "on" || v == "yes";
}

static void parseDisplaySettings(ClockSettings& cfg) {
    cfg.driftPixels = (uint8_t)serverIntArg("drift_pixels", cfg.driftPixels, 0, 5);
    cfg.driftMinutes = (uint16_t)serverIntArg("drift_minutes", cfg.driftMinutes, 1, 120);
    cfg.nightBrightness = (uint8_t)serverIntArg("night_brightness", cfg.nightBrightness, 10, 100);
    cfg.clockBrightness = (uint8_t)serverIntArg("clock_brightness", cfg.clockBrightness, 25, 100);
    cfg.backgroundBrightness = (uint8_t)serverIntArg("background_brightness", cfg.backgroundBrightness, 0, 100);
    cfg.citySpeed = (uint8_t)serverIntArg("city_speed", cfg.citySpeed, 0, 3);
    if (configServer.hasArg("time_format")) cfg.use24Hour = configServer.arg("time_format") == "24";
    if (configServer.hasArg("temperature_unit")) cfg.fahrenheit = configServer.arg("temperature_unit") == "F";
    cfg.weatherText = serverBoolArg("weather_text", cfg.weatherText);
    cfg.dayBar = serverBoolArg("day_bar", cfg.dayBar);
    clampSettings(cfg);
}

static bool parseNetworkSettings(ClockSettings& cfg, String& error) {
    if (configServer.hasArg("bridge_url")) {
        String url = configServer.arg("bridge_url");
        url.trim();
        if (!url.startsWith("http://")) {
            error = "Bridge URL must start with http:// on this ESP32 build.";
            return false;
        }
        if (url.length() >= sizeof(cfg.bridgeUrl)) {
            error = "Bridge URL is too long.";
            return false;
        }
        url.toCharArray(cfg.bridgeUrl, sizeof(cfg.bridgeUrl));
    }
    if (configServer.hasArg("timezone_rule")) {
        String tz = configServer.arg("timezone_rule");
        tz.trim();
        if (tz.length() == 0 || tz.length() >= sizeof(cfg.timezoneRule)) {
            error = "Timezone rule is empty or too long.";
            return false;
        }
        if (tz.indexOf('\n') >= 0 || tz.indexOf('\r') >= 0) {
            error = "Timezone rule contains invalid characters.";
            return false;
        }
        tz.toCharArray(cfg.timezoneRule, sizeof(cfg.timezoneRule));
    }
    return true;
}

static void sendJsonDocument(JsonDocument& doc, int code = 200) {
    char out[1100];
    size_t n = serializeJson(doc, out, sizeof(out) - 1);
    if (n >= sizeof(out)) n = sizeof(out) - 1;
    out[n] = '\0';
    configServer.sendHeader("Cache-Control", "no-store");
    configServer.send(code, "application/json", out);
}

static void sendApiMessage(bool ok, const char* message, int code = 200) {
    StaticJsonDocument<256> doc;
    doc["ok"] = ok;
    doc["message"] = message;
    sendJsonDocument(doc, code);
}

static const char* videoTestModeName(VideoIsolationMode mode) {
    switch (mode) {
        case VIDEO_TEST_FREEZE: return "FREEZE";
        case VIDEO_TEST_RENDER_ONLY: return "RENDER_ONLY";
        case VIDEO_TEST_SWAP_ONLY: return "SWAP_ONLY";
        default: return "NONE";
    }
}

static bool videoTestActive() {
    return videoTestMode != VIDEO_TEST_NONE &&
           videoTestUntil != 0 &&
           (int32_t)(millis() - videoTestUntil) < 0;
}

static bool videoFreezeActive() {
    return videoTestActive() && videoTestMode == VIDEO_TEST_FREEZE;
}

static void refreshRenderDiagnosticCache(bool force = false) {
    const uint32_t now = millis();
    if (!force && now - lastRenderDiagCacheAt < RENDER_DIAG_CACHE_INTERVAL_MS) return;
    lastRenderDiagCacheAt = now;
    cachedDiagWifiConnected = WiFi.status() == WL_CONNECTED;
    cachedDiagRssi = cachedDiagWifiConnected ? WiFi.RSSI() : -127;
    cachedDiagFreeHeap = ESP.getFreeHeap();
    cachedDiagLargestHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static void populateSceneDiagnosticsFromCache() {
    scene.wifiConnected = cachedDiagWifiConnected;
    scene.diagFreeHeap = cachedDiagFreeHeap;
    scene.diagLargestHeap = cachedDiagLargestHeap;
    scene.diagHttpTests = 0;
    scene.diagHttpTarget = 0;
    scene.diagHttpOk = 0;
    scene.diagHttpFail = 0;
    scene.diagHttpLastMs = 0;
    scene.diagHttpMaxMs = 0;
    scene.diagRssi = cachedDiagRssi;
    scene.diagWifiStressActive = false;
}

static void waitForVideoBlanking() {
    if (!videoHardwareInitialized) return;

    // The video library renders NTSC line-by-line from the currently selected
    // framebuffer. Repointing that framebuffer during an active raster can
    // produce a split frame / visible jump. Wait until the active 240-line
    // region has finished, then swap during the ~1.4 ms NTSC blanking window.
    const uint32_t started = micros();
    while (RawCompositeVideoBlitter::_line_counter < RawCompositeVideoBlitter::_active_lines) {
        if ((uint32_t)(micros() - started) > 18000U) {
            ++videoBlankWaitTimeouts;
            return;
        }
        delayMicroseconds(20);
    }
}

static void handleApiSettings() {
    StaticJsonDocument<1024> doc;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["bridge_url"] = settings.bridgeUrl;
    doc["timezone_rule"] = settings.timezoneRule;
    doc["drift_pixels"] = settings.driftPixels;
    doc["drift_minutes"] = settings.driftMinutes;
    doc["night_brightness"] = settings.nightBrightness;
    doc["clock_brightness"] = settings.clockBrightness;
    doc["background_brightness"] = settings.backgroundBrightness;
    doc["city_speed"] = settings.citySpeed;
    doc["use_24h"] = settings.use24Hour;
    doc["fahrenheit"] = settings.fahrenheit;
    doc["weather_text"] = settings.weatherText;
    doc["day_bar"] = settings.dayBar;
    doc["preview_active"] = previewActive;
    sendJsonDocument(doc);
}

static void handleApiStatus() {
    StaticJsonDocument<896> doc;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["wifi"] = WiFi.status() == WL_CONNECTED;
    doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127;
    doc["bridge"] = scene.bridgeReachable && scene.bridgeCompatible;
    doc["time_valid"] = scene.timeValid;
    doc["time_source"] = scene.timeValid ? (scene.timeFromNtp ? "NTP" : "Bridge") : "None";
    doc["free_heap"] = ESP.getFreeHeap();
    doc["largest_block"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    doc["video_freeze"] = videoFreezeActive();
    doc["video_test"] = videoTestModeName(videoTestMode);
    doc["video_test_active"] = videoTestActive();
    doc["video_test_remaining_sec"] = videoTestActive()
        ? (uint32_t)((videoTestUntil - millis() + 999U) / 1000U)
        : 0U;
    doc["video_test_steps"] = videoTestSteps;
    doc["blank_wait_timeouts"] = videoBlankWaitTimeouts;
    doc["uptime_sec"] = millis() / 1000U;
    doc["weather"] = scene.weatherValid ? retro::conditionLabel(scene.condition) : "No data";
    char eventText[32] = "None";
    if (scene.calendarValid && scene.nextEventValid) {
        if (scene.nextEventDay[0]) snprintf(eventText, sizeof(eventText), "%s %s", scene.nextEventDay, scene.nextEventTime);
        else snprintf(eventText, sizeof(eventText), "%s", scene.nextEventTime);
    }
    doc["next_event"] = eventText;
    sendJsonDocument(doc);
}

static void handleApiPreview() {
    ClockSettings next = settings;
    parseDisplaySettings(next);
    settings = next;
    previewActive = true;
    previewExpiresAt = millis() + 60000U;
    renderFrame();
    sendApiMessage(true, "Preview applied. It will revert in 60 seconds unless saved.");
}

static void handleApiRevert() {
    settings = savedSettings;
    previewActive = false;
    previewExpiresAt = 0;
    copySettingsToRuntime();
    applySettingsToScene();
    renderFrame();
    sendApiMessage(true, "Preview reverted to saved settings.");
}

static void handleApiDefaults() {
    settings = ClockSettings();
    clampSettings(settings);
    savedSettings = settings;
    persistSettings(settings);
    copySettingsToRuntime();
    applySettingsToScene();
    previewActive = false;
    previewExpiresAt = 0;
    applyTimezoneLive();
    forceBridgeRefresh = true;
    renderFrame();
    sendApiMessage(true, "Factory clock settings restored. WiFi credentials were kept.");
}

static void handleApiSave() {
    ClockSettings next = settings;
    parseDisplaySettings(next);
    String error;
    if (!parseNetworkSettings(next, error)) {
        sendApiMessage(false, error.c_str(), 400);
        return;
    }
    const bool bridgeChanged = strcmp(next.bridgeUrl, savedSettings.bridgeUrl) != 0;
    const bool timezoneChanged = strcmp(next.timezoneRule, savedSettings.timezoneRule) != 0;
    const bool timeFormatChanged = next.use24Hour != savedSettings.use24Hour;
    settings = next;
    clampSettings(settings);
    savedSettings = settings;
    persistSettings(settings);
    copySettingsToRuntime();
    applySettingsToScene();
    previewActive = false;
    previewExpiresAt = 0;
    if (timezoneChanged) applyTimezoneLive();
    if (bridgeChanged || timezoneChanged || timeFormatChanged) forceBridgeRefresh = true;
    renderFrame();
    sendApiMessage(true, "Settings saved and applied.");
}

static void handleApiRefresh() {
    forceBridgeRefresh = true;
    sendApiMessage(true, "Bridge refresh queued.");
}

static void handleApiDiagnostics() {
    scene.diagnosticsMode = !scene.diagnosticsMode;
    renderFrame();
    sendApiMessage(true, scene.diagnosticsMode ? "Diagnostics enabled." : "Diagnostics disabled.");
}

static void handleApiRestart() {
    restartRequested = true;
    restartRequestedAt = millis() + 500U;
    sendApiMessage(true, "Restarting clock.");
}

static void queueVideoIsolationTest(VideoIsolationMode mode, const char* message) {
    requestedVideoTestMode = mode;
    sendApiMessage(true, message);
}

static void handleApiVideoTest() {
    String mode = configServer.arg("mode");
    if (mode == "freeze") {
        queueVideoIsolationTest(VIDEO_TEST_FREEZE,
                                "Freeze test queued for 30 seconds. No render or framebuffer swap.");
    } else if (mode == "render") {
        queueVideoIsolationTest(VIDEO_TEST_RENDER_ONLY,
                                "Render-only test queued for 30 seconds. Backbuffer will redraw; displayed framebuffer stays fixed.");
    } else if (mode == "swap") {
        queueVideoIsolationTest(VIDEO_TEST_SWAP_ONLY,
                                "Swap-only test queued for 30 seconds. Identical prepared buffers will swap every 500 ms.");
    } else {
        sendApiMessage(false, "Unknown video test mode.", 400);
    }
}

static void handleApiVideoFreeze() {
    // Backward-compatible endpoint retained for V3.9.1 browser bookmarks.
    requestedVideoTestMode = VIDEO_TEST_FREEZE;
    sendApiMessage(true, "Freeze test queued for 30 seconds. No render or framebuffer swap.");
}

static void startConfigWebServer() {
    if (webServerStarted || WiFi.status() != WL_CONNECTED) return;
    configServer.on("/", HTTP_GET, []() {
        configServer.sendHeader("Cache-Control", "no-store");
        configServer.send_P(200, "text/html", WEB_UI_HTML);
    });
    configServer.on("/api/settings", HTTP_GET, handleApiSettings);
    configServer.on("/api/status", HTTP_GET, handleApiStatus);
    configServer.on("/api/preview", HTTP_POST, handleApiPreview);
    configServer.on("/api/revert", HTTP_POST, handleApiRevert);
    configServer.on("/api/defaults", HTTP_POST, handleApiDefaults);
    configServer.on("/api/save", HTTP_POST, handleApiSave);
    configServer.on("/api/refresh", HTTP_POST, handleApiRefresh);
    configServer.on("/api/diagnostics", HTTP_POST, handleApiDiagnostics);
    configServer.on("/api/restart", HTTP_POST, handleApiRestart);
    configServer.on("/api/video-freeze", HTTP_POST, handleApiVideoFreeze);
    configServer.on("/api/video-test", HTTP_POST, handleApiVideoTest);
    configServer.onNotFound([]() { configServer.send(404, "text/plain", "Not found"); });
    configServer.begin();
    webServerStarted = true;
    Serial.printf("Clock web UI: http://%s/\n", WiFi.localIP().toString().c_str());
    logHeap("web-server");
}

static retro::Condition parseCondition(const char* s) {
    if (!s || !s[0]) return retro::CONDITION_UNKNOWN;
    char lower[40];
    size_t i = 0;
    for (; s[i] && i < sizeof(lower) - 1; ++i) {
        char c = s[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    }
    lower[i] = '\0';
    if (strstr(lower, "thunder") || strstr(lower, "lightning")) return retro::CONDITION_THUNDER;
    if (strstr(lower, "storm") || strstr(lower, "squall")) return retro::CONDITION_STORM;
    if (strstr(lower, "snow") || strstr(lower, "sleet") || strstr(lower, "ice") || strstr(lower, "blizzard")) return retro::CONDITION_SNOW;
    if (strstr(lower, "drizzle")) return retro::CONDITION_DRIZZLE;
    if (strstr(lower, "rain") || strstr(lower, "shower")) return retro::CONDITION_RAIN;
    if (strstr(lower, "fog") || strstr(lower, "mist") || strstr(lower, "haze") || strstr(lower, "smoke")) return retro::CONDITION_FOG;
    if (strstr(lower, "wind")) return retro::CONDITION_WIND;
    if (strstr(lower, "partly") || strstr(lower, "few clouds")) return retro::CONDITION_PARTLY;
    if (strstr(lower, "night") || strstr(lower, "moon")) return retro::CONDITION_CLEAR_NIGHT;
    if (strstr(lower, "cloud") || strstr(lower, "overcast")) return retro::CONDITION_CLOUD;
    if (strstr(lower, "sun") || strstr(lower, "clear")) return retro::CONDITION_SUN;
    return retro::CONDITION_UNKNOWN;
}

static bool systemClockValid() {
    return time(nullptr) > 1700000000;
}

static void configureNtp() {
    if (ntpConfigured) return;

    // Use the timezone-aware SNTP setup so the configured POSIX rule is
    // preserved and daylight-saving transitions are handled automatically.
    configTzTime(timezoneRule,
                 "pool.ntp.org",
                 "time.cloudflare.com",
                 "time.google.com");

    ntpConfigured = true;
    Serial.printf("NTP configured with TZ rule: %s\n", timezoneRule);
}

static bool waitBrieflyForNtp() {
    const uint32_t started = millis();
    while (millis() - started < NTP_WAIT_MS) {
        if (systemClockValid()) {
            time_t now = time(nullptr);
            struct tm localNow {};
            localtime_r(&now, &localNow);

            Serial.println("NTP synchronized before video startup.");
            Serial.printf("Resolved local time: %04d-%02d-%02d %02d:%02d:%02d %s\n",
                          localNow.tm_year + 1900,
                          localNow.tm_mon + 1,
                          localNow.tm_mday,
                          localNow.tm_hour,
                          localNow.tm_min,
                          localNow.tm_sec,
                          localNow.tm_isdst > 0 ? "DST" : "STD");
            return true;
        }
        delay(50);
    }

    Serial.println("NTP not ready yet; bridge fallback will be used until it syncs.");
    return false;
}

static void savePortalParameters() {
    if (bridgeParam) {
        const char* value = bridgeParam->getValue();
        if (value && value[0]) strlcpy(settings.bridgeUrl, value, sizeof(settings.bridgeUrl));
    }
    if (timezoneParam) {
        const char* value = timezoneParam->getValue();
        if (value && value[0]) strlcpy(settings.timezoneRule, value, sizeof(settings.timezoneRule));
    }
    clampSettings(settings);
    savedSettings = settings;
    persistSettings(settings);
    copySettingsToRuntime();
    Serial.printf("Saved bridge URL: %s\n", bridgeUrl);
    Serial.printf("Saved timezone rule: %s\n", timezoneRule);
}

static bool sameDate(const struct tm& a, const struct tm& b) {
    return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday == b.tm_mday;
}

static void formatEventEpoch(uint32_t eventEpoch) {
    if (eventEpoch < 1600000000U) {
        scene.nextEventValid = false;
        scene.nextEventDay[0] = '\0';
        strlcpy(scene.nextEventTime, "--:--", sizeof(scene.nextEventTime));
        return;
    }

    time_t rawEvent = (time_t)eventEpoch;
    struct tm eventTm {};
    localtime_r(&rawEvent, &eventTm);

    if (settings.use24Hour) {
        snprintf(scene.nextEventTime, sizeof(scene.nextEventTime),
                 "%02d:%02d", eventTm.tm_hour, eventTm.tm_min);
    } else {
        int eventHour12 = eventTm.tm_hour % 12;
        if (eventHour12 == 0) eventHour12 = 12;
        snprintf(scene.nextEventTime, sizeof(scene.nextEventTime),
                 "%d:%02d %s",
                 eventHour12,
                 eventTm.tm_min,
                 eventTm.tm_hour >= 12 ? "PM" : "AM");
    }

    if (systemClockValid()) {
        time_t rawNow = time(nullptr);
        struct tm nowTm {};
        localtime_r(&rawNow, &nowTm);

        if (sameDate(nowTm, eventTm)) {
            strlcpy(scene.nextEventDay, "TODAY", sizeof(scene.nextEventDay));
        } else {
            struct tm tomorrowTm = nowTm;
            tomorrowTm.tm_mday += 1;
            tomorrowTm.tm_hour = 12;
            tomorrowTm.tm_min = 0;
            tomorrowTm.tm_sec = 0;
            mktime(&tomorrowTm); // normalize month/year/DST boundaries

            if (sameDate(tomorrowTm, eventTm)) {
                strlcpy(scene.nextEventDay, "TMR", sizeof(scene.nextEventDay));
            } else {
                strlcpy(scene.nextEventDay,
                        retro::weekdayName(eventTm.tm_wday),
                        sizeof(scene.nextEventDay));
            }
        }
    } else {
        // NTP may still be synchronizing. The event's local weekday is still
        // more useful than presenting an incorrect TODAY/TMR label.
        strlcpy(scene.nextEventDay,
                retro::weekdayName(eventTm.tm_wday),
                sizeof(scene.nextEventDay));
    }

    scene.nextEventValid = true;
}

static BridgeFetchResult parseSimpleBridge(JsonDocument& doc) {
    // Compatibility with the deployed Ultramagnus bridge:
    // {"next_event_epoch":...,"weather":{"condition":"CLOUDY","temp_c":16}}
    scene.bridgeCompatible = true;

    JsonObject weather = doc["weather"].as<JsonObject>();
    scene.weatherValid = !weather.isNull() && weather.containsKey("temp_c");
    scene.weatherFresh = scene.weatherValid;
    if (scene.weatherValid) {
        const float tempC = weather["temp_c"].as<float>();
        scene.tempC10 = (int)lroundf(tempC * 10.0f);
        scene.condition = parseCondition(weather["condition"] | "unknown");
    }

    // This bridge does not currently publish sunrise/sunset. Keep the V3
    // renderer's conservative fixed-time ambient fallback.
    scene.sunValid = false;

    scene.calendarValid = doc.containsKey("next_event_epoch");
    scene.calendarFresh = scene.calendarValid;
    const uint32_t eventEpoch = doc["next_event_epoch"] | 0U;
    if (scene.calendarValid && eventEpoch > 0U) {
        formatEventEpoch(eventEpoch);
    } else {
        scene.nextEventValid = false;
        scene.nextEventDay[0] = '\0';
        strlcpy(scene.nextEventTime, "--:--", sizeof(scene.nextEventTime));
    }

    // No generated_epoch is present in this compact bridge, so NTP remains
    // the authoritative clock source. A failed NTP sync correctly renders
    // SYNC TIME rather than fabricating a timestamp.
    return BRIDGE_FETCH_FRESH;
}

static BridgeFetchResult fetchBridge() {
    if (WiFi.status() != WL_CONNECTED) {
        scene.bridgeReachable = false;
        scene.weatherFresh = false;
        scene.calendarFresh = false;
        return BRIDGE_FETCH_FAILED;
    }

    if (strncmp(bridgeUrl, "http://", 7) != 0) {
        Serial.println("Bridge URL must start with http:// on this build.");
        scene.bridgeReachable = false;
        scene.weatherFresh = false;
        scene.calendarFresh = false;
        return BRIDGE_FETCH_FAILED;
    }

    lastFetchAttemptAt = millis();
    logHeap("before HTTP");

    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(1800);
    http.setTimeout(BRIDGE_READ_TIMEOUT_MS);

    auto beginBridgeHttp = [&]() -> bool {
        return http.begin(client, bridgeUrl);
    };

    if (!beginBridgeHttp()) {
        Serial.println("HTTP begin failed");
        scene.bridgeReachable = false;
        scene.weatherFresh = false;
        scene.calendarFresh = false;
        return BRIDGE_FETCH_FAILED;
    }

    int code = http.GET();
    if (code == HTTPC_ERROR_READ_TIMEOUT) {
        Serial.printf("Bridge GET timed out: %d %s; retrying once in %u ms\n",
                      code, HTTPClient::errorToString(code).c_str(),
                      (unsigned)BRIDGE_RETRY_DELAY_MS);
        http.end();
        delay(BRIDGE_RETRY_DELAY_MS);
        client.stop();
        if (beginBridgeHttp()) code = http.GET();
        else code = HTTPC_ERROR_CONNECTION_REFUSED;
    }

    if (code != HTTP_CODE_OK) {
        Serial.printf("Bridge GET failed: %d %s\n",
                      code, HTTPClient::errorToString(code).c_str());
        if (code == 301 || code == 302 || code == 307 || code == 308) {
            Serial.println(
                "Bridge returned an HTTP redirect. If it redirects to HTTPS, "
                "disable Always Use HTTPS for this hostname or provide a plain-HTTP endpoint; "
                "the plain ESP32/WROOM build intentionally does not perform TLS."
            );
        }
        http.end();
        scene.bridgeReachable = false;
        scene.weatherFresh = false;
        scene.calendarFresh = false;
        return BRIDGE_FETCH_FAILED;
    }

    StaticJsonDocument<896> doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
        Serial.printf("JSON error: %s\n", err.c_str());
        scene.bridgeReachable = false;
        scene.weatherFresh = false;
        scene.calendarFresh = false;
        return BRIDGE_FETCH_FAILED;
    }

    scene.bridgeReachable = true;

    // Native V3 bridge contract.
    if (doc.containsKey("v")) {
        const int apiVersion = doc["v"] | 0;
        if (apiVersion != BRIDGE_API_VERSION) {
            Serial.printf("Bridge API version mismatch: got %d expected %d\n",
                          apiVersion, BRIDGE_API_VERSION);
            scene.bridgeCompatible = false;
            scene.weatherFresh = false;
            scene.calendarFresh = false;
            return BRIDGE_FETCH_FAILED;
        }
        scene.bridgeCompatible = true;

        uint32_t generated = doc["generated_epoch"] | 0U;
        int offset = doc["utc_offset_seconds"] | 0;
        if (generated > 1600000000U) {
            bridgeClockBaseEpoch = generated;
            bridgeClockBaseMillis = millis();
            bridgeUtcOffsetSeconds = offset;
            bridgeClockValid = true;
        }

        JsonObject weather = doc["weather"];
        scene.weatherValid = weather["valid"] | false;
        scene.weatherFresh = weather["fresh"] | false;
        if (scene.weatherValid) {
            scene.tempC10 = weather["temp_c10"] | scene.tempC10;
            scene.condition = parseCondition(weather["condition"] | "unknown");
        }

        JsonObject sun = doc["sun"];
        scene.sunValid = sun["valid"] | false;
        if (scene.sunValid) {
            scene.sunriseMinute = (uint16_t)(sun["sunrise_min"] | scene.sunriseMinute);
            scene.sunsetMinute = (uint16_t)(sun["sunset_min"] | scene.sunsetMinute);
        }

        JsonObject calendar = doc["calendar"];
        scene.calendarValid = calendar["valid"] | false;
        scene.calendarFresh = calendar["fresh"] | false;

        JsonObject event = doc["next_event"];
        scene.nextEventValid = scene.calendarValid && (event["valid"] | false);
        if (scene.nextEventValid) {
            strlcpy(scene.nextEventDay, event["day"] | "", sizeof(scene.nextEventDay));
            strlcpy(scene.nextEventTime, event["time"] | "--:--", sizeof(scene.nextEventTime));
        } else {
            scene.nextEventDay[0] = '\0';
            strlcpy(scene.nextEventTime, "--:--", sizeof(scene.nextEventTime));
        }

        logHeap("after HTTP");
        const bool allFresh = scene.weatherFresh && scene.calendarFresh;
        return allFresh ? BRIDGE_FETCH_FRESH : BRIDGE_FETCH_STALE;
    }

    // Compact schema used by the user's already-running bridge.
    if (doc.containsKey("next_event_epoch") || doc["weather"].is<JsonObject>()) {
        const BridgeFetchResult result = parseSimpleBridge(doc);
        logHeap("after HTTP/simple");
        return result;
    }

    Serial.println("Bridge JSON schema not recognized.");
    scene.bridgeCompatible = false;
    scene.weatherFresh = false;
    scene.calendarFresh = false;
    return BRIDGE_FETCH_FAILED;
}

static void updateClockFields() {
    struct tm t {};
    bool valid = false;
    bool fromNtp = false;

    if (systemClockValid()) {
        time_t now = time(nullptr);
        localtime_r(&now, &t);
        valid = true;
        fromNtp = true;
    } else if (bridgeClockValid) {
        uint32_t elapsed = (millis() - bridgeClockBaseMillis) / 1000U;
        time_t localPseudoEpoch = (time_t)bridgeClockBaseEpoch
                                + elapsed
                                + bridgeUtcOffsetSeconds;
        gmtime_r(&localPseudoEpoch, &t);
        valid = true;
    }

    scene.timeValid = valid;
    scene.timeFromNtp = fromNtp;

    if (!valid) {
        scene.secondsToday = 0;
        return;
    }

    const int hour24 = t.tm_hour;
    scene.hour24 = hour24;
    scene.hour12 = hour24 % 12;
    if (scene.hour12 == 0) scene.hour12 = 12;
    scene.minute = t.tm_min;
    scene.second = t.tm_sec;
    scene.pm = hour24 >= 12;

    scene.year = t.tm_year + 1900;
    scene.month = t.tm_mon + 1;
    scene.day = t.tm_mday;
    scene.weekday = t.tm_wday;
    scene.secondsToday = (uint32_t)(hour24 * 3600 + t.tm_min * 60 + t.tm_sec);
}

static void drawSceneToBackbuffer(const retro::SceneData& frameScene) {
    graphics.setHue(0);
    graphics.begin(0);
    retro::render(backend, frameScene, sceneCache);
}

static void presentPreparedBackbuffer() {
    // The scanout ISR dereferences the current framebuffer once per active
    // scanline. Do not change that pointer halfway down the visible raster.
    // Wait for NTSC vertical blanking, then swap and repoint immediately.
    if (videoHardwareInitialized) waitForVideoBlanking();
    graphics.end();
    if (videoHardwareInitialized) {
        composite.sendFrameHalfResolution(&graphics.frame);
    }
}

static void prepareLiveScene() {
    updateClockFields();
    applySettingsToScene();
    scene.frameMs = millis();
    populateSceneDiagnosticsFromCache();
}

static void renderFrame() {
    prepareLiveScene();
    drawSceneToBackbuffer(scene);
    presentPreparedBackbuffer();
}

static void captureVideoTestScene() {
    // Freeze every time-dependent input once so the isolation tests measure
    // only the requested framebuffer operation, not WiFi/RSSI/heap queries or
    // changes in the clock/city animation between frames.
    prepareLiveScene();
    videoTestScene = scene;
}

static void prepareSwapOnlyBuffers() {
    // Put the same frozen scene in BOTH framebuffers. Once this preparation is
    // complete, the test loop performs only graphics.end() + framebuffer
    // handoff every 500 ms. Any jitter during the ACTIVE period therefore
    // implicates the swap/handoff path rather than pixel rendering.
    drawSceneToBackbuffer(videoTestScene);
    presentPreparedBackbuffer();
    drawSceneToBackbuffer(videoTestScene);
}

static void beginVideoIsolationTest(VideoIsolationMode mode) {
    if (!videoStarted || mode == VIDEO_TEST_NONE) return;

    requestedVideoTestMode = VIDEO_TEST_NONE;
    videoTestMode = mode;
    videoTestSteps = 0;
    videoTestLastStepAt = millis();
    captureVideoTestScene();

    if (mode == VIDEO_TEST_SWAP_ONLY) {
        Serial.println("VIDEO ISOLATION: preparing two identical framebuffers for SWAP_ONLY test...");
        prepareSwapOnlyBuffers();
    }

    videoTestUntil = millis() + VIDEO_ISOLATION_TEST_MS;
    Serial.printf("VIDEO ISOLATION: %s ACTIVE for 30 seconds. Bridge refresh and diagnostic-cache updates are paused.\n",
                  videoTestModeName(mode));
}

static void finishVideoIsolationTest() {
    const VideoIsolationMode finished = videoTestMode;
    const uint32_t steps = videoTestSteps;
    videoTestMode = VIDEO_TEST_NONE;
    videoTestUntil = 0;
    videoTestLastStepAt = 0;
    videoTestSteps = 0;
    refreshRenderDiagnosticCache(true);
    Serial.printf("VIDEO ISOLATION: %s complete after %u test steps; normal rendering resumed.\n",
                  videoTestModeName(finished), (unsigned)steps);
    renderFrame();
    lastDrawAt = millis();
}

static void serviceVideoIsolationTest(uint32_t now) {
    if (requestedVideoTestMode != VIDEO_TEST_NONE && !videoTestActive()) {
        beginVideoIsolationTest(requestedVideoTestMode);
        now = millis();
    }

    if (videoTestMode == VIDEO_TEST_NONE) return;

    if (!videoTestActive()) {
        finishVideoIsolationTest();
        return;
    }

    if (videoTestMode == VIDEO_TEST_FREEZE) return;
    if (now - videoTestLastStepAt < VIDEO_ISOLATION_STEP_MS) return;
    videoTestLastStepAt = now;

    if (videoTestMode == VIDEO_TEST_RENDER_ONLY) {
        // Intentionally redraw the hidden/back buffer only. The currently
        // displayed front-buffer pointer never changes during this test.
        drawSceneToBackbuffer(videoTestScene);
        ++videoTestSteps;
        return;
    }

    if (videoTestMode == VIDEO_TEST_SWAP_ONLY) {
        // Both buffers were pre-rendered with identical pixels. Only swap and
        // handoff them here; no pixel drawing occurs during the active test.
        presentPreparedBackbuffer();
        ++videoTestSteps;
    }
}

static bool startVideo() {
    if (videoStarted) return true;

    Serial.println("Starting composite video after WiFi/NTP/bridge startup phase...");
    logHeap("pre-video");

    const uint32_t cpuMHz = getCpuFrequencyMhz();
    Serial.printf("CPU frequency before video: %u MHz\n", (unsigned)cpuMHz);
    if (cpuMHz != 240U) {
        Serial.printf("WARNING: composite library expects a 240 MHz ESP32 CPU; current=%u MHz\n",
                      (unsigned)cpuMHz);
    }

    // Do not move these earlier. WiFi setup must complete before video starts.
    composite.init();
    graphics.init();
    videoHardwareInitialized = true;

    logHeap("post-video");
    refreshRenderDiagnosticCache(true);
    renderFrame();

    // The scanout engine now runs asynchronously from the library's I2S
    // interrupt/DMA path. We only repoint it after graphics.end() swaps the
    // double buffers; there is intentionally no core-0 sender task.
    videoStarted = true;
    Serial.println("Composite scanout active; no continuous video sender task.");
    Serial.println("Normal bridge refresh cadence active.");
    startConfigWebServer();
    return true;
}

static void performWifiReset() {
    Serial.println("Resetting WiFi credentials...");
    if (videoStarted) {
        scene.wifiResetPercent = 100;
        renderFrame();
        delay(700);
    }

    wm.resetSettings();
    WiFi.disconnect(true, true);
    delay(150);
    ESP.restart();
}

static void updateButton() {
    const uint32_t now = millis();
    const bool rawDown = digitalRead(WIFI_RESET_PIN) == LOW;

    if (rawDown != buttonRawDown) {
        buttonRawDown = rawDown;
        buttonRawChangedAt = now;
    }

    if (now - buttonRawChangedAt < BUTTON_DEBOUNCE_MS) return;

    if (buttonStableDown != buttonRawDown) {
        buttonStableDown = buttonRawDown;

        if (buttonStableDown) {
            buttonPressedAt = now;
        } else {
            const uint32_t held = now - buttonPressedAt;
            scene.wifiResetPercent = 0;

            if (held >= BUTTON_DEBOUNCE_MS && held <= SHORT_PRESS_MAX_MS && videoStarted) {
                scene.diagnosticsMode = !scene.diagnosticsMode;
                Serial.printf("Diagnostics mode: %s\n", scene.diagnosticsMode ? "ON" : "OFF");
                renderFrame();
            }
        }
    }

    if (buttonStableDown) {
        const uint32_t held = now - buttonPressedAt;
        uint32_t pct = (held * 100U) / WIFI_RESET_HOLD_MS;
        if (pct > 100U) pct = 100U;
        scene.wifiResetPercent = (uint8_t)pct;

        if (held >= WIFI_RESET_HOLD_MS) {
            performWifiReset();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.printf("\nRetro CRT clock V%s boot\n", FIRMWARE_VERSION);

    disableCore0WDT();
    pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
    buttonRawDown = digitalRead(WIFI_RESET_PIN) == LOW;
    buttonStableDown = buttonRawDown;
    buttonRawChangedAt = millis();
    if (buttonStableDown) buttonPressedAt = millis();

    loadSettings();

    bridgeParam = new WiFiManagerParameter(
        "bridge",
        "Bridge URL (plain HTTP)",
        bridgeUrl,
        sizeof(bridgeUrl) - 1
    );

    timezoneParam = new WiFiManagerParameter(
        "tzrule",
        "POSIX timezone rule",
        timezoneRule,
        sizeof(timezoneRule) - 1
    );

    WiFi.mode(WIFI_STA);
    wm.setTitle("Retro CRT Clock Setup");
    wm.setConfigPortalBlocking(false);
    wm.setConnectTimeout(12);
    wm.setSaveConnectTimeout(12);
    wm.addParameter(bridgeParam);
    wm.addParameter(timezoneParam);
    wm.setSaveParamsCallback(savePortalParameters);

    // No video hardware exists yet, so captive-portal reliability is maximized.
    bool connected = wm.autoConnect("RetroCRT-Setup");
    portalActive = !connected;

    if (WiFi.status() == WL_CONNECTED) {
        portalActive = false;
        wifiConnectedAt = millis();
        Serial.print("WiFi connected: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Captive portal active: connect to RetroCRT-Setup");
    }
}

void loop() {
    updateButton();

    if (!videoStarted) {
        if (portalActive) wm.process();

        if (WiFi.status() == WL_CONNECTED) {
            if (portalActive) {
                wm.stopConfigPortal();
                portalActive = false;
                wifiConnectedAt = millis();
                Serial.print("WiFi connected from portal: ");
                Serial.println(WiFi.localIP());
            } else if (wifiConnectedAt == 0) {
                wifiConnectedAt = millis();
            }

            if (millis() - wifiConnectedAt >= WIFI_GRACE_MS) {
                configureNtp();
                waitBrieflyForNtp();

                // First HTTP fetch still happens before the double framebuffer
                // consumes its large contiguous memory block.
                const BridgeFetchResult initial = fetchBridge();

                startVideo();

                if (lastFetchAttemptAt == 0) lastFetchAttemptAt = millis();
                nextFetchDelay = (initial == BRIDGE_FETCH_FRESH)
                    ? FETCH_FRESH_INTERVAL_MS
                    : FETCH_STALE_INTERVAL_MS;
            }
        }

        delay(2);
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        startConfigWebServer();
        if (webServerStarted) configServer.handleClient();
    }

    const uint32_t now = millis();

    serviceVideoIsolationTest(now);

    // Keep WiFi/RSSI/heap queries out of the twice-per-second render path.
    // Pause even this small cache refresh during isolation tests so each test
    // has one clear variable.
    if (!videoTestActive()) refreshRenderDiagnosticCache(false);

    if (previewActive && (int32_t)(now - previewExpiresAt) >= 0) {
        settings = savedSettings;
        previewActive = false;
        previewExpiresAt = 0;
        copySettingsToRuntime();
        applySettingsToScene();
        Serial.println("Web preview expired; restored saved settings.");
        if (!videoTestActive()) renderFrame();
    }

    if (restartRequested && (int32_t)(now - restartRequestedAt) >= 0) {
        delay(50);
        ESP.restart();
    }

    // Never reopen a captive portal while video is running. Ordinary station
    // reconnection is safe; GPIO4 reset reboots into the video-off setup path.
    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t lastReconnectAt = 0;
        if (millis() - lastReconnectAt > 10000U) {
            WiFi.reconnect();
            lastReconnectAt = millis();
        }
    }

    const uint32_t renderInterval = buttonStableDown
        ? BUTTON_RENDER_INTERVAL_MS
        : NORMAL_RENDER_INTERVAL_MS;

    if (!videoTestActive() && now - lastDrawAt >= renderInterval) {
        lastDrawAt = now;
        renderFrame();
    }

    // Normal production refresh cadence. Video scanout remains interrupt/DMA driven.
    if (!videoTestActive() && WiFi.status() == WL_CONNECTED &&
        (forceBridgeRefresh || now - lastFetchAttemptAt >= nextFetchDelay)) {
        forceBridgeRefresh = false;
        const BridgeFetchResult result = fetchBridge();
        nextFetchDelay = (result == BRIDGE_FETCH_FRESH)
            ? FETCH_FRESH_INTERVAL_MS
            : FETCH_STALE_INTERVAL_MS;
        if (videoStarted && !videoTestActive()) renderFrame();
    }

    if (!videoTestActive() && now - lastHealthLogAt >= HEALTH_LOG_INTERVAL_MS) {
        lastHealthLogAt = now;
        Serial.printf("[health] free=%u largest8=%u rssi=%d blankWaitTimeouts=%u uptime=%lus\n",
                      (unsigned)ESP.getFreeHeap(),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                      WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127,
                      (unsigned)videoBlankWaitTimeouts,
                      (unsigned long)(now / 1000U));
    }

    delay(1);
}
