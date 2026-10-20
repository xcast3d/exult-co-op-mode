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

## Co-Op Mode menus

Start both without the Friend Mode options:
`./exult --bg --nocrc` (host) and
`./exult --bg --nocrc -c .friend-mode-local/guest.cfg` (guest).

- [ ] Host title menu: "Co-Op Mode" shows below Journey Onward.
- [ ] Host: Co-Op Mode, then Host a Game: back at the main menu, the line
      reads "Co-Op Mode: Hosting".
- [ ] Host: Co-Op Mode again: it offers Stop Hosting; Back and Escape
      return to the main menu without quitting.
- [ ] Host: Journey Onward: "Co-Op Mode is open on port 1992".
- [ ] Guest: Co-Op Mode, then Join a Game: player name and address, then
      the host's world downloads and the companion picker shows.
- [ ] Without hosting, start a game, press Escape: "Open to Co-Op" is
      there. Pick it: the menu closes and "Co-Op Mode is open on port
      1992" shows; a guest can then join. Escape again: the button is gone.
- [ ] On the guest, the Escape menu has no "Open to Co-Op".

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
- [ ] Guest double-clicks a bed in town: "The Avatar must do this", and
      nothing happens on the host.

## Everyday objects and shortcut keys

- [ ] Guest takes the bottle from the kit's gold chest and double-clicks it:
      the companion drinks it on both screens.
- [ ] Guest double-clicks the torch from that chest: it lights on both;
      again, and it goes out.
- [ ] Guest double-clicks a grandfather clock in town, and a pocketwatch if
      the companion has one: the time shows over it on both.
- [ ] Guest presses F with food in the companion's pack: the companion eats
      on both screens (not the Avatar).
- [ ] Guest presses K, then clicks the kit's locked chest, with its key in
      the companion's pack: it unlocks on both.
- [ ] Guest presses M: the map opens on the guest only.
- [ ] Guest double-clicks a sign, a book and a scroll (any in town): the text
      opens on the guest only; the host sees nothing.
- [ ] Guest uses a telescope or sextant if you find one: it works on the
      guest only.

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

## Creatures around the guest

Best in Trinsic near the start of the game (the stables).

- [ ] Guest walks about a screen away from the Avatar toward the stables:
      the horses appear on the guest without the host coming closer, and
      on the host if it walks over.
- [ ] Guest stands near them with the Avatar a screen away: they move
      (they don't freeze) on both screens.
- [ ] Guest walks past a spot where monsters appear when the Avatar walks
      there (a dungeon entrance, a road at night): they appear for the guest.
- [ ] Guest walks over a spot that starts music or a script for the Avatar:
      nothing happens until the Avatar goes there.

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

## Painting, coins, theft and sleepers

- [ ] Guest takes the brush and pigments from the supplies bag, double-clicks
      the brush, clicks the pigments, then the kit's easel: the painting
      appears on both, with a remark.
- [ ] Guest double-clicks the pigments alone: a remark, nothing painted.
- [ ] Guest double-clicks the easel: a remark on the guest only.
- [ ] Guest double-clicks gold with Iolo near the Avatar: the Avatar and
      Iolo trade "Call it." and heads or tails, on the guest only; the host's
      windows stay open.
- [ ] In a shop, the guest takes an item that isn't theirs while the
      shopkeeper is watching: a warning over the shopkeeper (on both); doing
      it again a few times calls the guards.
- [ ] Out of sight of anyone: sometimes "heard something", no guards.
- [ ] Guest moves an item inside its own pack: never counts as theft.
- [ ] At night, the guest's companion walks right past a sleeping NPC (with
      the Avatar further away): after a few tries the NPC wakes, says
      something and goes back to sleep later.

## Spectating and switching companions

- [ ] Guest presses Escape at the companion picker: the bar says "You are
      Spectating. J: choose a companion. X: watch another player."; the view follows the
      Avatar as the host walks, creatures and items keep updating, and chat
      still works.
- [ ] Guest presses J and picks a companion: the bar goes and the guest
      controls it.
- [ ] Guest presses J again and picks another: "<name> switches to
      <companion>" on the host; the first companion follows the party again.
- [ ] Guest presses J during a host conversation: a red X, nothing opens.
- [ ] Host loads a save without the guest's companion, then one with it:
      the guest watches, then picks it again with J.
- [ ] Guest opens Game Engine options: there is no Cheats line.

## Combat

Shift+F9 on the host gives a rat, a wolf and a rabbit to fight.

- [ ] Guest presses C: the guest is in combat mode; the guest's view of the
      Avatar doesn't start fighting on its own.
- [ ] Guest double-clicks the wolf: the companion walks up and attacks it on
      both screens, with attack frames. The host log says "<companion>
      attacks Wolf for guest".
- [ ] When the wolf dies, the companion stops and waits; its body shows on
      both. Host log: "guest's attack ended (foe defeated)".
- [ ] Guest double-clicks the rat, then walks away mid-fight: the companion
      stops fighting and follows the guest's steps ("guest moved").
- [ ] Guest double-clicks a peaceful townsperson in combat mode: "The Avatar
      must do this", and nothing happens on the host.
- [ ] Guest double-clicks a door in combat mode: it opens as usual.
- [ ] Host toggles combat mode while the guest's companion fights: the
      companion keeps its foe, or stops and waits; it never wanders off
      following the Avatar.

## Missiles, spells and health

- [ ] Give the guest's companion a bow and arrows (or a crossbow), fight a
      creature: the arrows fly on both screens.
