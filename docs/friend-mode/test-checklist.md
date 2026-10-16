# Friend Mode test checklist

A repeatable two-player pass on one Mac. Run it after any change to Friend
Mode, and tick off what you tried. Black Gate only for now.

## Setup

1. Build everything with a full `make -j8`. `make exult` does not rebuild the
   libraries in `objs/`, `usecode/`, `gumps/` and so on, so changes there are
   silently missing.
2. Start the host:

   ```bash
   ./exult --bg --nocrc --friend-host
   ```

3. On the host, load a save with at least one companion besides the Avatar
   (for example Iolo or Spark), and stand somewhere open.
4. Press **F9** on the host to place the test kit around the Avatar. The host
   shows "Friend Mode test kit placed".
   - **Do this before the guest joins.** Objects created after a guest joins
     are not synced yet, but the kit is part of the world the guest downloads.
5. Start the guest:

   ```bash
   ./exult --bg --nocrc -c .friend-mode-local/guest.cfg --friend-join 127.0.0.1
   ```

### The test kit

| Object | Shape | Where (from the Avatar) |
| --- | --- | --- |
| Vertical door | 270 | west, slightly north |
| Horizontal door | 376 | north |
| Closed shutters | 290 | north-east |
| Lamp post | 889 | east, slightly north |
| Sconce | 481 | east |
| Light source | 336 | east, slightly south |
| Lever | 787 | west |
| Chest with food, 25 gold, a bottle, a torch and a key | 800 | south-west |
| Bag with 2 food | 802 | south-east |
| Other door styles | 270 and 376 at frames 8 and 16 | row to the south |
| Portcullis, portcullis door | 271, 935 | row to the south |
| Secret walls | 828, 845 | further south |
| Locked door (key quality 78) | 270 frame 2 | far west, slightly north |
| Locked chest with 50 gold (key quality 77) | 522 | far west, south |
| Locked chest with food, no key (for lockpicks) | 522 | far west, further south |
| Supplies bag: both keys, 5 lockpicks, potions (frames 0-7), 3 food | 802 | east, south |
| Teleporter pads (4x4 carpet, linked) | 190 + eggs 275 | far west and far east |

Each teleporter pad drops you just south of the other one. The eggs under
them are invisible; the carpet marks where they are.

The spawned lever isn't linked to any gate, so it only tests that its frame
change syncs. Any shape or frame the game doesn't have is skipped and logged on the host.

## Join

- [ ] Guest: **Connect to Host** asks for a player name and address.
- [ ] Guest: "Connecting to host", "Waiting for the host's world", then
      "Downloading the host's world".
- [ ] Guest: the test kit is there, matching the host.
- [ ] Host: "<name> has joined", then "<name> enters the world as <companion>".
- [ ] Guest: the companion picker lists the party companions; pick one.
- [ ] Guest: "Joined <host>'s world".

## Chat and names

- [ ] Each side: Enter, type, Enter. The text appears over the speaker in both
      windows, wrapped, and stacks if you send a few quickly.
- [ ] Guest: clicking the host's character shows "Avatar (<host name>)".
- [ ] Host: clicking the guest's companion shows "<companion> (<guest name>)".

## Movement and leash

- [ ] Guest: walk away from the Avatar. Past one screen: "Straying from the
      Avatar" and a green edge arrow pointing back.
- [ ] Guest: keep going. The companion stops: "You must return to the Avatar".
- [ ] Host: walk the Avatar away from the guest. Guest: "The Avatar needs you",
      then about 5 seconds later "You are beckoned by the Avatar" next to the
      Avatar.

## Doors

- [ ] Host opens and closes each door; the guest sees both changes.
- [ ] Guest opens and closes each door from a normal double-click distance;
      both windows match.
- [ ] Guest tries a locked door (not in the kit; find one in town): "Locked",
      nothing changes.

## Shutters, lights and the lever

For each of: shutters, lamp post, sconce, light source, lever.

