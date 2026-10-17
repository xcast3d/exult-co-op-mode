# Multi-guest sessions and companion switching (plan)

**Status:** M1 and M2 built and tested 2026-10-17 (protocol version 26); M3–M5 to do. Decided: a connection interface in its own files,
shared change caches broadcast to every guest, and up to 4 watchers beyond
one guest per companion (trim to 7 guests later if internet tests need it).
**Goal:** several guests in one host session, each controlling a different
companion, with free companion switching in the style of Marvel Ultimate
Alliance. Target 4 players for testing; design for the full party.

## Decisions

- **The host always controls the Avatar.** Guests never control it, which
  removes the Avatar handoff and any question of moving authority. A later
  Avatar swap can reuse the switching mechanism once dialogue choices can come
  from a guest.
- **Switching needs no request or approval.** A guest picks any eligible
  companion that nobody controls. The host grants it at once, and the companion
  they left becomes free.
- **Capacity is the party.** The number of active guests is limited by
  eligible companions (up to 8 in Exult). Further connections become
  spectators, as the main plan already describes.
- **The host stays the only authority.** Guests still send requests and render
  what the host sends them.

## Single-guest assumptions today

| Where | Assumption |
| --- | --- |
| `friendnet.h` | One `peer_socket`, one receive buffer, one `outgoing` queue, one set of `pending_*` reassembly fields, one `remote_player_name`. |
| `gamewin.h/.cc` | One `remote_friend_actor` and one saved schedule (`reserve_friend_mode_actor`, `release_remote_friend_mode_actor`). |
| `exult.cc` host loop | One `host_assigned_actor`, one `last_host_state`, `friend_roster_sent` set once, `friend_host_peer_name` for notices. |
| `exult.cc` chat/labels | `Friend_mode_remote_speaker` and `Friend_mode_display_name` assume one remote player. |
| Snapshots | NPC snapshots skip just `host_assigned_actor`; guests apply every NPC state. |

## Architecture

### 1. Split the session into connections (`friendnet`)

- Add a `Peer` struct holding one connection's socket, receive buffer and
  partial packet, outgoing queue, handshake flag, player name, and its own
  reassembly state.
- The host keeps a list of `Peer`s plus the listening socket. A guest has
  exactly one `Peer` (the host).
- Every incoming event the host reads carries the sender's `player_id`, e.g.
  `take_move_request(player_id&, ...)`.
- Sending gets two forms: `send_to(player_id, ...)` and `broadcast(...)`, with
  an optional "except this player".
- **Cleanup that pays for itself:** speech, chat, identity and dialogue text
  each have their own begin/chunk code and reassembly fields. Replace them with
  one generic text message: a begin packet holding kind, metadata and length,
  followed by chunks. That leaves one piece of reassembly state per peer
  instead of four.

### 2. Player IDs and the controller table

- The host gives each accepted connection a small `player_id` (host = 0,
  guests 1..N) and sends it in the accept packet.
- The host keeps a **controller table**: `player_id -> {name, actor or
  spectator}`. Whenever the table changes it broadcasts a `PlayerList`.
- Guests use the `PlayerList` for name labels (`Iolo (Marcus)`), chat sender
  names, and the list of available companions. This replaces both
  `remote_player_name` and the one-off roster.
- `Game_window` replaces the single `remote_friend_actor` with a map of
  reserved actors and their saved schedules: `reserve_friend_mode_actor(npc)`,
  `release_remote_friend_mode_actor(npc)`, `is_remote_controlled(npc)`.

### 3. Companion switching

1. A guest (or spectator) sends `SwitchRequest(npc)`.
2. The host checks that the companion is eligible
   (`Friend_actor_is_eligible`) and not in the controller table. The host
   handles packets one at a time in its main loop, so two guests asking for the
   same companion cannot both get it.
3. On success the host releases the guest's old companion (restoring its
   party AI), reserves the new one, updates the table, sends `ActorAssigned` to
   that guest, and broadcasts the new `PlayerList`.
4. On failure it sends `SwitchRejected` with a reason (taken, ineligible, in
   dialogue).
5. **Guest UI:** `Tab` (unbound in the default BG keys) opens the existing
   **Select Your Character** carousel,
   showing your current companion and the free ones, using live data from the
   `PlayerList`.

