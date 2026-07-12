# Fireworks Display — Design Spec (effect id 28)

## Summary

A choreographed fireworks show on the 60x60 matrix, modelled on professional display
doctrine: shells rise with a comet tail, pause at apex, then break into one of nine
shell types. The show is a looping ~75 s programme with distinct phases (opener,
colour volleys, golden interlude, rhythm section, black-sky lull, accelerating
finale, hard stop). Dark sky is the contrast engine: background is lifted navy
`#0A0E1A`, never pure black, and the sky is never allowed to saturate outside the
finale.

## Canvas & coordinates

- 60x60, `(0,0)` top-left, y increases downward. All writes via `leds[XY(x,y)]`.
- Ground = row 59. Shells launch from the bottom edge; mines erupt from it.
- Apex band for breaks: y 10-26 depending on shell calibre.

## Architecture

- New header `fireworks.h`, entry `void fireworks()`, dispatched from the 50 ms
  `EVERY_N_MILLISECONDS` block as `case 28:`. Ends with `FastLED.show()`.
- Internal timestep: effect tracks its own `millis()` delta (`dt` seconds, clamped
  to 100 ms) so physics is framerate-independent at the ~20 Hz callback.
- All state module-static, no heap. xorshift PRNG (same idiom as lightCycleRace).

### Two-buffer rendering (per-effect trail persistence)

A single global fade constant would make peony and willow look identical, so:

- `static CRGB fwTrail[NUM_LEDS]` — persistent trail buffer. Each frame it is
  scaled by a decay factor; **trailed** particles (chrysanthemum, willow, palm,
  comets, crossette comets, rising shells) stamp into it every frame.
- Compose each frame: `leds[] = navy background`, then `leds += fwTrail`, then
  **untrailed** particles (peony, dahlia, ring, strobe, mine stars, salute flash)
  are drawn fresh into `leds[]` only — crisp, no persistence.
- Per-particle trail strength: willow stamps at high brightness (long streamers),
  chrysanthemum at medium, comets fat (also stamp one neighbour pixel).
- Additive blends saturate via CRGB `+=`; final gamma via brightness curve on
  particle life, quadratic fade `b = (1-t/T)^2` with ember colour-shift in the
  last 20% of life.

### Particle system

- `static FwStar stars[FW_MAX_STARS]` (160): `x, y, vx, vy` (float), `hue/colour`,
  `age, life` (ms), `type` flags (trailed, strobe, gravity multiplier, secondary-
  break countdown for crossette).
- `static FwShell shells[FW_MAX_SHELLS]` (12): rising rockets — `x, y, vx, vy`,
  `burstType, burstY, colour`. Rise ~1.0-1.5 s, decelerating under gravity, with
  a random +/- ~3 degree launch-angle tilt off vertical so ascents vary;
  100-200 ms dark apex pause before break (the key realism cue).
- Physics (px/s units): gravity g = 10 px/s² (stars), drag ×~0.92 per 50 ms
  (exp decay); star ejection v0 = 25-45 px/s giving terminal burst radius 6-14 px;
  ±12% jitter on v0 and life (except ring: uniform on purpose).

## Shell types

| # | Type | Stars | Trails | Life | Signature |
|---|------|-------|--------|------|-----------|
| 0 | Peony | 18 | no | 1.0 s | uniform radial, single saturated colour |
| 1 | Chrysanthemum | 18 | med | 1.5 s | peony + silver-tinged persistent trails |
| 2 | Dahlia | 9 | no | 1.8 s | few big fast stars, largest radius |
| 3 | Willow | 14 | heavy | 4.5 s | gold, low v0, gravity droop, hanging streamers |
| 4 | Palm | 6 | fat | 2.0 s | thick comet arms up/outward, gold/silver, fronds |
| 5 | Crossette | 5 | med | 0.6 s + pop | comets that each split into 3-4 small stars |
| 6 | Ring | 16 | no | 1.0 s | uniform expanding circle, contrasting pistil |
| 7 | Strobe | 14 | no | 4.0 s | white/silver, slow drift, per-star 3-8 Hz blink |
| 8 | Salute | 0 | no | 0.4 s | 7 px white disc, blooms bright then fades quickly (no hard flash) |

Ground-level: **mine** (fan of 8 short tailed stars from bottom edge, no rise),
**comet** (rise + fade, no burst) as punctuation, and **fountain** (continuous
gold spark spray from a fixed bottom-edge point, ~3 s) — opens the show and
bridges phase seams so the sky is never dead outside the lull/stop.

## Colour doctrine

Real chemistry palette: red (strontium), green (barium), amber/gold (sodium),
white/silver (magnesium), purple and blue as rare accents (blue reads badly —
used sparingly). One colour per shell, two max (pistil). Volleys use
complementary pairs (red+green, purple+amber). Golden interlude is gold/amber
only. Finale converges on gold + white.

## Show programme (~75 s loop)

| Phase | Dur | Density | Content |
|-------|-----|---------|---------|
| OPENER | 6 s | 3-4 launches in first 2 s, then 1/1.5 s | comets + mines, then red/gold/silver peonies & dahlias |
| VOLLEYS | 15 s | 1 per 2-3 s, occasional mirrored 2-shell volley | peonies, chrysanthemums, one ring |
| GOLD | 14 s | 1 per 4-5 s, at most two alight, staggered | willows solo centre-sky, palms, ground comets |
| RHYTHM | 15 s | metronomic 1 per 1.7 s alternating low mine / high break | crossettes, rings, strobe, dahlias |
| LULL | 5 s | 2.5 s black sky, then a single slow gold willow | suspense before finale |
| FINALE | 14 s | ramp 1/s → 3/s, full width, cap lifted | dahlias + chrysanthemums + willows stacked, last 2 s salutes only |
| STOP | 6 s | nothing | hard cut, embers fade 2-3 s, dark, loop restarts |

Concurrency cap outside finale: max 2 simultaneous bursts, centres ≥ 20 px apart.
Scheduler cues **break time**, working launch time backwards (professional
"appear time" practice) so breaks land on the intended rhythm.

## Wiring (id 28)

- `AllEffects_FastLED.ino`: `#include "fireworks.h"`, `case 28:` in 50 ms block,
  slider label `"Pattern (0-23)"`, max 1311.
- `effectChanging.h`: append 28 before trailing 13 in all three arrays; cap 23.
- `viewer.html`: add "fireworks" to NAMES, clamp 23, slider max 1311,
  `SLIDER_NAME 'Pattern (0-23)'`.
- Mirror everything to `.sim\AllEffects_FastLED\`.

## Verification

WASM sim compile (`fastled .sim\AllEffects_FastLED --just-compile --no-interactive`,
touch `.ino` mtime first for header-only edits), then `.sim\run-viewer.ps1` and
watch at `http://127.0.0.1:8200/viewer.html`, slider notch 22 (effect 28).
Iterative visual review loop: watch full 75 s programme, critique against this
spec (phase legibility, burst readability, trail differentiation, finale impact,
hard stop), refine constants, repeat.

## Out of scope (YAGNI)

Sound sync, user-configurable programmes, fish/swimmers wobble (stretch goal),
per-lane brightness compensation.