- [ ] Host toggles it; the guest sees it, including the light at night.
- [ ] Guest toggles it on and off; both windows match.
- [ ] At night, the guest's view brightens around a lit lamp near the guest's
      companion (lighting is measured from whoever the view follows).
- [ ] Lever: the guest's companion walks to it, not the Avatar.

## Secret walls

Black Gate's sliding secret walls are wall shapes 828 and 845. Double-clicking
one with quality 0 runs Usecode 0x33C/0x34D, which calls 0x81F/0x820 to slide
it, swapping between the two shapes with sounds 30/31. The kit places one of
each (quality 0). Other hidden passages are illusory walls you walk through,
which have no state to sync.

- [ ] Host slides each secret wall open and closed; the guest sees it.
- [ ] Guest slides each one open and closed; both windows match.
- [ ] When you find a real one in town or a dungeon, try it from the guest.

## Items from the host (W3)

- [ ] Host takes the food out of the kit's bag; the guest's bag empties too.
- [ ] Host drops an item on the floor; it appears on the guest.
- [ ] Host moves gold from the chest to the floor; the chest contents and the
      floor match on the guest.
- [ ] Guest has the chest open while the host changes it; the guest's chest
      window stays open and shows the new contents.

## Food, potions, keys and lockpicks

- [ ] Guest takes food from the supplies bag and double-clicks it: the
      guest's own companion eats and remarks on it, on both screens, and one
      food goes away on both sides. The host log names who ate ("used shape
      377 on <name>").
- [ ] Guest drinks a few potions; each one is used up on both sides and its
      message shows.
- [ ] Guest drinks a poison potion: on both screens the companion shows as
      poisoned and flashes red as it loses health.
- [ ] Guest double-clicks the quality-77 key, then clicks the locked chest:
      it unlocks on both sides, and both screens show "Unlocked".
- [ ] Guest uses the quality-78 key on the locked door: it unlocks on both.
- [ ] Guest uses a lockpick on the second locked chest. It may take a few
      tries, and a pick can break; both sides match after each try.
- [ ] Guest presses Escape at the crosshair: nothing happens.
- [ ] Guest double-clicks a locked chest: "Locked".
- [ ] Guest tries food from the Avatar's pack: refused.

## Teleporters

- [ ] Guest walks onto the west pad: the companion lands south of the east
      pad on both screens, and the Avatar stays put.
- [ ] Guest walks onto the east pad and comes back.
- [ ] Host walks onto a pad: the Avatar and the whole party, including the
      guest's companion, arrive at the other pad.
- [ ] Find a real teleporter in the game and try it from the guest.

## The Avatar must do this

- [ ] Guest double-clicks an NPC who isn't in the party: "The Avatar must do
      this" over them, and no conversation starts.
- [ ] Guest opens the kit's first chest (the one with gold) and
      double-clicks the bottle, then the torch: "The Avatar must do this"
      each time, and nothing happens on the host. A bed in town works too.

## Inventories and guest item moves (W4)

- [ ] Host moves an item within the Avatar's or a companion's inventory; the
      guest's paperdoll or pack shows it.
- [ ] Guest takes the food from the bag into the companion's pack; the host
      sees the bag empty and the companion holding it.
- [ ] Guest puts it back into the bag; the host sees it.
- [ ] Guest drops an item on the floor and picks it up again.
- [ ] Guest moves part of the chest's gold (the quantity prompt) into the pack;
      both windows show the split.
- [ ] Guest equips and unequips something on the companion's paperdoll.
- [ ] Guest tries to put something into the Avatar's pack: refused, and the
      guest's view snaps back.
- [ ] Guest has the chest open while the host drags an item out of it: the
      guest's chest window stays open and updates.

## Conversations

- [ ] Host talks to an NPC. The guest sees the portraits, text and choices,
      can't pick a choice or move, and sees "The Avatar is having a
      conversation. Please wait." at the bottom, not over the conversation.
- [ ] With the guest's chat line open, the bar sits above it.
- [ ] When the conversation ends, the bar disappears and the guest can move.
- [ ] Host uses a lockpick on an unlocked chest: a one-line remark with a
      portrait ("Try those on a locked chest or door."). The guest's wait bar
      goes away when it closes, with no "bye".

## Two people grabbing the same item (needs two testers)

A picked-up item disappears for everyone, so normally the other person can't
grab it at all. If both start a drag at nearly the same instant, the host takes
the first request and the second person's drag snaps back.

- [ ] Host picks up an item: the guest can't see it to grab it.
- [ ] Guest picks up an item: the host can't see it to grab it.
- [ ] Both try to grab the same item at the same moment: one gets it, the
      other's drag snaps back, and there is only one copy afterwards.
- [ ] Both take from the same gold stack at once; the totals add up on both
      sides afterwards.

## Host pause

- [ ] Host presses Escape for the menu. Guest: a bar at the bottom says "The
      host has paused the game." and stays; the companion can't move or
      interact; chat still works.
- [ ] Host closes the menu. The bar goes, and nothing snaps back.
- [ ] Host starts dragging an item out of the chest: it disappears on the
      guest at once and reappears where the host drops it.
- [ ] Guest starts dragging an item out of the chest: it disappears on the host
      at once. Dropping it places it on both; cancelling the drag puts it back
      on both.
- [ ] Guest drops part of a gold stack: the dropped part lands, and the rest
      goes back to where it was on both sides.

## Creatures

- [ ] Guest joins near some animals or monsters: each one on the guest is in
      the same place as on the host, and they move the same way.
- [ ] Host presses **Shift+F9**: a rat, a wolf and a rabbit appear near the
      Avatar on both screens.
- [ ] The creatures walk around: the guest sees each one move the way it does
      on the host (no creature moving in another's place).
- [ ] The host's party kills one: it disappears on the guest, and its body
      appears on both.
- [ ] Host walks the party well away from them (into the next town area) and
      comes back: the creatures are where the host has them, with none
      doubled or missing.
- [ ] Guest walks over a spot where the host's monsters appear (a monster
      egg): no creature appears only on the guest.

## Clock and weather

- [ ] Guest joins: the time of day looks the same on both (light, palette).
- [ ] Host turns on cheats and presses **Ctrl-T** a few times to skip ahead
      three hours each: the guest's light changes with the host's, including
      into night.
- [ ] Guest opens the clock or watch (if the party has one) and compares with
      the host: the same time.
- [ ] When it rains, snows or clouds over on the host, the guest gets the same
      weather, and it stops when the host's stops.
- [ ] After a few game hours, the guest's companion's food level matches the
      host's and the guest shows no hunger messages of its own.

## Saving, loading and cheats

- [ ] Guest presses Escape: the menu has no Load/Save entry.
- [ ] Guest presses S, Ctrl-S and Ctrl-R: "The host saves the game" each time.
- [ ] Guest tries to turn on cheats (Game Engine options) or presses a cheat
      key such as Ctrl-T: nothing happens.
- [ ] Host loads a save: the guest shows "The host loaded a saved game", ends
      up in the same world as the host, and controls the same companion.
- [ ] Host loads a save where that companion isn't in the party: the host
      says so and the guest is left watching.

## Bodies and temporary objects

- [ ] Kill a creature (Shift+F9 helps), walk the Avatar about one and a half
      screens away: the body is still there on both when you come back.
- [ ] Walk much further away and back: the body is gone on both (as in U7).
- [ ] With the guest's companion standing by a body, walk the Avatar away a
      screen and a half: the body stays on both.

## Leaving

- [ ] Close the guest. Host: "<name> has left; <companion> rejoins the party".
- [ ] Rejoin, then close the host. Guest: "Host disconnected. Exult will
      close." and it exits after about 4 seconds.

## Known gaps to ignore for now

- Theft by guests. (Guests can't give items to other party members; that is
  by design.)
- Stats other than health, food level and status effects (poison, sleep,
  paralysis, charm, curse, protection, might) aren't mirrored to the guest.
- Creature health and combat (Phase 5); creatures only move and appear or
  disappear on the guest.
- Doors whose script swaps to a shape that isn't flagged as a door.
- Sounds for changes made on the other machine.