- [ ] Host's Avatar or another companion shoots or throws something: it
      flies on the guest too.
- [ ] Host casts a spell with a missile or a visible effect (fireball,
      lightning, heal): the guest sees it.
- [ ] With the combat mode option not "Original", in combat mode the guest
      clicks the wolf during a fight: the number after "Wolf" goes down as
      it is hit, matching the host.
- [ ] Guest turns on Game Engine → "combat shows hits": "-6(2)" style
      labels show over whoever is hit, on the guest too (even with the
      host's setting off).
- [ ] A cat or other animal near the guest, a screen from the Avatar:
      it moves on the guest.
- [ ] Nothing extra happens on the guest: no second hit, no extra arrows
      left on the ground.

## Sounds and health bars

- [ ] Host opens a door near the guest: the guest hears it. Guest opens
      one: both hear it once (not twice on the guest).
- [ ] Guest slides a secret wall or pulls a lever: both hear it.
- [ ] Guest's companion eats or drinks: the sound plays on both.
- [ ] A fight near the guest: hit sounds play on the guest; an explosion
      (if any) too.
- [ ] Birds and crickets play on each machine as before, without doubling.
- [ ] Game Engine → "Enemy health bars" on: a hurt wolf gets a small
      green and red bar that shrinks as it is hit; peaceful NPCs and the
      party get none. Moving creatures leave no trails of old bars.
- [ ] The same on the guest with its own option on.

## Trainers

Markus trains in Trinsic. The companion needs training points (from
experience) and the party needs gold.

- [ ] Guest double-clicks Markus: the conversation opens on the guest only,
      with the companion's portrait.
- [ ] "train": "Which of you wishes to train?" lists only the guest's
      companion (and "Nobody").
- [ ] Train: the gold leaves the party and the companion's stats and
      training points change, on both machines (stats window).
- [ ] With no training points: Markus says the companion needs more
      experience; nothing changes.
- [ ] With too little gold: refused; nothing changes.
- [ ] Host trains with Markus as the Avatar: the whole party is listed, as
      usual.
- [ ] After the guest's conversation ends, the host log shows nothing
      refused and the guest's packs still match the host's.

## Stats and experience

- [ ] Guest opens its companion's stats (paperdoll, stats button) and the
      host opens the same: every number matches, including experience,
      level and training points.
- [ ] The guest's companion kills a creature (Shift+F9): its experience
      goes up on both.
- [ ] Host raises the companion's experience past a level (cheat screen,
      or more fights): the guest's stats window shows the new level and the
      same training points as the host's.

## Shops

Trinsic has Dell's shop (weapons, armour, provisions); go during shop hours.

- [ ] Guest double-clicks Dell: the conversation opens on the guest only;
      the host sees nothing and can keep playing.
- [ ] The guest's side of the conversation shows the companion's portrait,
      not the Avatar's.
- [ ] "job", then "buy", then a category: Dell's wares and prices show.
- [ ] Buy something: the gold leaves the party and the item appears in the
      guest's companion's pack, on both machines (another member's pack
      only if the companion's is full).
- [ ] Buy something the party can't afford: Dell refuses, nothing changes.
- [ ] Outside shop hours (Ctrl-T on the host to skip time): Dell says to
      come back during business hours, as for the Avatar.
- [ ] Guest double-clicks a non-merchant: "The Avatar must do this".
- [ ] At a mage who sells spells (Nicodemus, Rudyom): trying to buy a spell
      says "The Avatar must do this" and takes no gold.
- [ ] Host double-clicks Dell afterwards: the conversation is the Avatar's
      as usual, unaffected by the guest's chat.

## Journal

- [ ] Host writes a note and closes the journal (N, type, Escape). Guest
      opens the journal: the note is there.
- [ ] Guest writes a new note and closes the journal. Host opens it: the
      note is there, at the end.
- [ ] Guest changes an old note and clicks into another note: the host's
      journal shows the change (open it again if it was open).
- [ ] Both have the journal open. Host types in a note: on the guest, the
      host's name shows next to that note's date, and typing in it gives a
      red X. When the host clicks elsewhere, the name goes and the guest
      sees the new text.
- [ ] The same the other way: guest types, host gets the red X.
- [ ] Both type in the same note at the same instant: one keeps it; the
      other's change goes back.
- [ ] An automatic story note on the host (if one comes up) shows on the
      guest.
- [ ] Host loads a save: the guest's journal matches the save's.
- [ ] Host opens the journal: the guest sees "Someone is writing in the
      party journal." at the bottom until the host closes it.
- [ ] Guest opens the journal: the host sees the same bar.
- [ ] With the bar showing, open your own journal: the bar is still readable.

## Music

Turn music on in the guest's audio options if it is off.

- [ ] Guest joins: it plays the same tune as the host.
- [ ] Host walks into a place with its own music (a town, a dungeon): the
      guest's tune changes with it.
- [ ] Kill a creature (Shift+F9): the combat and victory music plays on the
      guest when it does on the host.
- [ ] Guest walks alone past a spot that starts music: nothing changes on
      the guest unless the host's music changes.
- [ ] Guest's companion alone (host not in combat) attacks the wolf: the
      battle music starts on the guest, and the victory tune plays when it
      dies.
- [ ] Host disables audio, then the guest fights the rat: the guest still
      plays the battle and victory music.

## Two guests (Phase 6)

Three windows on one Mac: the host and two guests, each guest with its own
config and gamedat. The host needs a party with at least two companions
(Iolo and Spark, or later). The second guest's config has audio off.

```bash
./exult --bg --nocrc -c .friend-mode-local/guest2.cfg --friend-join 127.0.0.1
```

- [ ] Guest 1 joins and takes Iolo; guest 2 joins. Guest 2's picker shows
      only the companions nobody has (not Iolo).
- [ ] Guest 2 takes Spark. Host: "<name> enters the world as Spark".
- [ ] Each guest sees the other's companion walk (not stand still or run
      its own AI), and the Avatar.
