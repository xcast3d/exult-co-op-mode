# Website notes

Things to add to the website (the separate `ultima-7-friends-site` repo).
Take an item off this list once it's on the site.

## To add

### 0.8.12 (released 2026-11-28, build 310)

Written on the site 2026-11-28 (site 2d3234d): a "A keyring, at last" card
with the Target System's, the formations in the movement chapter and Party
order, the night fade and longer torches in Lamps and torches, and in the
guide the keyring, torches and night light under Everyday changes and the
original formations under Party movement. Version 0.8.12 on the home page
and guide; changelog page rebuilt; dist/ built with play/ (web 0.8.12). Not
done: a screenshot of a full party of eight in formation.

Changes since 0.8.11, from the changelog.

**New**
- A keyring (from the Keyring mod), sold wherever lockpicks are (Budo,
  Dell, Greg, Jehanne), 20 gold. The keys belong to the party, so a lost
  keyring is bought again with every key still on it. Keys go on by
  dropping one on it, "Add to keyring" on a key, or "Add keys" for all of
  them; "Remove keys" puts them back in the pack. Keys on the ring open
  doors and chests like keys in a pack: K, the Target System, or
  double-click the keyring. Inn keys and Alagner's key stay off it.

**Changed**
- The party walks in the original game's formations, one for each party
  size; on diagonals it keeps its facing and slides along.
- Torches burn twice as long.
- At night, light from lamps and torches fades softly instead of jumping
  in three steps.

**Fixed**
- After changing the party's order, companions no longer walk back and
  forth or fall behind.
- After a turn, companions catch up to their places instead of trailing
  in a line.
- In co-op, a guest back from a menu sees the whole party in place (no
  one flies in); outside a fight, the guest's companion stays put while
  the guest is in a menu.
- Torches can be readied from the item menu and always go in a hand (the
  off hand if a weapon is held).
- Switching party members shows only the latest "Leading" notice.
- Taking something that won't fit or is too heavy says so.
- Guests can use the mining machine.
- The guest's "Following the Avatar" bar no longer flickers under a menu.
- Stairs indoors no longer flicker or leave a shadow on the floor above.
- With the interface kept in the day palette, the status bar, skills bar
  and minimap no longer darken during the night-light fade.

**For the site (tested by the user 2026-11-28: keyring using, adding,
removing and buying all work):**
- **Keyring**, the headline. A card on the home page ("A keyring, at
  last": buy it from a provisioner, drop keys on it, never dig for a key
  again; a lost one comes back with every key). In the guide: where to
  buy it (Budo, Dell, Greg, Jehanne, 20 gold), the item menu (Add keys,
  Remove keys, and Add to keyring on any key), and that K, E and a
  double-click use its keys. Credit the Keyring mod (Exult team) for the
  picture. No screenshots: Exult players know the keyring, and anyone
  else gets the idea (one ring instead of a pile of keys).
- **Formations**: update the party-order section (Shift+O): the original
  game's formation for each party size, turning only after two steps the
  new way, and sliding along diagonals. A screenshot of a full party of
  eight in formation.
- **Lighting**: a line in the lamps and torches card: light now fades in
  as you walk toward a lamp, instead of stepping. Torches burn twice as
  long, and Ready puts one in a hand.
- **At release**: the guide's "Written for version", and changelog.html
  (`node scripts/changelog.mjs`).

### Lighting (new in 0.8.6, experimental)

Written on the site 2026-11-19: a Lighting chapter on the home page
(`#lighting`, six cards: sun and shadow, lamps and torches, daylight
indoors, magic glows, night edges, tuning), Lighting in the menu,
Ctrl+Shift+L in the shortcuts table, and a Lighting section in the guide
with the menu's rows. Before/after slider carousel added the same day (site
abf3f0e): Trinsic by day, Lord British's castle, the stables at night, a
cave. Maybe later: sunbeams through an open door, a moongate, a town at
dusk.

### Feeding and the party's packs (new in 0.8.5)

Written on the site 2026-11-14: the guide's Target System section (the hint's
first, second and third actions, More, the party's packs as one inventory,
Drop all and Give all, W and S in lists), an Eating line in its General list,
the controller notes (the second action, the item menu), and on the home page
a "Feeding, your way" card and Ctrl+Space and F in the shortcuts table.
Maybe later: a screenshot of the food list and the who-eats list.

### Combat System (new in 0.8.4)

Written on the site 2026-11-09 (site 455fb55): a fourth system card
("04 / Tactics"), a Combat chapter on the home page (`#combat`), Ctrl+C in
the shortcuts table, and a Combat System section in the guide, with two
screenshots (`qotc-combat-orders.avif`, `qotc-position-damage.avif`). Maybe
later: an archer keeping its distance, for Enemy tactics. At release: the
guide's "Written for version" and changelog.html (`node scripts/changelog.mjs`).

### Aimed drops (new in 0.8.3)

A small section of its own on the site, with screenshots. A video comes
later, for Reddit.

**Headline:** Drop anything anywhere, no dragging.

**Short text:** Choose where every item goes, on keyboard or controller.
Press drop, aim at a person, a chest or a spot on the ground, and press
again. The hint tells you what will happen before you commit: "Give to
Iolo", "Put in Chest", "Drop here". Or press twice to drop at your feet.

**How it works:**

| | Keyboard and mouse | Controller |
|---|---|---|
| Drop | Space or Shift+Space on your item | Cross (A) or L2+Cross (LT+A), or Drop in the item menu (hold Cross) |
| Aim at a thing | E | R1 (RB) or D-pad up and down |
| Aim at a person | Q | L1 (LB) |
| Aim anywhere | Mouse | Right stick |
| Confirm | Space or click | Cross (A) |
| At your feet | Space again without aiming | Cross (A) again without aiming |
| Cancel | Escape | Circle (B) |

**Screenshots:** the hints are the story, so catch them on screen: "Give to
Spark" on a companion, "Put in Chest" on a chest, "Drop here" on open ground.

**Video for Reddit, later, in order:**

1. Two presses: an item lands at your feet, the old habit still works.
2. Aim at a spot with the mouse or stick: "Drop here".
3. Step through people with Q: "Give to Spark", and Spark has it.
4. "Put in Chest" with no window open, then open the chest to show it.
5. All of it again with only a controller.

**Hold back for now:** quest puzzles such as Penumbra's plaques and the Black
Gate pedestals. Drops should trigger them, but this hasn't been tested in
play yet.
