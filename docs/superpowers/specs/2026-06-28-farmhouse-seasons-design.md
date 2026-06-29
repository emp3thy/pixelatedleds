# Farmhouse Seasons — `farmhouseSeasons` Effect

**Date:** 2026-06-28
**Status:** Approved design (brainstorm)
**Target:** FastLED 60×60 LED matrix (Teensy 4.0 hardware; WASM sim for preview)

## Summary

A still farmstead scene that plays one full year on a slow loop (~4 minutes):
a stone wall with a wooden gate in the foreground, a curvy path leading to the
gate, a hero tree, a red gambrel barn, a large wheat field, and two rolling
wooded hills under a sun-arced sky. Nothing in the scene moves through space —
the drama is **colour and state change** as the seasons turn: the wheat field
goes plowed-earth → green → gold → stubble → snow → earth; the trees leaf,
turn, and shed; the hero tree blossoms in spring; flowers bloom and fade in the
foreground; the sun rides high in summer and low in winter; snow falls to start
winter and melts to start spring.

This is the same family as `oceanSunrise`: a **layered compositor**, drawn
back-to-front, each layer a self-contained function of the year-phase.

## Geometry

- Canvas: 60 columns × 60 rows, addressed via `XY(x, y)`. `y = 0` top.
- Approximate vertical bands (back → front):
  - **Sky** rows ~0–18 (varies; hills rise into it).
  - **Hills + wood** domes peaking ~row 8, meeting the field at a **rolling
    ground line** that undulates ~rows 21–27 (not a flat horizon).
  - **Wheat field** from the ground line down to ~row 46, with a thin **grass
    margin** (2–4 rows) between the treeline and the crop.
  - **Barn** ~rows 31–46, right of centre (~x34–51); **hero tree** ~rows 31–47,
    left (~x10–15).
  - **Stone wall** rows ~46–50, full width, two courses, with a **wooden gate**
    gap at ~x27–32.
  - **Foreground grass** rows ~50–59, with the **path** curving up to the gate
    and **flowers** dotted in.

Exact coordinates are tuning details; the prototype values above come from the
approved 60×60 mockups.

## Rendering technique (important)

Draw the scene as **vector shapes** (arcs, curves, polygons, gradients) sampled
into the 60×60 buffer with **coverage anti-aliasing**, rather than hard pixel
blocks. Curved/diagonal edges (sun, path, grass↔wheat boundary, hill contour,
tree canopies) then get soft sub-pixel edges "for free", exactly like the sun.
The physical panel's translucent diffuser blends neighbours further, so the
final look is soft, not blocky. The sim preview should mimic this (smooth
upscale + slight blur) so design intent matches hardware.

## Cycle & timing

- One year `T ≈ 240 s` (4 min; tunable 180–300). `phase = frac(millis()/T_ms)`
  ∈ [0,1), four 60 s seasons.
- Each season = a **hold** (steady state) plus a **transition** into the next.
  Rough split per season: ~40% hold, ~60% transition (tunable). Transitions are
  continuous crossfades/morphs — no hard switches.
- Season order: **Spring → Summer → Autumn → Winter → (Spring)**.

## Layer architecture (back → front)

Each layer is an independent helper taking the shared cycle state. Drawn in
order so later layers composite over earlier ones.

1. **Sky** — gradient + sun + (optional clouds).
2. **Hills + wood** — hill grass and the tree mass on the slopes.
3. **Field** — grass margin + wheat crop.
4. **Barn** — its own small layer (in front of field, behind wall).
5. **Wall + hero tree** — stone wall, gate, and the foreground hero tree.
6. **Foreground** — path, grass, flowers, wheat ears.
7. **Weather particles** — falling leaves / snow / blossom, on the very top.

Two cross-cutting helpers sampled by multiple layers:

- **`sunHeight(phase)`** and **sky palette crossfade** (see Sky).
- **`snowCoverage(x, y, phase)`** — accumulation value in [0,1] each ground/
  object layer samples to lay snow correctly (roof caps vs ground vs wall top).
  Driven by the winter snowfall and cleared by the spring melt. **The path is
  excluded** (stays visible all winter).

## Per-element seasonal states & transitions

### Sky (layer 1)

- Four approved palettes (spring soft blue, summer bright blue, autumn hazy
  paler, winter pale grey), **continuously crossfading** with `phase`.