- [ ] Clicking another guest's companion shows `Spark (Name)` on every
      machine.
- [ ] Chat from guest 1 shows over Iolo on the host and on guest 2; chat
      from guest 2 over Spark on the host and guest 1; host chat over the
      Avatar on both guests.
- [ ] Both guests pick up, drop and use items; each guest's held item is
      its own (guest 2 can't take what guest 1 holds).
- [ ] Each guest's leash works on its own ("The Avatar needs you").
- [ ] Guest 1 writes in the journal: the host and guest 2 see the bar and
      guest 1's name on the note.
- [ ] Guest 1 presses J and picks a companion guest 2 has just taken:
      "That companion is taken".
- [ ] Close guest 1: "<name> has left; Iolo rejoins the party"; guest 2
      keeps playing.
- [ ] The host loads a save with both connected: both get the new world
      and their companions back.
- [ ] Join, "enters the world as", switch and leave notices show on the
      host and on both guests.
- [ ] Guest 1 presses J and moves from Iolo to a free companion (Dupre,
      Shamino, or whoever is in the party): everyone sees "<name> switches
      to <companion>"; Iolo follows the party again on all three screens.
- [ ] Guest 2 presses J: Iolo is offered now, and guest 2 takes it.
- [ ] In combat mode, a guest switches away mid-fight: the companion left
      behind keeps fighting.
- [ ] Both guests press Tab and pick the same free companion: the second
      gets "That companion is taken" and the picker again.
- [ ] J does nothing (red X) while the Avatar talks to someone.

## Co-op quality of life (built 2026-10-17, protocol version 29)

- [x] **Player list:** hold Tab on the host and on a guest: Player,
      Character and Health for everyone; spectators show "Spectating", a
      following companion "(following)". Let go and it closes.
- [x] **Picker on J:** J opens the companion picker (J does nothing in the
      journal or chat line).
- [x] **Picker card text:** a long description ("Longtime friend of the
      Avatar") wraps inside the card.
- [x] **Rejoin:** close a guest that has Iolo: "<name> has left; Iolo
      follows the party until they return", and the other guest's picker
      doesn't offer Iolo. Rejoin with the same name within 3 minutes: no
      picker, straight into Iolo, "<name> is back as Iolo".
- [ ] **Rejoin too late:** wait 3 minutes instead: "Iolo is free again".
- [x] **Chat log:** chat, whispers and session notices (joins, leaves,
      switches, autosaves on the host) show bottom left, 20 pixels above
      the bottom, in white letters the size of overhead chat (whispers in
      purple), and fade after about 20 seconds; L hides and shows it, with
      "Chat Log Disabled" / "Chat Log Enabled" (those work). It stays put
      while you walk. (2026-10-18: it showed top left and vanished on
      walking; fixed, retest.) Spectator chat shows in the
      log (near the top only while the log is hidden).
- [x] **Notices in one place:** a join, leave or switch shows in the log
      only (not near the top too); with the log hidden (L), near the top
      only.
- [x] **Log above the bars:** while spectating (or following, or with the
      host paused), the log sits above the bar, and above the chat line
      too when that is open.
- [x] **Notices stay on screen:** joins, switches, "Chat Log Enabled" and
      spectator chat with the log hidden show centred near the top and
      stay there while you walk, then go after a few seconds.
- [x] **Whispers:** `/w <name> text` from a guest to the other guest, to
      the host, and from the host to a guest: only those two see it,
      "[to Name]" and "[Name whispers]". A name with a space works; an
      unknown name gets "Whisper to whom?".
- [x] **Tab completion:** in the chat line, `/w J` + Tab gives `/w Jeff `;
      Tab again cycles to the next player starting with J; `/wa` + Tab gives
      `/wave `; no match flashes a red X. Holding Tab outside the chat line
      still shows the player list.
- [x] **Emotes:** `/wave`, `/bow`, `/point`, `/cheer` pose for a moment;
      `/kneel` and `/sit` until you move. Everyone sees "*waves*" over the
      actor (the asterisks were invisible: font 0 has none, so one is drawn
      in now), and "Marc waves." in the log. From the host (the Avatar) and from a guest. A spectator gets
      "Spectators have no one to move"; anything else starting with "/"
      lists the commands.
- [x] **No arrows to players:** off-screen companions of other players
      get no edge arrow (removed); a straying guest still gets the green
      arrow back to the Avatar.
- [x] **Spectator camera:** a spectating guest presses X: "Watching Iolo
      (Marc)", the view follows Iolo; X again goes to the next player and
      back to the Avatar. If the watched player switches or leaves, the
      view goes back to the Avatar.
- [x] **Autosave:** with a guest connected, the host autosaves every 5
      minutes ("Autosaved" in the host's log) into "Co-Op Autosave" slots
      997-999, over the oldest; your own saves stay untouched. Load one.
- [ ] **Held item in saves:** a guest holds an item (mid-drag) while the
      host saves or another guest joins: the save, and the newcomer's
      world, still have the item.

## Main Menu, Suggest a reply, WASD (built 2026-10-18, protocol version 30)

- [x] **Main Menu:** Escape → Main Menu → "Return to the main menu?" Yes:
      Exult restarts at the title menu without the intro, and you can host
      or join from there. On the host, guests see it leave; on a guest,
      the host keeps its companion for a rejoin. No leftover window or
      process. Answering No stays in the game.
- [x] **Suggest a reply:** the Avatar talks to an NPC; a guest clicks a
      choice: it turns that guest's color on the host and every guest,
      and the log says "<name> suggests: <choice>". The guest clicks
      another: the first goes back to yellow. Two guests: both colors at
      once. The Avatar's pick clears them. A click elsewhere does nothing
      new.
- [ ] **WASD off** (the default): W uses the watch, S opens Save/Restore,
      as before.
- [x] **Chat during a conversation:** while the Avatar talks, a guest's
      chat (and a whisper) shows on the host and the other guest at once,
      not only when the conversation ends.
- [x] **Host chats during its conversation:** Enter opens the chat line
      over the conversation, typing doesn't pick choices, Enter sends (the
      guests see it at once), Escape closes it without leaving the
      conversation. Choices still work with clicks and 1-9.
- [x] **No overhead chat during a conversation:** chat then shows only in
      the log (near the top with the log hidden), not over anyone's head;
      overhead chat is back once the conversation ends.
- [ ] **WASD on** (Game Input → "WASD movement: Yes"): W, A, S, D walk; two
      keys walk diagonally; letting go stops (walking is fluid, 2026-10-18;
      slower in combat, as U7 always is). Shift+W shows the watch,
      Shift+S opens Save/Restore, Ctrl+S quicksaves. Typing in the chat
      line or the journal doesn't walk. The setting is kept after a
      restart.
- [x] **WASD in combat:** C toggles combat; walking with WASD in a fight
      (F9 with Shift spawns test creatures) never leaves the Avatar stuck
      walking; double-click attacks as usual.
- [x] **Targeting (WASD on), out of combat:** Q outlines the nearest
      person or creature on screen (townsfolk too) and nothing is
      attacked; Q again goes to the next nearest, Shift+Q back. Nobody on
      screen: a red X.
- [ ] **Arrow, clearing:** the pick gets a small arrow over its head,
      white out of combat and red in combat mode (no outline; a hit still
      flashes red); it follows the target and leaves no marks. Q past the last target clears it; the next Q
      picks the nearest again (Shift+Q likewise, from the far end). Each
      pick shows "Targeting <name>" near the top, replacing the last one;
      clearing shows "No target" (also when the target dies).
- [ ] **Targeting toggle:** Game Input shows "WASD movement" and
      "Targeting (Q, Space)" as separate options; targeting is on in a
      fresh config; off, Q does nothing, Space pauses combat as before and
      any arrow goes; the setting is kept after a restart.
- [x] **Peaceful targets in combat:** in combat mode Q picks hostiles
      first, then townsfolk and animals; a hostile pick says "Attacking
      Wolf" and is attacked, a peaceful one says "Targeting Deer" and
      isn't until Space ("Attacking Deer").
- [x] **Stopping:** in a fight through targeting, Q past the last target
      ("No target") stops the Avatar attacking; so does walking (WASD or
      right-click), with the arrow still on the target, and Space attacks
      it again. With nothing picked and Q never used, C fights as U7 always
      did. Save mid-fight, load: the Avatar's attack mode is its own.
- [x] **Guests target too:** with WASD off on a guest, Q picks and the
      arrow shows; in combat mode the companion attacks the pick; clearing
      it stops the companion; walking stops it as before.
- [ ] **Space attacks the target:** pick a townsperson with Q out of
      combat, press C: nothing; press Space: combat comes on and the
      Avatar attacks them. In a fight with a hostile picked, Space pauses
      combat as before (if that combat mode pauses). On a guest, Space on
      a peaceful pick gets "The Avatar must do this".
- [x] **Targeting in combat:** with combat on (C) and test creatures
      (Shift+F9), Q cycles only hostiles and the Avatar attacks each pick.
      Picking a wolf out of combat, then C: the Avatar attacks the wolf.
      Picking a townsperson, then C: nobody attacks them. The outline goes
      when the target dies.
- [x] **WASD on a guest:** walking works for a guest's companion (the
      host's leash and refusals apply).

## Fixes from the first internet test (built 2026-10-19, protocol version 31)

Found when a second person joined over the internet (host's public
address). Best tested over the internet again, the guest at a large
resolution.

- [ ] **Join screen:** no flicker. The player name and host address from
      the last join are filled in; Enter alone connects (and the same
      name gets the companion straight back).
- [ ] **Stuck conversation bar:** the host talks to someone (Chantu, the
      Trinsic healer, ends with Escape); the guest's "The Avatar is
      having a conversation" goes when it ends. If it ever stays, it
      should go by itself: the host's log says "conversation left open;
      ending it for guests", or the guest's says "the Avatar moved; ending
      a conversation we never saw end" once the Avatar walks.
- [ ] **Running as a guest over the internet** (locally fine, 2026-10-19;
      waits for the next Windows build): fastest mouse speed and
      WASD walk smoothly, no stutter, flicker or pulling back. A step the
      host refuses (the leash) still snaps the companion back once.
- [x] **Knocked out is not done:** Space on a deer (or a hostile) in
      combat: the attack goes on after it falls unconscious, until it is
      dead; then the next hostile on screen is picked and attacked, never
      a peaceful one, or "No target". On the host and on a guest.
- [x] **Minimap size:** with the minimap on, a large game area (Video
      options) shows it the same size as at a small one; the HUD size in
      the UI options changes it. Still hidden in dungeons and menus.
- [x] **Guest at a merchant:** only name, job, bye and the trade answers
      (Dell: buy, then weapons, armour, provisions, nothing). No "murder"
      or "Fellowship". The shop and training still work, including Perrin
      (training from "job") and Snel.

## Targeting for everything (built 2026-10-19, protocol version 31)

- [x] **Space talks:** out of combat, Q to a townsperson, Space: the
      conversation starts. On a guest: a merchant talks, anyone else says
      "The Avatar must do this". Space on a wolf out of combat does not
      start a fight; C attacks it (hostile pick), and C then Space attacks
      a peaceful one.
- [x] **Party in Q:** out of combat the Avatar's Q reaches Iolo and the
      rest after everyone else; Space talks to them. In combat they aren't
      offered, and a party pick clears when combat starts. Not on guests.
- [x] **E:** cycles chests, crates, barrels, bags and bodies on screen,
      nearest first (Shift+E back), with a green arrow; past the last,
      "No target". Space opens the pick, or "Too far away". Q after E (and
      E after Q) replaces the pick. A pick taken by someone goes.
- [x] **Y follows** on a guest (E no longer does).
- [x] **Range frame:** at a small game area nothing shows. At a game area
      wider or taller than 81 tiles (648 pixels), combat mode or Q/E shows
      a faint square with a border around your character; Q picks nothing
      outside it.
- [ ] **Guest at a large resolution:** Q a deer at the edge of the guest's
      screen (off the host's), C or Space: the companion goes after it
      and fights. The deer exists on the guest out to the frame.

## Targeting, second round (2026-10-19)

- [x] **E reach** and **doors** below: tested on one Mac, working.
- [ ] **Range circle:** at a large game area, combat mode or Q/E shows a
      faint circle with a border, wholly in view (it shrinks to fit the
      shorter side). Small game areas: none.
- [x] **E reach:** E skips containers you can't reach (one in the next
      building, behind a wall); every pick opens with Space.
- [x] **Doors:** stand still in front of a closed door, then walk into it
      (WASD, and holding the mouse): it opens and you walk through. Walking
      into it at an angle, too. A locked door says "Locked" (once every
      1.5 seconds). On a guest, the host opens it for everyone.

## Targeting things (built 2026-10-19)

- [x] **E in the world:** a chest, a body and a dagger on the floor:
      nearest first, the dagger included; a loose item across town is not.
- [x] **E in a window:** Space opens the chest; E now cycles its items,
      with a green arrow over each in the window (also with the window
      scaled in UI options); Space takes one into the pack (a stack of gold
      with no "how many?") and the next is picked. Escape closes the
      window and E is on the world again.
- [x] **Staying in place:** a chest with 12 things, E to the 9th, Space:
      the pick moves to what is now 9th (the 10th before), not back to the
      first; taking the last picks the one before it. Dropping from your
      own pack does the same.
- [x] **Shift:** Shift+Space on 30 gold: "How many?" 10 → 10 taken, 20
      left. Shift+Space on a single item in a body: everything taken.
      Shift+Space on a bag on the floor picks the bag up.
- [x] **Own things:** backpack open, E on a potion, Space drinks it; on a
      spare sword, Space wields it and the old one goes to the pack;
      Space on the wielded one puts it away; on a gem, "Drop the gem?
      Space again", and the second Space drops it beside you.
- [x] **Not yours:** a shop item: the first Space warns, the second takes
      it, and theft follows as usual. Shift+Space on a shop chest asks once.
- [x] **Too heavy:** refused with the usual cursor; the item stays.
- [ ] **On a guest:** the same, with the items moving on both machines.

## Targeting options and groups (built 2026-10-19)

- [x] **Notices:** "Targeting 10 gold coins" (not "10 10").
- [x] **Options page:** Escape → Target System: "Enable" first; set to
      No, the other lines hide. Nine lines in all, loot with "(gold, gems,
      equipment)". Game Input no longer has Targeting. OK keeps changes
      (also after a restart), Cancel drops them.
- [x] **Signs:** E offers a sign, plaque or tombstone within reach; Space
      reads it.
- [x] **Moving restarts the cycle:** Q twice (second nearest), take a step,
      Q: the nearest again. Standing still, Q goes on, then "No target",
      then round. Same for E.
- [x] **Groups:** in a house, E skips chairs, candles, plates, cups and
      papers by default; turn decor or furniture on and they come in. With
      doors and lamps on, E offers a door and Space opens it; Space on a
      candle lights it. A lever (levers on by default): Space pulls it.
- [x] **Circle and party:** "Show range circle: No" hides it; "Q: party
      out of combat: No" leaves companions out of Q.
- [x] **Targeting off:** Q, E and Space do nothing new; Space pauses
      combat as in Exult.
- [x] **Backpacks:** Shift+I opens the Avatar's backpack, again Iolo's,
      and so on; with all open, Shift+I shuts them all. Shift+1 opens the
      Avatar's, Shift+1 again shuts it; Shift+2 Iolo's. E cycles what is
      open. On a guest, only its companion's opens.
- [x] **Paperdolls:** 1 opens the Avatar's, 1 again shuts it; 2 Iolo's.
- [x] **Space closes signs:** Space on a sign reads it; Space again
      closes it. A book: Space turns the pages, then closes. A crosshair
      (a key on a door) is not answered by Space.

## Reading with Space (2026-10-19)

- [x] **On the floor or in a chest:** E on a book, scroll or letter: Space
      reads it and leaves it there; Shift+Space picks it up (one item,
      not the whole chest).
- [x] **In your pack:** Space reads it; Shift+Space drops it beside you,
      with no confirm.

## Tidy containers (2026-10-19)

- [x] **Smart placement:** with the backpack open, take five things with
      E and Space: each lands in a free place, not on top of each other in
      the top left. Buy something from a shop: same.
- [x] **Shift+C:** a messy backpack open: Shift+C lays it out by kind,
      in rows across the whole window, top to bottom; look-alikes (bread,
      arrows of one kind) stacked a couple of pixels apart. A chest open
      and no backpack: "No backpack open".
- [x] **Sort button:** "Sort", a little below the checkmark, on backpacks
      and bags in someone's inventory only (not chests, bodies,
      paperdolls); it sorts that window.
