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
| **Leash** | Guests walk on their own but stay near the Avatar, inside the part of the world the host is running. Past one screen they see "Straying from the Avatar" and an arrow back; at 32 tiles they can't go further. Left behind when the Avatar moves on, they're beckoned to the Avatar's side after 5 seconds, or at once on a change of map. | [Leash](docs/friend-mode/README.md#leash) |
| **Lobby codes** | The host gets a short code (`MOONGLOW-4271`); friends type it in. No port forwarding, and a relay steps in when two home networks can't link directly. Joining by IP address and a session password work too. | [Lobby codes](#lobby-codes) |
| **Class System** | Each companion gets a few skills from their Ultima IV class, on a small skill bar, with cooldowns of about a minute: Iolo plays like a bard, Jaana like a healer. The Avatar gets a spell bar filled from the spellbook. Companions nobody plays use their skills through the Class AI. | [Companion skills](docs/friend-mode/companion-skills.md) |
| **Target System** | Pick foes, doors, levers, lamps and other things nearby from the keyboard (Q, E, Space) instead of hunting with the mouse. Which things it offers is set by group. | [Targeting groups](docs/friend-mode/targeting-groups.md) |
| **Action controls** | Walk with WASD (doors open in front of you and close behind you), R to run, and a corner minimap that shows where everyone is. | [Project brief, Phase 9](docs/ultima-7-friend-mode.md#phase-9--action-controls-optional) |
| **Leading a companion** | The host can take a companion in hand the way a guest does, use their skill bar, and have the party follow them. | [Leading companions](docs/friend-mode/leading-companions.md) |
| **Shops and trainers** | Guests buy from merchants and train with trainers using the party's gold, without moving the story. | [Shops and trainers](docs/friend-mode/shops-and-trainers.md) |
| **Controller** | Play with an Xbox or PlayStation pad, alongside the keyboard and mouse: targeting on the shoulders, a pad pointer on the right stick, every menu driven from the pad. In development. | [Controller](#controller) |

The Class System, the Target System and the action controls each have an
on/off switch in the game menu. With them off, it plays like Ultima VII
did.

## Controller

Plug in an Xbox or PlayStation pad (USB or Bluetooth) and play; the
keyboard and mouse keep working at the same time. The game menu's
**Controller** page lists the buttons with your pad's own names. In
development: the rings' plates are placeholders until their art is done.

PlayStation names first, then Xbox. "Held" means held for about a third of
a second; a tap does the other thing when you let go.

| Button | In the world |
| --- | --- |
| Left stick | Walk (as fast as R's run or walk allows) |
| L3 / LS (press the left stick) | Run or walk |
| Right stick | The pointer (it hides after 5 seconds) |
| R3 / RS (press the right stick) | Combat on or off |
| L1 / LB | Next person or creature (with L2 / LT: previous) |
| R1 / RB | Next thing (with L2 / LT: previous) |
| L2 / LT, held | Shift (L2+Cross: take all or split); with the pointer up, the left mouse button (hold to drag) |
| R2 / RT, held | Alt (R2+Cross: drop some); with the pointer up, the right mouse button |
| Cross / A | Act on the pick: talk, open, take, use, attack (in a companion's pack or paperdoll: ready or put away on them); held: the item menu (give, put in, split, drop) |
| Circle / B | Close the newest window (with none, clear the pick); held: sort it |
| Square / X, held | Skills ring: push the left stick to an entry, let go to use it |
| Triangle / Y, held | Utilities ring: eat, watch, map, journal, lockpick, keys, hold / follow, spellbook |
| D-pad up | Next character; held: the party holds or follows |
| D-pad left / right / down | Paperdoll / next backpack / stats |
| Share / View | The next open window to the front (its first thing picked); held: the player list (co-op) |
| Options / Menu | Clear the pick, then close windows, then the game menu; held: arrange the open windows side by side |

In menus, the D-pad or left stick moves the highlight, Cross chooses,
Circle or Options goes back, and left / right change a setting (L1 / R1 by
ten in "How many?"; in Load/Save, up / down pick a save and left / right
the buttons). In conversations, R1 / L1 or up / down pick a reply, Cross
says it, Options says bye. Cross also skips the intro.

In the Avatar's open spellbook (Triangle's ring, or B), the D-pad moves the
bookmark, L1 / R1 turn the circles, Cross casts the bookmarked spell at your
pick (L1 / R1 in the world), and holding Square puts it on one of the spell
bar's three slots (the left stick picks the slot, letting go puts it there).
On the keyboard, Ctrl+1 to Ctrl+3 do the same with the book open.

When the game asks you to click on something (a key, a spell with no pick),
R1 / L1 step through the things nearby and Cross clicks on the one picked.
On the keyboard: the item menu is Ctrl+Space, and E / Shift+E and Space do
the stepping and clicking.

The buttons can be changed in `exult.cfg` under
`config/gameplay/controller/buttons` (and `.../hold` for held actions).
Details: [the controller plan](docs/friend-mode/controller-plan.md). On a
Mac, a pad that won't open may need Input Monitoring allowed; the Steam
Controller works with Steam Input off for Exult.

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
