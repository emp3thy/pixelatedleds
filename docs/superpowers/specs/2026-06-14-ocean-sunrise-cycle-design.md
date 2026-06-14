# Ocean Day/Night Cycle — `oceanSunrise` Effect

**Date:** 2026-06-14
**Status:** Approved design
**Target:** FastLED 60×60 LED matrix (Teensy 4.0 hardware; WASM sim for preview)

## Summary

A continuous, looping seascape that plays a full day in ~3–4 minutes: the sun
rises from the ocean horizon up the centre of the sky, holds high through the
day, then sets in a fiery ocean sunset. As it sets the moon and stars fade in
and hold through the night until the next sunrise. The ocean fills the bottom
quarter as a slow, wave-rippled mirror of the sky, with sun/moon reflection
columns and occasional foam. Optional clouds (0–25% coverage, re-rolled each
cycle) drift across the sky tinted by the current light.

## Geometry

- Canvas: 60 columns × 60 rows, addressed via `XY(x, y)`.
- Orientation: `y = 0` is the top of the sky, `y = 59` the bottom.
- **Horizon at `y = 45`.** Sky = rows 0–44 (top 3/4). Ocean = rows 45–59 (bottom 1/4, 15 rows).
- Sun/moon reflections fall in the ocean band directly below their disc.
- Sun travels the centre column `x = 30`. Moon sits high on the **right** side
  (≈ `x = 44`, `y = 12`); fixed position, does not track.

## Cycle State

- One cycle period `T ≈ 210 s` (tunable 180–240 s).
- `phase = frac(millis() / T_ms)` ∈ [0,1) drives all layers.
- Sub-phases:
  | Phase | `phase` range | Notes |
  |-------|---------------|-------|
  | Sunrise / dawn | 0.00 – 0.18 | sun emerges at horizon, sky warms |
  | Day | 0.18 – 0.40 | sun rides up and holds high, bright blue |
  | Sunset | 0.40 – 0.62 | sun descends to horizon, fiery sky |
  | Night | 0.62 – 1.00 | sun gone, moon + stars, dark blue |
- **Cycle wrap detection:** when `phase` wraps from ~1.0 back toward 0, run
  once-per-cycle updates:
  - Re-roll cloud coverage (see Clouds).
  - Increment `dayCount`; `moonPhase = (dayCount % 30) / 30.0` (0 = full, waxes/wanes across 30 cycles).
- **Night factor** `nf ∈ [0,1]`: 0 during day, ramps to 1 across sunset, holds
  1 through night, ramps to 0 across sunrise. Drives stars + moon opacity and
  ocean darkness.
- **Sun altitude** `alt ∈ [0,1]` (and below-horizon when night): 0 at horizon,
  1 at peak. Smoothstep-eased. Sun disc `y_sun = HORIZON - alt * (HORIZON - PEAK_Y)`,
  `PEAK_Y ≈ 8`. Sun hidden when in the night sub-phase.

## Architecture

Layered compositor in a single self-contained header `oceanSunrise.h`. Each
frame clears nothing implicitly; layers are drawn back-to-front so later layers
overwrite/blend over earlier ones. Each layer is an independent helper function
taking the shared cycle state.

Draw order (back → front):

1. **Sky** — for every sky pixel, a vertical gradient from `topColour` to
   `horizonColour`. Both colours are produced by crossfading the four approved
   palettes (dawn / day / sunset / night) as a function of `phase` using control
   points. Night palette is a *visible dark blue* (LEDs emit light; true black
   reads as "off"). Palettes:
   - Dawn: top `#232A63` → horizon `#FFD98C` (via magenta/pink mid).
   - Day: top `#2F7FD6` → horizon `#D8F0FF`.
   - Sunset: top `#2E2350` → horizon `#FFCE5A` (via red/orange mid).
   - Night: top `#04050C`-ish lifted to a projectable navy → horizon `#122044`.
