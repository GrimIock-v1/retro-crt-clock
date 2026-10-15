#pragma once

// ESP32CompositeColorVideo (pinned 24b7573) unconditionally initializes an
// optional PWM audio output on GPIO18 inside video_init_hw(). This clock does
// not use that audio path. The upstream request (2 MHz at 7-bit LEDC) is not
// achievable on this Arduino-ESP32/core combination and produces a noisy,
// harmless ledcSetup() error at every video start.
//
// Keep the video library itself pinned and untouched. Suppress ONLY those
// three Arduino LEDC setup calls while CompositeColorOutput/video_out.h is
// being parsed. The DAC/I2S/APLL/DMA composite-video path is unaffected.
#define ledcSetup(...)      (0.0)
#define ledcAttachPin(...)  ((void)0)
#define ledcWrite(...)      ((void)0)
#include "CompositeColorOutput.h"
#undef ledcWrite
#undef ledcAttachPin
#undef ledcSetup


static DRAM_ATTR uint32_t retroSoftNtscPalette[256];

inline void retroApplySoftNtscPalette(uint8_t chromaPercent = 60) {
    if (chromaPercent > 100) chromaPercent = 100;
    const uint32_t* source = RawCompositeVideoBlitter::ntsc_palette();

    for (int i = 0; i < 256; ++i) {
        const uint32_t w = source[i];
        const int s0 = (int)(w & 0xFFU);
        const int s1 = (int)((w >> 8) & 0xFFU);
        const int s2 = (int)((w >> 16) & 0xFFU);
        const int s3 = (int)((w >> 24) & 0xFFU);
        const int mean = (s0 + s1 + s2 + s3 + 2) / 4;

        auto soften = [mean, chromaPercent](int sample) -> uint8_t {
            int v = mean + ((sample - mean) * (int)chromaPercent) / 100;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            return (uint8_t)v;
        };

        const uint32_t a = soften(s0);
        const uint32_t b = soften(s1);
        const uint32_t c = soften(s2);
        const uint32_t d = soften(s3);
        retroSoftNtscPalette[i] = a | (b << 8) | (c << 16) | (d << 24);
    }

    RawCompositeVideoBlitter::_palette = retroSoftNtscPalette;
    RawCompositeVideoBlitter::ntsc_rebuild_phase_palette();
}


inline void retroSetNtscColorEnabled(bool enabled) {
    RawCompositeVideoBlitter::_ntsc_color_enabled = enabled;
}
