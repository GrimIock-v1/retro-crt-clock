# V3.6 visual layout notes

Native canvas: 336x240 NTSC.

Approximate layout:

- date: x=12, y=12
- weather: upper-right
- moon: x=286, y=80, radius=12
- time: y=96, horizontally centered, 6x native 8px bitmap glyph scale
- AM/PM: immediately right of time
- NEXT label: x=46, y=149
- event: x=46, y=162
- skyline grounds: around y=214-216, three parallax layers extending far upward behind the time
- water/horizon: y=216 onward

Animation is intentionally cheap and deterministic:

- far skyline scrolls slowest
- middle skyline scrolls faster
- near skyline scrolls fastest
- windows change only in slow time buckets
- clouds drift independently
- stars twinkle sparsely
- foreground information drifts +/-2 px on a slow 13-position cycle for burn-in mitigation

All background luminance values are deliberately below foreground text luminance.


## V3.7 tweak update

- moon moved up and behind the buildings
- thicker day-progress horizon at the waterfront
- weather icon rendering now uses the provided custom icon set instead of generated shapes


## V3.8 polish update

- Added a weather descriptor label such as SUNNY, CLOUDY, or WINDY.
- Increased the size and contrast of the NEXT label.
- Reduced burn-in drift so the screen no longer looks like it is constantly bumping around.
- Brightened the skyline and windows so the city scene is more noticeable.
- Moved the moon a bit higher.


## V3.8.1 alignment tweak

- Aligned the weather descriptor directly under the temperature block.
- Lowered the AM/PM marker so it aligns better with the bottom of the main clock digits.


## V3.8.2 alignment correction

- Nudged the AM/PM marker back up for better baseline alignment.
- Moved the weather descriptor higher so it tucks more tightly under the temperature.


## V3.8.3 moon size tweak

- Increased the moon size by about 10 percent for better visibility near thin phases.


## Video timing isolation strategy (V3.9.2)

Because a static displayed framebuffer was observed to remain vertically stable while normal updates jittered, diagnosis now separates three operations: scanout only, backbuffer rendering only, and framebuffer swaps only. The render-only and swap-only tests use a frozen `SceneData` snapshot to avoid time/animation differences and pause unrelated periodic network/diagnostic work. This keeps the experiment useful on real analog hardware and avoids prematurely changing ISR priority or the composite generator itself.


## V3.9.3 skyline luminance

The real CRT required substantially more low-end luma than the desktop preview suggested. Skyline body/window base luminance was raised while preserving the 0-100% Background brightness control. This keeps the clock dominant while giving the physical CRT enough range to make the city clearly visible.


## V3.9.4 lower scene cleanup

The animated water/reflection treatment was removed because it did not read clearly on the physical CRT. The skyline terminates against a clean dark lower field, while the day-progress bar remains at the former horizon line.


## V3.9.5 lower skyline

Only the foreground skyline is extended to the bottom edge. The far and mid layers retain their original ground lines so the scene keeps its layered depth. The day-progress bar is drawn afterward and therefore remains readable over the foreground buildings.


## V3.9.6 city base

All three parallax skyline layers now continue to the physical bottom edge, with window lights continuing through their lower facades. The day-progress indicator is moved to the bottom edge so it no longer visually divides the city.
