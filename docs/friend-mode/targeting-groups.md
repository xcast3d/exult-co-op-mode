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
| Furniture | off | uses (Shift: picks up) | the list below (the anvil moved to Usable, 2026-11-04); also pieces fixed in the map, which can't be picked up (2026-11-01: they were never offered, so the cradles in Lord British's castle never came up) |
| Doors, hidden walls, lamps | on | uses (a locked one: Unlock with a key that fits, else Pick Lock with lockpicks where they work; 2026-11-01) | doors, door-like walls and portcullises (250, 303, 876, 935, 936; 2026-11-04), secret walls (828, 845), shutters, sconces, curtains, lamp posts, torches fixed on walls (595, 701 that can't be picked up; 2026-11-01). On by default since 2026-10-24 (was off: with it on, E finds secret walls, which Black Gate means to be found by looking) |
| Usable (was levers and switches) | on | uses (Shift: picks up) | levers (787), switches (788), the bucket (810), the gavel (675 frame 0), artist's equipment (823: pigments, paint brush), the easel (837, was furniture), beds (696, 1011) and the bed roll (583, was loot; Space sleeps, 2026-10-29), winches (949, 950; 2026-10-31). From a scan of every shape with a script that E never offered (2026-11-01): the mining machine (410, the mines' ore extractor), loom (261), spinning wheel (651), bellows (431), wells (470, 740), water troughs (719, 741), keg and cask (258, 434), chicken coop (210), laboratory burner (307), grandfather clocks (252, 695), sundial (284), telescope (744), crystal ball (729), mirror (848), harpsichord, xylophone, harp (689, 690, 745), the fair's strength tester, Virtue Roulette Wheel, Triples, Rat Race (743, 725, 809, 763), puppet show (503), stocks (708), guillotines (709, 734), archery target (735). Vehicles (2026-11-01; Space names the action from the vehicle's state, host and single player only): the gangplank (150 lowered, 781 raised), sails (251), carts (301, 652, 660, 757, 773, 774) and the draft horse (796), the flying carpet (840). The pool of water (893, fountains; Space drinks; was decor, 2026-11-01). What tools and quests ask for (2026-11-04, see below): firepit (739), anvil (991), Well of Souls (748), pedestals (577), the ferry's horns (888), the Ferryman (155), the Dark Core (990), the Enclosed Prism (968), giant bones (418), the bleeding stone (932 frames 2 and 3), the baking hearth (831; 2026-11-08), flour and what works dough (863 frames 0, 8, 9; was decor; 2026-11-08: Space says Pour Flour on the sack, and in a crosshair E also aims at the tables 1003 and 1018 its script takes), cannons (702, was furniture; 2026-11-08: their script fires with a powder keg and a cannon ball beside, the Avatar's on a guest). The lyre, lute and whistle (691-693, were loot; 2026-11-12: Space plays, Alt+Space takes) and the mysterious craft (267, was decor; Space inspects: its tune). Space says Play on the instruments and the ferry's horn. Decor is what has no use (2026-10-23) |
| Signs, plaques, graves | on | reads | sign (379), plaque (820), tombstone (715); picture signs (360, 361) have nothing to read |

**Loot (besides weapons, armour and ammunition):** pocketwatch, cloth map,
Knight's Bridge piece, cloak, wedding ring, rings of invisibility and
regeneration, Virtue Stone, potion, food, hood, ammunition, kidney
belt, shoes, bottle, lockpick, ring, key, book, gold coins, nuggets and bars,
sleeping powder, venom, sextant, fishing rod, sword blank, lens, Caddellite chunks, pants, Orrery Crystal, Soul Cage, vial,
caltrops, gem, spellbook, smokebomb, Rudyom's Wand, honey, Orb of the Moons,
Vortex Cube, invisibility dust, scroll, bandage, Avatar Costume, hourglass,
reagent, Rune, blackrock, lead, iron ore, betting marker, Gargoyle jewelry,
amulet, Prism. Quest things from a scan of the scripts (2026-11-04): iron bars
(264, Trent's cage), stone chips (815), the stone heart (203 frame 10), orrery
viewer (770), bee stingers (947), sacks of wheat (677), bale and spindle (653,
654), dough (658).

**Furniture:** light sources (lit, unlit and spent: candles and the like),
chair, podium, powder keg, Iron Maiden, the Rack,
anvil, cradle and baby in a cradle, cauldron, rocking horse.

**Decor (everything else):** debris, small rock, top, iron bars, banner,
cooking and eating utensils, kite, garbage, broken dish, cup, bale of wool,
spindle of thread, dough, desk items, sack of wheat, jar, glass item, cannon
ball, log saw, plate, baby, stuffed toy, alchemist's apparatus, music box,
small orrery, orrery viewer, stone chips, diaper,
pitcher, cloth, kitchen items, thumbscrews, test tubes, pieces of wood, pot,
bee stinger, tongs, horseshoe, plant, haystack. The pool of water (893,
Black Gate's fountains) was here until 2026-11-01; it's Usable now, and
Space drinks.

## Borderline, for review

Placed by judgment; move any that feel wrong in play.

| Thing | Placed in | Why it could go elsewhere |
| --- | --- | --- |
| Bottle (616) | Loot | Wine and such can be drunk, but most bottles are table dressing. |
| Pitcher (824), cup (628) | Decor | Not drinkable in Black Gate. |
| Honey (772) | Loot | Food for the honey quest; also a kitchen prop. |
| Dough (658) | Decor | Only for baking bread. |
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

## What the game's scripts ask for (2026-11-04)

From `tools/friend-mode/ucread.py`: every script that calls `click_on_item`
and the shapes it compares the answer with, so a tool's target can be offered.

| Tool | Script | Wants |
| --- | --- | --- |
| Pick (624, held) | 0270 | the bleeding stone (932, frames 2 and 3; Forge of Virtue) |
| Hammer (623) | 026F | the sword blank on the anvil |
| Sword blank (668) | 029C | firepit, anvil, water trough |
| Soul Cage (747) | 02EB | Well of Souls, then the Liche |
| Vial (754) | 02F2 | the Liche, the Soul Cage |
| Prism (981) | 03D5 | the pedestals (577) |
| Bucket (810) | 032A | wells, troughs, fires, the dough |

Space uses the tool itself on the stone, firepit and Well
(`Friend_tool_for`); the others go through the tool's own crosshair, where E
aims at the target.

Crafting (2026-11-08, guests too): the bale of wool (653) wants the spinning
wheel (651), the spindle of thread (654) the loom (261), and dough (658,
frames 1 and 2; frame 0 is flour, which the bucket's water makes into dough)
the baking hearth (831). On the map, Space on one says Use (its crosshair);
Space on the wheel, loom or hearth uses one the party carries (a guest: its
own companion's), its "on what?" answered (`Friend_craft_target`).

Eggs that fire when something lands on them (`something_on`): Penumbra's
plaque (egg at 2727,1394, within 5 tiles, script 0334 wants hammers, then a
lockpick, a ring, a spindle of thread, then gold) and the Black Gate's three
pedestals (one tile each). Drops by Space go through the same egg check as a
drag, and the host does it for a guest's drop (2026-11-04: neither did).

Serpent Isle needs its own lists (Phase 10).
