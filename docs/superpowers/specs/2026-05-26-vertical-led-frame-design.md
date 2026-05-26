# Vertical-Strip LED Frame — Design Spec

**Status:** Draft for review
**Author:** brainstorming session 2026-05-26
**Related:** Tower Lamp v45 (Fusion 360), PixelatedLights firmware (GitHub `emp3thy/pixelatedleds`)

---

## 1. Goal

Add a second LED-frame design to the Tower Lamp that holds **vertical** LED strips instead of horizontal rings. Combined with the existing modular shell stack, this lets the user build a tower of arbitrary height by stacking more frames, with LED strips running continuously top-to-bottom of the whole tower (cut to length, snake-wired).

The new frame is a **drop-in alternative** to the existing horizontal-ring LED Frame: same outer envelope (160 × 160 × 48 mm), same stacking peg/hole interface, but a fundamentally different internal layout.

## 2. Architecture

A single 3D-printed **Vertical LED Frame** module. Square ring, hollow inner cavity, four outer walls each carrying a row of vertical channels for LED strips. Frames stack via four corner pegs.

```
┌────────────────────────────────────────┐  ← outer translucent shell
│                                        │     (existing — handles diffusion)
│  ┌──────────────────────────────────┐  │
│  │                                  │  │  ← 18 mm air gap (existing geometry)
│  │  ▓▓▓▓▓▓▓ ▓▓▓▓ ▓▓▓▓ ▓▓▓▓ ▓▓▓▓▓▓▓ │  │
│  │  ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ │  │  ← LED strips in channels
│  │  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │  │     (vertical, recessed flush)
│  │  └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ │  │
│  │            FRAME WALL (8 mm)      │  │
│  │  ┌────────────────────────────┐  │  │
│  │  │     INNER CAVITY (empty)   │  │  │
│  │  │      144 × 144 mm          │  │  │
│  │  │      open top + bottom     │  │  │
│  │  └────────────────────────────┘  │  │
│  └──────────────────────────────────┘  │
│                                        │
└────────────────────────────────────────┘
```

- Strips are continuous, top-to-bottom of the entire tower (not divided per frame)
- Frames are pure mechanical housing; no electrical features inside
- Snake jumpers + power injection live in the existing Shell Bottom / TopShell cavities at the very base + top of the tower

## 3. Outer dimensions

| Dimension | Value | Source |
|---|---|---|
| Outer footprint X / Z | 160 × 160 mm | match existing LED Frame |
| Frame height (Y) | 48 mm | match existing LED Frame |
| Outer wall thickness | 8 mm | gives 2 mm channel + 6 mm material backing strip |
| Inner cavity | 144 × 144 mm | hollow, open top and bottom |

## 4. Strip channels (outer wall)

