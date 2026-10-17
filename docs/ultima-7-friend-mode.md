# Ultima VII: Friend Mode

**Status:** Concept / pre-development planning  
**Project type:** Experimental cooperative multiplayer fork of Exult  
**Initial target:** macOS development, Mac/Windows cross-play  
**Players:** 2 initially, eventually one active player per eligible party actor plus spectators

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
- Companions can eventually interact with objects, manage their own equipment and inventory, and participate in combat.
- Any guest can request control of the Avatar. The current Avatar controller accepts or declines. On acceptance, the two players swap their assigned actors, **not** the underlying actor identities.
- Later, companions can explore distant areas with independent cameras.

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

## Avatar control and character switching

Distinguish these concepts in the engine:

| Concept | Meaning | Changes during a swap? |
| --- | --- | --- |
| Story Avatar | The existing Avatar actor and canonical quest identity | No |
| Network host | The machine running authoritative world simulation | No |
| Player controller | A local or remote human's input assignment to an actor | Yes |

### Request-and-approval flow

1. Player B selects **Request Avatar**.
2. Current Avatar controller receives a nonblocking request: **Accept / Decline**.
3. If declined or timed out, nothing changes.
4. If accepted, the host atomically reassigns controllers: Player B → Avatar; former Avatar controller → Player B's previous companion.
5. Each client updates input focus and camera target. Character inventories, stats, NPC IDs, and story state stay with the actors.
6. Avoid swapping during a cutscene, active dialogue, scripted movement, or another unsafe transition; queue or reject the request with an explanation.

For 3–4 players, later allow companion-to-companion swaps with consent from both affected players.

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

### Later: independent exploration

- Each client camera follows its assigned actor.
- Characters can occupy distant parts of Britannia without being snapped back.
- Investigate whether Exult's simulation, map loading, NPC schedules, proximity processing, and audio are currently tied to the Avatar's position or viewport.
- Preserve one authoritative world while supporting multiple independently observed regions.

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
- `ControlSwapRequest`, `ControlSwapDecision`, `ControlSwapCommitted`
- `CompanionReturn`, `Disconnect`, `Heartbeat`

Later add object interactions, inventory transactions, combat events, state snapshots, resynchronization, and save/load coordination.

Protocol considerations: versioning, explicit serialization (no raw C++ structs), stable actor/object identifiers, sequence numbers, validation, reconnect behavior, and mismatched game/mod versions. Start with a simple reliable transport for a LAN prototype; evaluate latency and other transports later.

## Roadmap

### Phase 0 — Research and baseline

- [ ] Obtain legitimate original Ultima VII game data.
- [ ] Clone the official Exult source and compile an **unmodified** build on macOS.
- [ ] Launch The Black Gate and verify saves, party behavior, dialogue, and combat.
- [ ] Trace actor identity, input dispatch, camera focus, party AI, script invocation, and object ownership in source.
- [ ] Document build instructions and a reproducible test save.

**Exit criterion:** Clean local Exult build and a short architecture note identifying the control points.

### Phase 1 — Local character possession

- [ ] Add a development command to select an active party member as the controlled actor.
- [ ] Redirect movement input and camera focus without changing the canonical Avatar actor.
- [ ] Suspend companion follow AI while locally controlled.
- [ ] Restore NPC AI on release.
- [ ] Verify dialogue/quest logic still references the Avatar.
- [ ] Support switching back safely, including scripted interruptions.

**Exit criterion:** One local user can switch between Avatar and Iolo without breaking the story.

### Phase 2 — Two-player network movement

- [ ] Start a LAN listen-server session from the host.
- [ ] Add host-address entry after **Connect to Host**.
- [ ] Send the eligible, unclaimed party roster to a connected guest.
- [ ] Let the guest select a companion and have the host atomically assign it.
- [ ] Keep an unassigned guest in a non-interactive spectator state when no
      companion is available.
- [ ] Synchronize independent movement and actor positions.
- [ ] Keep a shared Avatar-centered camera initially.
- [ ] Implement distance-based companion return.
- [ ] Handle disconnect by restoring companion AI.

**Exit criterion:** Two computers can walk around the same Britannia area without diverging.

### Phase 3 — Overhead chat and Avatar handoff

- [x] Add UO-style overhead text chat.
- [x] Permit connected spectators to use chat while they wait for an actor.
- [ ] Implement request/accept/decline for Avatar control.
- [ ] Atomically swap controller assignments without swapping actor identities.
- [ ] Test host remains authoritative after handing over Avatar control.

**Exit criterion:** Two players can talk and hand over the Avatar during a live session.

### Phase 4 — Shared world interactions

