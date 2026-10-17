# Friend Mode development

Friend Mode is an experimental cooperative multiplayer extension for Exult. The
project vision and roadmap live in [the original project brief](../ultima-7-friend-mode.md).

Development starts from a verified, unmodified Exult build. Keep the story
Avatar, the human-controlled actor, and the network host as separate concepts;
the first proof of concept changes only the local controller and camera target.

## Current milestone

Phase 1: harden the working local party-member possession prototype.

- [x] Import the official Exult source and history.
- [x] Configure GitHub as `upstream` and Bitbucket as `origin`.
- [x] Configure and compile an unmodified Exult binary on macOS.
- [x] Launch with legally obtained Ultima VII data and record a baseline save.
- [x] Trace the initial input, camera, and party-AI control points.
- [x] Implement and manually verify local Avatar/Iolo possession.
- [x] Restore Avatar control automatically across save/load.
- [x] Restore Avatar control automatically across unsafe scripted transitions.
- [x] Verify dialogue interactions return to and use the canonical Avatar.
- [x] Allow companion-owned paperdolls, bags, ordinary containers, and item
  dragging while preserving companion control.
- [x] Restrict companion controllers to their own inventory while retaining
  Avatar access to all party inventories.

See [macOS setup](macos-setup.md) and [control-point notes](control-points.md).
To play with someone outside your network, see [internet play](internet-play.md);
for Windows, the [Windows build plan](windows-build.md).
For a repeatable two-player pass, use the [test checklist](test-checklist.md);
**F9** on the host places a test kit of usable objects around the Avatar.
The guest-action boundary is defined in the [interaction authority
policy](interaction-policy.md).

## Phase 2 checkpoint

The first loopback networking slice is in place. A host can listen on a local
TCP port and a guest can use a dedicated **Connect to Host** title-menu entry.
The peers exchange a versioned handshake and reject incompatible game variants.
This has been verified with two isolated Exult instances on one Mac.

The temporary local guest profile and copied runtime data live under
`.friend-mode-local/`, which is ignored by Git.

The next assignment slice now transfers the host's eligible companion roster,
shows the guest a **Select Your Character** carousel using the companions'
existing framed portraits, names, short descriptions, and Exult arrow artwork,
and has the host validate the requested assignment. The guest then takes local
input and camera control of the accepted companion. This has been verified with
Iolo over two local instances. Moving connection-address entry and this selector
into one uninterrupted pre-game flow remains a later UI refinement.

The current synchronization prototype has also been verified for Avatar and
companion position/facing, nearby NPC and creature position/facing, companion
movement across building thresholds, client-specific roofs, and disconnect
notification. The guest camera and movement cursor remain anchored to the
assigned companion. Guest-side NPC simulation is suspended so it does not fight
host snapshots.

Inspection labels from single clicks remain local to the client that generated
them. Network guests can use their own inventory and ordinary containers, but
NPC/sign activations that could start Avatar-owned story Usecode are currently
blocked without revoking companion control.

Host-generated NPC remarks and animal barks are now replicated to the guest.
Host-started conversations also place the guest in a temporary spectator
state: movement and interaction are paused, portraits, text, and response
choices are mirrored, and only the host can choose a response. The guest
resumes control of the same companion when the conversation ends. All four
behaviors have been verified together over two local instances.

## Next session

**Phase 6, M1 and M2, built and tested 2026-10-17 (protocol version 26).**
Three windows on one Mac (host, guests on Iolo and Spark): movement, chat,
opening and closing windows, teleporting and a host load with both guests
connected all work. The host takes several guests:

- **One connection interface.** The socket code is now in `friendtcp.cc`
  behind `Friend_connection` and `Friend_listener` (`friendconn.h`), a
  reliable byte stream, so a WebRTC data channel (Phase 8) can slot in
  without touching the messages.
- **One session, several peers.** `Friend_mode_session` keeps a `Peer` per
  connection (framing, outgoing queue, name, half-arrived text and blobs,
  the latest requests). Guests get a player number (host 0, guests 1 and
  up) when accepted. Every send goes to every guest unless it names one;
  what the host reads says which guest sent it. Speech, chat, names and
  dialogue text share one text message.