2. **Clouds** — soft Perlin-noise patches confined to the sky region. Per-cycle
   coverage `c%` rolled uniform [0,25]; clamp rule: `c < 5 → 0`, `c > 25 → 25`.
   Coverage thresholds the noise field (higher coverage → lower threshold → more
   cloud). Clouds drift slowly sideways. Tinted to the current sky/light: grey-
   white by day, pink/gold edges at dawn & sunset, dim grey-blue at night.
3. **Stars** — ~30 fixed pseudo-random points in the upper sky. Each twinkles
   (per-star sine of time). Opacity = `nf` (only visible at night).
4. **Moon** — pale blue-white disc, radius ≈ 3 (≈6px across), at the fixed right
   high spot. Lunar phase shape via an offset occluding mask circle filled with
   sky colour, driven by `moonPhase` (full → gibbous → crescent → new → … over
   30 cycles). Soft halo. Opacity = `nf`. Reflection handled in layer 7.
5. **Sun** — centre-column disc (radius ≈ 5), radial halo that warms nearby sky,
   and sunburst rays. Colour by altitude: deep orange-red `#FF5A2A` low →
   white-gold `#FFF4D0` high. Halo + rays strongest at low altitude (sunrise/
   sunset), fading near midday. Not drawn during night. Reflection in layer 7.
6. **Ocean** (rows 45–59) — overwrites the sky in this band. Base colour is a
   darker mirror of the current sky (sampled near the horizon colour, scaled
   down, darkened further by `nf`). Slow horizontal wave bands modulate
   brightness: `wave = sin(x*k1 + y*k2 + t*slow)`. Occasional white **foam**
   sparkle: sparse random pixels where the wave value is near a crest, low
   spawn probability, brief.
7. **Reflections** — vertical shimmer column on the ocean directly below the
   sun (and moon). Colour = sun/moon colour, brightness wobbles with the wave
   field and decays with depth below the horizon. Sun reflection strongest when
   the sun is low; moon reflection dimmer and white. Drawn additively over the
   ocean.

## Data / State (module-static)

- `float` cycle phase derived from `millis()`; previous-phase tracker for wrap detect.
- `uint16_t dayCount`; derived `moonPhase`.
- `uint8_t cloudCoveragePct` (rolled per cycle).
- Fixed star table (positions + per-star twinkle phase), generated once.
- Cloud noise uses `inoise8` with a drifting offset (no large buffer needed).
- Optional small scratch as needed; no large heap allocations (Teensy-friendly).

## Wiring

- New file `AllEffects_FastLED/oceanSunrise.h` (and mirrored `.sim` copy).
- `#include` it in `AllEffects_FastLED.ino`; add a `case` in a ~30–50 ms loop
  block calling `oceanSunrise()`.
- Add as effect index **25**. In `effectChanging.h`, append `25` to both
  `listOfPatternsForRectangularMatrix` and `listOfPatternsForSquareMatrix`
  **before** the trailing `13` (jusBlack), so black stays last. Bump table
  length / UI slot count and clamp.
- In `viewer.html`, add label `'ocean sunrise'` to `NAMES` before
  `'jusBlack (off)'`; extend slider `max` and the index clamp accordingly.
- Both root and `.sim` copies kept in sync; rebuild via touching the `.ino`
  (FastLED's cache does not track header-only changes) then
  `fastled .sim\AllEffects_FastLED --just-compile`.

## Performance

Full-canvas float compute per frame across ~7 layers over 3600 pixels. In line
with the existing float-heavy effects (voronoi, aurora, kusama) which run at
60–140 fps in the sim. Teensy 4.0 has an FPU; acceptable. If hardware fps is
low, the cloud/reflection layers can be throttled or simplified.

## Out of Scope (YAGNI)

- Weather beyond clouds (rain, lightning).
- Boats, birds, land masses, or other silhouettes.
- User-tunable cycle length at runtime (compile-time constant for now).
- Clouds casting shadows/colour onto the water.

## Open Tunables (set during implementation, easy to adjust)

- Cycle period `T`, peak sun height `PEAK_Y`, sun/moon radii.
- Palette control-point positions for the crossfade.
- Wave speed/scale, foam spawn rate, cloud drift speed.
- Moon side (currently right) and high position.
