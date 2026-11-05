# Changelog

What changed in each release of Quest of the Companions, newest first,
written for players. (`ChangeLog`, without the extension, is Exult's own.)

## Versions

Versions are **0.MINOR.PATCH** until the public release, which is 1.0.0:

- **MINOR** follows the roadmap phases (docs/ultima-7-friend-mode.md).
  0.8 is Phase 8, lobby codes and the first release builds; 0.9 is the
  release candidate (packaging, installation instructions, a test with
  three or four players).
- **PATCH** goes up for each release of fixes within a phase.
- The **build** is the number of commits since we branched from Exult
  (`git rev-list --count 58f31e831..HEAD`), so two builds of one version
  can be told apart.

Work that isn't in a release yet is under **Unreleased**, with the version
it's heading for.

This version is ours, kept apart from Exult's (`1.13.1git`, which stays as
upstream has it). It lives in `qotcversion.h` (`QOTC_VERSION`); the build
number is counted when the game is built (`qotcbuild.h`). The game shows
both in `--version` and the version files saved with each game.

## Unreleased (0.8.4)

### Changed

- **The update window on a Mac** now says Quest of the Companions has a new
  version, not exult.

## 0.8.3, 2026-11-04 (build 249)

### Added

- **A new way to drop things:** dropping is no longer "at your feet or
  nothing". Every drop, by keyboard or controller, now lets you choose where
  the item goes, without dragging it.
  - **Drop:** press the drop button and press it again to drop at your feet.
    The cursor stays out of the way until you start to aim.
  - **Aim:** E (the D-pad on a controller) steps through things, Q through
    people, and the mouse or right stick goes anywhere. Then a click or
    Space (Cross) drops it there.
  - **The hint says what will happen:** "At your feet", "Drop here", "Give
    to Iolo", "Put in Chest" or "Drop on Plaque", changing as you aim.
  - **Give and store from anywhere in reach:** a party member takes the item,
    and an unlocked chest or body holds it, with no window to open. A
    locked or full one has it set on top.
- **Q and E while aiming:** while the cursor is asking where (a drop, a
  spell, a tool), moving it and pressing Q or E picks what's nearest it, and
  pressing again steps to the next, looping round. They stuck on the
  nearest before.
- **More things to target for quests:** E now offers the bleeding stone
  (Space uses your pick), the firepit (the sword blank), the anvil, the
  Well of Souls (the Soul Cage), pedestals, the Skara Brae ferry's horns
  and the Ferryman, giant bones, metal walls and portcullises. Iron bars,
  stone chips, the stone heart, the orrery viewer, bee stingers, sacks of
  wheat, wool, thread and dough now count as Loot, so they're offered
  without turning Decor on.

### Fixed

- **Puzzles that wait for a dropped item:** Penumbra's plaques ignored
  anything dropped with Space, and anything a guest dropped. Both now count.
- **Tools with a window open:** while a tool or spell asks "on what?", E
  picks from the world instead of the open backpack.

## 0.8.2, 2026-11-02 (build 242)

### Added

- **Updates:** the game checks for a new build once a day, so there's no
  more downloading each release. On a Mac it installs when you quit. On
  Windows it asks first, then closes the game (you're asked to quit as
  usual), updates and starts it again. Game menu → Check Updates checks
  right away (on a Mac, also the app menu (exult) → Check for Updates…).
  Your saves and settings aren't touched. (This build is the last one to
  download by hand.)
- **Windows installer:** Windows builds come as an installer instead of a
  zip. It installs for you alone, with no administrator prompt, adds a
  Start menu shortcut, and updates go where you installed it. Copy your
  game data into its blackgate folder as before, or install into the
  folder you've been using: your game data there stays.

### Changed

- **Our own folder:** settings and saves now live in a folder of the
  game's own, apart from Exult's: on a Mac
  `~/Library/Application Support/Quest of the Companions`, on Windows
  `%LOCALAPPDATA%\Quest of the Companions`. The first time you start this
  build, your settings and saves are copied over from Exult's folder, so
  your game goes on where you left it. Exult's folder isn't touched, and
  from then on the two keep their own. New on a Mac: put the game's
  `STATIC` folder in that folder's `blackgate` folder
  (`~/Library/Application Support/Quest of the Companions/blackgate/static`);
  no administrator password needed.
- **Co-op:** hosts and guests need the same release (the co-op protocol is
  now 39).
- **Winches:** the Target System offers them with levers and switches, and
  Space turns them.
- **Locks:** with a locked chest or door targeted, Space says Unlock and
  uses the party's key that fits. With no key it says Locked, and offers
  Pick Lock when you carry lockpicks and the lock can be picked. K (Try
  Keys) uses your target too, with no crosshair.