- **Sun: height-only arc.** A single smooth curve: rises spring → **summer
  (highest)** → drops slightly into autumn → keeps dropping to **mid-winter
  (lowest)** → starts rising again in the back half of winter → into spring.
  Sun disc also slightly larger/warmer in summer, smaller/paler in winter.
- Clouds optional (drifting, tinted to the sky) — YAGNI for v1 unless cheap.

### Hills + wood (layer 2)

- **Hill grass:** green (summer) → drier olive (autumn) → snow (winter) → fresh
  green (spring). Hills meet the field on the undulating ground line.
- **Wood (hill trees):** many small individual trees, each a canopy on a short
  trunk, **staggered at different heights** up the slope, the canopy following
  each dome (hills still read; open flanks). Lifecycle:
  - Winter: **bare trunks** (+ snow on/around them).
  - Spring: **leaf-out** — canopy fills bare → fresh light green (**no blossom**
    on hill trees).
  - Summer: full green, varied shades (same palette as the hero tree).
  - Autumn: **staggered turn** green → yellow/orange/red (each tree drifts to a
    slightly different shade so the hillside turns as a mass), then **leaves
    fall** (feeds the autumn particle layer) thinning canopy back toward bare.

### Field (layer 3)

The big mid-ground block; the headline seasonal element. A thin **grass margin**
sits between the treeline and the crop (wheat never touches the trees), with a
**smooth anti-aliased** grass↔crop boundary.

**Hard constraint: the field tone is always visibly distinct from the grass
tone, every phase — including winter** (see below).

- **Spring:** bare **plowed earth** — dark brown with furrow stripes (crop
  planted, not yet grown).
- **Spring → Summer:** **green grows in** — plants emerge and fill the soil with
  green (speckled per-cell emergence, earth → green crop).
- **Summer:** **green wheat**, striped, kept clearly **lighter/yellower than the
  grass**.
- **Summer → Autumn:** **ripen** — green sweeps to gold in place.
- **Autumn:** **ripe gold wheat**, striped.
- **Autumn → Winter:** **harvest** cuts gold to bright pale **stubble**, then
  **snowfall** accumulates white over the stubble (see Snow model).
- **Winter:** snow-covered. To stay distinct from snowy grass, **snow-on-field**
  is slightly cooler/flatter with **faint furrow shadows** (plow rows read under
  thin snow); **snow-on-grass** is brighter/cleaner with a faint warm/green
  undertone.
- **Winter → Spring:** **melt** clears snow back to bare plowed earth, ready to
  re-plant; green returns to the grassy areas first.

### Barn (layer 4)

- Constant **red gambrel barn**: pale gambrel roof (shallow upper + steep lower
  slope = inverted-bell), red board walls, big dark **open doorway**, small
  hayloft opening in the peak. ~18×15 px. No cupola/weathervane/X-trim/windows
  (sub-pixel at 60×60).
- Winter: **snow cap** on the roof (via `snowCoverage`).

### Wall + hero tree (layer 5)

- **Stone wall:** two courses, full width, light cap line, mortar line between
  courses. Constant; **snow caps** the top in winter.
- **Wooden gate:** in a gap (~x27–32) — two posts, panel, two rails. Constant.
- **Hero tree:** the crisp close-up — trunk with a cluster of overlapping
  leaf-clumps (multi-shade), foliage sitting **on** the trunk. Same lifecycle as
  the hill wood **except it blossoms in spring**:
  - Winter: bare branches (+ snow).
  - Spring: **blossom** (pink/white) flush first, then leaf-out to fresh green.
    Blossom drift feeds the spring particle layer.
  - Summer: full multi-shade green.
  - Autumn: multi-shade turn (yellow/orange/red), then leaf fall to bare.

### Foreground (layer 6)

- **Path:** a soft, **curvy** dirt path meandering from the gate down to the
  front, widening toward the viewer (perspective), drawn as a filled curve so
  edges fuzz like the sun. **Always visible** — stays clear through winter.
- **Grass:** foreground grass band. Green in summer/spring, snow-dusted in
  winter (path excluded), drier in autumn.
- **Flowers (foreground only):** small dots (white / yellow / pink). Lifecycle:
  none in winter → **bud & bloom in spring** (dots fade in and multiply) →
  **peak bloom in summer** (most, brightest, gentle per-flower twinkle) →
  **fade/die back in autumn** (fewer, duller) → gone before snow. The grass
  margin under the hill trees stays **plain grass** (no flowers there).
