#include <assert.h>
#include <string.h>
#include "../include/retro_scene.h"

int main() {
    retro::SceneData d;
    d.timeValid = true;
    d.year = 2026;
    d.month = 9;
    d.day = 6;
    d.weekday = 0;
    d.weatherValid = true;
    d.tempC10 = 189;
    d.calendarValid = true;
    d.nextEventValid = true;
    retro::copyText(d.nextEventDay, "TMR");
    retro::copyText(d.nextEventTime, "8:30 PM");
    d.sunValid = true;
    d.sunriseMinute = 6 * 60 + 34;
    d.sunsetMinute = 19 * 60 + 38;

    retro::SceneCache cache;
    retro::prepareSceneCache(cache, d);

    assert(cache.initialized);
    assert(cache.farPeriod > retro::SCREEN_W);
    assert(cache.midPeriod > retro::SCREEN_W);
    assert(cache.nearPeriod > retro::SCREEN_W);
    assert(cache.moon.dateKey == 20260906);
    assert(strcmp(cache.text.dateText, "SUN SEP 6") == 0);
    assert(strcmp(cache.text.tempText, "19") == 0);
    assert(strcmp(cache.text.eventText, "TMR 8:30 PM") == 0);

    // Correct temperature conversion and rounding.
    assert(retro::roundedTempC(189) == 19);
    assert(retro::roundedTempC(-6) == -1);
    assert(retro::roundedTempC(-4) == 0);
    assert(retro::roundedTempF(0) == 32);
    assert(retro::roundedTempF(100) == 50);

    // Real sunrise/sunset driven ambient state.
    d.secondsToday = 12U * 3600U;
    assert(retro::nightLevel(d) == 0);
    d.secondsToday = 2U * 3600U;
    assert(retro::nightLevel(d) == 255);
    d.secondsToday = 6U * 3600U + 34U * 60U;
    assert(retro::nightLevel(d) > 0 && retro::nightLevel(d) < 255);

    // Burn-in drift obeys the configurable pixel amplitude and interval.
    d.driftPixels = 0;
    d.driftIntervalMinutes = 1;
    for (uint32_t slot = 0; slot < 9; ++slot) {
        d.secondsToday = slot * 60U;
        int dx = 9, dy = 9;
        retro::burnInDrift(d, dx, dy);
        assert(dx == 0 && dy == 0);
    }
    d.driftPixels = 5;
    for (uint32_t slot = 0; slot < 9; ++slot) {
        d.secondsToday = slot * 60U;
        int dx = 0, dy = 0;
        retro::burnInDrift(d, dx, dy);
        assert(dx >= -5 && dx <= 5);
        assert(dy >= -5 && dy <= 5);
    }
    d.driftPixels = 1;
    d.driftIntervalMinutes = 30;

    // Moon geometry changes cache key only when the date changes.
    const int oldMoonKey = cache.moon.dateKey;
    retro::prepareSceneCache(cache, d);
    assert(cache.moon.dateKey == oldMoonKey);
    d.day = 7;
    retro::prepareSceneCache(cache, d);
    assert(cache.moon.dateKey == 20260907);


    // Exact Press Start 2P bitmap checks (8px source strike).
    const uint8_t* a = retro::glyphFor('A');
    assert(a[0] == 0x38 && a[4] == 0xFE);
    const uint8_t* seven = retro::glyphFor('7');
    assert(seven[0] == 0xFE && seven[3] == 0x18);
    assert(retro::textWidth("TEST", 1.0f) == 32);

    // Fractional text scaling remains supported for tuning tools.
    assert(retro::textWidth("TEST", 1.55f) > retro::textWidth("TEST", 1.0f));

    // Brightness and animation controls stay bounded and deterministic.
    assert(retro::scaleLuma(50, 100) == 50);
    assert(retro::scaleLuma(50, 0) == 0);
    d.citySpeed = 0;
    d.frameMs = 10000;
    assert(retro::cityFrameMs(d) == 0);
    d.citySpeed = 1;
    assert(retro::cityFrameMs(d) == 5000);
    d.citySpeed = 2;
    assert(retro::cityFrameMs(d) == 10000);
    d.citySpeed = 3;
    assert(retro::cityFrameMs(d) == 20000);

    // Theme IDs stay compact and stable because they are persisted in NVS.
    assert(retro::THEME_CITY == 0);
    assert(retro::THEME_DAYLIGHT == 1);
    assert(retro::THEME_RPG == 2);
    assert(strcmp(retro::themeName(retro::THEME_CITY), "Rich Cityscape") == 0);
    assert(strcmp(retro::themeName(retro::THEME_DAYLIGHT), "Daylight") == 0);
    assert(strcmp(retro::themeName(retro::THEME_RPG), "Retro RPG") == 0);

    return 0;
}
