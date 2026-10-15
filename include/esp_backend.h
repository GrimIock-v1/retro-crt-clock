#pragma once

#include "CompositeGraphics.h"
#include "retro_scene.h"

struct EspCompositeBackend {
    CompositeGraphics& graphics;

    explicit EspCompositeBackend(CompositeGraphics& g) : graphics(g) {}

    RETRO_INLINE void setHue(uint8_t hue) {
        graphics.setHue((char)(hue & 0x0F));
    }

    RETRO_INLINE void fillRect(int x, int y, int w, int h, uint8_t brightness) {
        // This wrapper MUST remain inline and in this header.
        graphics.fillRect(x, y, w, h, brightness);
    }

    RETRO_INLINE void pixel(int x, int y, uint8_t brightness) {
        // This wrapper MUST remain inline and in this header.
        graphics.dot(x, y, brightness);
    }
};