- **A record per guest on the host** (`Friend_guest` in `exult.cc`):
  companion, last position sent, held item, leash timer, attack, journal
  window. `Game_window` keeps a list of reserved companions, and combat,
  eggs, teleporters, waking sleepers, item sync and "creatures stay awake
  near guests" work for each of them.
- **Player list.** The host sends every player's name and companion when it
  changes, so guests label each other's companions and name chat senders.
  Chat from a guest is shown on the host and passed on to the other guests.
- The picker offers each guest only companions nobody else has. Guests
  beyond one per companion plus 4 watching are turned away with "The
  host's game is full".
- The "last sent" caches stay shared: changes go to every guest, and a new
  guest's world download resends everything to everyone.

Fixed after that test: a lit lamp post (or any animated object) made item
sync resend its chunk four times a second, because its frame keeps
changing; animated objects now count as the first frame of their cycle.
Guests get their player number when accepted, so port probes no longer use
numbers up.

To test with three windows, see "Two guests" in the
[test checklist](test-checklist.md). The second guest uses
`.friend-mode-local/guest2.cfg` (own gamedat and saves, audio off).

Next: M3–M5 in the [multi-guest plan](multi-guest-plan.md) (join/leave
notices on every machine, Tab switching between guests, party-change edge
cases).

## Earlier sessions

The item pick-up model, the host pause bar and chest windows staying open on
the guest were tested on one Mac on 2026-10-16 and work. The two-tester
contention checks have not been run with a second person yet.

Also tested on 2026-10-16 and working: guests using food, potions (including
poison, which now shows on the guest), keys, lockpicks and teleporters;
"Locked" and "Unlocked" on the guest; and the larger **F9** test kit. Real
in-game teleporters still need a try, to see how the leash behaves after
long jumps.

The guest's wait bar now ends with every host script, so a one-line remark
with a portrait (no "bye") no longer leaves it stuck; tested and working.

Tested 2026-10-16 and working: creature sync (existing and new creatures,
deaths and bodies) and clock sync (join, Ctrl-T skips, hunger). Clouds are
random on each machine and keep drifting on the guest while the host is
paused; low priority. Protocol version 13.

Also tested 2026-10-16 and working: guests can't save, load or use cheats;
the host loading a save sends the new world to the guest; bodies stay near
either player and go when both are far away. Not separately tested: script,
missile-trap and button eggs doing nothing on the guest.

Found in testing:

- A guest whose companion isn't in the party after a host load is left
  without a body: the camera doesn't follow the Avatar, creatures don't show,
  and there is no way to pick a companion again (even after the host loads a
  save that has one). Chat still works and shows at the top of the host's
  view.
- The cheats option still shows on guests (it can't be turned on); hide it
  like Load/Save.
- Guests can switch to combat mode but their companion doesn't attack
  (built since; see below).
- Bottles, torches and other everyday objects with scripts say "The Avatar
  must do this"; guests should be able to use the safe ones.
- An inactive window can leave a stale mouse arrow drawn until it is clicked;
  cosmetic.

Tested 2026-10-16 and working (protocol version 14): everything in the list
below except grandfather clocks and the telescope/sextant (none found yet).
The map key (M) needs a map in the companion's pack, as in U7.

Built and tested 2026-10-16 (fixes for what that test round found):

- **Theft by guests.** When the host carries out a guest's item move, it
  runs Exult's theft check the way it does for the Avatar's drags (an item
  not okay to take, from outside the party, that really moved, outside
  dungeons), but with witnesses looking at the guest's companion: "heard
  something", warnings, then guards. `Game_window::theft` and
  `find_witness` take an optional thief for this.
- **Waking sleepers.** A sleeping NPC can be woken by the guest's companion
  walking close by, as by the Avatar (same distance, clear line and chance).
  The check only runs for NPCs near the host's view, which the leash mostly
  covers.
- **Painting** (rebuilt after testing; retested and working). The brush script
  asks for two clicks: double-click the brush (frames 2 and up of shape
  823), click the pigments (frames 0-1), then the easel. The guest picks
  both, and the host answers both clicks in order (the usecode machine has a
  small queue of click answers for this). Double-clicking the pigments alone
  gets a remark. Double-clicking the easel runs on the guest only.
- **Coin flip** (tested, left as is). Gold's script is a fixed exchange
  between the Avatar and Iolo, who must be nearby, with the text over them;
  it runs on the guest only because it closes open windows.
  The F9 kit now has an easel, and a brush and pigments in the supplies bag.

Built 2026-10-16:

- **Spectating and picking a companion again.** A guest without a companion
  follows the Avatar's view (roofs too), keeps getting items, creatures and
  the clock, and sees a bar: "You are watching. Press Tab to choose a
  companion." **Tab** opens the companion picker at any time (not during a
  host conversation or pause); picking another companion hands the old one
  back to the party's AI and the host says "<name> switches to <companion>".
  The cheats option is hidden on guests.
