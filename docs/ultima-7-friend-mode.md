# Ultima VII: Friend Mode

**Status:** In development. Two-player co-op works on Mac, and Mac and Windows have played together  
**Project type:** Experimental cooperative multiplayer fork of Exult  
**Initial target:** macOS development, Mac/Windows cross-play  
**Players:** 2 today; next, one player per eligible party companion plus spectators

## Vision

Experience the original **Ultima VII** story together with friends without turning it into an MMO or rewriting the campaign. One player controls the Avatar; friends temporarily take direct control of existing party companions such as Iolo, Shamino, or Dupre. The world still treats the Avatar as the single story protagonist.

The emphasis is on preserving the original game: its story, handcrafted world, companions, inventory, combat, and interactions. Friend Mode adds human companionship, not a new quest structure.

## Core principles

1. **One Avatar, one story.** The Avatar remains the canonical story actor. Only the player currently controlling the Avatar may initiate story dialogue and deliberate story interactions.
2. **Companions remain NPCs.** Guest players control existing party actors; their NPC identity, inventory, stats, equipment, and story references are unchanged.
3. **Host-authoritative world.** The host Exult instance owns simulation, Usecode execution, NPC schedules, combat results, object state, quest flags, and save files. Guest clients send requested actions.
4. **Seamless handoff.** A disconnected guest's companion returns to normal AI control. The session can continue as single-player.
5. **Cross-platform by design.** macOS and Windows clients speak the same versioned network protocol; development can happen entirely on a Mac.
6. **Minimal original-game disruption.** Favor small, inspectable engine changes. Preserve compatibility with original game data and ordinary saves where feasible; verify before promising save compatibility.
7. **Prototype before infrastructure.** No accounts, matchmaking, Supabase, or hosted game server are needed for the first milestone.

## Intended player experience

- The host loads an Ultima VII save and starts a Friend Mode session.
- A guest selects an unclaimed eligible member of the Avatar's active party.
- Both players move independently, initially within a shared camera/view area.
- Only the Avatar controller initiates story dialogue, quest decisions, and scripted story interactions.
- Players communicate through **Ultima Online-style overhead text**, rendered above their characters and fading after a few seconds.
- Companions interact with objects, manage their own equipment and inventory, and take part in combat.
- The Avatar stays with the host. Guests don't take over the Avatar; a guest can switch to another free companion (Tab).

## Joining, party capacity, and spectators

The planned guest flow is:

1. Select **Connect to Host**.
2. Enter the host address. The final address/discovery experience is still to
   be decided.
3. After compatibility and session validation, receive the host's current list
   of eligible, unclaimed party companions.
4. Select a companion and request control. The host grants the claim
   atomically so two guests cannot take the same actor.
5. If no companion is available, join as a spectator and wait for one to
   become available.

The Avatar is never offered as an ordinary join choice. Active guest capacity
is determined by the eligible companions currently in the party, not by a
separate fixed multiplayer limit. The traditional target is the Avatar plus up
to seven companions; Exult currently reserves eight non-Avatar party slots
internally, so the implementation must use the live eligible roster rather than
hard-code either number.

Spectators follow the Avatar's view and cannot move actors, interact with the
world, open inventories, or initiate transactions. They can use player chat.
When an unclaimed eligible companion becomes available, waiting spectators may
request control. If a controlled companion leaves the party or otherwise
becomes ineligible, its player returns to spectator mode. On disconnect, the
companion returns to normal AI and becomes available again.

Spectator support and spectator chat are lower priority than the normal case of
joining an available companion, but the connection and assignment state model
should leave room for them from the start:

`Connecting -> Spectating -> Selecting -> Controlling companion -> Spectating`

The host may eventually expose a session option to reject excess connections
instead of allowing spectators, with spectators allowed as the default.

## Who controls what

Keep these concepts separate in the engine:

| Concept | Meaning |
| --- | --- |
| Story Avatar | The existing Avatar actor and canonical quest identity, always the host's |
| Network host | The machine running authoritative world simulation |
| Player controller | A local or remote human's input assignment to an actor |

Handing the Avatar to a guest was considered and dropped: the Avatar stays
with the host, which keeps story authority simple. Guests may switch between
free companions.

## Story and script safety

