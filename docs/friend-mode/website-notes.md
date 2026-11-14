# Website notes

Things to add to the website (the separate `ultima-7-friends-site` repo).
Take an item off this list once it's on the site.

## To add

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
