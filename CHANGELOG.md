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
both: `--version`, the title menu (bottom left), Exult's game picker, and
the version files saved with each game.

## Unreleased (0.8.1)

### Changed

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

### Fixed

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

### Changed

- The co-op protocol is now 37: hosts and guests need the same release.

## 0.8.0, 2026-10-26 (build 205, 617fa85)

The first release builds: an optimized Mac disk image (Apple silicon) and
a Windows build. Mac and Windows players can play together.

Before 0.8.0 there were no releases. Over 205 commits from 2026-10-14
(Exult's 1.13.1 development version), the roadmap's first phases were
built: playing a companion, two and then several players over the network,
overhead chat, a shared world (items, doors, containers, the journal),
co-op combat and story, shops and trainers for guests, lobby codes over the
internet, the Class System, the Target System and controller support.
