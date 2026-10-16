# World sync: items, containers and doors (plan)

**Status:** W1 (join with the host's live save), W2 (doors and other toggles) and W3 (host item changes, by chunk reconciliation) built; W4 planned. This is Phase 4 of the
[project brief](../ultima-7-friend-mode.md).

## What's broken today

- A guest taking bread from a bag only changes the guest's own world. The host
  still sees the bread in the bag, and putting it back is invisible to the host.
- Doors opened by the Avatar don't open for guests, and guests can't open doors
  at all (door use runs Usecode, which guests may not start).
- Underneath both: the guest plays in **its own older copy of the world**. Only
  party membership is copied from the host. Items, containers, doors, quest
  flags and NPC state can all differ before anyone touches anything.

## Principles

1. **The host owns every object.** Guests never change the world directly. They
   ask the host, and the host performs the change with Exult's own code.
2. **Guests start from the host's world.** Fixing changes after join is
   pointless if the starting worlds differ.
3. **Record changes where the engine makes them.** Hook the few engine places
   where objects are added, removed, moved or change frame, so every change is
   caught: the Avatar, NPCs, Usecode, combat loot and guests alike.
4. **Only what's near players.** Send changes within the area the host keeps
   loaded around the Avatar. The leash keeps guests inside it.

## Design

### 1. Join with the host's current world

- When a guest joins, the host writes its current game to a temporary save
  (the same code as a normal save, about 190 KB) and sends it to the guest.
- The guest loads it before picking a companion. Party, quest flags, items,
  doors and NPC schedules then all match the host at that moment.
- **Transport change:** today every packet is 20 bytes with 12 bytes of data,
  which would take about 16,000 packets for a save. Add one variable-length
  message type (length + payload) for bulk data. Keep the existing packets
  as-is.
- This also removes the stale-world problems seen so far: Spark not in the
  party, and NPCs out of step far from the Avatar.

### 2. Referring to an object across machines

Exult has no stable object IDs, but after the join both machines hold the same
world, so an object can be found by where it is and what it is:

| Where | Reference |
| --- | --- |
| On the map | tile (x, y, z) + shape + frame |
| In a container | the container's reference + shape + frame + position in its contents |
| In an actor's inventory | NPC number + shape + frame (+ slot if equipped) |

Quantity and quality go with the object's data, not its reference. If a
reference can't be resolved, the guest asks for a resync of that chunk (see 5).

### 3. Host to guests: object changes (built as chunk reconciliation)

Hooking every engine path that adds, removes or moves an object turned out to
be fragile: dragging, Usecode creation and deletion, combat drops, theft and
stacking all take different paths, and the low-level chunk add/remove is also
used for map loading and NPC movement. Item sync reconciles state instead:

- Every 250 ms the host serializes the movable (ireg) objects in the loaded
  chunks within 3 chunks of the Avatar and 2 of the guest's companion: shape,
  frame, tile, quality (which is also the count for stackables), flags and
  container contents recursively. Actors, eggs and barges are excluded.
- Each chunk's sorted list is fingerprinted (FNV-1a). The first fingerprint of
  a chunk is the baseline (right after a join both sides match); when it
  changes, the host sends the chunk's whole list as a `chunk_objects` blob.
- The guest keeps objects whose serialized form matches exactly, deletes the
  rest (closing any open container window and deleting contents first, so
  nothing spills) and creates what is missing.

Door and toggle changes still go out immediately through the Usecode hooks;
reconciliation also catches anything those miss. This makes W5's drift
recovery largely automatic for the area around the players.

### 4. Guest to host: actions

| Guest action | Request | Host checks | Host does |
| --- | --- | --- | --- |
| Open or close a door | `use_door(ref)` | in reach, really a door, not locked, shape allowed | Exult's NPC door trick: quality to 0, activate, restore (`actions.cc:383`) |
| Take an item | `move_item(ref, to = own inventory)` | in reach, not someone else's inventory, weight and volume | the normal engine move |
| Drop or put an item | `move_item(ref, to = map tile or container)` | owns it, destination in reach and fits | the normal engine move |
| Open a container | (none) | — | stays local: viewing doesn't change the world |

- The guest **waits for the host** instead of moving the item locally first.
  On a LAN that's a few milliseconds. It avoids having to undo a move the host
  turns down.
- The host handles requests one at a time, so if two players grab the same
  item, the first one gets it and the second sees it disappear.
- **Theft:** taking items marked as not okay to take sets off the game's own
  theft logic on the host, the same as when the Avatar steals. Guards come for
  the party.

### 5. Recovering from drift

If a guest can't resolve a reference, or a periodic checksum of a chunk
doesn't match, the guest asks for that chunk's objects and the host sends
them. This is the safety net, not the normal path.

## Milestones

| # | Slice | Done when |
| --- | --- | --- |
| W1 | Variable-length messages; the guest joins using the host's live save | A guest joining mid-game sees exactly the host's world: items, doors, party |
| W2 | Host change hooks for frames, plus guest door use | The Avatar opens a door and the guest sees it; a guest opens an unlocked door and the host sees it |
| W3 | Chunk reconciliation for objects on the map and in containers (built) | The Avatar picks up or drops items and the guest sees it |
| W4 | Guest item requests: take, drop, put in a container, move within own inventory | Spark takes bread from a bag; the host sees the bag without it and Spark holding it |
| W5 | Chunk resync, theft, two players grabbing the same item, a long two-player session | No drift after 30 minutes of looting, fighting and doors |

W1 comes first because every later step assumes both worlds start the same.

## Out of scope here

- Combat sync (damage, death, loot drops from creatures beyond the add hook).
  That's Phase 5, but the add hook from W3 already covers loot appearing.
- Locked doors and keys: the first version only allows unlocked doors for
  guests.
- More than one guest. The [multi-guest plan](multi-guest-plan.md) adds that;
  everything here is written per guest so it carries over.

## Open questions

1. **Inventory between companions:** should a guest be able to give items to
   another party member (Avatar or companion) by dragging onto their paperdoll?
   The interaction policy currently limits guests to their own inventory.
2. **Saves:** only the host saves. Should the host's save also be sent to
   guests on request, so a guest can keep a copy?
3. **Locked doors:** later, should a guest holding the right key be able to
   unlock a door?
