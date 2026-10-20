# Branches, layers and forks

Status: a plan (2026-10-20). Nothing is split yet.

**The aim:** anyone can take the parts of Friend Mode they want (only the
targeting, or co-op without the Class System) from clean branches, while the
everyday branch stays exactly as it is: one branch, every feature, one
build. The clean branches are made from it when it's time to publish; they
never replace it.

## The everyday branch

`feature/friend-mode` stays the working branch, with its full history and
every feature. Nothing about building it changes. Commits may keep mixing
things when that's quicker; the habits below only make the later split
cheaper.

## The layers

Each layer builds on the ones before it, and each, with those below, builds
and works on its own:

1. **Engine hooks.** Small, general changes to Exult's own files, useful by
   themselves: `Gump::is_hud`, `Time_queue::is_paused`, the spellbook's aim
   target, the per-window pause flag, the stat and damage hooks. As generic
   callbacks, not `Friend_*` calls, so upstream could take them.
2. **Single-player improvements.** The Target System (Q, E, Space, groups,
   hints, Steal, reading), WASD movement, doors opening by walking, smart
   placement and Sort, backpack and paperdoll keys, enemy health bars, Main
   Menu, the corner minimap. No network. See [upstreaming](upstreaming.md).
3. **Co-op core.** The connection layer and TCP, world sync, chat, emotes,
   whispers, the chat log, the leash, follow mode, the picker and
   spectating, the shared journal, guests' shops and trainers, autosave,
   rejoining, the co-op pause rules.
4. **Lobby and WebRTC.** `tools/lobby`, the `third_party` submodules
   (libdatachannel, Mbed TLS), the WebRTC connection: a second way to
   connect.
5. **Class System.** Skills, the skill and spell bars, Class AI, the effect
   preview. Uses 2 (picks, the range circle) and 3 (guests); works in single
   player too.

So someone can stop after 2 (a better single-player Exult), 3 (co-op over
LAN or Tailscale) or 4 (co-op with codes), and take 5 or leave it.

## Habits from now on (cheap)

- **One system per commit when it's easy;** the subject starts with it
  ("Class System:", "Target System:", "Co-op:", "Lobby:", "Docs:"). A fix
  that touches two systems is better as two commits, but needn't be.
- **Docs in their own commits,** or at least only under `docs/friend-mode/`,
  so a clean branch can leave them out.
- **New features in their own files** (as `friendclass.cc`, `friendmap.cc`,
  `friendnet.cc`); `exult.cc` doesn't grow.
- **Exult's own files only get hooks:** one or two lines, with a comment
  naming the system.
- **Single-player features get neutral names;** `Friend_` stays for co-op.
- **Upstream Exult merged in now and then,** so the clean branches never
  drift far from it.

## The one refactor before a public fork

`exult.cc` holds about 11,000 lines, with co-op mixed among single-player
features. Before publishing, split its Friend Mode code by system, moving
code only (no change in behaviour):

| File | What moves there |
| --- | --- |
| `friendtarget.cc` | The Target System: Q, E, Space, the arrow, the range circle, groups, hints |
| `friendinput.cc` | WASD, backpack and paperdoll keys, doors by walking |
| `friendchat.cc` | Chat, emotes, whispers, the chat log, notices |
| `friendcoop.cc` | Guests, sync, the leash, follow mode, the picker, journal, shops |
| `exult.cc` | Exult's main loop again, with hook calls |

Then a build switch (for example `--enable-friend-mode`, and a CMake or
Visual Studio equivalent) builds Exult with layers 1 and 2 only. The
everyday build keeps everything on.

## Making the clean branches (when publishing)

1. From upstream Exult's `master`, one branch per layer in order
   (`friend/1-hooks`, `friend/2-single-player`, `friend/3-coop`,
   `friend/4-lobby`, `friend/5-classes`), each stacked on the one before.
2. Each a short series of small commits, one feature each. With the code
   split by file, a feature's commit is mostly "add these files, add these
   hooks".
3. Check with `git range-diff` and a full build that the top branch
   adds up to the same code as `feature/friend-mode`.
4. A README on the fork: the layers, which branch to start from, how to
   build, and the licences (Exult GPL; libdatachannel MPL-2.0; Mbed TLS
   Apache-2.0 or GPL).

`feature/friend-mode` keeps going as before; the clean branches are made
again at each release, or kept up by cherry-picking.

## Why not one squashed snapshot

A single commit of everything against upstream (as some forks offer) is
easy to make but hard to review and impossible to take parts from. The
layered series costs the refactor once; after that it is close to
mechanical.