**Design intent:** The game continues to perceive one Avatar and ordinary party companions. However, ordinary NPC movement, combat, object interactions, proximity triggers, and Usecode can also change game state. Merely treating a guest-controlled companion as an NPC does **not** automatically prevent scripts from firing.

Initial restrictions:

- Avatar controller alone can open story dialogue and make quest decisions.
- Guests cannot directly invoke arbitrary Usecode or trigger story-critical interactions.
- Host validates every requested action against the controlled actor and current world state.
- Scripted sequences can temporarily suspend guest control of a companion and restore it afterward.
- Investigate proximity and object-triggered scripts, party separation, pathfinding, and combat scripts before enabling broad interactions.
- Define expected behavior for death, unconsciousness, resurrection, party removal, and save/load during a session.

## Camera and movement

### First playable version: shared camera

- All clients view the area around the Avatar.
- Companions move independently inside a permitted range.
- When a controlled companion strays too far, return them near the Avatar, inspired by existing party catch-up behavior.
- Use a forgiving distance threshold or grace period to avoid abrupt teleporting at the edge of the viewport.
- Teleport/return decisions come from the host and are synchronized to clients.

Independent exploration (separate cameras far apart) is a possible future
project; see the end of the roadmap.

## Overhead chat

- Press **Enter** to type; **Enter** sends, **Escape** cancels.
- Text appears above the speaker's character and fades after a short interval.
- The host relays chat to all participants.
- Chat is a separate, non-Usecode system: it must not trigger NPC conversations or quest scripts.
- Include basic length limits, sanitization, and a compact on-screen queue for overlapping messages.
- Optional later enhancements: chat history, distance-based visibility, emotes, and accessibility settings.

## Technical architecture

**Recommended model:** Host-authoritative *listen server*, rather than fully decentralized peer-to-peer.

```text
                   Mac or Windows host
                ┌────────────────────────┐
                │ Exult world simulation │
                │ Avatar + all NPCs      │
                │ Usecode / saves / AI   │
                │ Network session host   │
                └───────────┬────────────┘
                            │
            Commands in / authoritative state out
                            │
                ┌───────────┴───────────┐
                │                       │
        Guest Exult client      Guest Exult client
        Companion controller    Companion controller
        Camera / UI / chat      Camera / UI / chat
```

The host maintains a mapping of **connection → assigned actor** and accepts input only for that actor. Clients render replicated state and submit actions; they must not independently decide quest results or mutate authoritative world state.

Suggested early network messages:

- `JoinRequest`, `JoinAccepted`, `SessionRejected`
- `PartyRoster`, `ActorClaimRequest`, `ActorClaimAccepted`, `ActorClaimRejected`
- `SpectatorState`, `ActorAvailable`
- `ActorAssignment`, `MovementInput`, `ActorState`
- `ChatMessage`
- `CompanionReturn`, `Disconnect`, `Heartbeat`

Later add object interactions, inventory transactions, combat events, state snapshots, resynchronization, and save/load coordination.

Protocol considerations: versioning, explicit serialization (no raw C++ structs), stable actor/object identifiers, sequence numbers, validation, reconnect behavior, and mismatched game/mod versions. Start with a simple reliable transport for a LAN prototype; evaluate latency and other transports later.

## Roadmap

Progress notes and what each piece does are in
[the Friend Mode README](friend-mode/README.md).

### Phase 0 — Research and baseline (done)

- [x] Obtain legitimate original Ultima VII game data.
- [x] Clone the official Exult source and compile an **unmodified** build on macOS.
- [x] Launch The Black Gate and verify saves, party behavior, dialogue, and combat.
- [x] Trace actor identity, input dispatch, camera focus, party AI, script invocation, and object ownership in source.
- [x] Document build instructions and a reproducible test save.

### Phase 1 — Local character possession (done)

- [x] Add a development command to select an active party member as the controlled actor.
- [x] Redirect movement input and camera focus without changing the canonical Avatar actor.
- [x] Suspend companion follow AI while locally controlled.
- [x] Restore NPC AI on release.
- [x] Verify dialogue/quest logic still references the Avatar.
- [x] Support switching back safely, including scripted interruptions.

### Phase 2 — Two-player network movement (done)

- [x] Start a LAN listen-server session from the host.
- [x] Add host-address entry after **Connect to Host** (now **Co-Op Mode →
      Join a Game**).
