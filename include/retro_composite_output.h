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