- [x] **Options:** Target System page: "Smart pickup placement" and "Sort
      container (Shift+C)" show even with Enable off; turning them off
      brings back the old pile, and Shift+C does nothing.

## What Space does (2026-10-19)

- [x] Under each "Targeting ..." a second line: a person "Talk (Space)"
      (a deer or rat "Attack (C, then Space)"; on a guest, anyone but a
      merchant or trainer "Avatar only")
      (in combat "Attack (Space)", while attacking it "Pause (Space)"); a
      chest "Open (Space)", a bag on the floor "Open (Space)  Pick up
      (Shift+Space)"; a stack in a chest "Take (Space)  Split
      (Shift+Space)", a single item there "Take (Space)  Take all
      (Shift+Space)"; a lamp post "Use (Space)"; a sign "Read (Space)"; a
      book "Read (Space)  Take (Shift+Space)"; in the pack a potion "Use
      (Space)  Drop (Shift+Space)", a sword "Ready (Space)  Drop
      (Shift+Space)" or "Put away (Space)  Drop (Shift+Space)", a gem
      "Drop (Space)", food "Use (Space)  Drop some (Shift+Space)";
      Shift+Space drops at once (a stack asks how many). A shop item says "Steal (Space)", and Space asks "Steal?
      Space again". Each matches what Space then does.