| Item | Value |
|---|---|
| Strips per side | 11 |
| Total strips around perimeter | 44 |
| Slot width (X along wall) | 11 mm (10 mm strip + 1 mm wiggle room) |
| Slot depth (Z into wall) | 2 mm (= strip thickness; LED face sits flush with outer wall plane) |
| Slot height (Y) | 48 mm (full frame height — open top + bottom so strip passes through) |
| Slot pitch | 14.25 mm |
| Gap (between slots, and corner to first/last slot) | 3.25 mm (uniform, exact fit on 160 mm side) |
| Channel floor finish | Flat (strip's 3M adhesive backing bonds to floor) |

Slot positions on each side (measured from outer corner along the wall):
- Slot 1: 3.25–14.25 mm
- Slot 2: 17.5–28.5 mm
- Slot 3: 31.75–42.75 mm
- … (14.25 mm pitch)
- Slot 11: 145.75–156.75 mm
- (3.25 mm to opposite corner)

## 5. Stacking interface (peg/hole)

Four pegs on the bottom face of each frame, four matching holes on the top face. Frame N's pegs drop into Frame N-1's holes when stacked.

| Feature | Hole (nominal) | Peg (shrunk by 0.1 mm/face) |
|---|---|---|
| Width (X) | 10.0 mm | 9.8 mm |
| Depth (Z, into wall) | 3.0 mm | 2.8 mm |
| Height (Y, protrusion into hole) | 5.0 mm | 4.9 mm |

Position of each peg/hole (top-down, measured inward from outer corner):
- Centred at (X, Z) = (12.5, 5.5) mm from outer corner (peg X extends 7.5–17.5 mm, Z extends 4–7 mm)
- Mirrored to all 4 corners

This position gives a **2 mm clearance** between the peg's outer face (Z = 4 mm) and the slot 1 channel floor (Z = 2 mm). No mechanical interference between pegs and strip slots.

**Existing LED Frame must be updated** to use the same peg/hole dimensions so both frame types can stack interchangeably. The existing peg shape was approximately 10 × 4.5 mm at Z = 1.5–6 mm; the new shared spec is 10 × 3 mm at Z = 4–7 mm (peg pulled inward + thinned).

## 6. LED strip + wiring (information for assembly)

- **Strip:** WS2811 / WS2812B, 60 LEDs/m, 10 mm IP30 (bare PCB with 3M adhesive backing), 16.67 mm LED pitch.
- **Strip length per pillar:** equal to the full tower height (number of frames × 48 mm). Cut at LED-cut markers.
- **LEDs per strip:** ⌊tower_height_mm / 16.67⌋.
- **Total LEDs:** 44 strips × LEDs-per-strip. For 5 frames (240 mm) → 14 LEDs/strip × 44 = 616 LEDs. For 10 frames (480 mm) → 28 × 44 = 1232 LEDs.
- **Snake wiring:** at tower base (or top), connect end of strip N to start of strip N+1 with short jumper. Direction alternates (strip 1 bottom→top, strip 2 top→bottom, …) — standard serpentine.
- **Power injection:** parallel +5 V and GND rail at the tower base; tap into the start of each strip's +5 V and GND so voltage drop along the snake doesn't dim distant strips.
- **Wiring housing:** all jumpers + power rails fit inside the existing Shell Bottom cavity at the base. (TopShell stays empty unless user wants to also inject at the top.)

## 7. Firmware impact

The existing firmware (`AllEffects_FastLED`) defines `NUM_COLS` in `AllEffects_FastLED/configuration.h`. For the vertical-strip tower change it to `NUM_COLS = 44` (44 columns = 44 strips). `NUM_ROWS` changes from the current horizontal-ring value (6) to the per-strip LED count, which varies with tower height. Recalculate `NUM_LEDS` accordingly (`NUM_COLS × NUM_ROWS`).

Existing serpentine `XY(x, y)` mapping in `XYMatrix.h` already supports any column count; only the `#define` in `configuration.h` needs updating.

Effects that depend on row/column count are unaffected (they iterate over `NUM_COLS` / `NUM_ROWS`).

## 8. Material + print

- **Material:** PETG (opaque) for frame. (Translucent PETG for outer shell — unchanged.)
- **Print orientation:** frame lies on bed with the TOP face (recess-hole side) DOWN. Holes print as first-layer cavities (no overhangs — they're just material missing from the first few layers). Pegs print last on the top of the print as 5 mm tall protrusions. Channels on the outer walls run vertically along the print Z axis — no overhangs, no supports.
- **Supports:** rectangular pegs + holes need no overhang supports. Channels are open vertical slots — no overhangs.

## 9. Component boundaries

Each unit, what it does, what it depends on:

| Component | Responsibility | Depends on |
|---|---|---|
| **Vertical LED Frame** (new) | Holds 44 LED strips in vertical channels on its outer wall. Stacks vertically via 4 corner pegs/holes. | LED strip dim (10 mm IP30); existing outer envelope (160 × 160 × 48); existing peg/hole interface (after sync update). |
| **Existing LED Frame** (modified) | Same as today (horizontal LED rings) but with updated peg/hole dimensions to match Vertical Frame. | New peg/hole spec only. |
| **Outer translucent shell** | Light diffusion. | Existing geometry — no change. |
| **Shell Bottom + Base** | Houses snake-wiring jumpers + power injection. | No new geometry — wiring is hand-built inside the existing cavity. |
| **Top Shell** | Caps the top of the tower. Optional secondary power injection. | Existing geometry — no change. |
| **Firmware** | Drives 44 columns × N rows in serpentine pattern. | Changed `#define NUM_COLS 44`. |

## 10. Error/edge handling at assembly

| Risk | Mitigation |
|---|---|
| LED strip jams in slot during install | 1 mm wiggle room (slot 11 mm > strip 10 mm). |
| Strip lifts off channel floor at corners (peg conflict) | 2 mm clearance between peg outer face and slot floor — verified during design. |
| Frames mis-align when stacked | 4 corner peg/hole engagement (5 mm depth) gives positive lateral alignment + rotational lock. |
| Strip cannot pass through stacked frames | Channels open top + bottom; all aligned via corner pegs. Verify channel alignment in test print of 2 stacked frames before scaling. |
| Voltage drop dims top of long tower | Power injection rail at base, tapped per strip. Optional second injection at top. |
| Wrong `NUM_COLS` in firmware | Build script compile error if mismatch (FastLED array bounds checked at compile time when buffers sized from `NUM_COLS`). |
| 3D print peg sticks in hole | Peg dimensioned 0.1 mm undersize on every face vs hole (standard 3D-print slip fit). |

## 11. Testing / acceptance

- **Print one Vertical LED Frame.** Inspect: slot dims, peg dims, hole dims (calipers). Slot 11 mm ± 0.2 mm. Pegs 9.8 ± 0.1, 2.8 ± 0.1, 4.9 ± 0.1.
- **Print two and stack them.** Verify pegs fit holes with slight slop, no force. Frames sit flush. No rotation.
- **Insert one LED strip into slot 1 of stacked pair.** Confirm strip slides through both frames' aligned channels. Confirm adhesive backing sticks.
- **Insert at all 11 slots on one side.** Confirm even spacing visually; no slot conflicts with peg/corner.
- **Repeat for all 4 sides.** Confirm 44 total slots.
- **Mount stacked pair in existing Shell Bottom + Top Shell.** Confirm outer shell fits over frame stack without interference.
- **Wire up snake + power injection** at base. Power on, send single-pixel test (chase) through all 44 columns. Confirm pixel order matches expected snake direction in firmware.
- **Full effect test** with PixelatedLights firmware after `NUM_COLS` set to 44.

## 12. What's NOT in this spec (deferred)

- Detailed wiring diagram of snake + injection (depends on user's preference for injection points)
- End-cap frames at very top/bottom (not needed — existing Shell Bottom + Top Shell serve)
- Heat dissipation analysis (not expected to matter at LED power levels; cavity is open for natural convection)
- Firmware tuning (effect parameters may want to adapt to new NUM_COLS=44)
- Cosmetic chamfers/fillets on the frame (left to taste during CAD implementation)

## 13. Open questions

None at present. Ready for implementation plan.
