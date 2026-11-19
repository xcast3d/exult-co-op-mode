# Water enhancement experiment

Status: **abandoned** (2026-11-19). Not part of the main build, and no Water
option appears in any menu.

The prototype is preserved on branch `feature/water`, commit `fe596291f`
(`Prototype and park water visual enhancements`). It was deliberately not
merged into `feature/friend-mode`.

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
- sparse directional sun glints;
- (2026-11-19, not committed) a bold, static colour pass instead of
  marks: lighter, greener shallows near shore fading into deep blue, and
  the water tinted by the hour (bright blue at midday, gold and rose at
  sunrise and sunset, silver at night), with a murky green on swamp.

The later versions fit the original pixel art much better than the initial
smooth overlay and preserved the game's palette-cycled water. Even so, the
result did not yet provide enough visual improvement to justify the renderer
complexity and continuous repaint cost.

## Decision

Abandoned on 2026-11-19. Without new art, every treatment was either barely
noticeable or read as a layer pasted over the game; none looked like a
remaster. Reflections of the game's own pixels were considered but not
built: they would only show at harbours and on ships, since the game never
draws a sky to reflect on open sea.

`feature/water` is kept as a record. If water is ever revisited, it likely
needs new art (a remaster texture pack), which is outside the "enhance
without changing" approach.
