# NTSC 227.5 timing experiment

This branch is intentionally separate from `feature/themes`.

It vendors the pinned ESP32CompositeColorVideo source at upstream commit
`24b75732dc2e1127fe9583cc9ff0f50c0a0d1cfa` and changes the NTSC DMA
scanline from 228 color clocks (912 samples at 4 samples/color-clock) to the
NTSC 455/2 relationship: 227.5 color clocks (910 samples).

The goal is to A/B whether the proper half-color-clock line relationship helps
the Sony CRT's Y/C separation and reduces color fringing / dot crawl.

The safe 60% chroma palette and Moonlit Cabin text-edge filtering from V3.13.4
are also present in this branch.

## Hardware test

1. Flash `feature/themes` V3.13.4 and photograph fine white text and Moonlit Cabin.
2. Flash this branch. The UI/serial version must read `3.13.4-2275`.
3. Compare color fringing, horizontal stability, sync lock, and geometry.
4. If sync is worse or the image rolls/tears, return to `feature/themes`.

No changes from this branch should be merged until the real CRT A/B is clearly better.


## Pass 2: line-to-line chroma phase

The first 227.5-clock build produced a much cleaner monochrome image on the Sony
but the TV stopped decoding Moonlit Cabin in color. That result exposed a missing
part of the NTSC relationship: a 227.5-color-clock line advances the subcarrier
by 180 degrees on each scanline.

Pass 2 therefore:
- flips the color-burst phase by 180 degrees on alternate lines,
- uses an equally phase-rotated active-video palette on those lines,
- rebuilds the alternate palette after the 60% soft-chroma palette is installed.

The firmware identifies this pass as `3.13.4-2275p2`.


## Pass 3: theme-aware monochrome/color

The p2 photos showed that the Sony was decoding some chroma in Moonlit Cabin
(the blue/cyan mountain palette was visible), but grayscale themes again showed
some false-color fringe and several low-saturation Cabin hues were collapsing.

Pass 3 therefore keeps the p2 227.5 timing + line-phase color handling, but:
- disables NTSC colorburst entirely for Rich Cityscape, Retro RPG, Digital Rain,
  and the diagnostics screen,
- enables colorburst only for Moonlit Cabin,
- raises the softened color palette from 60% to 90% chroma so greens, browns,
  warm windows, and blues separate more clearly in the color theme.

The CRT should therefore treat grayscale themes as real monochrome composite,
even with its CHROMA control left at the normal position.

Firmware identifies this pass as `3.13.4-2275p3`.


## Pass 4: natural line phase + VBlank color switching

The p3 result confirmed true-monochrome output is the right solution for the
grayscale themes. Moonlit Cabin still showed distorted/incomplete color and a
bright flash when switching into the color theme.

Pass 4:
- keeps 910 samples / 227.5 color clocks per line,
- removes the explicit software 180-degree palette/burst rotation from p2/p3,
  relying on the continuous DAC clock plus half-cycle-short scanline to produce
  the natural NTSC line-to-line phase advance,
- keeps colorburst disabled for grayscale themes,
- keeps colorburst enabled for Moonlit Cabin,
- switches colorburst mode only during VBlank at the framebuffer handoff so
  the CRT never changes decoder mode mid-active-frame.

Firmware identifies this pass as `3.13.4-2275p4`.


## Pass 5: restore working color phase + keep VBlank switching

The p4 Cabin lost color completely, while p3 had visible color. That confirms the
explicit alternate-line phase handling is required on this ESP32 I2S/DMA path.

Pass 5 therefore:
- keeps 910 samples / 227.5 color clocks per scanline,
- restores p3's alternate-line burst and active-palette phase rotation,
- keeps p4's VBlank-only monochrome/colorburst switching to avoid the bright
  decoder-relock flash,
- keeps true monochrome output for grayscale themes,
- keeps Moonlit Cabin at the stronger 90% chroma setting.

Firmware identifies this pass as `3.13.4-2275p5`.
