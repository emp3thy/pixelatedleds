# Light-Cycle Race — `lightCycleRace` Effect

**Date:** 2026-06-29
**Status:** Approved design
**Target:** FastLED 60×60 LED matrix (Teensy 4.0; WASM sim for preview)

## Summary

A Tron-style one-on-one light-cycle race: a **red** and a **blue** AI cycle ride
the 60×60 arena leaving solid coloured trails. Each crashes when its next cell is
the border, its own trail, or the opponent's trail. The survivor wins; their
trail **flashes** for a moment, the arena clears after a short pause, and a new
round starts automatically. Loops forever. The AI plays an **aggressive cut-off**
style — it tries to cut across / box in the opponent while staying alive.

Self-contained header `lightCycleRace.h`, same convention as `oceanSunrise.h` /
`farmhouseSeasons.h` (module-static state, draw into `leds[XY(x,y)]`,
`FastLED.show()` at the end). It is a **stateful simulation**, not a per-frame
recompute: the occupancy grid and cycle state persist across frames.

## Arena & coordinates

- 60×60, addressed via `XY(x, y)`; `(0,0)` top-left.
- A 1-pixel **border wall** around the edge (rows/cols 0 and 59).
- Playable interior is x,y ∈ [1, 58].
- Background is a lifted dark navy `#0A0E1A` (never pure black — emissive rule).

## State (module-static, Teensy-friendly)

- `uint8_t lcGrid[NUM_LEDS]` — occupancy: `0` empty, `1` red trail, `2` blue
  trail, `3` wall. ~3.6 KB, trivial on Teensy 4.0.
- Two `LcCycle { int8_t x,y,dx,dy; bool alive; }` — red and blue.
- `uint8_t lcPhase` — `RACING`, `FLASH`, `PAUSE`.
- `uint32_t lcPhaseStart` (millis) and `uint16_t lcStep` counters for timing.
- `uint32_t lcRng` — xorshift PRNG state, re-seeded each round from a round
  counter (deterministic per round, varied across rounds).
- `uint8_t lcWinner` — 0 none/draw, 1 red, 2 blue (for the flash colour).

## Directions

Four directions only (no diagonals). `dx,dy ∈ {(1,0),(-1,0),(0,1),(0,-1)}`.
"No reverse": a cycle may go straight, turn left, or turn right relative to its
current heading, never 180°.

## AI — aggressive cut-off (per step, per cycle)

1. Build the 3 candidate moves (straight, left-turn, right-turn) from the current
   heading.
2. **Safety filter:** a candidate is *safe* if the destination cell is in-bounds
   and `lcGrid` there is empty (not wall/trail). Drop unsafe candidates.
3. If no safe candidate → the cycle **crashes** this step.
4. If one safe → take it.
5. If multiple safe → **score** each and take the best:
   - **Aggression:** prefer the move whose destination is closer to an
     *interception point* a few cells **ahead of the opponent** along the
     opponent's heading (`opp.x + opp.dx*K, opp.y + opp.dy*K`, K≈6). Lower
     Manhattan distance to that point = higher score.
   - **Breathing room:** add a small bonus for the move with more empty cells in
     a short look-ahead straight line (avoid driving into a near-dead-end the
     safety filter alone wouldn't catch).
   - **Anti-stall jitter:** add a tiny RNG term so symmetric situations don't
     produce identical mirrored play every round.
   - Weighted sum; highest wins (ties broken by straight > turn, then RNG).
6. Apply the chosen move: update heading, advance, mark the **new** cell in
   `lcGrid` with the cycle's colour.

Both cycles decide from the **same pre-step grid snapshot** within a step (read
positions before either moves) so neither gets an unfair within-step advantage;
crashes are evaluated against the grid as it stands plus the other cycle's chosen
destination (head-on into the same cell = both crash = draw).

## Round lifecycle (phase machine)

- **RACING:** every `LC_STEP_MS` (~70 ms) advance one step. Detect crashes:
  - one crashes → `lcWinner` = other, go to **FLASH**.
  - both crash same step (mutual / same target cell) → `lcWinner = 0` (draw),
    go to **FLASH** (both trails flash white briefly).
- **FLASH:** ~2 s. The winner's trail pulses bright (blink between full and dim
  on a ~200 ms cycle); the loser's trail is dimmed to ~30%. Draw → both pulse
  white-ish.