- **More things to use:** the Target System now offers the mining
  machine, loom, spinning wheel, bellows, wells, water troughs, kegs,
  chicken coops, clocks, the sundial, telescope, crystal ball, mirror,
  instruments, the fair's games, stocks and more, with levers and
  switches. Torches on walls come with lamps, and furniture fixed in
  place (cradles, chairs) is offered when Furniture is on.
- **Ships, carts and the flying carpet:** the Target System offers their
  parts with levers and switches, and Space says what it will do: Lower
  or Raise Gangplank, Set Sail or Furl Sails, Drive or Stop for a cart
  or its horse, Fly or Land for the carpet. They're the Avatar's to
  drive, so guests aren't offered them. When a ride sets off, guests go
  into Follow (Y), with its bar, and walking takes their companion back as
  usual; before, a companion was left following after the ride with no
  bar, and walking didn't end it. On a guest's screen the vehicle now
  moves in step with the host, its riders aboard, instead of lagging
  behind with the riders snapping into place.
- **Conversations and scenes:** the skill bar, the minimap, the
  Follow / Stop button, and the notices and targeting line at the top
  hide while anyone talks and during scenes (the opening too), so they
  no longer pile up over the conversation. The chat log stays.
- **Conversation choices:** the D-pad's left and right (and the arrow
  keys) move along the choices; up and down move to the row above or
  below. With one row, up and down move along it as before.
- **Shortcut bar:** with Exult's shortcut bar on, the targeting line
  and its hint sit just under it instead of covering it.
- **Fountains:** pools of water are in the Target System's Usable group
  (with levers and switches) instead of Decor, so they're offered without
  turning Decor on.
- **Shared experience:** every kill's experience is shared by the party
  members near it (alive and awake), divided among them with a small
  bonus for each extra member, so healers and support companions level
  with the fighters. Kills by Katrina's traps and summoned creatures now
  count too. Game menu → Class System → Experience switches back to
  "Killing blow", Black Gate's own rule.
- **Doors and the party:** a door the party walks through stays open
  until the last of them is through, then closes. Companions used to shut
  it on each other and get stuck inside. Near a door the party walks in
  single file, so no one steps behind the open door. A companion still caught behind
  a closed door opens it after a moment and follows, and one boxed in
  behind an open door walks round it (or, with no way round, catches up
  beside you).
- **Tidier containers:** Sort, and things dropped into a bag, go in the
  first free place where they touch nothing, and overlap only once the
  bag is full.
- **Abacus:** guests can use it, and the controller's utilities ring has
  it whenever anyone in the party carries one. Guests' ring no longer
  shows the Spellbook.
- **Guests and the Avatar's conversations:** when the Avatar starts
  talking, a guest's target clears and their windows close.
- **No mods:** the game plays Black Gate as it shipped, so every co-op
  player runs the same game. Mods are no longer listed or started; for
  mods, use Exult itself. Your installed mods stay where they are.

### Fixed

- **Talking while leading a companion:** on the host or in single
  player, Space on a party member talked to nobody and showed their
  character, and other people said "The Avatar must do this". Now you
  talk to them as the Avatar would, and go back to leading after.
  Merchants and trainers talk to the Avatar too (before, they traded with
  the companion you led); only a guest's companion shops for itself.
- **Guests target the party:** Q now picks the other companions too, not
  just the Avatar and your own, for items and skills on them.
- **Guests' Talk:** Space now talks only to the merchants and trainers a
  guest may visit; with anyone else there's no Talk to try.
- **Conversations at night:** the highlighted choice shows white again
  at night (and in other darkened light), where it looked unhighlighted.

- **Skill range while the Avatar talks:** a guest hovering the skill bar
  during a conversation no longer leaves a range circle stuck on screen.
  The bar ignores the mouse until the conversation ends.
- **Number answers in conversations:** when the Avatar answers with a
  number (how many to buy, for instance), guests now see the Avatar's
  slider move as the host sets it, instead of an empty space. Guests can't
  use it.
- **Bags inside chests:** guests can open a bag or box inside a chest or
  body. They used to get a red X.
- **Cloaks after loading:** with Serpent Isle paperdolls, a worn cloak
  showed as a plain item after loading a game, until worn again.
- **Ready with Space:** a cloak, two-handed weapon or heavy gloves readied
  from the Target System goes in its proper spot.
- **Skill range while the game waits:** a range circle no longer stays on
  screen through the host's conversation or a pause, and the skill bar
  can't be used then.
