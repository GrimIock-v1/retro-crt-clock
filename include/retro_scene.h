#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "arcade_bitmap_font.h"
#include "weather_icon_bitmaps.h"

#if defined(__GNUC__)
#define RETRO_INLINE inline __attribute__((always_inline))
#else
#define RETRO_INLINE inline
#endif

namespace retro {

static constexpr int SCREEN_W = 336;
static constexpr int SCREEN_H = 240;
static constexpr int SAFE_LEFT = 12;
static constexpr int SAFE_RIGHT = 324;
static constexpr int SAFE_TOP = 9;
static constexpr int SAFE_BOTTOM = 232;
static constexpr int MOON_RADIUS = 13;
static constexpr int FAR_BUILDING_COUNT = 28;
static constexpr int MID_BUILDING_COUNT = 24;
static constexpr int NEAR_BUILDING_COUNT = 20;

// Fixed-point text scales used by the hot rendering path. 256 == 1.0x.
static constexpr uint16_t SCALE_090 = 230;
static constexpr uint16_t SCALE_100 = 256;
static constexpr uint16_t SCALE_115 = 294;
static constexpr uint16_t SCALE_135 = 346;
static constexpr uint16_t SCALE_155 = 397;
static constexpr uint16_t SCALE_165 = 422;
static constexpr uint16_t SCALE_200 = 512;
static constexpr uint16_t SCALE_215 = 550;
static constexpr uint16_t SCALE_240 = 614;

static constexpr uint32_t BURNIN_DRIFT_STEP_SECONDS = 30U * 60U;

enum Condition : uint8_t {
    CONDITION_SUN = 0,
    CONDITION_CLOUD = 1,
    CONDITION_RAIN = 2,
    CONDITION_SNOW = 3,
    CONDITION_UNKNOWN = 4,
    CONDITION_THUNDER = 5,
    CONDITION_FOG = 6,
    CONDITION_WIND = 7,
    CONDITION_PARTLY = 8,
    CONDITION_CLEAR_NIGHT = 9,
    CONDITION_DRIZZLE = 10,
    CONDITION_STORM = 11
};

struct SceneData {
    bool timeValid = false;
    bool timeFromNtp = false;
    int hour12 = 12;
    int hour24 = 0;
    int minute = 0;
    int second = 0;
    bool pm = false;

    int year = 2026;
    int month = 1;
    int day = 1;
    int weekday = 4;
    uint32_t secondsToday = 0;

    bool weatherValid = false;
    bool weatherFresh = false;
    int tempC10 = 0;
    Condition condition = CONDITION_UNKNOWN;

    bool calendarValid = false;
    bool calendarFresh = false;
    bool nextEventValid = false;
    char nextEventDay[10] = "";
    char nextEventTime[16] = "--:--";

    bool sunValid = false;
    uint16_t sunriseMinute = 8 * 60;
    uint16_t sunsetMinute = 18 * 60;

    // User-adjustable display settings. Defaults match the current design.
    uint8_t driftPixels = 1;          // 0..5
    uint16_t driftIntervalMinutes = 30; // 1..120
    uint8_t nightBrightnessPct = 75;  // 10..100
    uint8_t clockBrightnessPct = 100; // 25..100
    uint8_t backgroundBrightnessPct = 100; // 0..100
    bool use24Hour = false;
    bool fahrenheit = false;
    bool showWeatherText = true;
    bool showDayBar = true;
    uint8_t citySpeed = 2; // 0=off, 1=slow, 2=normal, 3=fast

    uint32_t frameMs = 0;
    bool wifiConnected = true;
    bool bridgeReachable = false;
    bool bridgeCompatible = true;
    bool diagnosticsMode = false;
    uint8_t wifiResetPercent = 0;

    uint32_t diagFreeHeap = 0;
    uint32_t diagLargestHeap = 0;
    uint16_t diagHttpTests = 0;
    uint16_t diagHttpTarget = 0;
    uint16_t diagHttpOk = 0;
    uint16_t diagHttpFail = 0;
    uint32_t diagHttpLastMs = 0;
    uint32_t diagHttpMaxMs = 0;
    int16_t diagRssi = -127;
    bool diagWifiStressActive = false;
};

struct Building {
    uint8_t width = 20;
    uint8_t height = 40;
    uint8_t roof = 0;
    uint32_t seed = 1;
};

struct MoonCache {
    int dateKey = -1;
    int8_t diskLeft[MOON_RADIUS * 2 + 1] = {0};
    uint8_t diskWidth[MOON_RADIUS * 2 + 1] = {0};
    int8_t litLeft[MOON_RADIUS * 2 + 1] = {0};
    uint8_t litWidth[MOON_RADIUS * 2 + 1] = {0};
};

struct TextCache {
    int dateKey = -1;
    char dateText[20] = "--- --- --";

    int tempKey = INT32_MIN;
    bool weatherValidKey = false;
    bool fahrenheitKey = false;
    char tempText[12] = "--";
    char tempUnit = 'C';