- **PAUSE:** ~0.5 s blank arena (just border + background).
- Then **reset:** clear `lcGrid` to walls+empty, increment round counter, reseed
  RNG, place the two cycles at fresh start positions, phase = RACING.

## Start positions

Cycles start on opposite sides facing each other, **row-offset** so they don't
immediately head-on:
- Red at `(8, rowR)` heading **right** `(+1,0)`.
- Blue at `(51, rowB)` heading **left** `(-1,0)`.
- `rowR` and `rowB` chosen per round from the RNG within `[12, 47]`, with
  `rowR != rowB` (offset ≥ 4) for variety.
Each start cell is marked in the grid immediately.

## Rendering (each frame)

Draw order into `leds[]`:
1. Background fill `#0A0E1A`.
2. Border walls `#2A3040` (the `lcGrid==3` cells).
3. Trails: red cells `#E03030`, blue cells `#3060E0`. In FLASH, apply the
   pulse/dim per the winner.
4. **Heads** (only in RACING): each live cycle's current cell drawn brighter —
   a coloured glow (`#FF7060` red / `#6090FF` blue) with a near-white core, so
   the leaders read clearly against their trails.
5. `FastLED.show()`.

(The diffusing PETG on the real panel softens everything; the sim's Diffusion
slider previews it.)

## Colours

| Element | Colour |
|---|---|
| Background | `#0A0E1A` |
| Border wall | `#2A3040` |
| Red trail / head / core | `#E03030` / `#FF7060` / `#FFE0D8` |
| Blue trail / head / core | `#3060E0` / `#6090FF` / `#D8E4FF` |
| Draw flash | white-ish `#E8ECF4` |

## Timing constants (tunable `#define`s)

- `LC_STEP_MS 70` — race step interval.
- `LC_FLASH_MS 2000`, `LC_FLASH_BLINK_MS 200`.
- `LC_PAUSE_MS 500`.
- `LC_INTERCEPT_K 6` — how far ahead of the opponent to aim.

## Wiring (id 27 — follows the PR #5 lessons)

- New file `AllEffects_FastLED/lightCycleRace.h` (+ mirror to `.sim`).
- `AllEffects_FastLED.ino`: `#include` after `farmhouseSeasons.h`; `case 27:
  lightCycleRace();` in the 50 ms switch; bump sim UISlider **max 1197 → 1254**
  and label `"Pattern (0-21)" → "Pattern (0-22)"`.
- `effectChanging.h`: append `27` before the trailing `13` in **all THREE**
  arrays (`listOfPatternsForRectangularMatrix`, `listOfPatternsForSquareMatrix`,
  **and** `listOfPatternsForSimpleLedStrip`), keeping all three length 23; raise
  the cap `if(result>21) → 22`.
- `viewer.html` (root **and** `.sim`): add `'light cycle race'` to `NAMES`
  before `'jusBlack (off)'`; `idx = Math.min(22, …)`; slider `max="1254"`; row
  end-label `21 → 22`; `SLIDER_NAME = 'Pattern (0-22)'` (must match the `.ino`).

## Verification

Visual in the WASM sim (no unit tests for effects), plus the AI/PRNG helpers are
pure and can be eyeballed. The Teensy `arduino-cli` CI compile must pass.

## Assumptions / defaults (flag to change)

- 4-direction movement, no diagonals; no reverse.
- Both cycles AI (no input); aggressive cut-off style.
- Flash winner → pause → new round; loops forever.
- 1 cell/step at ~70 ms (≈14 cells/s).
- Border is a hard wall (crash), not wrap-around.

## Out of scope (YAGNI)

- Player control / buttons.
- Score display, round counter on screen.
- Power-ups, speed boosts, multiple cycles (>2).
- Diagonal movement or variable speed.