- **Everyday objects for guests.** Black Gate's object scripts were checked
  (the decompiler in tools/ucxt can't read the data, so a small script listed
  each script's engine calls and flags). Guests may now use bottles (the same
  script as food), torches (light and put out), the pocketwatch and
  grandfather clocks (say the time). Eating or drinking something that isn't
  yours calls the guards, as in single-player.
- **Read-only objects open on the guest only.** Their scripts only show
  something, so a guest runs them on its own copy and the host never sees
  it: signs, tombstones, books, scrolls, the cloth map, telescope, sextant,
  sundial, orrery viewer, crystal ball, and the pot and stuffed toy remarks.
  Three books and one scroll set a story flag when read; for those the story
  only counts when the Avatar reads them, so a guest can find one and point
  the Avatar to it. The mirror (sets a flag) and beds (sleep moves time and
  the party) stay Avatar-only.
- **Shortcut keys on guests** (F food, W watch, P lockpick, K keys and the
  like) used to run item scripts on the guest's own copy. They now use only
  the companion's own items and go through the host like a double-click; the
  cloth map (M) still opens locally.

Built and tested 2026-10-17:

- **Creatures appear around the guest.** Monster eggs hatch for the
  guest's companion as for the Avatar (`Egg_object::is_active`: "cached
  in" and "Avatar near" eggs of the monster type only). Script, music,
  weather and other eggs stay the Avatar's. So creatures such as the
  Trinsic stable horse appear wherever the guest walks inside the leash.
- **Creatures near the guest stay awake.** Exult stops creatures and NPCs
  that are off the host's screen and more than a screenful from the
  Avatar, and wakes them only when the host draws them. Now they also stay
  awake within a screenful of the guest's companion
  (`Game_window::near_remote_friend`), and the host wakes those four times
  a second (`Npc_actor::wake`), so they walk, fight and send their moves to
  the guest.