- **Wheat ears:** a few tall ears poking up in the bottom corners, **season-
  coloured** — green in summer, gold in autumn; absent in spring (just planted)
  and winter (harvested/under snow).

### Weather particles (layer 7, top)

- **Autumn:** falling leaves (tinted to the turning canopy) drifting down — also
  the visual mechanism for the trees shedding to bare.
- **Winter:** **falling snow** — this is the **trigger that starts winter**:
  snow begins falling after harvest and drives `snowCoverage` accumulation over
  the ground/objects (path excluded). Continues lightly through winter.
- **Spring:** drifting **blossom** petals from the hero tree.

Particle layers reuse the existing scrolling/particle pattern (see `rain.h`).

## Snow model

- `snowCoverage(x, y, phase)` ∈ [0,1]: how much snow has accumulated at a cell.
- Autumn→winter: the **falling snow** raises coverage over time until the ground/
  hills/roofs/wall are white. Each object layer samples it to place snow
  correctly (e.g. roof and wall-top accumulate; vertical faces less).
- **Path is excluded** from accumulation — visible all winter.
- Winter→spring: **melt** lowers coverage back to 0; grassy areas green first,
  the field returns to bare plowed earth.

## Data / state (module-static, Teensy-friendly)

- `float` year phase from `millis()`; previous-phase tracker for wrap detect.
- Fixed tables generated once: hill-tree positions (x, height-on-slope, shade
  seed), hero-tree clump layout, flower positions/seeds, wheat-ear positions,
  star-free (daytime scene).
- Particle pools for leaves / snow / blossom (small fixed-size arrays).
- No large heap allocations; `leds[]` = 3600×3 B ≈ 10.8 KB is trivial on Teensy
  4.0 (~1 MB RAM, FPU available).

## Wiring (follows `oceanSunrise`)

- New file `AllEffects_FastLED/farmhouseSeasons.h` (+ mirrored `.sim` copy).
- `#include` in `AllEffects_FastLED.ino`; add a `case` in a ~30–50 ms loop block
  calling `farmhouseSeasons()`.
- Add as the next effect index after `oceanSunrise` (currently 25). Append the
  new index to both `listOfPatternsForRectangularMatrix` and
  `listOfPatternsForSquareMatrix` in `effectChanging.h` **before** the trailing
  `13` (jusBlack), so black stays last. Bump table length / UI slot count and
  clamp.
- In `viewer.html`, add the label `'farmhouse seasons'` to `NAMES` before
  `'jusBlack (off)'`; extend the slider `max` and index clamp.
- Keep root and `.sim` copies in sync; rebuild via touching the `.ino` then
  `fastled .sim\AllEffects_FastLED --just-compile`.

## Performance

Full-canvas float compositor across ~7 layers over 3600 pixels per frame, in
line with the existing float-heavy effects (voronoi, aurora, ocean) which run
60–140 fps in the sim. Teensy 4.0 FPU handles it; if hardware fps dips, throttle
the particle/clouds layers or cache static layers (sky/hills) between frames
since they change slowly.

## Assumptions / defaults (flag any to change)

- Cycle period 240 s; 4 equal 60 s seasons; ~40/60 hold/transition split.
- Sun is **height-only** (no left-right arc); fixed x near the right.
- Hill trees do **not** blossom; only the hero tree does (spring).
- Flowers are **foreground only**; white/yellow/pink.
- Snow arrives by **falling/accumulating** (not a sideways wipe); path stays
  clear; melt clears the rest.
- Clouds are optional / deferred.
- Daytime scene only — no day/night within the year (sun never sets).

## Out of scope (YAGNI for v1)

- Day/night cycle within the year.
- Animals, people, vehicles, smoke from the barn.
- Rain/lightning/wind weather beyond the three seasonal particle types.
- Runtime-tunable cycle length (compile-time constant for now).
- Interactive time-of-year control (auto-runs).

## Open tunables (set during implementation)

- Cycle period `T`, hold/transition split per season.
- Sun arc shape (peak/trough heights), disc size/colour per season.
- Exact season palettes and crossfade control points.
- Field grow speckle vs bottom-up; ripen sweep direction; harvest cut direction.
- Snowfall rate, accumulation curve, melt rate; minimum field/grass winter
  contrast.
- Flower density curve (spring→summer→autumn), colours, twinkle.
- Particle counts/speeds for leaves, snow, blossom.
