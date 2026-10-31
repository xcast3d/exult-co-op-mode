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

## Unreleased (0.8.2)

### Changed

- **Co-op:** hosts and guests need the same release (the co-op protocol is
  now 38).
- **Winches:** the Target System offers them with levers and switches, and
  Space turns them.

### Fixed

- **Skill range while the Avatar talks:** a guest hovering the skill bar
  during a conversation no longer leaves a range circle stuck on screen.
  The bar ignores the mouse until the conversation ends.
- **Number answers in conversations:** when the Avatar answers with a
  number (how many to buy, for instance), guests now see the Avatar's
  slider move as the host sets it, instead of an empty space. Guests can't
  use it.
- **Bags inside chests:** guests can open a bag or box inside a chest or
  body. They used to get a red X.
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
