# Ultima VII Co-Op (working name)

Play **Ultima VII: The Black Gate** with your friends. One player is the
Avatar; each friend plays a companion in the Avatar's party: Iolo, Shamino,
Dupre, Jaana and the rest. Same story, same world, same party, now with
people in it.

This is a fork of [Exult](https://exult.info), the engine that runs
Ultima VII on modern computers. Exult's own readme is in
[README-exult.md](README-exult.md). You need your own copy of Ultima VII;
no game files come with this project.

**Status:** in development. Mac and Windows play together; there's no
public release yet.

## How it plays

- **One Avatar, one story.** The host plays the Avatar and starts the
  conversations and story decisions. Guests can follow along, suggest a
  reply, and talk among themselves, but the story moves only as it would
  in the original.
- **Companions stay themselves.** A guest plays the real companion, with
  their own stats, pack, paperdoll and equipment. If a guest leaves, the
  companion goes back to the party's AI, and the same player rejoining gets
  them straight back.
- **The host keeps the game.** The host's machine runs the world and owns
  the save; it autosaves while guests play. A guest's own game is never
  touched by joining someone else's.

## Major systems

| System | What it does | More |
| --- | --- | --- |
| **Co-op party** | One guest per companion in the party, plus spectators. Shared world: doors, items, fights, merchants and the clock match on every screen. Overhead chat, emotes, a chat log and a player list. | [Project brief](docs/ultima-7-friend-mode.md) |
| **Lobby codes** | The host gets a short code (`MOONGLOW-4271`); friends type it in. No port forwarding, and a relay steps in when two home networks can't link directly. Joining by IP address and a session password work too. | [Lobby codes](#lobby-codes) |
| **Class System** | Each companion gets a few skills from their Ultima IV class, on a small skill bar, with cooldowns of about a minute: Iolo plays like a bard, Jaana like a healer. The Avatar gets a spell bar filled from the spellbook. Companions nobody plays use their skills through the Class AI. | [Companion skills](docs/friend-mode/companion-skills.md) |
| **Target System** | Pick foes, doors, levers, lamps and other things nearby from the keyboard (Q, E, Space) instead of hunting with the mouse. Which things it offers is set by group. | [Targeting groups](docs/friend-mode/targeting-groups.md) |
| **Action controls** | Walk with WASD (doors open in front of you and close behind you), R to run, and a corner minimap that shows where everyone is. | [Project brief, Phase 9](docs/ultima-7-friend-mode.md#phase-9--action-controls-optional) |
| **Leading a companion** | The host can take a companion in hand the way a guest does, use their skill bar, and have the party follow them. | [Leading companions](docs/friend-mode/leading-companions.md) |
| **Shops and trainers** | Guests buy from merchants and train with trainers using the party's gold, without moving the story. | [Shops and trainers](docs/friend-mode/shops-and-trainers.md) |

The Class System, the Target System and the action controls each have an
on/off switch in the game menu. With them off, it plays like Ultima VII
did.

## Lobby codes

The **lobby** is a small server that hands out codes and introduces the
host and its guests to each other, then steps aside. It never carries game
traffic: once players are introduced, they connect directly over WebRTC.
When two networks can't connect directly, the connection goes through a
TURN relay instead, which passes along the game traffic but can't read it.

- The project's lobby is a Cloudflare Worker at
  `u7-multiplayer-lobby.xcast3d.com`, and the game uses it unless told
  otherwise. Its source is in [tools/lobby](tools/lobby).
- Anyone can run their own lobby and point the game at it with one setting.
  [The lobby protocol](docs/friend-mode/lobby-protocol.md) describes what
  the game expects.
- [How the lobby works](docs/friend-mode/lobby.md),
  [playing over the internet](docs/friend-mode/internet-play.md) and
  [security](docs/friend-mode/security.md) cover the details.

## Building

- **Mac:** [macOS setup](docs/friend-mode/macos-setup.md).
- **Windows:** built on GitHub Actions, see
  [the Windows build](docs/friend-mode/windows-build.md). Builds are
  unsigned for now, so Windows warns the first time you run one.

Development notes, test checklists and the roadmap are in
[docs/friend-mode](docs/friend-mode/README.md).

## Credits and license

Built on [Exult](https://github.com/exult/exult) by the Exult team, and
licensed like Exult under the GNU General Public License, version 2 (see
[COPYING](COPYING)). Ultima VII is © Origin Systems / Electronic Arts. This
is a fan project, not affiliated with or endorsed by Electronic Arts or the
Exult team.
