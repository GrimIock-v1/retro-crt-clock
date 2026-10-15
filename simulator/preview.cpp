#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include "../include/retro_scene.h"

struct PreviewBackend {
    int w;
    int h;
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> hues;
    uint8_t currentHue = 0;

    PreviewBackend(int width, int height)
        : w(width), h(height), pixels(width * height, 0), hues(width * height, 0) {}

    RETRO_INLINE void setHue(uint8_t hue) {
        currentHue = hue & 0x0F;
    }

    RETRO_INLINE void fillRectPalette(int x, int y, int rw, int rh, uint8_t palette) {
        int x0 = std::max(0, x);
        int y0 = std::max(0, y);
        int x1 = std::min(w, x + rw);
        int y1 = std::min(h, y + rh);
        const uint8_t hue = (palette >> 4) & 0x0F;
        const uint8_t lum = (uint8_t)(((palette & 0x0F) * 54U + 7U) / 15U);

        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx) {
                const int idx = yy * w + xx;
                pixels[idx] = lum;
                hues[idx] = hue;
            }
    }

    RETRO_INLINE void fillRect(int x, int y, int rw, int rh, uint8_t c) {
        int x0 = std::max(0, x);
        int y0 = std::max(0, y);
        int x1 = std::min(w, x + rw);
        int y1 = std::min(h, y + rh);

        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx) {
                const int idx = yy * w + xx;
                pixels[idx] = c;
                hues[idx] = currentHue;
            }
    }

    RETRO_INLINE void pixel(int x, int y, uint8_t c) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            const int idx = y * w + x;
            pixels[idx] = c;
            hues[idx] = currentHue;
        }
    }

    void clear() {
        std::fill(pixels.begin(), pixels.end(), 0);
        std::fill(hues.begin(), hues.end(), 0);
        currentHue = 0;
    }

    void writePPM(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) return;

        static const uint8_t hueRgb[16][3] = {
            {255,255,255},{233,252,139},{255,238,150},{255,223,178},
            {255,210,218},{255,203,255},{255,202,255},{251,208,255},
            {227,220,255},{204,234,255},{188,249,255},{181,255,237},
            {186,255,194},{201,255,160},{223,255,141},{247,244,143}
        };

        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (size_t i = 0; i < pixels.size(); ++i) {
            const uint8_t q = (uint8_t)((pixels[i] * 255U) / 54U);
            const uint8_t hue = hues[i] & 0x0F;
            if (hue == 0) {
                fputc(q, f); fputc(q, f); fputc(q, f);
            } else {
                fputc((hueRgb[hue][0] * q) / 255U, f);
                fputc((hueRgb[hue][1] * q) / 255U, f);
                fputc((hueRgb[hue][2] * q) / 255U, f);
            }
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

    auto rpgTheme = evening;
    rpgTheme.theme = retro::THEME_RPG;
    rpgTheme.frameMs = 118000;
    writeScene(g, cache, rpgTheme, "preview-theme-rpg.ppm");

    auto digitalRainTheme = evening;
    digitalRainTheme.theme = retro::THEME_DIGITAL_RAIN;
    digitalRainTheme.frameMs = 118000;
    writeScene(g, cache, digitalRainTheme, "preview-theme-digital-rain.ppm");

    auto moonlitCabinTheme = evening;
    moonlitCabinTheme.theme = retro::THEME_MOONLIT_CABIN;
    moonlitCabinTheme.frameMs = 121000;
    writeScene(g, cache, moonlitCabinTheme, "preview-theme-moonlit-cabin.ppm");

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