    bool calendarValidKey = false;
    bool nextEventValidKey = false;
    char eventDayKey[10] = "";
    char eventTimeKey[16] = "";
    char eventText[28] = "NO CAL";
    int eventTextWidth = 0;
};

struct SceneCache {
    bool initialized = false;
    Building farBuildings[FAR_BUILDING_COUNT];
    Building midBuildings[MID_BUILDING_COUNT];
    Building nearBuildings[NEAR_BUILDING_COUNT];
    int farPeriod = 0;
    int midPeriod = 0;
    int nearPeriod = 0;
    MoonCache moon;
    TextCache text;
};

// Press Start 2P uses an 8x8 fixed cell. The actual visible uppercase and
// digit forms occupy seven rows, matching the original arcade bitmap source.
RETRO_INLINE const uint8_t* glyphFor(char c) {
    return arcadefont::glyph(c);
}

RETRO_INLINE int fxRound(int value256) {
    return (value256 + 128) >> 8;
}

RETRO_INLINE uint16_t scale256(float scale) {
    int v = (int)lroundf(scale * 256.0f);
    if (v < 64) v = 64;
    if (v > 2048) v = 2048;
    return (uint16_t)v;
}

RETRO_INLINE uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

RETRO_INLINE int clampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

template <size_t N>
RETRO_INLINE void copyText(char (&dst)[N], const char* src) {
    if (!src) src = "";
    snprintf(dst, N, "%s", src);
}

template <typename G>
RETRO_INLINE void fill(G& g, int x, int y, int w, int h, uint8_t c) {
    if (w <= 0 || h <= 0) return;
    g.fillRect(x, y, w, h, c);
}

template <typename G>
RETRO_INLINE void pixel(G& g, int x, int y, uint8_t c) {
    g.pixel(x, y, c);
}

template <typename G>
RETRO_INLINE void rect(G& g, int x, int y, int w, int h, uint8_t c, int t = 1) {
    fill(g, x, y, w, t, c);
    fill(g, x, y + h - t, w, t, c);
    fill(g, x, y, t, h, c);
    fill(g, x + w - t, y, t, h, c);
}

template <typename G>
RETRO_INLINE void drawGlyphFx(G& g, int x, int y, char ch, uint16_t s256, uint8_t c, int bold = 0) {
    const uint8_t* rows = glyphFor(ch);
    for (int row = 0; row < 8; ++row) {
        const int y0 = y + fxRound(row * s256);
        const int y1 = y + fxRound((row + 1) * s256) - 1;
        if (rows[row] == 0) continue;
        for (int col = 0; col < 8; ++col) {
            if ((rows[row] & (uint8_t)(0x80U >> col)) == 0) continue;
            const int x0 = x + fxRound(col * s256);
            const int x1 = x + fxRound((col + 1) * s256) - 1 + bold;
            fill(g, x0, y0, x1 - x0 + 1, y1 - y0 + 1, c);
        }
    }
}

template <typename G>
RETRO_INLINE void drawGlyph(G& g, int x, int y, char ch, float scale, uint8_t c, int bold = 0) {
    drawGlyphFx(g, x, y, ch, scale256(scale), c, bold);
}

RETRO_INLINE int charAdvanceFx(uint16_t s256, int bold = 0) {
    return fxRound(8 * s256) + bold;
}

RETRO_INLINE int textWidthFx(const char* s, uint16_t s256, int bold = 0) {
    const int n = (int)strlen(s);
    if (n <= 0) return 0;
    return n * charAdvanceFx(s256, bold);
}

RETRO_INLINE int textWidth(const char* s, float scale, int bold = 0) {
    return textWidthFx(s, scale256(scale), bold);
}

template <typename G>
RETRO_INLINE void drawTextFx(G& g, int x, int y, const char* s, uint16_t s256, uint8_t c, int bold = 0) {
    int cx = x;
    const int advance = charAdvanceFx(s256, bold);
    while (*s) {
        drawGlyphFx(g, cx, y, *s, s256, c, bold);
        cx += advance;
        ++s;
    }
}

template <typename G>
RETRO_INLINE void drawText(G& g, int x, int y, const char* s, float scale, uint8_t c, int bold = 0) {
    drawTextFx(g, x, y, s, scale256(scale), c, bold);
}

template <typename G>
RETRO_INLINE void drawClock(G& g, int xOffset, int yOffset,
                            int hour12, int hour24, int minute, int second,
                            bool pm, bool use24Hour, uint8_t c) {
    char timeText[8];
    if (use24Hour) {
        snprintf(timeText, sizeof(timeText), "%02d%c%02d",
                 hour24, ((second & 1) == 0) ? ':' : ' ', minute);
    } else {
        snprintf(timeText, sizeof(timeText), "%d%c%02d",
                 hour12, ((second & 1) == 0) ? ':' : ' ', minute);
    }

    const uint16_t timeScale = 1664; // 6.5x
    const int timeW = textWidthFx(timeText, timeScale);
    const int timeX = (SCREEN_W - timeW) / 2 + xOffset;
    const int timeY = 94 + yOffset;

    drawTextFx(g, timeX, timeY, timeText, timeScale, c, 0);

    if (!use24Hour) {
        const uint16_t suffixScale = SCALE_155;
        const int suffixX = timeX + timeW + 3;
        drawTextFx(g, suffixX, timeY + 33, pm ? "PM" : "AM",
                   suffixScale, c > 15 ? c - 15 : c, 0);
    }
}

template <typename G>
RETRO_INLINE void drawSyncClock(G& g, uint8_t c) {
    const char* syncClock = "--:--";
    const uint16_t timeScale = 1664;
    const int w = textWidthFx(syncClock, timeScale);
    drawTextFx(g, (SCREEN_W - w) / 2, 94, syncClock, timeScale, c, 0);
}

template <typename G>
RETRO_INLINE void drawBitmapIcon(G& g, int x, int y, const WeatherIconBitmap& icon, uint8_t c) {
    for (int row = 0; row < icon.h; ++row) {
        const uint32_t bits = icon.rows[row];
        for (int col = 0; col < icon.w; ++col) {
            if (bits & (1UL << (icon.w - 1 - col))) pixel(g, x + col, y + row, c);
        }
    }
}

template <typename G>
RETRO_INLINE void drawWeatherIcon(G& g, int x, int y, Condition condition, uint8_t hi) {
    const WeatherIconBitmap* icon = &ICON_UNKNOWN;
    switch (condition) {
        case CONDITION_SUN: icon = &ICON_SUN; break;
        case CONDITION_CLOUD: icon = &ICON_CLOUD; break;
        case CONDITION_RAIN: icon = &ICON_RAIN_HEAVY; break;
        case CONDITION_SNOW: icon = &ICON_SNOW; break;
        case CONDITION_THUNDER: icon = &ICON_THUNDER; break;
        case CONDITION_FOG: icon = &ICON_FOG; break;
        case CONDITION_WIND: icon = &ICON_WIND; break;
        case CONDITION_PARTLY: icon = &ICON_PARTLY; break;
        case CONDITION_CLEAR_NIGHT: icon = &ICON_NIGHT; break;
        case CONDITION_DRIZZLE: icon = &ICON_DRIZZLE; break;
        case CONDITION_STORM: icon = &ICON_STORM; break;
        default: icon = &ICON_UNKNOWN; break;
    }
    drawBitmapIcon(g, x, y, *icon, hi);
}

RETRO_INLINE const char* conditionLabel(Condition condition) {
    switch (condition) {
        case CONDITION_SUN: return "SUNNY";
        case CONDITION_CLOUD: return "CLOUDY";
        case CONDITION_RAIN: return "RAINY";
        case CONDITION_SNOW: return "SNOWY";
        case CONDITION_THUNDER: return "THUNDER";
        case CONDITION_FOG: return "FOGGY";
        case CONDITION_WIND: return "WINDY";
        case CONDITION_PARTLY: return "PARTLY";
        case CONDITION_CLEAR_NIGHT: return "CLEAR";
        case CONDITION_DRIZZLE: return "DRIZZLE";
        case CONDITION_STORM: return "STORMY";
        default: return "WEATHER";
    }
}

template <typename G>
RETRO_INLINE void drawWeatherDescriptor(G& g, int x, int y, Condition condition, uint8_t c) {
    const char* label = conditionLabel(condition);
    drawTextFx(g, x, y, label, SCALE_090, c, 0);
}

template <typename G>
RETRO_INLINE void drawTemperatureCached(G& g, int xRight, int y, const char* number, char unit, uint8_t c) {
    const uint16_t sc = SCALE_135;
    const int numW = textWidthFx(number, sc);
    char unitText[2] = {unit, 0};
    const int unitW = textWidthFx(unitText, sc);
    const int degreeW = 9;
    int x = xRight - (numW + degreeW + unitW);

    drawTextFx(g, x, y, number, sc, c, 0);
    x += numW;
    rect(g, x + 1, y + 1, 6, 6, c > 5 ? c - 5 : c, 2);
    x += degreeW;
    drawTextFx(g, x, y, unitText, sc, c, 0);
}

RETRO_INLINE const char* weekdayName(int weekday) {
    static const char* names[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    if (weekday < 0 || weekday > 6) return "---";
    return names[weekday];
}

RETRO_INLINE const char* monthName(int month) {
    static const char* names[] = {"---","JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    if (month < 1 || month > 12) return "---";
    return names[month];
}

RETRO_INLINE int roundedTempC(int tempC10) {
    if (tempC10 >= 0) return (tempC10 + 5) / 10;
    return -(((-tempC10) + 5) / 10);
}

RETRO_INLINE int roundedTempF(int tempC10) {
    const int tempF10 = (tempC10 * 9) / 5 + 320;
    if (tempF10 >= 0) return (tempF10 + 5) / 10;
    return -(((-tempF10) + 5) / 10);
}

RETRO_INLINE double julianDay(int year, int month, int day) {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    int A = year / 100;
    int B = 2 - A + A / 4;
    return floor(365.25 * (year + 4716))
         + floor(30.6001 * (month + 1))
         + day + B - 1524.5 + 0.5;
}

RETRO_INLINE float moonPhaseForDate(int year, int month, int day) {
    constexpr double SYNODIC = 29.530588853;
    double age = (julianDay(year, month, day) - 2451550.1) / SYNODIC;
    double p = age - floor(age);
    if (p < 0.0) p += 1.0;
    return (float)p;
}

RETRO_INLINE void prepareMoonCache(MoonCache& moon, int year, int month, int day) {
    const int key = year * 10000 + month * 100 + day;
    if (moon.dateKey == key) return;

    const float phase = moonPhaseForDate(year, month, day);
    const float theta = 2.0f * 3.14159265358979323846f * phase;
    const float widthFactor = 1.0f - cosf(theta);
    const bool waxing = phase < 0.5f;

    for (int i = 0; i < MOON_RADIUS * 2 + 1; ++i) {
        const int dy = i - MOON_RADIUS;
        const float rr = (float)(MOON_RADIUS * MOON_RADIUS - dy * dy);
        if (rr < 0.0f) {
            moon.diskLeft[i] = 0;
            moon.diskWidth[i] = 0;
            moon.litLeft[i] = 0;
            moon.litWidth[i] = 0;
            continue;
        }
        const int xr = (int)floorf(sqrtf(rr));
        const int diskSpan = xr * 2 + 1;
        int litSpan = (int)lroundf(xr * widthFactor);
        if (widthFactor > 1.999f) litSpan = diskSpan;
        litSpan = clampInt(litSpan, 0, diskSpan);

        moon.diskLeft[i] = (int8_t)-xr;
        moon.diskWidth[i] = (uint8_t)diskSpan;
        moon.litLeft[i] = (int8_t)(waxing ? (xr - litSpan + 1) : -xr);
        moon.litWidth[i] = (uint8_t)litSpan;
    }
    moon.dateKey = key;
}

RETRO_INLINE void prepareBuildings(SceneCache& cache) {
    if (cache.initialized) return;

    cache.farPeriod = 0;
    for (int i = 0; i < FAR_BUILDING_COUNT; ++i) {
        uint32_t h = mix32(0x514E1D2BU ^ (uint32_t)i * 2654435761U);
        Building& b = cache.farBuildings[i];
        b.width = 10 + (h & 7);
        b.height = 92 + ((h >> 6) % 65); // tallest layer reaches high behind clock
        b.roof = (h >> 13) & 3;
        b.seed = h;
        cache.farPeriod += b.width + 3;
    }

    cache.midPeriod = 0;
    for (int i = 0; i < MID_BUILDING_COUNT; ++i) {
        uint32_t h = mix32(0x7F4A7C15U ^ (uint32_t)i * 3266489917U);
        Building& b = cache.midBuildings[i];
        b.width = 12 + (h & 7);
        b.height = 68 + ((h >> 6) % 62);
        b.roof = (h >> 12) & 3;
        b.seed = h;
        cache.midPeriod += b.width + 4;
    }

    cache.nearPeriod = 0;
    for (int i = 0; i < NEAR_BUILDING_COUNT; ++i) {
        uint32_t h = mix32(0x9E3779B9U ^ (uint32_t)i * 2246822519U);
        Building& b = cache.nearBuildings[i];
        b.width = 16 + (h & 9);
        b.height = 42 + ((h >> 5) % 54);
        b.roof = (h >> 12) & 3;
        b.seed = h;
        cache.nearPeriod += b.width + 5;
    }

    cache.initialized = true;
}

RETRO_INLINE void prepareTextCache(TextCache& text, const SceneData& d) {
    if (d.timeValid) {
        const int dateKey = d.year * 10000 + d.month * 100 + d.day;
        if (text.dateKey != dateKey) {
            snprintf(text.dateText, sizeof(text.dateText), "%s %s %d",
                     weekdayName(d.weekday), monthName(d.month), d.day);
            text.dateKey = dateKey;
        }
    }

    if (text.tempKey != d.tempC10 || text.weatherValidKey != d.weatherValid ||
        text.fahrenheitKey != d.fahrenheit) {
        if (d.weatherValid) {
            const int shown = d.fahrenheit ? roundedTempF(d.tempC10) : roundedTempC(d.tempC10);
            snprintf(text.tempText, sizeof(text.tempText), "%d", shown);
        } else {
            copyText(text.tempText, "--");
        }
        text.tempUnit = d.fahrenheit ? 'F' : 'C';
        text.tempKey = d.tempC10;
        text.weatherValidKey = d.weatherValid;
        text.fahrenheitKey = d.fahrenheit;
    }

    if (text.calendarValidKey != d.calendarValid ||
        text.nextEventValidKey != d.nextEventValid ||
        strcmp(text.eventDayKey, d.nextEventDay) != 0 ||
        strcmp(text.eventTimeKey, d.nextEventTime) != 0) {

        if (!d.calendarValid) {
            copyText(text.eventText, "NO CAL");
        } else if (!d.nextEventValid) {
            copyText(text.eventText, "NONE");
        } else if (d.nextEventDay[0]) {
            snprintf(text.eventText, sizeof(text.eventText), "%s %s", d.nextEventDay, d.nextEventTime);
        } else {
            copyText(text.eventText, d.nextEventTime);
        }

        copyText(text.eventDayKey, d.nextEventDay);
        copyText(text.eventTimeKey, d.nextEventTime);
        text.calendarValidKey = d.calendarValid;
        text.nextEventValidKey = d.nextEventValid;
        text.eventTextWidth = textWidthFx(text.eventText, SCALE_155);
    }
}

RETRO_INLINE void prepareSceneCache(SceneCache& cache, const SceneData& d) {
    prepareBuildings(cache);
    if (d.timeValid) prepareMoonCache(cache.moon, d.year, d.month, d.day);
    prepareTextCache(cache.text, d);
}

RETRO_INLINE uint8_t fallbackNightLevel(uint32_t secondsToday) {
    const int minute = (int)(secondsToday / 60U);
    if (minute >= 8 * 60 && minute < 18 * 60) return 0;
    if (minute >= 21 * 60 || minute < 5 * 60) return 255;
    if (minute >= 18 * 60) return (uint8_t)(((minute - 18 * 60) * 255) / (3 * 60));
    return (uint8_t)(((8 * 60 - minute) * 255) / (3 * 60));
}

RETRO_INLINE uint8_t nightLevel(const SceneData& d) {
    if (!d.timeValid) return 180;
    if (!d.sunValid || d.sunriseMinute >= d.sunsetMinute) {
        return fallbackNightLevel(d.secondsToday);
    }

    const int minute = (int)(d.secondsToday / 60U);
    const int sunrise = d.sunriseMinute;
    const int sunset = d.sunsetMinute;
    const int dawnStart = clampInt(sunrise - 60, 0, 1439);
    const int dayStart = clampInt(sunrise + 30, 0, 1439);
    const int duskStart = clampInt(sunset - 30, 0, 1439);
    const int nightStart = clampInt(sunset + 60, 0, 1439);

    if (minute < dawnStart || minute >= nightStart) return 255;
    if (minute >= dayStart && minute < duskStart) return 0;
    if (minute < dayStart) {
        const int span = dayStart - dawnStart;
        return (uint8_t)(255 - ((minute - dawnStart) * 255) / (span > 0 ? span : 1));
    }
    const int span = nightStart - duskStart;
    return (uint8_t)(((minute - duskStart) * 255) / (span > 0 ? span : 1));
}

RETRO_INLINE uint8_t scaleLuma(uint8_t base, uint8_t pct) {
    if (pct > 100) pct = 100;
    return (uint8_t)(((uint16_t)base * pct + 50U) / 100U);
}

RETRO_INLINE uint8_t dimForNight(uint8_t base, uint8_t night, uint8_t nightPct = 75) {
    if (nightPct < 10) nightPct = 10;
    if (nightPct > 100) nightPct = 100;
    // Linearly interpolate from 100% in daytime to the configured minimum at full night.
    const uint16_t range = (uint16_t)(100 - nightPct);
    const uint16_t pct = (uint16_t)(100 - ((uint32_t)range * night) / 255U);
    return scaleLuma(base, (uint8_t)pct);
}

RETRO_INLINE void burnInDrift(const SceneData& d, int& dx, int& dy) {
    static const int8_t driftX[9] = {0, 0, 1, 1, 0, 0, -1, -1, 0};
    static const int8_t driftY[9] = {0, 0, 0, 1, 1, 0, 0, -1, -1};
    if (!d.timeValid || d.driftPixels == 0) {
        dx = 0;
        dy = 0;
        return;
    }
    const uint32_t interval = (uint32_t)clampInt((int)d.driftIntervalMinutes, 1, 120) * 60U;
    const uint32_t slot = (d.secondsToday / interval) % 9U;
    const int pixels = clampInt((int)d.driftPixels, 0, 5);
    dx = driftX[slot] * pixels;
    dy = driftY[slot] * pixels;
}

template <typename G>
RETRO_INLINE void drawBuilding(G& g, int x, int groundY, const Building& b,
                               uint8_t bodyLum, uint8_t windowLum,
                               uint32_t flickerBucket, bool sparseWindows,
                               bool extendToBottom) {
    const int y = groundY - b.height;
    const int bodyHeight = extendToBottom ? (SCREEN_H - y) : b.height;
    fill(g, x, y, b.width, bodyHeight, bodyLum);

    // A few very simple roof forms keep the skyline legible but cheap.
    if (b.roof == 0 && b.height > 78) {
        fill(g, x + b.width / 2, y - 9, 1, 9, bodyLum ? (uint8_t)(bodyLum + 1) : 0);
        if ((b.seed & 1U) == 0U) fill(g, x + b.width / 2 + 2, y - 6, 1, 6, bodyLum ? (uint8_t)(bodyLum + 1) : 0);
    } else if (b.roof == 1 && b.width > 18) {
        fill(g, x + 4, y - 3, b.width - 8, 3, bodyLum);
    } else if (b.roof == 2 && b.height > 65) {
        fill(g, x + b.width / 2 - 2, y - 5, 5, 5, bodyLum);
    }

    const int cols = (b.width - 6) / 6;
    // When a skyline layer extends to the bottom, continue the window grid
    // through that lower facade too. Otherwise the extension reads like a
    // featureless slab on the CRT.
    const int rows = ((extendToBottom ? bodyHeight : b.height) - 10) / 10;
    const int threshold = sparseWindows ? 9 : 13;

    int slot = 0;
    for (int ry = 0; ry < rows; ++ry) {
        for (int cx = 0; cx < cols; ++cx, ++slot) {
            const uint32_t h = mix32(b.seed ^ (uint32_t)slot * 3266489917U);
            if ((int)(h % 100U) >= threshold) continue;

            bool on = true;
            // Only a minority of windows actually flicker.
            if ((h % 1000U) < 160U) {
                const uint32_t dyn = mix32(h ^ flickerBucket * 668265263U);
                on = (dyn % 100U) < 58U;
            }
            if (!on) continue;

            const uint8_t wl = windowLum ? (uint8_t)(windowLum + ((h >> 8) & 3U)) : 0;
            if (wl) fill(g, x + 3 + cx * 6, y + 6 + ry * 10,
                         sparseWindows ? 1 : 2, sparseWindows ? 2 : 3, wl);
        }
    }
}

template <typename G, int N>
RETRO_INLINE void drawSkylineLayer(G& g, const Building (&buildings)[N], int period,
                                    int groundY, int scrollPx, int gap,
                                    uint8_t bodyLum, uint8_t windowLum,
                                    uint32_t flickerBucket, bool sparseWindows,
                                    bool extendToBottom) {
    if (period <= 0) return;
    const int offset = scrollPx % period;
    for (int repeat = 0; repeat < 3; ++repeat) {
        int x = repeat * period - offset - period;
        for (int i = 0; i < N; ++i) {
            drawBuilding(g, x, groundY, buildings[i], bodyLum, windowLum,
                         flickerBucket, sparseWindows, extendToBottom);
            x += buildings[i].width + gap;
        }
    }
}

template <typename G>
RETRO_INLINE void drawSceneCloud(G& g, int x, int y, int scale, uint8_t lum) {
    fill(g, x + 2 * scale, y + 2 * scale, 12 * scale, 2 * scale, lum);
    fill(g, x + 5 * scale, y + 1 * scale, 6 * scale, 1 * scale, lum);
    fill(g, x + 7 * scale, y, 3 * scale, 1 * scale, lum);
    fill(g, x + 12 * scale, y + 3 * scale, 5 * scale, 1 * scale, lum);
}

template <typename G>
RETRO_INLINE void drawStars(G& g, uint32_t frameMs, uint8_t bgPct, uint8_t night, uint8_t nightPct) {
    static const uint16_t xs[11] = {28, 62, 91, 132, 157, 188, 216, 248, 271, 301, 319};
    static const uint8_t ys[11] = {38, 57, 29, 49, 20, 35, 62, 28, 50, 36, 67};
    const uint32_t twinkle = frameMs / 3500U;
    for (int i = 0; i < 11; ++i) {
        uint32_t h = mix32((uint32_t)i * 2246822519U ^ twinkle);
        if ((h & 3U) == 0U) continue;
        const uint8_t base = (uint8_t)(7 + ((h >> 4) & 3U));
        const uint8_t lum = dimForNight(scaleLuma(base, bgPct), night, nightPct);
        if (lum) pixel(g, xs[i], ys[i], lum);
    }
}

RETRO_INLINE uint32_t cityFrameMs(const SceneData& d) {
    switch (d.citySpeed) {
        case 0: return 0;
        case 1: return d.frameMs / 2U;
        case 3: return d.frameMs * 2U;
        default: return d.frameMs;
    }
}

template <typename G>
RETRO_INLINE void drawSkyline(G& g, const SceneData& d, const SceneCache& cache) {
    const uint8_t night = nightLevel(d);
    const uint8_t dayLift = (uint8_t)((255U - night) / 85U); // 0..3
    const uint32_t animMs = cityFrameMs(d);
    const uint32_t flickerBucket = animMs / 5600U;
    const uint8_t bgPct = d.backgroundBrightnessPct;

    drawStars(g, animMs, bgPct, night, d.nightBrightnessPct);

    const int cloudA = 330 - (int)((animMs / 3600U) % 430U);
    const int cloudB = 260 - (int)((animMs / 5200U) % 400U);
    const int cloudC = 390 - (int)((animMs / 7000U) % 500U);
    drawSceneCloud(g, cloudA, 42, 1, dimForNight(scaleLuma((uint8_t)(4 + dayLift), bgPct), night, d.nightBrightnessPct));
    drawSceneCloud(g, cloudB, 68, 1, dimForNight(scaleLuma((uint8_t)(4 + dayLift), bgPct), night, d.nightBrightnessPct));
    drawSceneCloud(g, cloudC, 28, 1, dimForNight(scaleLuma((uint8_t)(3 + dayLift), bgPct), night, d.nightBrightnessPct));

    drawMoonCached(g, 284, 54, cache.moon,
                   dimForNight(scaleLuma(30, bgPct), night, d.nightBrightnessPct));

    const int farScroll = (int)(animMs / 2600U);
    const int midScroll = (int)(animMs / 1550U);
    const int nearScroll = (int)(animMs / 900U);

    // V3.9.3: give the real CRT substantially more skyline headroom.
    // Around 50% now lands near the old 100% city-body brightness, while
    // 100% remains safely below the hero clock luminance.
    drawSkylineLayer(g, cache.farBuildings, cache.farPeriod, 214, farScroll, 3,
                     dimForNight(scaleLuma((uint8_t)(16 + dayLift), bgPct), night, d.nightBrightnessPct),
                     dimForNight(scaleLuma(20, bgPct), night, d.nightBrightnessPct), flickerBucket, true, true);
    drawSkylineLayer(g, cache.midBuildings, cache.midPeriod, 215, midScroll, 4,
                     dimForNight(scaleLuma((uint8_t)(13 + dayLift), bgPct), night, d.nightBrightnessPct),
                     dimForNight(scaleLuma(24, bgPct), night, d.nightBrightnessPct), flickerBucket, true, true);
    // All skyline layers continue to the physical bottom edge. Windows also
    // continue through the lower facades, so gaps between foreground towers
    // retain depth instead of revealing a flat dark strip.
    drawSkylineLayer(g, cache.nearBuildings, cache.nearPeriod, 216, nearScroll, 5,
                     dimForNight(scaleLuma((uint8_t)(10 + dayLift), bgPct), night, d.nightBrightnessPct),
                     dimForNight(scaleLuma(30, bgPct), night, d.nightBrightnessPct), flickerBucket, false, true);

}template <typename G>
RETRO_INLINE void drawMoonCached(G& g, int cx, int cy, const MoonCache& moon, uint8_t c) {
    for (int i = 0; i < MOON_RADIUS * 2 + 1; ++i) {
        const int dy = i - MOON_RADIUS;
        if (moon.diskWidth[i] > 0) {
            fill(g, cx + moon.diskLeft[i], cy + dy, moon.diskWidth[i], 1, 7);
        }
        if (moon.litWidth[i] > 0) {
            fill(g, cx + moon.litLeft[i], cy + dy, moon.litWidth[i], 1, c);
        }
    }
}

template <typename G>
RETRO_INLINE void drawDayHorizon(G& g, uint32_t secondsToday) {
    // Keep the day indicator at the very bottom of the raster so it reads as
    // a small HUD element rather than a horizon/separator through the city.
    const int x = SAFE_LEFT + 8;
    const int y = SCREEN_H - 4;
    const int w = (SAFE_RIGHT - SAFE_LEFT) - 16;
    int progress = (int)(((uint64_t)secondsToday * (uint64_t)w) / 86400ULL);
    progress = clampInt(progress, 0, w);

    fill(g, x, y, w, 2, 4);
    if (progress > 0) fill(g, x, y, progress, 2, 18);
    if (progress > 0) fill(g, x + progress - 1, y - 1, 2, 4, 28);
}

template <typename G>
RETRO_INLINE void drawNormalStatus(G& g, const SceneData& d, int dx, int dy, uint8_t c) {
    if (!d.wifiConnected) {
        drawTextFx(g, SAFE_LEFT + dx, 203 + dy, "OFFLINE", SCALE_100, c, 0);
    } else if (!d.bridgeCompatible) {
        drawTextFx(g, SAFE_LEFT + dx, 203 + dy, "BRIDGE VERSION", SCALE_100, c, 0);
    }
}

template <typename G>
RETRO_INLINE void drawResetOverlay(G& g, uint8_t percent) {
    if (percent > 100) percent = 100;
    const int x = 44, y = 91, w = 248, h = 59;
    fill(g, x, y, w, h, 1);
    rect(g, x, y, w, h, 46, 2);
    drawTextFx(g, x + 18, y + 10, "HOLD TO RESET WIFI", SCALE_200, 48, 0);
    fill(g, x + 16, y + 40, w - 32, 7, 9);
    fill(g, x + 16, y + 40, ((w - 32) * percent) / 100, 7, 48);
}

template <typename G>
RETRO_INLINE void diagnosticHelperBlock(G& g, int x, int y, int w, int h, uint8_t c) {
    // Intentionally a separate inline helper. Compare this against the direct
    // and generic-helper paths on the real CRT when investigating brightness.
    g.fillRect(x, y, w, h, c);
}

template <typename G>
RETRO_INLINE void drawDiagnostics(G& g, const SceneData& d) {
    fill(g, 0, 0, SCREEN_W, SCREEN_H, 0);
    rect(g, SAFE_LEFT, SAFE_TOP, SAFE_RIGHT - SAFE_LEFT, SAFE_BOTTOM - SAFE_TOP, 18, 1);

    drawTextFx(g, 18, 15, "CRT CLOCK TEST", SCALE_115, 48, 0);
    drawTextFx(g, 18, 34, "336X240 NTSC", SCALE_090, 28, 0);

    char line[48];
    snprintf(line, sizeof(line), "HEAP %luK MAX %luK",
             (unsigned long)(d.diagFreeHeap / 1024U),
             (unsigned long)(d.diagLargestHeap / 1024U));
    drawTextFx(g, 18, 50, line, SCALE_090, 32, 0);

    snprintf(line, sizeof(line), "WIFI %s RSSI %d",
             d.wifiConnected ? "OK" : "NO", (int)d.diagRssi);
    drawTextFx(g, 18, 64, line, SCALE_090, 32, 0);

    snprintf(line, sizeof(line), "TIME %s BRDG %s",
             d.timeValid ? (d.timeFromNtp ? "NTP" : "BRIDGE") : "NONE",
             (d.bridgeReachable && d.bridgeCompatible) ? "OK" : "NO");
    drawTextFx(g, 18, 78, line, SCALE_090, 32, 0);

    drawTextFx(g, 18, 101, "LUMA", SCALE_090, 24, 0);
    const uint8_t levels[6] = {8,16,24,32,40,48};
    for (int i = 0; i < 6; ++i) fill(g, 60 + i * 42, 98, 31, 12, levels[i]);

    drawTextFx(g, 18, 124, "PATH A", SCALE_090, 24, 0);
    drawTextFx(g, 124, 124, "PATH B", SCALE_090, 24, 0);
    drawTextFx(g, 230, 124, "PATH C", SCALE_090, 24, 0);
    fill(g, 18, 138, 88, 11, 32);
    g.fillRect(124, 138, 88, 11, 32);
    diagnosticHelperBlock(g, 230, 138, 88, 11, 32);

    drawTextFx(g, 18, 166, "PIXEL", SCALE_090, 24, 0);
    for (int x = 65; x < 318; x += 4) fill(g, x, 164, 2, 13, ((x / 4) & 1) ? 42 : 12);

    drawTextFx(g, 18, 197, "SHORT PRESS EXIT", SCALE_090, 30, 0);
    drawTextFx(g, 18, 211, "HOLD 3 SEC RESET WIFI", SCALE_090, 30, 0);
}

template <typename G>
RETRO_INLINE void render(G& g, const SceneData& d, SceneCache& cache) {
    prepareSceneCache(cache, d);

    if (d.diagnosticsMode) {
        drawDiagnostics(g, d);
        if (d.wifiResetPercent > 0) drawResetOverlay(g, d.wifiResetPercent);
        return;
    }

    fill(g, 0, 0, SCREEN_W, SCREEN_H, 0);
    drawSkyline(g, d, cache);

    if (!d.timeValid) {
        drawTextFx(g, SAFE_LEFT, 12, "RETRO CRT CLOCK", SCALE_100, 32, 0);
        drawSyncClock(g, 44);
        const char* sync = "SYNC TIME";
        const int syncW = textWidthFx(sync, SCALE_100);
        drawTextFx(g, (SCREEN_W - syncW) / 2, 151, sync, SCALE_100, 30, 0);
        if (!d.wifiConnected) drawTextFx(g, SAFE_LEFT, 203, "OFFLINE", SCALE_100, 28, 0);
        if (d.wifiResetPercent > 0) drawResetOverlay(g, d.wifiResetPercent);
        return;
    }

    const uint8_t night = nightLevel(d);
    const uint8_t clockLum = scaleLuma(dimForNight(54, night, d.nightBrightnessPct), d.clockBrightnessPct);
    const uint8_t primaryLum = dimForNight(47, night, d.nightBrightnessPct);
    const uint8_t secondaryLum = dimForNight(42, night, d.nightBrightnessPct);
    const uint8_t quietLum = dimForNight(29, night, d.nightBrightnessPct);

    int dx = 0, dy = 0;
    burnInDrift(d, dx, dy);

    // Header: approved mockup placement, now all true Press Start 2P bitmaps.
    drawTextFx(g, SAFE_LEFT + dx, 12 + dy,
               cache.text.dateText, SCALE_135, secondaryLum, 0);

    drawWeatherIcon(g, 236 + dx, 10 + dy,
                    d.weatherValid ? d.condition : CONDITION_UNKNOWN,
                    primaryLum);
    drawTemperatureCached(g, SAFE_RIGHT + dx, 12 + dy,
                          cache.text.tempText, cache.text.tempUnit, primaryLum);
    char tempUnitText[2] = {cache.text.tempUnit, 0};
    const int tempNumW = textWidthFx(cache.text.tempText, SCALE_135);
    const int tempUnitW = textWidthFx(tempUnitText, SCALE_135);
    const int tempDegreeW = 9;
    const int tempLeft = (SAFE_RIGHT + dx) - (tempNumW + tempDegreeW + tempUnitW);
    if (d.showWeatherText) {
        drawWeatherDescriptor(g, tempLeft, 25 + dy,
                              d.weatherValid ? d.condition : CONDITION_UNKNOWN,
                              quietLum);
    }
    if (d.weatherValid && !d.weatherFresh) {
        drawTextFx(g, 317 + dx, 36 + dy, "*", SCALE_100, quietLum, 0);
    }

    // Hero clock, visually centered.
    drawClock(g, dx, dy, d.hour12, d.hour24, d.minute, d.second,
              d.pm, d.use24Hour, clockLum);

    // Calendar sits just below the hero time.
    drawTextFx(g, 46 + dx, 147 + dy, "NEXT", SCALE_115, secondaryLum, 0);
    drawTextFx(g, 46 + dx, 162 + dy, cache.text.eventText, SCALE_155, primaryLum, 0);
    if (d.calendarValid && !d.calendarFresh) {
        const int eventW = textWidthFx(cache.text.eventText, SCALE_155);
        int markerX = 46 + dx + eventW + 3;
        if (markerX > SAFE_RIGHT - 8) markerX = SAFE_RIGHT - 8;
        drawTextFx(g, markerX, 161 + dy, "*", SCALE_100, quietLum, 0);
    }

    drawNormalStatus(g, d, dx, dy, quietLum);
    if (d.showDayBar) drawDayHorizon(g, d.secondsToday);

    if (d.wifiResetPercent > 0) drawResetOverlay(g, d.wifiResetPercent);
}

} // namespace retro