- [ ] Synchronize picking up, dropping, moving, and using permitted objects.
- [ ] Synchronize containers, equipment, and inventories.
- [ ] Resolve simultaneous interactions and object ownership conflicts.
- [ ] Add host-side validation and resynchronization checks.

**Exit criterion:** One player moves an item; both see the same result and inventories agree.

### Phase 5 — Co-op story and combat

- [ ] Synchronize combat, damage, death, and recovery.
- [ ] Ensure the Avatar alone starts story dialogue and makes quest choices.
- [ ] Mirror dialogue presentation to guests as spectators.
- [ ] Support scripted temporary control overrides.
- [ ] Validate saving, loading, and guest reconnect behavior.

**Exit criterion:** A short story sequence and combat encounter work end-to-end with two players.

### Phase 5b — Lobby codes over WebRTC

Joining today means typing the host's address and, over the internet, setting
up port forwarding. Replace that with a short code the host reads out and the
guest types in.

- [ ] Put the socket code in `friendnet.cc` behind one small connection
      interface, so the transport can change without touching game code. Do
      this together with multi-guest support, which needs several peers
      anyway ([multi-guest plan](friend-mode/multi-guest-plan.md)).
- [ ] A small lobby service that hands out codes and passes WebRTC connection
      details between host and guest. It never sees game traffic.
- [ ] Native WebRTC data channels (for example libdatachannel), reliable and
      ordered, which matches today's TCP stream. Send the world download in
      pieces.
- [ ] A relay (TURN) fallback for the networks where a direct connection fails.
- [ ] Keep direct address entry for LAN play and debugging.

**Exit criterion:** Two players on different home networks join with a code
and no router setup.

**When:** after the two-player gameplay sync settles (Phase 4 and 5 work), and
before Phase 7 distribution. It gets friends who aren't technical playing, and
it is also the transport a browser build would need.

### Phase 6 — Independent exploration

- [ ] Per-client camera tracking.
- [ ] Support distant regions with correct NPC and world simulation.
- [ ] Remove companion-return range restrictions.
- [ ] Test quests, schedules, sound, pathfinding, and world changes at long distance.

**Exit criterion:** Avatar and companion can explore separate locations without desynchronization.

### Phase 7 — 3–4 players and distribution

- [ ] Additional companion assignments and consensual companion swaps.
- [ ] Mac/Windows compatibility testing and automated builds (brought
      forward: see the [Windows build plan](friend-mode/windows-build.md)).
- [ ] Easier remote connections: lobby codes from Phase 5b.
- [ ] Packaging, compatibility matrix, installation instructions, and GPL-compliant distribution.

### Phase 8 (possible) — Exult in the browser

Play from a web page with nothing to install, joining by lobby code. Exult has
no web build today, but it is C++ on SDL3, which can target the browser
through Emscripten.

- [ ] Spike: build unmodified single-player Exult with Emscripten.
- [ ] Exult waits in its own loops (clicks, conversations, delays); a browser
      page must keep handing control back. Try Emscripten's Asyncify before
      rewriting any loops, and measure speed and download size.
- [ ] Game data and saves in browser storage (see Licensing and game data).
- [ ] Digital (OGG) music first; MIDI synthesizers later, if at all.
- [ ] Friend Mode on the browser's own WebRTC, through the Phase 5b connection
      interface.
- [ ] Hosting: browsers slow down background tabs, so a host in a hidden tab
      slows everyone. Warn the host, or keep hosting to native builds.

**Exit criterion:** A guest in a browser joins a native or browser host by
code and plays a short session.

## Infrastructure choices

| Need | Initial choice | Later possibility |
| --- | --- | --- |
| Source hosting | Private Bitbucket repository | Public release when ready |
| Main development machine | Mac | Windows CI / testing |
| Authoritative simulation | Host Exult process | Dedicated server only if needed |
| Connection | Loopback / LAN | WebRTC data channels with TURN relay fallback (Phase 5b) |
| Lobby and invitations | None | Small lobby service for codes (Supabase or similar) |
| Client platform | Native Mac / Windows | Browser via Emscripten (Phase 8, possible) |
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
7. What happens when the Avatar is controlled remotely but the original host remains connected as a companion?
8. How do we detect incompatible Exult builds, mods, and game data?
9. What parts of Exult Studio and the existing debugging tools can accelerate development?

## Immediate next steps

1. Find the original Ultima VII data (or obtain a legitimate digital copy).
2. Set up the private Bitbucket repository and preserve the official Exult upstream remote.
3. Compile and launch unmodified Exult on the Mac.
4. Inspect the actor/input/camera implementation before changing code.
5. Build **Phase 1: local party-member character switching** as the first proof of concept.

---

**Project motto:** *One Avatar. One Britannia. Bring a friend.*
