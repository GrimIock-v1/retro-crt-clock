#pragma once

#include "CompositeGraphics.h"
#include "retro_scene.h"

struct EspCompositeBackend {
    CompositeGraphics& graphics;

    explicit EspCompositeBackend(CompositeGraphics& g) : graphics(g) {}

    RETRO_INLINE void setHue(uint8_t hue) {
        graphics.setHue((char)(hue & 0x0F));
    }

    RETRO_INLINE void fillRectPalette(int x, int y, int w, int h, uint8_t palette) {
        if (w <= 0 || h <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > graphics.xres) w = graphics.xres - x;
        if (y + h > graphics.yres) h = graphics.yres - y;
        if (w <= 0 || h <= 0) return;

        for (int yy = y; yy < y + h; ++yy) {
            memset(graphics.backbuffer[yy] + x, (int)palette, (size_t)w);
        }
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