## Maps

The world map on U is shelved (see
[shelved/world-map.cc](shelved/world-map.cc)); U does nothing now.

- [x] Game Engine options → "Corner minimap: Yes": a small map at the top
      right follows your view, with player dots (2026-10-18; the dots were
      then halved, to 2x2 with a black edge).
- [x] The minimap's smaller dots are still easy to see.
- [x] Dungeon: the minimap hides, and comes back outside.
- [x] The setting is kept after restarting Exult.
- [x] No slowdown with the corner minimap on (walk around a town).

## Losing a companion, and four players (Phase 6, M5)

- [ ] A guest's companion dies in a fight (not just knocked out): everyone sees "<companion> has
      fallen; <name> is now spectating", and that guest's picker opens
      with the free companions.
- [x] The Avatar dismisses a guest's companion in conversation ("leave"):
      after the conversation, "<companion> has left the party; ..." and
      the guest's picker opens.
- [x] A guest's companion is put to sleep: it stays theirs, and they move
      it again when it wakes (2026-10-18, after a fix: the guest had gone
      to spectating).
- [x] A rejoin, or a host load, while it is asleep: it stays theirs.
- [ ] The same for knocked out and paralyzed.
- [x] The host loads a save where a guest's companion isn't in the party:
      "... is not in the party in this save; <name> is now spectating", and
      the picker opens; the other guest keeps its companion.
