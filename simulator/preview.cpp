#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include "../include/retro_scene.h"

struct PreviewBackend {
    int w;
    int h;
    std::vector<uint8_t> pixels;

    PreviewBackend(int width, int height)
        : w(width), h(height), pixels(width * height, 0) {}

    RETRO_INLINE void fillRect(int x, int y, int rw, int rh, uint8_t c) {
        int x0 = std::max(0, x);
        int y0 = std::max(0, y);
        int x1 = std::min(w, x + rw);
        int y1 = std::min(h, y + rh);

        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx)
                pixels[yy * w + xx] = c;
    }

    RETRO_INLINE void pixel(int x, int y, uint8_t c) {
        if (x >= 0 && x < w && y >= 0 && y < h)
            pixels[y * w + x] = c;
    }

    void clear() {
        std::fill(pixels.begin(), pixels.end(), 0);
    }

    void writePPM(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) return;

        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (uint8_t v : pixels) {
            uint8_t q = (uint8_t)((v * 255U) / 54U);
            fputc(q, f);
            fputc(q, f);
            fputc(q, f);
        }
        fclose(f);
    }
};

static retro::SceneData baseScene() {
    retro::SceneData d;
    d.timeValid = true;
    d.timeFromNtp = true;
    d.year = 2026;
    d.month = 9;
    d.day = 6;
    d.weekday = 0;
    d.weatherValid = true;
    d.weatherFresh = true;
    d.tempC10 = 187;
    d.condition = retro::CONDITION_CLOUD;
    d.calendarValid = true;
    d.calendarFresh = true;
    d.nextEventValid = true;
    snprintf(d.nextEventDay, sizeof(d.nextEventDay), "TMR");
    snprintf(d.nextEventTime, sizeof(d.nextEventTime), "8:30 PM");
    d.sunValid = true;
    d.sunriseMinute = 6 * 60 + 34;
    d.sunsetMinute = 19 * 60 + 38;
    d.wifiConnected = true;
    d.bridgeReachable = true;
    d.bridgeCompatible = true;
    d.diagFreeHeap = 65020;
    d.diagLargestHeap = 20468;
    d.diagHttpTests = 12;
    d.diagHttpTarget = 50;
    d.diagHttpOk = 12;
    d.diagHttpFail = 0;
    d.diagHttpLastMs = 138;
    d.diagHttpMaxMs = 421;
    d.diagRssi = -51;
    d.diagWifiStressActive = true;
    return d;
}

static void writeScene(PreviewBackend& g, retro::SceneCache& cache,
                       retro::SceneData d, const char* filename) {
    g.clear();
    retro::render(g, d, cache);
    g.writePPM(filename);
}

int main() {
    PreviewBackend g(retro::SCREEN_W, retro::SCREEN_H);
    retro::SceneCache cache;

    auto evening = baseScene();
    evening.hour12 = 7;
    evening.minute = 42;
    evening.second = 20;
    evening.pm = true;
    evening.secondsToday = 19U * 3600U + 42U * 60U + 20U;
    evening.frameMs = 128000;
    writeScene(g, cache, evening, "preview-evening.ppm");

    auto day = baseScene();
    day.hour12 = 10;
    day.minute = 18;
    day.second = 21;
    day.pm = false;
    day.secondsToday = 10U * 3600U + 18U * 60U + 21U;
    day.frameMs = 94000;
    day.condition = retro::CONDITION_SUN;
    day.tempC10 = 224;
    snprintf(day.nextEventDay, sizeof(day.nextEventDay), "TODAY");
    snprintf(day.nextEventTime, sizeof(day.nextEventTime), "2:15 PM");
    writeScene(g, cache, day, "preview-day.ppm");

    auto daylightTheme = day;
    daylightTheme.theme = retro::THEME_DAYLIGHT;
    daylightTheme.frameMs = 118000;
    writeScene(g, cache, daylightTheme, "preview-theme-daylight.ppm");

    auto rpgTheme = evening;
    rpgTheme.theme = retro::THEME_RPG;
    rpgTheme.frameMs = 118000;
    writeScene(g, cache, rpgTheme, "preview-theme-rpg.ppm");

    auto stale = evening;
    stale.weatherFresh = false;
    stale.calendarFresh = false;
    stale.bridgeReachable = false;
    writeScene(g, cache, stale, "preview-stale.ppm");

    auto sync = baseScene();
    sync.timeValid = false;
    sync.frameMs = 70000;
    writeScene(g, cache, sync, "preview-sync.ppm");

    auto diagnostics = evening;
    diagnostics.diagnosticsMode = true;
    writeScene(g, cache, diagnostics, "preview-diagnostics.ppm");

    auto reset = evening;
    reset.wifiResetPercent = 63;
    writeScene(g, cache, reset, "preview-reset.ppm");

    return 0;
}