- [x] Send the eligible, unclaimed party roster to a connected guest.
- [x] Let the guest select a companion and have the host atomically assign it.
- [x] Keep an unassigned guest in a spectator state when no companion is
      available (Tab picks one later).
- [x] Synchronize independent movement and actor positions.
- [x] Keep a shared Avatar-centered camera.
- [x] Implement distance-based companion return (the leash).
- [x] Handle disconnect by restoring companion AI.

### Phase 3 — Overhead chat (done)

- [x] Add UO-style overhead text chat.
- [x] Permit connected spectators to use chat while they wait for an actor.

### Phase 4 — Shared world interactions (done)

- [x] Synchronize picking up, dropping, moving, and using permitted objects.
- [x] Synchronize containers, equipment, and inventories.
- [x] Resolve simultaneous interactions and object ownership conflicts
      (built; the two-person checks still need a second player).
- [x] Add host-side validation and resynchronization checks.
- [x] Shared journal, world sounds, music, clock and weather.

### Phase 5 — Co-op story and combat (done)

- [x] Synchronize combat, damage, death, and recovery (guest attacks,
      missiles, spell effects, hits, health; optional enemy health bars).
- [x] Ensure the Avatar alone starts story dialogue and makes quest choices.
- [x] Mirror dialogue presentation to guests as spectators.
- [x] Pause guests during host conversations and menus.
- [x] Validate saving, loading, and guest reconnect behavior.
- [x] Co-Op Mode in the title and Escape menus; IPv4 and IPv6; quick
      connect and messages.

### Phase 6 — More than one guest

One player per eligible party companion, plus spectators. See the
[multi-guest plan](friend-mode/multi-guest-plan.md).

- [x] Several peers on the host, each with its own assignment, held items
      and leash.
- [x] The host relays chat and world updates to every guest.
- [x] Switching companions with Tab, follow mode (E) and Spectate.
- [x] Put the socket code in `friendnet.cc` behind one small connection
      interface at the same time, ready for WebRTC (Phase 8).
- [ ] The two-person checks (two people grabbing the same item), now with
      three or more.

**Exit criterion:** Three players (host and two guests) play a session
together.

### Phase 7 — Shops and trainers for guests

Guests can buy from merchants and train with trainers without touching the
story. The conversation runs on the guest's own copy only, like books and
signs today, so no story flag on the host changes; a guest who learns
something can tell the Avatar ("I think you have to talk to Nicodemus").
Only the purchase or training itself is carried out by the host.

- [x] Find how Black Gate's merchant and trainer scripts buy, sell and
      train, and which NPCs offer them
      ([findings and plan](friend-mode/shops-and-trainers.md)).
- [x] Let a guest's companion talk to merchants and trainers locally;
      other NPCs keep "The Avatar must do this".
- [x] Catch the purchase, sale or training on the guest and send it to the
      host as one request; the host checks and applies it and the usual
      sync brings the result back.
- [x] Stats, experience and level-ups reach guests.
- [x] Money stays the party's: the host checks and takes the party's gold,
      as the game already does, with no separate guest purse.
- [x] Keep anything else the local conversation changes (flags, items,
      remarks) on the guest only, and throw it away when it ends.

**Exit criterion:** A guest buys a sword in Britain and trains with a
trainer; the gold leaves the party and the item and stat change show on
both machines; no story flag changes.

### Phase 8 — Lobby codes over WebRTC, and release

The last step before an official release.

Joining today means typing the host's address and, over the internet, setting
up port forwarding. Replace that with a short code the host reads out and the
guest types in.

- [ ] A small lobby service that hands out codes and passes WebRTC connection
      details between host and guest. It never sees game traffic.
- [ ] Native WebRTC data channels (for example libdatachannel), reliable and
      ordered, which matches today's TCP stream. Send the world download in
      pieces.
- [ ] A relay (TURN) fallback for the networks where a direct connection fails.
- [ ] Keep direct address entry for LAN play and debugging.
- [ ] Optional session password; WebRTC also encrypts the connection.
- [x] Windows build ([Windows build plan](friend-mode/windows-build.md));
      Mac and Windows have played together.
- [ ] Mac/Windows compatibility testing and builds for release.
- [ ] Packaging, compatibility matrix, installation instructions, and
      GPL-compliant distribution.