- [x] Save on the host while guests hold companions, load it in single
      player: every companion follows the Avatar.
- [ ] Four players: host plus three guests, each on its own companion
      (needs three companions in the party). Third guest:

      ```bash
      ./exult --bg --nocrc -c .friend-mode-local/guest3.cfg
      ```

      Movement, chat and item use from all three; then close one guest and
      the others keep playing.
- [ ] A fifth window joins when every companion has a player: it can join
      to spectate (up to 4 spectators), and chat from it shows as
      "Name: text".

## Scripted events

Story scenes and other scripts run on the host only; guests watch. Try
them with two guests on companions.

- [ ] A script egg the Avatar steps on starts a scene: both guests get the
      conversation bar and the mirrored text and portraits; neither can
      move until it ends.
- [ ] A guest's companion steps on a script, missile-trap or button egg:
      nothing happens on the guest's machine or the host (only monster
      eggs fire for companions).
- [ ] A companion a guest controls speaks in the Avatar's conversation
      (Iolo or Spark interjecting): its portrait and lines show on every
      machine, and the guest gets it back afterwards.
- [ ] A script moves or turns the party (a scene that walks everyone, a
      moongate, the Orb of the Moons, sleeping in a bed): the guests'
      companions go along and the guests get control back where they land.
- [ ] A script that walks a guest's companion (a lever's script walks the
      user there): the guest's moves don't fight it.
- [ ] NPC remarks and barks during scenes show over the right heads on
      both guests.
- [ ] A scene that ends with a companion leaving or dying: see "Losing a
      companion".

## Follow mode and Spectate

- [ ] Guest presses E: the bar shows "Following the Avatar. Move to take
      over." Walk the Avatar a screen away: the companion follows, and the
      leash never beckons it.