- **Controller rings during conversations:** a guest's rings don't open
  while the host talks or pauses.
- **E with nothing in reach:** no more half-second stall. The Target
  System's keys no longer flash the red X, which froze the game for a
  moment; the line at the top says what happened.
- **Take all button:** it took nothing ("Nothing taken") from a body or
  chest; it now takes everything, as Shift+Space does.
- **Guest hints:** people a guest can't talk to no longer say "Avatar
  only".
- **Emotes on guests:** a guest's own /wave, /bow and the like now show on
  their own screen. Before, only the other players saw them.
- **Running is remembered:** after R switched to walking once, every
  start walked, even when you'd switched back to running. The controller
  page's "Enabled" and "Swap" choices had the same problem and now stick
  too.

## 0.8.1, 2026-10-30 (build 219)

### Changed

- **Co-op:** hosts and guests need the same release (the co-op protocol is
  now 37).
- **Taking books and bags:** Space reads a book or opens a bag everywhere,
  including inside a chest or someone's pack, so you can look before you
  take. Alt+Space (Option+Space on a Mac) takes it. On a stack, Alt+Space
  still splits. Shift+Space now only means "take all", inside a container.
- **Item menu (Ctrl+Space):** lists Read, Use or Open, Take and Split, as
  they apply. "Put in..." is gone.
- **Target hint:** the target's name and what the keys do stay at the top
  while you have a target, and go as soon as you clear it. They used to
  fade after a few seconds.
- **Mac keys:** hints and the Keys page say Option instead of Alt.
- **Take all:** the hint leaves it out when a container has only one thing
  in it, and says "Steal all" when any of it isn't yours.
- **Beds and bedrolls:** the Target System offers them. Space sleeps, and
  Alt+Space takes a bedroll.
- **Title screen:** a Quest of the Companions title under the Ultima VII
  logo, with the menu moved down to make room. The version line is gone
  from the title menu and Exult's game picker (it was too large or
  overlapped the menu); `--version` still shows it.
- **Targeting things:** E offers things within 18 tiles, and a picked thing
  is dropped once you're more than 18 tiles away or it goes off screen.
  Containers and signs used to be offered anywhere on screen. People keep
  their longer range.
- **Controller aiming:** with the pointer showing (right stick), L1 and R1
  pick the person or thing nearest the pointer first, then go on from
  there. Flick the stick toward a crowd and press. Without the pointer
  they still start nearest you.

### Fixed

- **Controller on Windows:** the right stick moves the pointer, and the
  triggers work, in the game itself. They only worked in menus.

- **Opening scene in co-op:** a guest who joined during the opening no longer
  plays a separate copy of it with its own conversation. Every scene now
  runs only on the host, and guests see it from there.
- **Scenes and cutscenes:** while the Avatar is in a scene, guests watch the
  Avatar, and their companion waits. When the scene ends, guests get their
  companion back.
- **The Avatar in the opening:** guests now see the Avatar arrive. Before,
  the Avatar stayed invisible for them.
- **Conversations stay in step:** a line too long for the dialogue box no
  longer stops the guest's game until the guest clicks. Guests turn the page
  when the host does.
- **Switching windows:** the host's game no longer pauses when you switch
  to another window during a conversation. Its world used to stop (scenes,
  people walking) and sometimes stayed stopped.
- **Targeting:** trolls and other tall creatures (cyclops, dragons) can be
  targeted. They were treated as hidden under a roof.
- **Attacking:** Space and Q always make the character you play attack the
  target: the first target, or one you switch to. Sometimes the Avatar
  stood still while the party fought, and a companion you led wouldn't
  attack the Avatar's foe.
- **Knocked-out Avatar:** if the Avatar is knocked out while you lead a
  companion, you keep control of that companion. The camera used to jump
  back to the Avatar and you couldn't move.
- **Labels:** the names shown when you click something sit just above it,
  whether it's a person, a thing on the map, or an item in a backpack or
  chest. Labels on things used to float too high, and in windows they
  covered the item.
- **Spittoon:** when a guest spits in a spittoon, everyone sees the "ptui!"
  and hears it, not only that guest.

## 0.8.0, 2026-10-26 (build 205, 617fa85)

The first release builds: an optimized Mac disk image (Apple silicon) and
a Windows build. Mac and Windows players can play together.

Before 0.8.0 there were no releases. Over 205 commits from 2026-10-14
(Exult's 1.13.1 development version), the roadmap's first phases were
built: playing a companion, two and then several players over the network,
overhead chat, a shared world (items, doors, containers, the journal),
co-op combat and story, shops and trainers for guests, lobby codes over the
internet, the Class System, the Target System and controller support.