Switching is allowed during combat. The companion left behind must rejoin the
fight: releasing it currently restores the schedule saved when it was
reserved, which may be "follow" rather than combat, so M4 must switch it to
combat when the party is fighting.

Switching is blocked while host dialogue is mirrored, during scripted control,
and while the guest's current companion is in a transition that
`enforce_friend_mode_control_safety` already guards.

### 4. Host update loop per guest

- **Move requests:** checked against *that guest's* actor (the existing
  distance ≤ 2 rule).
- **Authoritative actor state:** sent only to the owning guest, as today.
- **NPC snapshots:** broadcast to everyone, now including companions that
  *other* guests control. A guest skips NPC state for its own actor.
- **Avatar state, speech and dialogue mirroring:** broadcast.
- **Join, enter-world and leave notices:** shown on the host and broadcast to
  guests.

### 5. Chat relay

- A guest sends chat to the host, which ignores the speaker the guest claims
  and looks it up in the controller table.
- The host shows the message, then relays it to **every other guest** with the
  speaker's NPC and `player_id`.
- Spectators send no actor, so their chat appears at the top of the view as
  `Name: text`, using the `PlayerList` name.

### 6. Disconnects and party changes

- **A guest leaves:** release its companion, remove it from the table,
  broadcast. Other guests are unaffected.
- **A controlled companion dies, leaves the party, or becomes ineligible:**
  the host releases it, the guest becomes a spectator, and the carousel opens
  automatically so they can pick a free companion.
- **Host save/load:** restore every reserved companion's schedule before
  saving (today this is done for one companion), then re-reserve afterwards.
- **The host quits:** every guest shows the notice and exits (current
  behavior).

## Spectators (proposed, not yet decided)

- **Viewing:** spectators press `Tab` to open the same carousel. It lists free
  companions they can claim, plus "Watch" entries for the Avatar and each
  player-controlled companion. While everyone shares the Avatar-centred view,
  watching only re-centres the camera on that actor. Once each player has their
  own camera (independent exploration, a possible future project), the host must also send the area around the watched
  actor.
- **Chat:** with many players, overhead bubbles fade and overlap, and
  spectators have no body to speak from. Add a small chat log for everyone (a
  few recent lines, fading, toggleable), like Ultima Online's journal. Every
  message goes there; player messages also appear overhead. Channels and a
  full chat window are deferred.
- **Cap:** a configurable host limit on spectators, which protects the host
  rather than its performance (see below).

## Bandwidth check

Packets are 20 bytes. NPC snapshots go out every 100 ms; with about 30 nearby
NPCs that is roughly 6 KB/s per guest, or 42 KB/s for 7 guests. That's trivial
on a LAN and fine for a typical home upload. Nothing here requires changing the
TCP transport.

## Local testing with several guests

Each guest instance needs its own config, gamedat and save folders. Add
`.friend-mode-local/guest2.cfg` (and so on) and a small launch script that
starts N guests with `--friend-join 127.0.0.1`.

## Milestones

| # | Slice | Done when |
| --- | --- | --- |
| M1 | Connection interface (`friendconn.h`, `friendtcp.cc`), session split into peers plus generic text messages. Still one guest. | The current two-player test passes unchanged. |
| M2 | Player IDs, controller table, `PlayerList`, multiple reserved actors on the host. | Two guests control Iolo and Spark at the same time and see each other move. |
| M3 | Chat relay and broadcast notices. | All three players see each other's chat over the right heads. |
| M4 | Companion switching on `Tab`. | A guest moves from Iolo to Dupre, and a second guest then takes Iolo. |
| M5 | Party-change edge cases, save/load with several reserved companions, a 4-player test. | A companion dying or leaving moves its player to spectator cleanly. |

## Out of scope here

- Host-assigned creature network IDs (separate plan; needed before large fights).
- Independent cameras (a possible future project). Everyone still shares the Avatar-centred view.
- Inventory and object sync (Phase 4).

## Open questions

1. ~~Should spectators be capped?~~ Decided: 4 beyond one guest per
   companion. Revisit after internet tests (possibly 7 guests at most).
