#pragma once

#include <stdint.h>

namespace artwork_theme {

// Runtime representation is deliberately tiny. All large pixel/mask arrays
// remain const flash data generated on the build machine; the ESP32 never
// allocates a decoded background or mask in heap.
struct WindowSpan {
    uint16_t x;
    uint16_t y;
    uint8_t len;
    uint8_t component;
};

struct Layout {
    int16_t dateX;
    int16_t dateY;
    uint16_t dateScale;

    int16_t weatherIconX;
    int16_t weatherIconY;
    int16_t tempRightX;
    int16_t tempY;
    int16_t weatherDescY;

    int16_t clockY;
    uint16_t clockScale;
    uint16_t suffixScale;
    int8_t suffixDy;

    int16_t eventLabelX;
    int16_t eventLabelY;
    uint16_t eventLabelScale;
    int16_t eventX;
    int16_t eventY;
    uint16_t eventScale;
    int16_t eventMaxX;

    int16_t dayBarY;
};

enum FeatureFlags : uint8_t {
    FEATURE_STARS = 1U << 0,
    FEATURE_MOON = 1U << 1,
    FEATURE_LIGHTS = 1U << 2,
};

struct Asset {
    uint8_t id;
    const char* name;

    const uint8_t* rle;
    uint32_t rleSize;

    const uint8_t* skyMask;
    uint32_t skyMaskSize;

    const WindowSpan* windowSpans;
    uint16_t windowSpanCount;
    uint8_t windowComponentCount;

    uint16_t moonX;
    uint16_t moonY;

    uint8_t features;
    uint8_t burstPhaseQuarters;
    uint8_t neutralHue;
    uint8_t iceHue;
    uint8_t warmHue;
    uint32_t starSeed;

    // Maps the logical high-nibble hue stored by the art converter to the
    // physical composite hue index calibrated for the CRT.
    uint8_t hueMap[16];

    Layout layout;
};

inline bool skyAllows(const Asset& asset, uint16_t x, uint16_t y) {
    if (!asset.skyMask || x >= 336 || y >= 240) return false;
    const uint32_t index = (uint32_t)y * 336U + (uint32_t)x;
    const uint32_t byteIndex = index >> 3;
    if (byteIndex >= asset.skyMaskSize) return false;
    return (asset.skyMask[byteIndex] & (uint8_t)(1U << (index & 7U))) != 0;
}

} // namespace artwork_theme