- [ ] Turn on combat mode near a creature while following: the companion
      fights with the party.
- [ ] The guest walks (or presses E again): the bar goes and the companion
      is the guest's again, where it stood.
- [ ] While one guest follows, the other guest's J picker doesn't offer
      that companion.
- [ ] E does nothing (red X) while watching or during a host conversation;
      typing "e" in the journal or chat still works.
- [ ] J → Spectate: everyone sees "<name> is now spectating; <companion>
      rejoins the party"; the guest gets the spectating bar, and J again
      offers that companion.

## Pauses in co-op

Built 2026-10-20, untested. Only the host pauses the game.

- [ ] Host and guest: open a backpack or paperdoll during a fight: the
      world keeps going on both screens (with "Gumps pause game" on or
      off). Game Engine options shows that line as "Gumps pause game (not
      in co-op):"; in single player it works as before.
- [ ] Guest presses Escape during a fight: its companion follows the
      Avatar and fights (the host's log: "follows the Avatar"); closing
      the menu hands it back where it stands ("takes over again"), with no
      "Following" bar. A guest already following (Y) stays following.
- [ ] Combat pause does nothing in co-op, on the host too (the key, and
      Space while the Avatar attacks its pick); in single player it works
      as before. Opening to co-op while combat is paused resumes it.
- [ ] The host's Escape menu still pauses everyone, with the bar on guests.

## Class System (protocol 32)

Built 2026-10-20 without a test run (the user was away); everything here
is untested. Turn it on first: Game menu → Class System → Enable: Yes on
the host. For quick rounds, set `config/friend/class/short_cooldowns` to
`yes` in the host's cfg (5-second cooldowns; not on the options page).
The host's log says "Friend Class: Jaana uses Mending Circle on 3 targets
(+24 health)" for every skill used.

Options page:

- [ ] Game menu shows Class System under Target System. With Enable: No
      the other lines hide; Yes shows them.
- [ ] Skill bar: each of the five places (above the status bar, bottom
      left and right, top left and right) moves the bar there. Above the
      status bar it sits over the party's faces, not on them, with each
      Status Bars setting (Game Display). Top right sits under the corner
      minimap when that is on.
- [ ] The bar's size follows the gumps size setting (UI options), like the
      shortcut bar.
- [ ] On a guest the page shows only Skill bar and Show skill ranges, with
      "The host sets the rest."; the host turning Enable off hides the
      guest's bar.
- [ ] The settings survive a restart (`config/friend/class/...`).

Effect preview:

- [ ] Class System page → Effect preview closes the page and opens a panel
      at the top of the view; the game keeps running.
- [ ] Sprites: < and > play each sprite on your character with "Sprite 16
      (17 of N, 12 frames)"; << and >> step by ten; Play repeats it.
- [ ] Kind → Missiles: each flies from your character to the Q pick, or six
      tiles ahead; Kind → Sounds plays each sound (or says none are
      installed).
- [ ] A guest's preview shows on its own screen; the host's shows on
      guests too (sprites, missiles and sounds are sent).
- [ ] Close removes the panel.

The Avatar's spell bar (host, and single player):

The F9 test kit now has a bag with 20 of each reagent, puts every spell in
the Avatar's spellbook (a new book in that bag if the Avatar has none), and
fills the Avatar's mana. Each spell still needs the level of its circle
(Fire Blast 2, Lightning 4, Great Heal 5), as in U7.

- [ ] Three slots: Great Heal, Fire Blast, Lightning, greyed with the
      reason on a click ("You don't know Lightning", "not enough mana...",
      "No spellbook") as the Avatar's book and mana allow.
- [ ] The mouse over a slot shows its name above the bar.
- [ ] Fire Blast with a Q pick: cast at the pick with no crosshair, mana
      and reagents used as from the book.
- [ ] Fire Blast with no pick: the usual crosshair.
- [ ] Great Heal heals the Avatar, or the party member picked with Q (out
      of combat Q offers the party).

A guest's skill bar (Iolo, Shamino, Jaana, Dupre or Spark):

- [ ] The bar shows the companion's skills; locked ones say "L3" or "L6"
      and a click says "Battle Hymn: level 3". The mouse over a slot shows
      its name and what it does.
- [ ] No bar while spectating or in follow mode (Y); it comes back on
      walking.
- [ ] Switching companions (J) changes the bar; another companion's
      cooldowns are their own.
- [ ] Jaana: Mending Circle with someone hurt nearby heals the party
      within the green circle, "*calls on the earth to mend*" over her,
      sparkles on each; the slot counts down 60 and shows the number;
      clicking it meanwhile says "Mending Circle: 42 seconds".
- [ ] Jaana: Mending Circle with nobody hurt says "Nobody needs healing"
      and starts no cooldown.
- [ ] Iolo: Song of Warding gives the party nearby protection (U7's
      protection flag; the blue badge).
- [ ] Iolo at level 3+: Battle Hymn; the host's log line, and the party
      hits harder for 30 seconds (stats aren't shown on the guest yet).
- [ ] Shamino with a bow and arrows: Volley shoots at up to three
      hostiles, using an arrow each; without a bow: "Needs a bow, crossbow
      or sling, and ammunition".
- [ ] Spark: Stun Shot fires a stone; the target stops for about 3
      seconds, or "Resisted".
- [ ] Dupre: Stand Fast gives himself protection.
- [ ] The circle shows on every machine while the mouse is on a shown
      skill and for 2 seconds after it is used; Stun Shot and Volley show
      none. "Show skill ranges: No" hides them on that machine.
- [ ] Iolo at level 6: Song of Peace puts hostiles in the circle to sleep
      for about 5 seconds (some resist), music notes on Iolo and on each;
      the party stays awake.
- [ ] Sentri: Riposte, then let a foe swing at him: misses are answered
      ("Riposte!", a sword sprite on the foe) for 10 seconds. Blade Dance
      (level 3) strikes every foe beside him. Master's Strike (level 6) on
      an armoured foe does more than his normal hits.
- [ ] Tseramed with a bow: Sleep Arrow puts the target to sleep as the
      arrow lands; Crippling Shot makes it attack less often for 10 seconds;
      Hunter's Mark (level 6): party hits on it do more (compare "combat
      shows hits" numbers).
- [ ] Julia: Quick Fix on the weakest nearby (fewer hits get through for
      30 seconds); Flashbang (3): a keg flies to the pick and hostiles near
      it stop for 2.5 seconds; Time Bomb (6): a keg flies, sparks for 3
      seconds, then the keg's blast hits hostiles near it, not the party.
- [ ] Katrina: Healing Field leaves a green circle at her feet for 12
      seconds that heals whoever stands in it; Summon Creature (3): a
      sheepdog appears and fights for 30 seconds, then vanishes with a poof
      (guests see it too); Sanctuary (6): armour for everyone in the circle.
- [ ] Reactions over foes, emote-style, on every machine: *stunned*
      (Stun Shot), *asleep* (Sleep Arrow), *drowsy* (Song of Peace),
      *entangled* (Entangle), *dazed* (Flashbang), *crippled* (Crippling
      Shot), *marked* (Hunter's Mark), *enraged* (Challenge), *resisted*
      when a roll fails; *ripostes* over Sentri.
- [ ] Dupre at level 6: Challenge turns hostiles in the circle on him;
      for 6 seconds they keep coming back to him.

Class AI (companions nobody plays, host):

- [ ] Single player, Class System on, Jaana in the party: two people below
      half health nearby, and she uses Mending Circle by herself; someone
      poisoned near her (level 3+), Purify.
- [ ] Iolo sings Song of Warding when two or more hostiles are near in a
      fight; Shamino volleys at two or more.
- [ ] A guest's companion never uses skills by itself, but does in follow
      mode.
- [ ] Class AI: No on the page: nobody uses skills by themselves.
- [ ] Cooldowns: Lay on Hands 15 s; Mending Circle, Purify, Healing Field
      and single shots 30 s; the rest 60 s.
- [ ] Open the game menu during a fight: cooldown numbers, buffs and a
      Healing Field stop counting until it closes; the same during a
      conversation.
- [ ] A guest Q-picks a wolf that the companion isn't fighting, then Stun
      Shot: the picked wolf is stunned.
- [ ] During a host conversation the guest's bar is greyed; a click says
      "Wait for the Avatar".
- [ ] Save (or let the autosave run) while a foe is stunned and the
      sheepdog is out: the dog leaves with a poof, the foe stays stunned
      only its few seconds; load that save: no dog, nobody stuck asleep.
- [ ] Status icons under people, on every machine: a green up arrow
      (Battle Hymn), a blue shield (Quick Fix, Sanctuary, Riposte,
      protection), a red down arrow (Crippling Shot, Hunter's Mark); gone
      when it ends.
- [ ] Battle Hymn: health goes up by 3 and back down when it ends.
- [ ] The Class System page has no "Require class items" line.
- [ ] After the first test (2026-10-20): the bar's slots are square buttons
      with names; Left side and Right side make a column; corners are
      flush. Hovering a slot shows its description in the notice line.
- [ ] Fire Blast with a hostile pick out of combat: combat mode comes on
      and it casts; on a peaceful pick out of combat: "Fire Blast on Deer?
      Click again", a second click casts; in combat mode it casts at once.
      A guest's offensive skill on a peaceful pick: "The Avatar must do
      this".
- [ ] The companion picker shows no status icons or circles.
- [ ] The chat line is at the very bottom; the "Following" bar and the chat
      log sit above the faces and the bar.
- [ ] No targeting range circle; Q (or E) with nothing in range says "No
      target in range".
- [ ] Shamino or Iolo with a bow or crossbow: a foe closing to melee makes
      them step back about 4 tiles ("keeps its distance" in the host's
      log), then shoot; never more than 12 tiles from the Avatar.
- [ ] Jaana: two companions below half, the worst far off: she walks into
      range ("moves to heal") and uses Mending Circle; a foe beside her
      sends her back to the party ("falls back").
- [ ] Dupre and the others close in as before.
- [ ] A guest's companion is never moved by this; in follow mode it is.

## Leaving

- [ ] Close the guest. Host: "<name> has left; <companion> rejoins the party".
- [ ] Rejoin, then close the host. Guest: "Host disconnected. Exult will
      close." and it exits after about 4 seconds.

## Known gaps to ignore for now

- Theft by guests. (Guests can't give items to other party members; that is
  by design.)
- Stats other than health, food level and status effects (poison, sleep,
  paralysis, charm, curse, protection, might) aren't mirrored to the guest.
- Creature health (Phase 5); a guest's companion fights, but the guest
  doesn't see a creature's health go down.
- Doors whose script swaps to a shape that isn't flagged as a door.
- Sounds for changes made on the other machine.
