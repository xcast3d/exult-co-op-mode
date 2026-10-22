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
up at all, so E never offers it. Containers, bodies and signs are offered
anywhere within reach; everything else within 6 tiles.

Q and E start again at the nearest once you have moved; standing still,
they go on to the next, then "No target", then round again.

| Group | Default | What Space does | What is in it |
| --- | --- | --- | --- |
| Containers and bodies | always | opens (Shift: picks up a bag or basket) | chests, crates, barrels, bags, baskets, bodies |
| Loot | on | takes into the pack | everything the game calls a weapon, armour or ammunition, plus the list below |
| Decor | off | takes | anything not in another group |
| Furniture | off | uses (Shift: picks up) | the list below |
| Doors, hidden walls, lamps | off | uses | doors, secret walls (828, 845), shutters, sconces, curtains, lamp posts. Off by default: with it on, E finds secret walls, which Black Gate means to be found by looking |
| Levers and switches | on | uses | levers (787), switches (788) |
| Signs, plaques, graves | on | reads | sign (379), plaque (820), tombstone (715); picture signs (360, 361) have nothing to read |

**Loot (besides weapons, armour and ammunition):** pocketwatch, cloth map,
Knight's Bridge piece, cloak, wedding ring, rings of invisibility and
regeneration, Virtue Stone, potion, food, hood, ammunition, bed roll, kidney
belt, shoes, bottle, lockpick, ring, key, book, gold coins, nuggets and bars,
sleeping powder, venom, sextant, fishing rod, sword blank, lyre, lute,
whistle, lens, Caddellite chunks, pants, Orrery Crystal, Soul Cage, vial,
caltrops, gem, spellbook, smokebomb, Rudyom's Wand, honey, Orb of the Moons,
Vortex Cube, invisibility dust, scroll, bandage, Avatar Costume, hourglass,
reagent, Rune, blackrock, lead, iron ore, betting marker, Gargoyle jewelry,
amulet, Prism.

**Furniture:** light sources (lit, unlit and spent: candles and the like),
chair, podium, cannon, powder keg, Iron Maiden, the Rack, easel,
anvil, cradle and baby in a cradle, cauldron, rocking horse.

**Decor (everything else):** debris, small rock, top, iron bars, banner,
cooking and eating utensils, kite, garbage, broken dish, cup, bale of wool,
spindle of thread, dough, desk items, sack of wheat, jar, glass item, cannon
ball, log saw, plate, baby, stuffed toy, alchemist's apparatus, music box,
small orrery, orrery viewer, bucket, stone chips, diaper, artist's equipment,
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