- **Missiles and spell effects on the guest** (protocol version 18).
  Every missile the host fires (arrows, bolts, thrown weapons, missile
  traps, spells cast as missiles) is sent with its start, end, picture and
  speed, and flies on the guest as a picture only
  (`Projectile_effect`'s visual-only constructor); the host does the hit
  and anything left behind. Sprite effects (spells, explosions, blood) are
  sent the same way, following the actor they belong to.
- **Health on the guest** (protocol version 19). The host sends the
  health of creatures and NPCs near the players when it changes. Exult
  shows it in two optional places: after a creature's name when clicked in
  combat mode (only when the combat mode option isn't "Original"), and as
  "-6(2)" (damage, health left) over whoever is hit, when Game Engine
  options has "combat shows hits" on. Each hit is now sent to the guest
  whatever the host's setting, and shown if the guest's own setting is on.
- **NPCs near the guest send their moves** even when the host isn't
  drawing them (the host used to send only those in Exult's 'nearby' list,
  around its own view).

Built and tested 2026-10-17 (protocol version 24): **guests can train**
with trainers; the trainee list shows only the guest's companion and the
host applies the training with the party's gold. Details:
[shops and trainers](shops-and-trainers.md).

Built and tested 2026-10-17 (protocol version 22): **stats,
experience and levels reach guests.** Party status used to carry only
health, food level and status effects; it now also has strength,
dexterity, intelligence, combat, mana, magic, training points and
experience for every party member. The level is worked out from
experience, so level-ups show in the guest's stats window too (Exult shows
no level-up message of its own). Tested: everything matches, and the
guest's companion's experience goes up from its fights.

Fixed after that test (protocol version 23): new creatures showed a hurt
health bar on the guest. The guest's copy of a creature rolls its own
random strength (the most health), which differed from the host's; the
host now sends strength with health. Retested and working.

Built and tested 2026-10-17 (protocol version 21): **guests can shop.**
Double-clicking a merchant runs the conversation on the guest only, with
the companion's portrait; each purchase is made on the host with the
party's gold. Story flags and NPC schedules now reach guests. Details:
[shops and trainers](shops-and-trainers.md).

Built and tested 2026-10-17 (protocol version 20). Also checked while
testing: a guest's companion dying leaves the guest watching, and Tab picks
another; the Avatar dying reloaded the last save (early in the story, before
the healer resurrection), and the guest followed the host into the reloaded
world.

- **World sounds on the guest.** Each side hears the right sound whoever
  caused it, quieter the farther away. The host sends the sounds the world
  makes: scripted object sounds (doors, levers, secret walls, food and
  potions), hits, animation and pickup sounds, and explosions, placed where
  they happen. Ambient sounds (birds, crickets) and menu clicks are each
  machine's own and aren't sent. Sounds loop only on the host.
- **Enemy health bars** (an option, off by default: Game Engine →
  "Enemy health bars"). A small green and red bar, like Ultima Online's,
  over creatures and NPCs outside the party that are hurt; unhurt townsfolk
  get none. Works on host and guest alike (the guest gets health from the
  host).
- **Faster messages and a connect time limit.** Both sides set
  `TCP_NODELAY`, so a step goes out at once instead of waiting up to about
  200 ms to be bundled. Joining gives up after 5 seconds per address
  instead of freezing for a minute (checked: a test connect and handshake
  took 0.05 s; an unreachable address gave up after 5.0 s).

Tested 2026-10-17 and working: creatures appear and move near the guest
(the Trinsic horse); missiles from the guest's companion and from the
Avatar show on the guest; "combat shows hits" labels show on the guest.
The Trinsic cat near the guest is awake (it meows) but stays put until the
Avatar comes close: its schedule follows the Avatar, as in single-player.


Tested 2026-10-17 and working: **Co-Op Mode in the menus**, so no
special launch options are needed. On the title menu, **Co-Op Mode** (below
Journey Onward) opens three choices: **Host a Game** switches hosting on
and returns to the menu (the line then reads "Co-Op Mode: Hosting"; pick it
again to stop), after which Start New Game or Journey Onward work as usual;
**Join a Game** goes straight to the player name and host address, then
downloads the host's world; **Back** (or Escape). In a running game, the
Escape menu has **Open to Co-Op** to start hosting there and then; guests
can join at once, as they already join mid-game. Either way the host sees
"Co-Op Mode is open on port 1992". `--friend-host` and `--friend-join`
still work as shortcuts. The default port is 1992, the year Ultima VII
came out (`--friend-port` changes it). The Join player name starts empty.

Next (see the roadmap in the [project brief](../ultima-7-friend-mode.md)):
Phase 6, more than one guest; Phase 7, shops and trainers for guests;
Phase 8, lobby codes over WebRTC and the release. The Windows build, guest
egg hatching, sounds and the rest of Phase 5 are done.

Tested 2026-10-16/17 and working (protocol version 17): guest combat,
the shared journal and the journal bar, and music from the host, as
described below.

- **Journal bar.** While one side has the journal open, the other sees a
  bar like the pause bar: "Someone is writing in the party journal."
- **Music from the host.** Every track the host starts or stops (region
  music, combat, victory, scripts) is sent to the guest and played there,
  including the current track when the guest joins. While connected, the
  guest's own copy of the world starts no music of its own, since its eggs
  and fights would pick differently. The guest needs "digital music" off
  unless OGG music files are installed; with it on and no files, Exult
  plays no music at all.
- **Host audio off.** The host's combat music checks (danger, attacked,
  guards) use the track the game last asked for
  (`Audio::current_music_track`), not the host's music player, which
  doesn't exist with audio off, so guests still get the music. Background
  music for towns, weather and dungeons still follows the host's own synth:
  Exult only plays it with digital music or an MT-32.
- **Battle music for the guest's fights.** The guest's companion counts
  like the Avatar: its first strike starts the battle tune and killing a
  hostile (evil) monster plays the victory tune. Peaceful animals (a horse)
  give none, as in U7. The host sends "no music" only when music stops,
  not every second.
