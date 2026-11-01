# Targeting groups: what E offers

E cycles things near the player (see Phase 9 in the
[project brief](../ultima-7-friend-mode.md)). Which things is set per group
on the **Target System** options page (game menu → Target System;
with "Enable" off the other lines are hidden), kept in each
machine's config (`config/gameplay/targeting/...`).

Black Gate's own shape classes don't separate these: a chair, a plate and a
gem are all "breakable", and a cup is the same class as a key. So the groups
are our own lists, in `Friend_thing_group_of` (exult.cc), made from
`tools/friend-mode/pickup_items.py`, which lists the 234 shapes that can be
picked up. Heavy furniture (tables, stools, benches, beds) can't be picked
up at all, so E never offers it. Everything is offered on screen within 18
tiles, and a pick is dropped once the player is more than 18 tiles from it
(2026-10-29; containers, bodies and signs were offered anywhere on screen,
and loose things, doors, lamps and levers within 18: 6, then the whole
screen offered shutters at its edges, then 12).
All need a path that walls don't block, as Exult's picking up does (so a
thing just inside an open shutter can be offered). Something under a roof
is hidden unless it reaches the roof: a lamp post under a house's eave
stands outside (2026-10-23; it was never offered).

Q and E start again at the nearest once you have moved; standing still,
they go on to the next, then "No target", then round again.

| Group | Default | What Space does | What is in it |
| --- | --- | --- | --- |
| Containers and bodies | always | opens (Shift: picks up a bag or basket); a locked chest: Unlock with a key that fits, else Pick Lock with lockpicks (2026-11-01) | chests, crates, barrels, bags, baskets, bodies |
| Loot | on | takes into the pack | everything the game calls a weapon, armour or ammunition, plus the list below |
| Decor | off | takes | anything not in another group |
| Furniture | off | uses (Shift: picks up) | the list below; also pieces fixed in the map, which can't be picked up (2026-11-01: they were never offered, so the cradles in Lord British's castle never came up) |
| Doors, hidden walls, lamps | on | uses (a locked one: Unlock with a key that fits, else Pick Lock with lockpicks where they work; 2026-11-01) | doors, secret walls (828, 845), shutters, sconces, curtains, lamp posts, torches fixed on walls (595, 701 that can't be picked up; 2026-11-01). On by default since 2026-10-24 (was off: with it on, E finds secret walls, which Black Gate means to be found by looking) |
| Usable (was levers and switches) | on | uses (Shift: picks up) | levers (787), switches (788), the bucket (810), the gavel (675 frame 0), artist's equipment (823: pigments, paint brush), the easel (837, was furniture), beds (696, 1011) and the bed roll (583, was loot; Space sleeps, 2026-10-29), winches (949, 950; 2026-10-31). From a scan of every shape with a script that E never offered (2026-11-01): the mining machine (410, the mines' ore extractor), loom (261), spinning wheel (651), bellows (431), wells (470, 740), water troughs (719, 741), keg and cask (258, 434), chicken coop (210), laboratory burner (307), grandfather clocks (252, 695), sundial (284), telescope (744), crystal ball (729), mirror (848), harpsichord, xylophone, harp (689, 690, 745), the fair's strength tester, Virtue Roulette Wheel, Triples, Rat Race (743, 725, 809, 763), puppet show (503), stocks (708), guillotines (709, 734), archery target (735). Decor is what has no use (2026-10-23) |
| Signs, plaques, graves | on | reads | sign (379), plaque (820), tombstone (715); picture signs (360, 361) have nothing to read |

**Loot (besides weapons, armour and ammunition):** pocketwatch, cloth map,
Knight's Bridge piece, cloak, wedding ring, rings of invisibility and
regeneration, Virtue Stone, potion, food, hood, ammunition, kidney
belt, shoes, bottle, lockpick, ring, key, book, gold coins, nuggets and bars,
sleeping powder, venom, sextant, fishing rod, sword blank, lyre, lute,
whistle, lens, Caddellite chunks, pants, Orrery Crystal, Soul Cage, vial,
caltrops, gem, spellbook, smokebomb, Rudyom's Wand, honey, Orb of the Moons,
Vortex Cube, invisibility dust, scroll, bandage, Avatar Costume, hourglass,
reagent, Rune, blackrock, lead, iron ore, betting marker, Gargoyle jewelry,
amulet, Prism.

**Furniture:** light sources (lit, unlit and spent: candles and the like),
chair, podium, cannon, powder keg, Iron Maiden, the Rack,
anvil, cradle and baby in a cradle, cauldron, rocking horse.

**Decor (everything else):** debris, small rock, top, iron bars, banner,
cooking and eating utensils, kite, garbage, broken dish, cup, bale of wool,
spindle of thread, dough, desk items, sack of wheat, jar, glass item, cannon
ball, log saw, plate, baby, stuffed toy, alchemist's apparatus, music box,
small orrery, orrery viewer, stone chips, diaper,
pitcher, cloth, kitchen items, thumbscrews, test tubes, pieces of wood, pot,
bee stinger, tongs, horseshoe, plant, haystack. Also the pool of water
(893, Black Gate's fountains): it can't be picked up, but E offers it within
6 tiles while decor is on, and Space drinks (2026-10-22).

## Borderline, for review

Placed by judgment; move any that feel wrong in play.

| Thing | Placed in | Why it could go elsewhere |
| --- | --- | --- |
| Bottle (616) | Loot | Wine and such can be drunk, but most bottles are table dressing. |
| Pitcher (824), cup (628) | Decor | Not drinkable in Black Gate. |
| Honey (772) | Loot | Food for the honey quest; also a kitchen prop. |
| Dough (658) | Decor | Only for baking bread. |
| Lyre, lute, whistle (691-693) | Loot | Playable, but mostly decoration in houses. |
| Fishing rod (662) | Loot | Usable at water; also wall decoration. |
| Sextant, hourglass, lens | Loot | Quest or tool items, lying around rarely. |
| Blackrock, lead, iron ore | Loot | Quest materials (blackrock) and smithing ore. |
| Desk items (675) | Decor | Papers, seals, blotters, quills, inkwells and the abacus (frame 11, used by the G key). |
| Kitchen items (863) | Decor | Cooking props. |
| Artist's equipment (823) | Decor | The brush and pigments for painting at an easel. |
| Light sources (336/338/997) | Furniture | Candles can be lit (Space uses them); also taken for light. |
| Torch (595/701) | Loot | The game lists it as a weapon. |
| Cannon ball (703) | Decor | Only with a cannon. |
| Betting marker (921) | Loot | Buccaneer's Den gambling chips. |
| Music box, small orrery | Decor | Curiosities with a script. |

Serpent Isle needs its own lists (Phase 10).
