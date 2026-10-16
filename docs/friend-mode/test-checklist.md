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
      window closes and reopening shows the new contents.

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

## Host pause

- [ ] Host presses Escape for the menu. Guest: "The host has paused the game",
      and the companion can't move or interact; chat still works.
- [ ] Host closes the menu. Guest: "The game resumes", and nothing snaps back.

## Leaving

- [ ] Close the guest. Host: "<name> has left; <companion> rejoins the party".
- [ ] Rejoin, then close the host. Guest: "Host disconnected. Exult will
      close." and it exits after about 4 seconds.

## Known gaps to ignore for now

- Using items with a target, such as keys on locks: guests can't run item
  scripts yet.
- Theft by guests, giving items to other party members, and keys.
- Creatures that appear after joining, such as a cat wandering in.
- Doors whose script swaps to a shape that isn't flagged as a door.
- Sounds for changes made on the other machine.