- **Sound effects** need a Black Gate digital pack in `data/`
  (`sqsfxbg.flx`, Roland, or `jmsfx.flx`, from Exult's download page; both
  are git-ignored). This Mac now has the Roland pack. This build has no MIDI
  sound effects. Sounds made by the other machine's actions (a door the
  host opens) are not sent yet.

Built and tested 2026-10-16 (protocol version 15):

- **Guest combat** (start of Phase 5). In combat mode, a guest's
  double-click on a foe sends it to the host, which puts the companion on
  Exult's own combat schedule against that foe. The companion walks up and
  fights with whatever it has readied; its moves and attack frames come back
  as usual. It stops and waits for the guest again when the foe dies or is
  lost (out of the host's view, invisible, or the combat schedule picks
  another), or as soon as the guest walks. Foes are creatures and hostile
  NPCs (evil or chaotic) within 24 tiles; anyone peaceful gets "The Avatar
  must do this". Combat mode on a guest only changes what double-clicks do:
  the guest's own copies of the Avatar and party no longer start fights of
  their own. Creature health still isn't shown on the guest.
- **Shared journal.** The host's journal is the real one, and guests can
  read and write it (N). Writing in a note starts with the first change to
  it and ends when the cursor leaves it or the journal closes. The writer
  holds the note: the others see their name next to the note's date and
  get a red X if they try to type in it. The host decides who got a note
  first, and a guest that loses the race gets the finished text back. A
  guest's finished note (changed, or new) goes to the host, which applies
  it and sends the whole journal back; the host's own notes and automatic
  story notes go out the same way, checked twice a second. A guest's new
  note lands after the shared ones.


Waiting for a second tester: the two-tester contention checks and a real
in-game teleporter with the leash.

Untested: a guest double-clicking a scripted object it can't use (the bottle
or torch in the test kit's chest should say "The Avatar must do this").
NPCs saying it was tested and works.

Still to test:

- Undoing a guest's conversation (protocol version 25): after a merchant
  or trainer conversation, the host's story flags come back on the guest
  and the host resends the party's packs; a free gift from an NPC should
  vanish on the guest. Also a quick shop and training round on this build.
- Grandfather clocks and the telescope/sextant from a guest.
- Spell shops: a guest trying to buy a spell gets "The Avatar must do
  this" and pays nothing.
- Shop hours: a merchant outside business hours turns the guest away, as
  for the Avatar.

Minor, test later: an item a guest is mid-drag on when a new guest joins is
missing from the save the new guest downloads.

Later: multi-guest sessions (Phase 6, [plan](multi-guest-plan.md)) and a 30-minute
two-player soak test.

Decided: the Avatar is the game master. It can open and change every party
member's inventory, including giving items to a guest's companion. Guests are
limited to their own character and cannot give items to others.

## Phase 3 checkpoint: player identity and overhead chat

**Connect to Host** now asks for a player name (up to 20 characters) and a host
address before loading. The host uses the Avatar's name. Both peers exchange
names after the handshake, and clicking a player-controlled actor or its party
portrait shows `Character (Player)`.

While connected, press **Enter** to open the `Say:` line, **Enter** again to
send, or **Escape** to cancel. Messages are limited to 80 printable ASCII
characters and are filtered again on receipt. Text appears centred above the
speaker in the shadowed font, wraps by pixel width (long words are split), and
stays up longer for longer messages. Up to three recent messages per speaker
stack upward; older ones are dropped. Players without an actor (spectators)
appear near the top of the view as `Name: text`.

Chat packets carry the sender's NPC number. The host ignores the guest's claim
and uses its own assignment; guests trust the host. Chat never runs Usecode.
The protocol version is now 5. Relaying chat between multiple guests waits on
multi-peer sessions.

### Joining with the host's world

After **Connect to Host**, the guest connects from the title menu and asks for
the host's world. The host writes its live game to a temporary save (the
normal save code, about 190 KB) and sends it as one variable-length message:
a header packet with the length, then raw bytes. The guest shows download
progress, unpacks the save into its gamedat, and loads it, so party, quest
flags, items, doors and NPCs match the host at the moment of joining. Protocol
version is now 7.

### Doors, shutters and lamps

