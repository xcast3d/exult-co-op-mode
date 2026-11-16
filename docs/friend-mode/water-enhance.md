# Water enhancement experiment

Status: **parked; not part of the main build** (2026-11-16).

The prototype is preserved on branch `feature/water`, commit `fe596291f`
(`Prototype and park water visual enhancements`). It was deliberately not
merged into `feature/friend-mode`. We may revisit it later.

## What the prototype proved

- Water and swamp terrain can be tagged per visible pixel while the map
  paints.
- Boats, docks, bridges, actors and other objects can cleanly occlude the
  effect.
- Mixed land/water shoreline tiles require checking the painted pixel colour;
  the shape's water flag applies to the whole tile.
- A chamfer distance-to-shore field can orient crests parallel to irregular
  coastlines and move them toward land.
- Water can be enabled independently from experimental Lighting, with
  continuous repainting limited to views containing water.

## Treatments tested

- smooth world-anchored luminance ripples;
- domain-warped flowing bands;
- broken native-resolution pixel crests;
- shoreline-oriented incoming crests;
- subtle moving shadow bands;
- a shallow-water cyan gradient;
- sparse directional sun glints.

The later versions fit the original pixel art much better than the initial
smooth overlay and preserved the game's palette-cycled water. Even so, the
result did not yet provide enough visual improvement to justify the renderer
complexity and continuous repaint cost.

## Decision

Keep Water out of the main build for now. If the idea is revisited, start from
the preserved branch rather than reimplementing the masks. The most promising
next experiment is a smaller shore-only treatment or restrained lamp/window
reflections; it should establish a clear visual win before restoring general
open-water animation.
