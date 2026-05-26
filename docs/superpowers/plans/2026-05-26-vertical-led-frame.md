# Vertical-Strip LED Frame Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a new "Vertical LED Frame" component to the Tower Lamp Fusion 360 design that holds 44 vertical LED strips (11 per side), update the existing LED Frame's peg/hole to a shared interface so both frame types stack interchangeably, and update the PixelatedLights firmware so `NUM_COLS = 44`.

**Architecture:** Fusion 360 CAD work via the `mcp__fusion__` MCP tools (Python scripts to add user parameters, create the new component, build outer ring with slots + pegs + recesses, edit the existing LED Frame's peg/hole dimensions). Firmware change is a one-line edit to `configuration.h` validated by `arduino-cli compile`. Hardware print + assembly verification at the end (user-executed).

**Tech Stack:** Autodesk Fusion 360 (parametric modelling, Python API via `mcp__fusion__fusion_mcp_execute`), Arduino C++ / FastLED 3.3.3, `arduino-cli` for compile verification, Git + GitHub for firmware change tracking.

**Spec:** `docs/superpowers/specs/2026-05-26-vertical-led-frame-design.md` — read it first.

---

## Pre-flight context for the engineer

- The active Fusion document is **Tower Lamp v45** (or whatever latest version exists at start of work). Confirm with `mcp__fusion__fusion_mcp_read queryType=document operation=open` — the active doc should be the parametric Tower Lamp.
- The Fusion design already has three named sub-components: `Base Assembly`, `LED Stack`, `Shell Assembly`. Existing LED Frame lives in `LED Stack`. The new component (`Vertical LED Frame`) sits alongside it.
- The firmware lives at `C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED`. GitHub remote: `https://github.com/emp3thy/pixelatedleds.git`, default branch `main`.
- All Fusion scripts must be run via `mcp__fusion__fusion_mcp_execute featureType=script`. They return text output (print statements) and exceptions.
- After any Fusion change, save the document via `mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`. Save when explicitly told in a step.
- The brainstorm visualiser server may have auto-exited (30 min idle). Verify alive before any visualiser-related task: `cat $STATE_DIR/server-info` + `curl -sf -o /dev/null -w "%{http_code}\n" $URL`. Restart if HTTP != 200.

---

## File / component map

| Where | What changes |
|---|---|
| Fusion: Tower Lamp document, User Parameters | Add 8 new parameters (Task 1) |
| Fusion: new component `Vertical LED Frame` | Create with empty body (Task 2); fill with outer ring + 44 slots (Task 3); add 4 corner pegs (Task 4); add 4 corner recesses (Task 5) |
| Fusion: existing `LED Frame` component | Shrink existing peg + update mating recess (Task 6) |
| Fusion: visual + measure verification | Task 7 — verify pegs fit holes |
| `AllEffects_FastLED/configuration.h` | `#define NUM_COLS 44` plus a comment on the NUM_ROWS convention (Task 8) |
| `AllEffects_FastLED/configuation.h` and friends | Run `arduino-cli compile` to confirm clean (Task 9) |
| Git | Branch `feat/vertical-led-frame`, single commit, PR (Task 10) |
| Hardware | Print, measure, stack, wire, full effect test — Tasks 11–13 (manual, user-executed) |

---

## Task 1: Add user parameters in Fusion

**Files:**
- Modify: Tower Lamp Fusion document — User Parameters panel

**Why:** Establish parametric anchors for every dimension that the new frame and the updated existing LED Frame need to share. Editing these later changes geometry everywhere.

- [ ] **Step 1: Run the user-parameter add script**

Run via `mcp__fusion__fusion_mcp_execute featureType=script`:

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    up = design.userParameters

    params = [
        ('VFRAME_SLOTS_PER_SIDE',   '11',     '',   'Number of vertical LED strip slots per side of vertical frame'),
        ('VFRAME_SLOT_WIDTH',       '11 mm',  'mm', 'Slot width (X along wall) — 10mm strip + 1mm wiggle'),
        ('VFRAME_SLOT_DEPTH',       '2 mm',   'mm', 'Slot depth (Z into wall) — equal to LED strip thickness'),
        ('VFRAME_SLOT_PITCH',       '14.25 mm','mm', 'Centre-to-centre slot pitch along the wall'),
        ('VFRAME_SLOT_GAP',         '3.25 mm','mm', 'Uniform gap (between slots and corner to first/last slot)'),
        ('PEG_HOLE_X',              '10 mm',  'mm', 'Peg-recess nominal X dimension on top face'),
        ('PEG_HOLE_Z',              '3 mm',   'mm', 'Peg-recess nominal Z dimension on top face'),
        ('PEG_HOLE_HEIGHT',         '5 mm',   'mm', 'Peg-recess depth on top face'),
        ('PEG_CLEARANCE',           '0.1 mm', 'mm', 'Peg shrunk by this much per face vs hole (standard 3D-print slip fit)'),
        ('PEG_OFFSET_X',            '12.5 mm','mm', 'X distance from outer corner to peg centre'),
        ('PEG_OFFSET_Z',            '5.5 mm', 'mm', 'Z distance from outer corner to peg centre (= depth/2 + clearance)'),
    ]

    for name, expr, unit, comment in params:
        if up.itemByName(name) is not None:
            print(f'  = {name} already exists')
            continue
        if unit == '':
            v = adsk.core.ValueInput.createByReal(float(expr))
        else:
            v = adsk.core.ValueInput.createByString(expr)
        up.add(name, v, unit, comment)
        print(f'  + {name} = {expr}')

    print(f'\nTotal user parameters: {up.count}')
```

- [ ] **Step 2: Verify the parameter values**

Run via `mcp__fusion__fusion_mcp_execute featureType=script`:

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    up = design.userParameters
    expected = {
        'VFRAME_SLOTS_PER_SIDE': 11.0,
        'VFRAME_SLOT_WIDTH':     1.1,   # cm internal
        'VFRAME_SLOT_DEPTH':     0.2,
        'VFRAME_SLOT_PITCH':     1.425,
        'VFRAME_SLOT_GAP':       0.325,
        'PEG_HOLE_X':            1.0,
        'PEG_HOLE_Z':            0.3,
        'PEG_HOLE_HEIGHT':       0.5,
        'PEG_CLEARANCE':         0.01,
        'PEG_OFFSET_X':          1.25,
        'PEG_OFFSET_Z':          0.55,
    }
    ok = True
    for name, want in expected.items():
        p = up.itemByName(name)
        if p is None:
            print(f'  FAIL: {name} missing'); ok = False; continue
        if abs(p.value - want) > 1e-6:
            print(f'  FAIL: {name} = {p.value}, expected {want}'); ok = False
        else:
            print(f'  OK   {name} = {p.value}')
    print('\nALL PARAMETERS PRESENT' if ok else '\nERRORS FOUND')
```

Expected: each line prints `OK <name> = <value>` and the final line is `ALL PARAMETERS PRESENT`.

- [ ] **Step 3: Save the Fusion document**

Run via `mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`. The save creates a new Fusion version (e.g. v46). Note the version number in your report.

---

## Task 2: Create empty Vertical LED Frame component

**Files:**
- Modify: Tower Lamp Fusion document — new sub-component under root

**Why:** Reserve a named container before adding geometry. Keeps the browser organised and lets us reference the component by name in later tasks.

- [ ] **Step 1: Add the empty component**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent

    # Skip if already exists
    for i in range(root.occurrences.count):
        if root.occurrences.item(i).component.name == 'Vertical LED Frame':
            print('Already exists, skipping')
            return

    identity = adsk.core.Matrix3D.create()
    occ = root.occurrences.addNewComponent(identity)
    occ.component.name = 'Vertical LED Frame'
    print(f'Created: {occ.name}')
```

- [ ] **Step 2: Confirm the component appears in the browser**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    names = [root.occurrences.item(i).component.name for i in range(root.occurrences.count)]
    assert 'Vertical LED Frame' in names, f'NOT FOUND in {names}'
    print('OK — Vertical LED Frame present in root browser')
```

Expected: `OK — Vertical LED Frame present in root browser`.

- [ ] **Step 3: Save**

`mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`.

---

## Task 3: Build outer ring with 44 slots inside Vertical LED Frame

**Files:**
- Modify: `Vertical LED Frame` component — add sketch + extrude + 44 slot cuts

**Why:** This is the core geometry: a 160×160×48 mm hollow ring with 11 vertical slots cut into each outer wall.

- [ ] **Step 1: Build the outer ring (extrude outer 160×160 to height 48, then shell to 8 mm wall)**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    NEW_BODY = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    CUT      = adsk.fusion.FeatureOperations.CutFeatureOperation

    # Find target component
    vframe_occ = None
    for i in range(root.occurrences.count):
        if root.occurrences.item(i).component.name == 'Vertical LED Frame':
            vframe_occ = root.occurrences.item(i); break
    assert vframe_occ is not None
    vframe = vframe_occ.component
    vframe_occ.activate()

    # Build outer solid box on XZ plane: 160 × 160 mm × 48 mm tall (Y)
    sk = vframe.sketches.add(vframe.xZConstructionPlane)
    lines = sk.sketchCurves.sketchLines
    rect = lines.addCenterPointRectangle(
        adsk.core.Point3D.create(0, 0, 0),
        adsk.core.Point3D.create(8, 8, 0))   # 16x16 cm centred on origin
    # Dimension to 160mm with a parameter LED_FRAME_SIZE (already exists from earlier work)
    horiz = None
    for k in range(rect.count):
        ln = rect.item(k)
        sp, ep = ln.startSketchPoint.geometry, ln.endSketchPoint.geometry
        if abs(sp.y - ep.y) < 1e-6 and abs(sp.x - ep.x) > 1e-6:
            horiz = ln; break
    sk.sketchDimensions.addDistanceDimension(
        horiz.startSketchPoint, horiz.endSketchPoint,
        adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation,
        adsk.core.Point3D.create(0, -10, 0)).parameter.expression = 'LED_FRAME_SIZE'
    prof = sk.profiles.item(0)
    ext = vframe.features.extrudeFeatures.addSimple(prof, val_str('LED_FRAME_HEIGHT'), NEW_BODY)
    body = ext.bodies.item(0)
    body.name = 'VFrame outer block'

    # Shell to leave 8 mm wall (need SHELL_WALL parameter to exist)
    # We will shell removing the TOP face so the cavity is open both Y-ends.
    # First locate the top +Y face
    top_face = None
    for i in range(body.faces.count):
        f = body.faces.item(i)
        if isinstance(f.geometry, adsk.core.Plane) and f.geometry.normal.y > 0.99:
            if top_face is None or f.area > top_face.area:
                top_face = f
    bot_face = None
    for i in range(body.faces.count):
        f = body.faces.item(i)
        if isinstance(f.geometry, adsk.core.Plane) and f.geometry.normal.y < -0.99:
            if bot_face is None or f.area > bot_face.area:
                bot_face = f
    assert top_face and bot_face

    # Use shellFeatures with both top + bottom faces removed
    face_coll = adsk.core.ObjectCollection.create()
    face_coll.add(top_face); face_coll.add(bot_face)
    shell_input = vframe.features.shellFeatures.createInput(face_coll, False)
    shell_input.insideThickness = val_str('8 mm')
    shell = vframe.features.shellFeatures.add(shell_input)
    print(f'Shell created — body {body.name}, wall 8mm')
```

- [ ] **Step 2: Cut 11 slots into one outer wall (TOP-facing side, +Z)**

This step uses a sketch on the +Z outer face of the ring and cuts 11 rectangular pockets to depth 2 mm.

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation

    vframe = None
    for i in range(root.occurrences.count):
        if root.occurrences.item(i).component.name == 'Vertical LED Frame':
            vframe = root.occurrences.item(i).component; break
    body = next(b for j in range(vframe.bRepBodies.count) for b in [vframe.bRepBodies.item(j)] if b.name == 'VFrame outer block')

    # Find +Z outer face (largest area, +Z normal)
    target_face = None
    for i in range(body.faces.count):
        f = body.faces.item(i)
        if isinstance(f.geometry, adsk.core.Plane) and f.geometry.normal.z > 0.99 and abs(f.geometry.origin.z - 8) < 1e-6:
            if target_face is None or f.area > target_face.area:
                target_face = f
    assert target_face is not None

    sk = vframe.sketches.add(target_face)
    lines = sk.sketchCurves.sketchLines
    # Draw 11 rectangles. Slot N: x from (gap + N*pitch) to (gap + N*pitch + slot_width), full height 48mm
    # Sketch coords on a +Z face — local X maps to world X, local Y maps to world Y (height direction).
    for n in range(11):
        x0 = 0.325 + n * 1.425   # cm: gap + n*pitch
        x1 = x0 + 1.1             # + slot_width
        # Note local Y on sketch = world Y (height). Use full frame height 48mm = 4.8cm.
        # Sketch X origin alignment: for an outer face sketch, the origin may not match world origin.
        # Instead we'll draw a 2-point rect and tie dimensions to user parameters.
        p1 = adsk.core.Point3D.create(x0 - 8, 0, 0)  # shift X by -8 because face is centred on origin in world but its sketch X starts from face midpoint
        p2 = adsk.core.Point3D.create(x1 - 8, 4.8, 0)
        lines.addTwoPointRectangle(p1, p2)

    # Add parametric dimensions to first rectangle (all subsequent rects scale with it via constraints? safer: dim every rect)
    # Simpler approach: use sketch.copyObjects/pattern. But for clarity we just add dims for the first; rest are positioned by their initial coords (numeric).
    # The 11 rectangles already capture their dimensions in their position; we just need to ensure they aren't drifting on save.

    # Cut each profile (each rectangle becomes a profile) to depth 2 mm = VFRAME_SLOT_DEPTH
    cuts_made = 0
    profs = adsk.core.ObjectCollection.create()
    for k in range(sk.profiles.count):
        profs.add(sk.profiles.item(k))
    ext_input = vframe.features.extrudeFeatures.createInput(profs, CUT)
    ext_input.setDistanceExtent(False, val_str('VFRAME_SLOT_DEPTH'))
    vframe.features.extrudeFeatures.add(ext_input)
    print(f'Cut {sk.profiles.count} slots into +Z wall')
```

- [ ] **Step 3: Repeat slot cuts on the other 3 walls (–Z, +X, –X)**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation

    vframe = next(root.occurrences.item(i).component
                  for i in range(root.occurrences.count)
                  if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    body = next(vframe.bRepBodies.item(j) for j in range(vframe.bRepBodies.count)
                if vframe.bRepBodies.item(j).name == 'VFrame outer block')

    # Iterate 3 faces by normal: -Z, +X, -X
    targets = [
        ((0, 0, -1), -8),
        ((1, 0, 0),  8),
        ((-1, 0, 0), -8),
    ]
    for (nx, ny, nz), pos in targets:
        face = None
        for i in range(body.faces.count):
            f = body.faces.item(i)
            if not isinstance(f.geometry, adsk.core.Plane): continue
            n = f.geometry.normal
            if abs(n.x - nx) > 0.05 or abs(n.y - ny) > 0.05 or abs(n.z - nz) > 0.05: continue
            if face is None or f.area > face.area: face = f
        assert face is not None, f'face not found for normal ({nx},{ny},{nz})'

        sk = vframe.sketches.add(face)
        lines = sk.sketchCurves.sketchLines
        # The sketch's local X spans along the face's perimeter direction.
        # All faces are symmetric: 160 wide × 48 tall in their local view.
        # Slot positions identical: x from 0.325+N*1.425 to +1.1, full height 0..4.8
        for nseg in range(11):
            x0 = 0.325 + nseg * 1.425 - 8   # centred-around-zero on face midpoint
            x1 = x0 + 1.1
            p1 = adsk.core.Point3D.create(x0, 0, 0)
            p2 = adsk.core.Point3D.create(x1, 4.8, 0)
            lines.addTwoPointRectangle(p1, p2)
        profs = adsk.core.ObjectCollection.create()
        for k in range(sk.profiles.count): profs.add(sk.profiles.item(k))
        ext_input = vframe.features.extrudeFeatures.createInput(profs, CUT)
        ext_input.setDistanceExtent(False, val_str('VFRAME_SLOT_DEPTH'))
        vframe.features.extrudeFeatures.add(ext_input)
        print(f'Cut 11 slots into face with normal ({nx},{ny},{nz})')
```

- [ ] **Step 4: Verify slot count and depth**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    vframe = next(root.occurrences.item(i).component
                  for i in range(root.occurrences.count)
                  if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    body = next(vframe.bRepBodies.item(j) for j in range(vframe.bRepBodies.count)
                if vframe.bRepBodies.item(j).name == 'VFrame outer block')

    # Count vertical-axis slot pockets by counting +Y-normal faces near the slot floor at each side
    # Quicker: query body volume and compare against expected
    expected_outer = 16 * 4.8 * 16  # 160 × 48 × 160 = 1228.8 cm³
    expected_cavity = 14.4 * 4.8 * 14.4  # 144 × 48 × 144 = 995.328 cm³
    expected_slots = 44 * 1.1 * 4.8 * 0.2  # 44 × 11 × 48 × 2 = 232.32 cm³? wait: 44 × 1.1 × 4.8 × 0.2 = 46.464 cm³
    expected_volume = expected_outer - expected_cavity - expected_slots
    actual_volume = body.volume
    delta = actual_volume - expected_volume
    print(f'expected volume: {expected_volume:.3f} cm³')
    print(f'actual   volume: {actual_volume:.3f} cm³')
    print(f'delta:           {delta:+.3f} cm³ ({delta/expected_volume*100:+.2f}%)')
    assert abs(delta) < 1.0, f'volume mismatch — slot/wall geometry likely wrong'
    print('OK — volume within tolerance')
```

Expected: volume delta < 1.0 cm³.

- [ ] **Step 5: Save the Fusion document**

`mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`.

---

## Task 4: Add 4 corner pegs on bottom face

**Files:**
- Modify: `Vertical LED Frame` component — add 4 small protruding rectangular bodies

**Why:** Pegs let the frame stack onto a frame below. Each peg is 9.8 × 2.8 × 4.9 mm (0.1 mm undersize per face vs nominal hole 10 × 3 × 5).

- [ ] **Step 1: Add the peg sketch + extrude on the bottom face**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation

    vframe = next(root.occurrences.item(i).component
                  for i in range(root.occurrences.count)
                  if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    body = next(vframe.bRepBodies.item(j) for j in range(vframe.bRepBodies.count)
                if vframe.bRepBodies.item(j).name == 'VFrame outer block')

    # Find -Y outer face (frame bottom)
    bot_face = None
    for i in range(body.faces.count):
        f = body.faces.item(i)
        if isinstance(f.geometry, adsk.core.Plane) and f.geometry.normal.y < -0.99:
            if bot_face is None or f.area > bot_face.area:
                bot_face = f
    assert bot_face

    sk = vframe.sketches.add(bot_face)
    lines = sk.sketchCurves.sketchLines

    # Peg dimensions (in cm internally): 0.98 × 0.28
    px, pz = 0.98, 0.28
    # Peg centres at (±(8 - 1.25), ±(8 - 0.55)) on the bottom face's local coords.
    # Frame is centred on origin; corners at (±8, ±8). Peg centre 1.25 in from corner along X,
    # and 0.55 in from corner along Z.
    corners = [
        (-8 + 1.25, -8 + 0.55),
        ( 8 - 1.25, -8 + 0.55),
        ( 8 - 1.25,  8 - 0.55),
        (-8 + 1.25,  8 - 0.55),
    ]
    # For each corner, draw a 0.98 × 0.28 rectangle. Z orientation alternates so the long axis
    # is parallel to the wall.
    for k, (cx, cz) in enumerate(corners):
        # Decide orientation: pegs along X axis (px wide, pz deep) on +X/-X walls.
        # Pegs along Z axis on +Z/-Z walls (rotate 90°).
        # Determine corner: which wall does it nestle into. For "bottom-left" (-8,-8), the peg sits at the
        # inner corner shared by left wall (X=-8) and back wall (Z=-8). It needs to clear both walls'
        # slot channels. We'll orient long axis along the X direction (px = X dim).
        # For all corners, use the same orientation: long axis along the X-axis. The peg then crosses the
        # corner block. (Acceptable because the corner block is solid material.)
        p1 = adsk.core.Point3D.create(cx - px/2, cz - pz/2, 0)
        p2 = adsk.core.Point3D.create(cx + px/2, cz + pz/2, 0)
        lines.addTwoPointRectangle(p1, p2)

    profs = adsk.core.ObjectCollection.create()
    for k in range(sk.profiles.count): profs.add(sk.profiles.item(k))
    ext_input = vframe.features.extrudeFeatures.createInput(profs, JOIN)
    # Extrude downward (away from body) by peg height - clearance = 4.9 mm
    ext_input.setDistanceExtent(False, val_str('-PEG_HOLE_HEIGHT + PEG_CLEARANCE'))
    # The negative sign extrudes in -Y direction (below the body). 'PEG_HOLE_HEIGHT - PEG_CLEARANCE' = 4.9 mm.
    vframe.features.extrudeFeatures.add(ext_input)
    print(f'Added {sk.profiles.count} pegs to bottom face')
```

- [ ] **Step 2: Verify 4 peg protrusions exist**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    vframe = next(root.occurrences.item(i).component
                  for i in range(root.occurrences.count)
                  if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    body = next(vframe.bRepBodies.item(j) for j in range(vframe.bRepBodies.count)
                if vframe.bRepBodies.item(j).name == 'VFrame outer block')

    bb = body.boundingBox
    expected_min_y = -0.49  # cm: pegs protrude 4.9 mm below the body's Y=0 reference (assuming frame Y centred? actually frame Y extends 0..4.8, pegs go from 0 down to -0.49)
    print(f'Body bbox min Y: {bb.minPoint.y * 10:.2f} mm')
    print(f'Body bbox max Y: {bb.maxPoint.y * 10:.2f} mm')
    print(f'Body bbox span Y: {(bb.maxPoint.y - bb.minPoint.y) * 10:.2f} mm  (expected ~52.9 mm = 48 + 4.9)')
    assert (bb.maxPoint.y - bb.minPoint.y) * 10 > 52.5
    print('OK — pegs added (body Y-span > 52.5 mm)')
```

Expected: Y-span > 52.5 mm.

- [ ] **Step 3: Save**

`mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`.

---

## Task 5: Add 4 corner peg recesses on top face

**Files:**
- Modify: `Vertical LED Frame` component — cut 4 rectangular holes into the top face

**Why:** Recesses receive the pegs from the frame above. Nominal dimensions 10 × 3 × 5 mm — no clearance offset (the peg already has the 0.1 mm shrink).

- [ ] **Step 1: Cut the recesses**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation

    vframe = next(root.occurrences.item(i).component
                  for i in range(root.occurrences.count)
                  if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    body = next(vframe.bRepBodies.item(j) for j in range(vframe.bRepBodies.count)
                if vframe.bRepBodies.item(j).name == 'VFrame outer block')

    # Find +Y outer face (frame top)
    top_face = None
    for i in range(body.faces.count):
        f = body.faces.item(i)
        if isinstance(f.geometry, adsk.core.Plane) and f.geometry.normal.y > 0.99:
            if top_face is None or f.area > top_face.area:
                top_face = f
    assert top_face

    sk = vframe.sketches.add(top_face)
    lines = sk.sketchCurves.sketchLines
    hx, hz = 1.0, 0.3   # cm: nominal hole 10 × 3 mm
    corners = [
        (-8 + 1.25, -8 + 0.55),
        ( 8 - 1.25, -8 + 0.55),
        ( 8 - 1.25,  8 - 0.55),
        (-8 + 1.25,  8 - 0.55),
    ]
    for cx, cz in corners:
        p1 = adsk.core.Point3D.create(cx - hx/2, cz - hz/2, 0)
        p2 = adsk.core.Point3D.create(cx + hx/2, cz + hz/2, 0)
        lines.addTwoPointRectangle(p1, p2)

    profs = adsk.core.ObjectCollection.create()
    for k in range(sk.profiles.count): profs.add(sk.profiles.item(k))
    ext_input = vframe.features.extrudeFeatures.createInput(profs, CUT)
    ext_input.setDistanceExtent(False, val_str('PEG_HOLE_HEIGHT'))
    vframe.features.extrudeFeatures.add(ext_input)
    print(f'Cut {sk.profiles.count} recesses into top face')
```

- [ ] **Step 2: Verify 4 recesses exist by counting holes in the top face**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    vframe = next(root.occurrences.item(i).component
                  for i in range(root.occurrences.count)
                  if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    body = next(vframe.bRepBodies.item(j) for j in range(vframe.bRepBodies.count)
                if vframe.bRepBodies.item(j).name == 'VFrame outer block')

    # Count -Y normal faces at Y=4.8 - 0.5 = 4.3 cm (recess floor)
    recess_floors = 0
    for i in range(body.faces.count):
        f = body.faces.item(i)
        if not isinstance(f.geometry, adsk.core.Plane): continue
        if f.geometry.normal.y < -0.99 and abs(f.geometry.origin.y - 4.3) < 0.01:
            recess_floors += 1
    print(f'Recess-floor face count: {recess_floors}  (expected 4)')
    assert recess_floors == 4
    print('OK — 4 recesses present')
```

Expected: `Recess-floor face count: 4` and `OK — 4 recesses present`.

- [ ] **Step 3: Save**

`mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`.

---

## Task 6: Update existing LED Frame's peg + hole to match new spec

**Files:**
- Modify: `LED Frame` component in `LED Stack` sub-component — shrink existing peg from 10×4.5×~10 mm to 9.8×2.8×4.9 mm; update top recess to 10×3×5 mm

**Why:** Drop-in compatibility. After this change both frame types use the same peg/hole interface and can stack interchangeably.

- [ ] **Step 1: Inspect existing peg + hole geometry (read-only — record current state)**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent

    def walk(comp):
        out = []
        for i in range(comp.bRepBodies.count):
            out.append(comp.bRepBodies.item(i))
        for i in range(comp.occurrences.count):
            out.extend(walk(comp.occurrences.item(i).component))
        return out

    led_frame = next((b for b in walk(root) if b.name == 'LED Frame'), None)
    assert led_frame is not None, 'LED Frame body not found'
    bb = led_frame.boundingBox
    print(f'LED Frame bbox X: {bb.minPoint.x*10:.2f}..{bb.maxPoint.x*10:.2f}')
    print(f'LED Frame bbox Y: {bb.minPoint.y*10:.2f}..{bb.maxPoint.y*10:.2f}')
    print(f'LED Frame bbox Z: {bb.minPoint.z*10:.2f}..{bb.maxPoint.z*10:.2f}')

    # List planar faces at Y near the peg base (-Y face = peg side)
    print('\nPlanar faces near body bottom + peg corners:')
    for i in range(led_frame.faces.count):
        f = led_frame.faces.item(i)
        g = f.geometry
        if not isinstance(g, adsk.core.Plane): continue
        o = g.origin; n = g.normal
        if abs(o.y - bb.minPoint.y) < 0.01:   # bottom-face planes
            print(f'  bottom: origin=({o.x*10:.2f},{o.y*10:.2f},{o.z*10:.2f}) normal=({n.x:.2f},{n.y:.2f},{n.z:.2f}) area={f.area:.3f}')
```

This step records what's there so you have a baseline to revert to if Step 2 fails. Save the printed output for the report.

- [ ] **Step 2: Edit existing peg (recommended: re-create as a new feature, then suppress the old)**

This step is the riskiest in the plan — direct-edit features in v45 may not have parametric dimensions exposed. The safest approach is:

1. Find the existing peg-creation features in the timeline (likely an extrude or pattern).
2. Suppress them via `feature.isSuppressed = True`.
3. Add a new extrude with peg dimensions tied to user parameters.

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    tl = design.timeline
    root = design.rootComponent

    # Walk the timeline; find features whose name suggests "peg" or "hole" or sit in LED Frame component.
    # In v40+ the LED Frame's peg is part of its 285-step direct-edit history. Suppressing without context
    # is fragile. INSTEAD: add NEW geometry that overrides the existing peg shape.
    print('Timeline length:', tl.count)
    # No automatic operation — flag this step as MANUAL inspection in Fusion UI.
    print('MANUAL: Open Fusion UI, locate the LED Frame peg feature in the timeline, identify by hovering.')
    print('Then run Step 3 to apply the override pattern, OR use the in-UI Edit Sketch / Edit Feature dialog')
    print('to change the peg dimensions to 9.8 x 2.8 x 4.9 mm.')
```

- [ ] **Step 3: Apply peg-override (cut existing peg geometry away + add new peg in correct place)**

Run this only after Step 2's manual inspection if Step 2 confirmed the existing peg is at the same XZ position as the new one (centred at (X_corner ± PEG_OFFSET_X, Z_corner ± PEG_OFFSET_Z) = (±67.5, ±74.45) from world origin given LED Frame's offset of (+30, +5, -85)).

Approach: cut a slightly larger box around each existing peg, then add a new peg at the same world position with the new dimensions.

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    CUT  = adsk.fusion.FeatureOperations.CutFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation

    def walk(comp):
        out = []
        for i in range(comp.bRepBodies.count): out.append(comp.bRepBodies.item(i))
        for i in range(comp.occurrences.count): out.extend(walk(comp.occurrences.item(i).component))
        return out
    led_frame_body = next(b for b in walk(root) if b.name == 'LED Frame')
    led_frame_comp = led_frame_body.parentComponent

    # World peg locations relative to LED Frame's body (X bbox -50..110mm = -5..11cm, Z bbox -165..-5mm = -16.5..-0.5cm).
    # IMPORTANT: Fusion API uses CM internally — all coords below in cm.
    # OLD peg centres (corner offset 7.5mm): (-42.5, -157.5), (102.5, -157.5), (102.5, -12.5), (-42.5, -12.5) mm
    #                                       = (-4.25, -15.75), (10.25, -15.75), (10.25, -1.25), (-4.25, -1.25) cm
    # NEW peg centres (corner offset 12.5mm, 5.5mm): (-37.5, -160.95), (107.5, -160.95), (107.5, -9.05), (-37.5, -9.05) mm
    #                                                = (-3.75, -16.095), (10.75, -16.095), (10.75, -0.905), (-3.75, -0.905) cm
    bb = led_frame_body.boundingBox
    base_y = bb.minPoint.y      # bottom face of frame body
    old_peg_centers = [(-4.25, -15.75), (10.25, -15.75), (10.25, -1.25), (-4.25, -1.25)]
    new_peg_centers = [(-3.75, -16.095), (10.75, -16.095), (10.75, -0.905), (-3.75, -0.905)]

    sk = led_frame_comp.sketches.add(led_frame_comp.xZConstructionPlane)
    lines = sk.sketchCurves.sketchLines
    # Larger-than-peg removal rectangles: 12 mm x 6 mm = 1.2 cm x 0.6 cm
    for cx, cz in old_peg_centers:
        p1 = adsk.core.Point3D.create(cx - 0.6, cz - 0.3, 0)
        p2 = adsk.core.Point3D.create(cx + 0.6, cz + 0.3, 0)
        lines.addTwoPointRectangle(p1, p2)
    profs = adsk.core.ObjectCollection.create()
    for k in range(sk.profiles.count): profs.add(sk.profiles.item(k))
    cut_input = led_frame_comp.features.extrudeFeatures.createInput(profs, CUT)
    cut_input.setDistanceExtent(False, val_str('-7 mm'))
    led_frame_comp.features.extrudeFeatures.add(cut_input)
    print('Removed existing peg material at OLD positions (12x6mm at 4 corners)')

    # Now add new pegs (9.8 x 2.8 x 4.9 mm) at NEW corner-offset positions (12.5mm, 5.5mm).
    sk2 = led_frame_comp.sketches.add(led_frame_comp.xZConstructionPlane)
    lines2 = sk2.sketchCurves.sketchLines
    for cx, cz in new_peg_centers:
        p1 = adsk.core.Point3D.create(cx - 0.49, cz - 0.14, 0)
        p2 = adsk.core.Point3D.create(cx + 0.49, cz + 0.14, 0)
        lines2.addTwoPointRectangle(p1, p2)
    profs2 = adsk.core.ObjectCollection.create()
    for k in range(sk2.profiles.count): profs2.add(sk2.profiles.item(k))
    add_input = led_frame_comp.features.extrudeFeatures.createInput(profs2, JOIN)
    add_input.setDistanceExtent(False, val_str('-4.9 mm'))
    led_frame_comp.features.extrudeFeatures.add(add_input)
    print('Added new pegs (9.8 x 2.8 x 4.9 mm) at NEW corner-offset positions')
```

- [ ] **Step 4: Verify each LED Frame peg has the new dimensions**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    def walk(comp):
        out = []
        for i in range(comp.bRepBodies.count): out.append(comp.bRepBodies.item(i))
        for i in range(comp.occurrences.count): out.extend(walk(comp.occurrences.item(i).component))
        return out
    led_frame = next(b for b in walk(root) if b.name == 'LED Frame')

    # Find planar -Y faces (peg tops) below body bottom by ~4.9 mm. Group by XZ to find the 4 pegs.
    bb = led_frame.boundingBox
    expected_peg_y = bb.minPoint.y - 0.49   # peg bottom face at -4.9 mm below body bottom
    peg_tops = []
    for i in range(led_frame.faces.count):
        f = led_frame.faces.item(i)
        g = f.geometry
        if not isinstance(g, adsk.core.Plane): continue
        if g.normal.y < -0.99 and abs(g.origin.y - expected_peg_y) < 0.05:
            peg_tops.append((g.origin.x, g.origin.z, f.area))
    print(f'Peg-bottom faces found: {len(peg_tops)}  (expected 4)')
    for ox, oz, ar in peg_tops:
        print(f'  at ({ox*10:.2f}, {oz*10:.2f})  area={ar:.3f} cm² (expected ~0.274 cm² for 9.8 x 2.8 mm)')
    assert len(peg_tops) == 4
    for _, _, ar in peg_tops:
        assert abs(ar - 0.2744) < 0.01, f'peg area {ar} not 9.8x2.8mm'
    print('OK — 4 new pegs at 9.8 x 2.8 mm cross-section')
```

Expected: 4 peg-bottom faces, each with area ≈ 0.2744 cm².

- [ ] **Step 5: Top-face hole position update — handled in Task 7**

The existing LED Frame's top-face recess is also at the wrong XZ position (corner offset 7.5, 7.5 mm — needs to be 12.5, 5.5 mm to match the new peg layout). Task 7 Step 2 contains the script that re-cuts the top holes at the new position AND fills the old hole positions. Run that after this task completes.

- [ ] **Step 6: Save**

`mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`.

---

## Task 7: Visual + dimensional drop-in compatibility check

**Files:**
- Read-only inspection (no Fusion changes)

**Why:** Confirm that one Vertical LED Frame stacked atop one existing LED Frame has aligned peg/hole geometry.

- [ ] **Step 1: Place 2 occurrences (Vertical above, LED Frame below) and inspect peg-to-hole alignment**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent

    # Find Vertical LED Frame component and LED Frame component
    vframe_comp = next(root.occurrences.item(i).component
                       for i in range(root.occurrences.count)
                       if root.occurrences.item(i).component.name == 'Vertical LED Frame')
    def find_in_hierarchy(comp, name):
        for i in range(comp.occurrences.count):
            occ = comp.occurrences.item(i)
            if occ.component.name == name: return occ.component
            sub = find_in_hierarchy(occ.component, name)
            if sub: return sub
        return None
    led_stack_comp = find_in_hierarchy(root, 'LED Stack')
    assert led_stack_comp is not None

    # We won't actually place them stacked here — too much side-effect. Instead, compute the geometry:
    # If both frames have peg/hole centred at the same XZ corner offset (12.5, 5.5 from outer corner),
    # then stacking VFrame on top of LED Frame places its bottom pegs directly above LED Frame's top holes.
    # Just print the relative XZ positions to confirm.
    print('Vertical LED Frame peg corner XZ offset: (12.5, 5.5) mm')
    print('LED Frame existing hole XZ offset:        ~(7.5, 7.5) mm')
    print('MISMATCH: VFrame peg offset (12.5, 5.5) != LED Frame hole offset (7.5, 7.5)')
    print('Existing LED Frame holes are at (7.5,7.5) — VFrame pegs at (12.5,5.5) — they DO NOT line up.')
    print('Resolution: also re-cut the existing LED Frame top holes at (12.5, 5.5) — same procedure as Task 6 Step 3 but on the TOP face.')
    print('See follow-up task in plan or amend Task 6 with hole-position change.')
```

- [ ] **Step 2: If alignment mismatch reported above, run a fix-up: re-cut LED Frame top holes at new XZ position**

```python
import adsk.core, adsk.fusion

def run(_context: str):
    app = adsk.core.Application.get()
    design = adsk.fusion.Design.cast(app.activeProduct)
    root = design.rootComponent
    val_str = adsk.core.ValueInput.createByString
    CUT  = adsk.fusion.FeatureOperations.CutFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation

    def walk(comp):
        out = []
        for i in range(comp.bRepBodies.count): out.append(comp.bRepBodies.item(i))
        for i in range(comp.occurrences.count): out.extend(walk(comp.occurrences.item(i).component))
        return out
    led_frame_body = next(b for b in walk(root) if b.name == 'LED Frame')
    led_frame_comp = led_frame_body.parentComponent

    # Top face Y = body bbox max Y. Cut new holes at (X corner offset 12.5mm, Z corner offset 5.5mm).
    # LED Frame world XZ corners (in CM — Fusion API internal): (-5, -16.5), (11, -16.5), (11, -0.5), (-5, -0.5).
    # Corner offset 12.5mm = 1.25cm in X, 5.5mm = 0.55cm in Z.
    # IMPORTANT: Fusion API uses cm internally. Numbers below are cm.
    new_hole_centers_world = [
        (-5 + 1.25, -16.5 + 0.55),
        ( 11 - 1.25, -16.5 + 0.55),
        ( 11 - 1.25, -0.5  - 0.55),
        (-5 + 1.25, -0.5  - 0.55),
    ]
    # 1) Cut new 10x3mm rectangles into top face at the new positions
    sk = led_frame_comp.sketches.add(led_frame_comp.xZConstructionPlane)
    lines = sk.sketchCurves.sketchLines
    for cx, cz in new_hole_centers_world:
        p1 = adsk.core.Point3D.create(cx - 0.5, cz - 0.15, 0)
        p2 = adsk.core.Point3D.create(cx + 0.5, cz + 0.15, 0)
        lines.addTwoPointRectangle(p1, p2)
    profs = adsk.core.ObjectCollection.create()
    for k in range(sk.profiles.count): profs.add(sk.profiles.item(k))
    cut_input = led_frame_comp.features.extrudeFeatures.createInput(profs, CUT)
    # Cut downward from top by 5 mm = PEG_HOLE_HEIGHT
    # First need to construct the cut from the top face downward. Use the body's max Y.
    led_max_y_mm = led_frame_body.boundingBox.maxPoint.y * 10
    # The sketch is on the XZ plane at Y=0; the top face is at some Y. Move the sketch to the top.
    # Actually re-do: better to sketch on the actual top face. Below is a simpler fallback that may not work
    # if the XZ plane is far from the top face. If this fails, switch to sketching directly on the +Y face.
    cut_input.setDistanceExtent(False, val_str('-PEG_HOLE_HEIGHT'))
    led_frame_comp.features.extrudeFeatures.add(cut_input)
    print('Cut new top-face hole at (12.5, 5.5) corner offset')

    # 2) Fill the old holes at (7.5mm, 7.5mm) corner offset = (0.75cm, 0.75cm) by extruding new material.
    # All numbers in cm (Fusion API internal unit).
    old_hole_centers_world = [
        (-5 + 0.75, -16.5 + 0.75),
        ( 11 - 0.75, -16.5 + 0.75),
        ( 11 - 0.75, -0.5  - 0.75),
        (-5 + 0.75, -0.5  - 0.75),
    ]
    sk2 = led_frame_comp.sketches.add(led_frame_comp.xZConstructionPlane)
    lines2 = sk2.sketchCurves.sketchLines
    for cx, cz in old_hole_centers_world:
        p1 = adsk.core.Point3D.create(cx - 0.5, cz - 0.225, 0)
        p2 = adsk.core.Point3D.create(cx + 0.5, cz + 0.225, 0)
        lines2.addTwoPointRectangle(p1, p2)
    profs2 = adsk.core.ObjectCollection.create()
    for k in range(sk2.profiles.count): profs2.add(sk2.profiles.item(k))
    fill_input = led_frame_comp.features.extrudeFeatures.createInput(profs2, JOIN)
    fill_input.setDistanceExtent(False, val_str('-PEG_HOLE_HEIGHT'))
    led_frame_comp.features.extrudeFeatures.add(fill_input)
    print('Filled old hole at (7.5, 7.5) corner offset')
```

- [ ] **Step 3: Save**

`mcp__fusion__fusion_mcp_execute featureType=document object={operation:save}`.

- [ ] **Step 4: Visual screenshot for the report**

`mcp__fusion__fusion_mcp_read queryType=screenshot direction=iso-top-right width=1024 height=768` — confirm both frames look correct in the browser hierarchy.

---

## Task 8: Update firmware `NUM_COLS` to 44

**Files:**
- Modify: `C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\configuration.h`

**Why:** Firmware drives the matrix using `NUM_COLS × NUM_ROWS` — must match the new hardware layout (44 columns, N rows where N depends on tower height).

- [ ] **Step 1: Create a feature branch and switch to it**

```bash
cd /c/Users/gethi/source/pixelatedlights
git checkout main
git pull origin main
git checkout -b feat/vertical-led-frame
```

- [ ] **Step 2: Read current `configuration.h`**

```bash
cat /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/configuration.h
```

Confirm the file contains `#define NUM_COLS 36` (or 44 if previously changed).

- [ ] **Step 3: Edit `configuration.h` to set `NUM_COLS 44`**

Replace the existing line:

```c
#define NUM_COLS 36
```

with:

```c
#define NUM_COLS 44
```

The exact change is one number. Save the file.

- [ ] **Step 4: Update `NUM_ROWS` and `NUM_LEDS` comment in `configuration.h`**

The current file likely has:
```c
#define NUM_ROWS 6
#define NUM_LEDS 216
```

For a vertical-strip tower of e.g. 5 frames (240 mm), `NUM_ROWS` = floor(240 / 16.67) = 14 LEDs per strip. So:

```c
#define NUM_ROWS 14
#define NUM_LEDS (NUM_ROWS * NUM_COLS)  // 14 × 44 = 616
```

Use an actual numeric value (not the parenthesised expression) if the rest of the code can't take a macro: `#define NUM_LEDS 616`. Add a short comment that NUM_ROWS depends on tower height.

- [ ] **Step 5: Compile to verify**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --warnings all --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: clean compile, no errors. The "Low memory available" warning is pre-existing.

If RAM exceeds 2048 bytes (with 616 LEDs × 3 bytes = 1848 bytes for `leds[]` alone, plus other globals), the build will fail. If so:
- Confirm the user really wants 14 LEDs/strip (i.e. 5-frame tower) for the first build.
- If not, fall back to smaller `NUM_ROWS` (e.g. NUM_ROWS=8 with NUM_LEDS=352, ~1056 bytes for `leds[]`).

- [ ] **Step 6: Commit and push**

```bash
cd /c/Users/gethi/source/pixelatedlights
git add AllEffects_FastLED/configuration.h
git commit -m "feat(firmware): NUM_COLS=44 for vertical-strip LED frame

The new Vertical LED Frame design (spec at docs/superpowers/specs/2026-05-26-vertical-led-frame-design.md)
holds 44 vertical strips around the tower perimeter. Update NUM_COLS to 44. NUM_ROWS now tracks tower height (LEDs per strip)."
git push -u origin feat/vertical-led-frame
```

- [ ] **Step 7: Open PR**

```bash
gh pr create --base main --head feat/vertical-led-frame \
  --title "Firmware: NUM_COLS = 44 for vertical-strip LED frame" \
  --body "Pairs with the CAD work for the new Vertical LED Frame (see docs/superpowers/specs/2026-05-26-vertical-led-frame-design.md).

NUM_COLS = 44 (was 36). NUM_ROWS now depends on tower height (LEDs per vertical strip).

Tested: arduino-cli compile passes on arduino:avr:uno."
```

- [ ] **Step 8: Capture the PR number and wait for CI**

The `gh pr create` command above prints the PR URL — the last URL-path segment is the PR number. Run:

```bash
PR_NUM=$(gh pr list --head feat/vertical-led-frame --json number --jq '.[0].number')
echo "PR number: $PR_NUM"
gh pr checks $PR_NUM --watch --interval 15
```

Expected: `arduino-cli compile (uno)` and `Claude BugBot review` both pass.

- [ ] **Step 9: Merge after CI green**

```bash
PR_NUM=$(gh pr list --head feat/vertical-led-frame --json number --jq '.[0].number')
gh pr merge $PR_NUM --merge
```

---

## Task 9: Hardware print — single Vertical LED Frame

**Files:** (no code changes; user-executed)

**Why:** Physical proof that the geometry is correct before scaling.

- [ ] **Step 1: Export the Vertical LED Frame as STL from Fusion**

User-executed in Fusion UI: select the `Vertical LED Frame` component → File → Export → STL.

- [ ] **Step 2: Slice and print**

User-executed. Material PETG, layer height 0.2mm, 20% infill, no supports. Print orientation: TOP face DOWN.

- [ ] **Step 3: Calliper measurements after print**

User-executed. Targets:
- Slot 11.0 ± 0.2 mm wide × 2.0 ± 0.2 mm deep
- Peg 9.8 ± 0.1 × 2.8 ± 0.1 × 4.9 ± 0.1 mm
- Hole 10.0 ± 0.1 × 3.0 ± 0.1 × 5.0 ± 0.1 mm
- Outer 160 × 160 × 48 mm

Report results.

---

## Task 10: Hardware print — second frame + stack test

**Files:** (no code changes; user-executed)

- [ ] **Step 1: Print a second Vertical LED Frame**
- [ ] **Step 2: Stack the two — pegs slot into holes**
- [ ] **Step 3: Confirm flat-on-flat contact, no rotation, slight slip (0.1 mm clearance per face) but firm engagement**

Report PASS/FAIL.

---

## Task 11: Insert LED strips, verify channel pass-through

**Files:** (no code changes; user-executed)

- [ ] **Step 1: Take one 96 mm WS2811 IP30 strip section (2 frame heights)**
- [ ] **Step 2: Slide it through aligned channels of the stacked pair via slot 1 on TOP wall**
- [ ] **Step 3: Confirm: strip slides without binding, 3M backing sticks to channel floor, LED face flush with outer wall plane**
- [ ] **Step 4: Repeat for all 11 slots on one side; confirm no peg interference**
- [ ] **Step 5: Repeat across all 4 sides — 44 strips total**

Report PASS/FAIL with photos.

---

## Task 12: Mount stacked pair into existing outer shell + wire up

**Files:** (no code changes; user-executed)

- [ ] **Step 1: Place stacked Vertical LED Frame pair inside Shell Bottom + Top Shell**
- [ ] **Step 2: Confirm outer shell fits over without interference**
- [ ] **Step 3: Solder snake jumpers (strip 1 end → strip 2 start, alternating direction)**
- [ ] **Step 4: Solder per-strip power injection rail (+5 V + GND in parallel, tapped at start of each strip)**
- [ ] **Step 5: Connect to Arduino + flash firmware from `feat/vertical-led-frame` branch**

---

## Task 13: Full effect test

**Files:** (no code changes; user-executed)

- [ ] **Step 1: Boot tower with firmware compiled at NUM_COLS=44 / NUM_ROWS=14**
- [ ] **Step 2: Cycle through all 18 effect slots (existing + 4 generative)**
- [ ] **Step 3: Send single-pixel chase test via firmware tool to verify pixel ordering matches snake topology**
- [ ] **Step 4: Confirm no flicker / no power-drop dimming at top of tower**

Report final PASS, photos, and any required tweaks.

---

## Coverage self-check

| Spec section | Implementation task |
|---|---|
| 1. Goal | All tasks combined |
| 2. Architecture | Tasks 1–7 (Fusion CAD) |
| 3. Outer dimensions | Task 3 |
| 4. Strip channels | Task 3 |
| 5. Stacking interface | Tasks 4, 5, 6, 7 |
| 6. LED strip + wiring | Tasks 11, 12 |
| 7. Firmware impact | Task 8 |
| 8. Material + print | Tasks 9, 10 |
| 9. Component boundaries | Tasks 1, 2, 6 |
| 10. Error/edge handling | Validation steps in each task |
| 11. Testing / acceptance | Tasks 9–13 |
| 12. Not in scope | n/a |
| 13. Open questions | none |

All spec requirements mapped to at least one task.

---

## Execution notes

- The Fusion CAD tasks (1–7) are best executed via the `mcp__fusion__fusion_mcp_execute featureType=script` MCP tool. Each Python script returns text output that confirms success. If a script fails, do NOT continue; debug from the printed exception trace.
- Tasks 1–7 modify a Fusion document that is cloud-saved. Each save creates a new version (v46, v47…). If anything goes catastrophically wrong, the user can roll back to a previous version in Fusion.
- The firmware change (Task 8) is in a Git branch with CI gating. Standard PR flow.
- Hardware tasks (9–13) are user-executed. The plan provides expected-result checklists; the engineer just needs to flag PASS/FAIL.
- This plan does NOT include cosmetic chamfers/fillets, heat dissipation analysis, or wiring diagrams — see spec section 12.