**Exit criterion:** Two players on different home networks join with a code
and no router setup.

### Phase 9 — Action controls (optional)

Optional, more Diablo-like controls: walk with WASD, pick targets from the
keyboard, and keep the mouse for attacking and using things. It does not
depend on Phase 8, so it can ship in the first update after release or be
brought forward.

Both parts are off by default, each with its own toggle in the Game Input or
Game Engine options, and they work in single-player too. They change engine
input code (Exult's key bindings and combat targeting), so they are part of
our Exult build; a usecode mod pack cannot add them. Keep them in their own
commits, separate from the network code, so they could be offered to Exult
upstream.

- [ ] **WASD movement.** Build on Exult's existing keyboard walking
      (`ActionWalk*` in `keyactions.cc`), which already slows down in combat.
      Holding two keys walks diagonally (W+D = north-east).
- [ ] Move Save from S to Ctrl+S (Cmd+S on Mac) when WASD is on; the main menu
      still saves too. S is the only default key that conflicts.
- [ ] Guests: send held movement keys to the host as a steady walk stream, and
      check it feels responsive over a real internet connection, not just a
      local one.
- [ ] **Target cycling.** Q and E step to the previous or next hostile on
      screen, nearest first, with a highlight ring that works alongside the
      enemy health bars. Tab stays as the guest's character swap.
- [ ] Attack the selected target with a key or left-click, through the
      existing attack code. Hit chance, damage, and timing stay unchanged.
- [ ] Each player has their own target; the host relays it, and other players
      see a small marker for who is attacking what.
- [ ] Decide when party combat AI may switch away from a target a player
      picked by hand.
- [ ] Toggles in Game Input / Game Engine options, saved in the config; a
      guest's toggles apply to that guest only.

**Exit criterion:** With both toggles on, a guest walks with WASD, cycles
targets with Q/E and attacks the selected enemy, while the host plays with
the classic mouse controls.

### Co-op quality of life

Modern conveniences that fit U7's "the Avatar leads" style, decided
2026-10-17. Not tied to a phase; most are small and can land before or
after Phase 8.

- [ ] **Rejoin your companion.** When a guest drops, the host holds its
      companion for a few minutes (the party's AI drives it, as in follow
      mode). The same player name reconnecting gets it straight back,
      without the picker.
- [ ] **Player list.** A simple overlay: Player | Character | Health. Most
      games put it on Tab, so either the companion picker moves to another
      key or the list gets a free one. Alternative: add the player's name
      under each portrait in Exult's party bar.
- [ ] **Arrows to other players.** Like the arrow back to the Avatar when
      out of range, one per player, each in its own color.
- [ ] **Suggest a reply.** During the Avatar's conversation, a guest clicks
      a keyword to highlight it on the host's screen with their name. The
      Avatar still chooses. Few U7 choices really matter, but it lets
      guests take part.
- [ ] **Spectator camera.** Spectators press a key to cycle the view
      between players.
- [ ] **Autosave every 5 minutes** on the host, in their own rotating slots
      (for example the last 3), never over a player's own saves, with the
      oldest deleted so they don't pile up. Exult has no autosave of its own
      to build on.
- [ ] **Chat log.** A small log of recent lines that fades and can be
      dismissed, for spectators (who have no body) and everyone else.
      Players with a companion show overhead and in the log. Keep it to
      player chat and a few session notices, so it never fills up like an
      old MMO chat window.
- [ ] **Whispers and emotes**, Ultima Online style: `/w name text`, and
      emotes such as `/wave` or `/bow` that animate the companion.
- [ ] **Shared objective (to think about).** A short line the Avatar sets
      and keeps updated ("Find Christopher's murderer") as a reminder for
      the companions. No quest log or guide arrows.

Later: players' positions on the cloth map (M); pings to point at a spot
or object (more useful with a longer leash).

Decided against: returning to the title menu when the host leaves (Exult
doesn't work that way), votes or ready checks (the Avatar calls the shots;
revisit only with a much longer leash, like Minecraft's "everyone in bed"),
item requests (players ask in chat), auto-follow when idle (Spectate,
follow mode and the leash cover it), and stances (companions keep the
combat behavior Exult's AI already gives them).

## Possible future projects

Not part of this build-out; ideas for after the release.

### Independent exploration

Separate cameras, so the Avatar and companions can explore distant parts of
Britannia at once. See the [independent-views
research](friend-mode/independent-views-research.md): the host would have to
simulate several regions.

- [ ] Per-client camera tracking.
- [ ] Support distant regions with correct NPC and world simulation.
- [ ] Remove companion-return range restrictions.
- [ ] Test quests, schedules, sound, pathfinding, and world changes at long distance.

### Exult in the browser

Play from a web page with nothing to install, joining by lobby code. Exult has
no web build today, but it is C++ on SDL3, which can target the browser
through Emscripten.

- [ ] Spike: build unmodified single-player Exult with Emscripten.
- [ ] Exult waits in its own loops (clicks, conversations, delays); a browser
      page must keep handing control back. Try Emscripten's Asyncify before
      rewriting any loops, and measure speed and download size.
- [ ] Game data and saves in browser storage (see Licensing and game data).
- [ ] Digital (OGG) music first; MIDI synthesizers later, if at all.
- [ ] Friend Mode on the browser's own WebRTC, through the Phase 8 connection
      code.
- [ ] Hosting: browsers slow down background tabs, so a host in a hidden tab
      slows everyone. Warn the host, or keep hosting to native builds.

## Infrastructure choices

| Need | Initial choice | Later possibility |
| --- | --- | --- |
| Source hosting | Private Bitbucket repository | Public release when ready |
| Main development machine | Mac | Windows CI / testing |
| Authoritative simulation | Host Exult process | Dedicated server only if needed |
| Connection | LAN, or internet with port forwarding (TCP 1992) | WebRTC data channels with TURN relay fallback (Phase 8) |
| Lobby and invitations | None | Small lobby service for codes (Supabase or similar) |
| Client platform | Native Mac / Windows | Browser via Emscripten (possible future project) |
| Chat | Direct host-relayed packets | Optional history / moderation |
| Saves | Host local storage | Optional cloud backup |

Supabase is **not** needed to run the simulation. If introduced later, use it for optional lobby/session services, not as a real-time game-state authority.

## Repository workflow

Use a **private Bitbucket repo** as `origin` and the official Exult GitHub repository as `upstream`.

```bash
# Create an empty PRIVATE repository in Bitbucket first.
git clone https://github.com/exult/exult.git exult-friend-mode
cd exult-friend-mode
git remote rename origin upstream
git remote add origin git@bitbucket.org:YOUR_WORKSPACE/exult-friend-mode.git
git switch -c feature/friend-mode
git push -u origin feature/friend-mode
```

Keep project planning in `docs/friend-mode/` within the source tree. Track upstream changes separately and avoid broad refactors until the first local possession experiment works. Add Windows builds in CI once the project has a working baseline.

## Licensing and game data

- Exult is open source under GPL terms; confirm the exact repository license and bundled component licenses before distributing modified builds.
- Private development is permitted. Sharing binaries with testers can trigger obligations to provide corresponding source to those recipients.
- The original Ultima VII game data is **not** included with Exult and remains subject to its own copyright/licensing.
- Do not commit original game assets, purchased game files, or save data containing private information to the Bitbucket repository.
- Friend Mode is an experimental fan project, not an official Ultima release.
- A browser build must not host the Ultima VII files, even behind a "you must
  own the game" disclaimer; that is still redistribution. Players load their
  own copy once, and it stays in their browser's storage.

## Open technical questions

1. How strongly does Exult couple input, camera, and event dispatch to its main Avatar actor?
2. Can we safely suppress companion AI without suppressing scripted sequences that require that NPC?
3. Which Usecode triggers can ordinary party NPCs activate, even without initiating dialogue?
4. Does the engine update NPC schedules and objects outside the Avatar's vicinity?
5. What state needs authoritative replication for movement, object changes, and combat?
6. How do save/load and character-controller mappings interact?
7. How do we detect incompatible Exult builds, mods, and game data?
8. What parts of Exult Studio and the existing debugging tools can accelerate development?

## Next steps

1. **More than one guest** (Phase 6).
2. **Shops and trainers for guests** (Phase 7).
3. **Lobby codes over WebRTC**, the last step before an official release
   (Phase 8).

---

**Project motto:** *One Avatar. One Britannia. Bring a friend.*