The host shares every change a game script makes to a world object's shape or
frame, hooked in `Usecode_internal::set_item_frame` and `set_item_shape`. That
covers doors opened by the Avatar, NPCs or scripts, shutters, lamps, levers and
so on. Decorative animation (torches, fountains) uses `change_frame` directly
and is not sent. Ultima VII opens doors by swapping shape (closed 270/376 to
open 432/433), so an object is identified by tile plus old shape and frame,
falling back to any door at that tile.

Guests may use doors and a short list of world toggles (Black Gate only for
now): shutters 290/291/322/372, light sources 336/338, sconces 435/481,
curtains 657/678, levers 787, switches 788, secret walls 828/845 and lamp posts (889 unlit, which swaps to 526 lit lamp when turned on). The guest's double-click asks the host,
which checks the shape and reach (the normal double-click reach rule) and then
uses it: doors the way NPCs do (quality zeroed so only the door's own toggle
runs), the others with a normal activation. Locked doors show "Locked". If the object's
script walks someone there first (levers do), the guest's companion walks, not
the Avatar (`Friend_mode_usecode_walker`). Night lighting is measured from the
actor the view follows instead of always the Avatar, so a guest sees light
around their own companion. Levers
and switches are allowed so guests can help with puzzles; their scripts can
open gates or trigger effects elsewhere, which is accepted. Protocol version is 9.

### Items on the map and in containers

The host reconciles movable objects with the guest by chunk (see the
[world sync plan](world-sync-plan.md), W3): every 250 ms it fingerprints the
objects and container contents near the players and sends any chunk that
changed; the guest makes its copy match. Party inventories are reconciled the
same way. A guest's drag and drop happens locally and is also sent to the host
as a move request; the host checks reach, ownership, weight and room, makes
the move, and resends so both sides match (W4). When only a container's contents
change, the guest updates them in place, so a chest or pack window stays open.

When the host opens a menu or a dialog (including the quantity prompt when
splitting a stack), Exult pauses its world, so the host tells guests: their
movement and interaction pause, with a bar at the bottom saying "The host has
paused the game." until the host returns. Chat still works.

Picking an item up takes it out of everyone's world until it is dropped or
put back: the host's drags show up on guests at once, and a guest's pick-up is
held by the host for that guest (see the world sync plan). Two people can't end
up with the same item. Protocol
version is now 10.

### Food, potions, keys and lockpicks

A guest can double-click food or a potion to use it on their own companion,
and a key or lockpick to get a crosshair and pick a door or chest. The guest
sends the item and the target to the host. The host checks that the item is
the guest's or within reach (and the target too), then runs the item's
normal script with the target click already answered, so eating, drinking,
unlocking and breaking picks all behave as in the game. The results come back
through the usual item and door sync. Black Gate shapes: food 377, potion 340,
key 641, lockpick 627. Protocol version is now 11.

Anything else a guest double-clicks that would run a script, including people
outside the party, shows "The Avatar must do this" instead. Locked chests show
"Locked".

Text spoken by doors, chests and other objects ("Locked", "Unlocked") is sent
to guests with the object's tile and shape. The host also sends each party
member's health, food level and status effects (poison, sleep, paralysis,
charm, curse, protection, might) when they change. The guest copies them
without starting its own timers, since poison and the rest run on the host,
and flashes the red outline when health drops. Protocol version is now 12.

### Teleporters

When the host accepts a guest's step, it fires the eggs on that tile the way
it would for any party member. A teleporter triggered by the guest's companion
sends only that companion, not the Avatar and the party. The guest's own copy
of the world never fires teleporters itself; the host's move comes back as
the companion's new position. When the Avatar takes a teleporter, the guest's
companion goes with the party. Leash rules still apply on arrival.

### Creatures

Monsters (creatures without an NPC number) get a network ID from the host the
first time they are near the players, in the same chunks item sync covers.
When the set of creatures there changes (one appears, dies or leaves), the
host sends the whole list: ID, shape, frame, tile, alignment and invisibility.
The guest creates the ones it lacks, standing still, and deletes the rest,
including creatures its own map loading brings in. It checks this four times
a second, so a creature lost to the guest's own map unloading comes back.
Moves go out as creature states by ID, only when a creature moved or changed
frame. Monster eggs do nothing on the guest; the host's eggs make the
creatures (and items, which item sync shares). Health and combat are still
the host's alone (Phase 5). **Shift+F9** on the host places a rat, a wolf and
a rabbit near the Avatar.

