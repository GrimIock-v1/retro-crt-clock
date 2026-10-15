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