### Clock and weather

The host sends its day, hour, minute, tick and current weather (from
`Effects_manager::get_weather`) when they change, checked twice a second, and
right after sending its world to a joining guest. The guest's clock stops
advancing on its own, so hunger, healing and hourly NPC schedule changes stay
with the host, and jumps to the host's time, updating the palette. A change
of weather starts the same effect on the guest (snow, storm, fog, clouds,
sparkles) until the host's weather changes again. Weather eggs do nothing on
the guest. Protocol version is now 13.

### Party mirroring and spectating

The host sends its party list when a guest joins and whenever the party
changes, and the guest updates its local party to match before taking control.
This lets a guest control a companion recruited after the guest's copy of the
world was made (for example Spark). A guest without a companion is a
spectator: movement and double-clicks are blocked so it cannot move its local
copy of the Avatar, and the camera follows the host's Avatar. Protocol version
is now 6.

### Leash

Guest companions stay near the Avatar, which keeps them inside the area the
host simulates (see the [independent-views research](independent-views-research.md)).
Distance is measured horizontally from the Avatar:

- Beyond one screen (20 tiles): "Straying from the Avatar", and a green arrow
  at the screen edge points back to the Avatar.
- At 32 tiles the guest cannot step further ("You must return to the Avatar").
  The guest checks before stepping, and the host refuses such moves and resends
  the true position. Steps back toward the Avatar are always allowed.
- If the Avatar moves away and leaves the companion beyond 32 tiles, the guest
  sees "The Avatar needs you"; after 5 seconds the host moves the companion
  beside the Avatar ("You are beckoned by the Avatar"). A change of map moves
  it at once.

Raising the radius later is the path toward free roaming.

### Session notices

The host shows notices near the top of the view when a guest joins, enters the
world as a companion, and leaves (the companion returns to party AI). The guest
sees a notice when it joins the host's world. Connections that are not Friend
Mode clients, such as local port probes, are logged and closed.

## Local possession prototype

Launch the modified build with `./exult --bg --nocrc`, load a save with an
active companion, and press `Control-F`. Local movement and camera focus switch
to the first eligible party companion. Press `Control-F` again to restore
Avatar control and the companion's prior AI schedule.

Saving or loading while controlling a companion automatically hands control
back to the Avatar first, preventing the temporary suspended-AI schedule from
being written into a save.

Scripted control, incapacitation, party removal, and story-bearing Usecode
interactions revoke companion control. Dialogue therefore remains anchored to
the canonical Avatar.

Safe local interactions are now separated from that boundary. A controlled
companion can open only its own paperdoll and nested bags, open ordinary
non-scripted containers, and move items using its own reach. The Avatar remains
the party administrator and can inspect every party member. Pressing `I` while
controlling a companion opens that companion directly; Avatar control retains
normal inventory cycling.

The `--nocrc` flag is required because the development key binding changes the
generated Exult resource archive. It does not disable validation of the
original Ultima VII game data.

## Dialogue authority

Network dialogue is host-authoritative. When the Avatar begins a conversation,
connected guests temporarily enter a spectator state: movement and world
interaction pause, the host's conversation text and portraits are mirrored,
and only the host may choose responses. A bar at the bottom of the guest's
screen, clear of the conversation, says "The Avatar is having a conversation.
Please wait." Guest control resumes on the same companion when dialogue ends.

Inspection labels, including single-click object names, remain local to each
client and are not replicated. Spoken world text—including scheduled NPC
remarks, animal barks, and scripted item speech—is host-authoritative and is
replicated to guests.

## Known prototype gaps

- Guests start from the host's world and items, doors and party inventories
  stay in sync near the players, but theft by guests and giving items to other
  party members are not handled yet; see the
  [world sync plan](world-sync-plan.md).
- Creature identity currently relies on deterministic loaded-world ordering;
  spawned/despawned creature replication needs a durable network identity.
- The companion selector still appears after loading rather than in the
  pre-game connection flow.
- Exult has no in-game return to the title menu, so a guest whose host
  disconnects sees a notice and Exult closes after a few seconds.
- Sessions have a single peer, so chat is not yet relayed among several
  guests.
