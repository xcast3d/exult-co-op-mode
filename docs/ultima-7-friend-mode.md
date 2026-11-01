# Ultima VII: Friend Mode

**Status:** Feature-complete for a first release (2026-10-26). Co-op on Mac and Windows, tested many times, Mac and Windows together; joining by lobby code (WebRTC, with a TURN relay) or by address; the Class System (companion skills, the Avatar's spell bar, Class AI); the Target System and action controls; full controller support. Next: release builds and packaging  
**Project type:** Experimental cooperative multiplayer fork of Exult  
**Initial target:** macOS development, Mac/Windows cross-play  
**Players:** the host plus one guest per party companion (built; tested with two players so far), plus spectators

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

The guest flow (built):

1. Choose **Co-Op Mode → Join a Game** on the title menu.
2. Enter a name, the host's **lobby code** (`MOONGLOW-4271`) or address, and
   the session password if there is one.
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

- Press **Enter** to type (or **/**, which starts the line with "/" for a command); **Enter** sends, **Escape** cancels.
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
- [x] Switching companions with Tab, follow mode (Y, was E) and Spectate.
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

- [x] A small lobby service that hands out codes and passes WebRTC connection
      details between host and guest. It never sees game traffic. Built
      2026-10-20 in `tools/lobby/` (Cloudflare Worker, tested locally), for
      `u7-multiplayer-lobby.xcast3d.com`; deployed the same day. See
      [the lobby](friend-mode/lobby.md).
- [x] Native WebRTC data channels (libdatachannel with Mbed TLS), reliable
      and ordered, which matches today's TCP stream; the world download goes
      in 16 KB pieces. Built 2026-10-23 in `friendrtc.cc` behind
      `Friend_connection`: the host gets a code from the lobby (shown on
      screen, in the chat log, and on the Escape menu's Code button), and
      **Join a Game** takes a code or an address. Checked on one Mac through
      the deployed lobby with the session test
      (`tools/friend-mode/rtc-session-test/`): connected in about 1.2 s,
      300 KB intact, chat and steps both ways. Tested by hand the same day:
      host and guest on one Mac joined by code through the live lobby and
      played exactly as over TCP. Since then played across networks.
- [x] A relay (TURN) fallback for the networks where a direct connection
      fails (the lobby hands out short-lived TURN credentials).
- [x] Keep direct address entry for LAN play and debugging: the host always
      listens on TCP 1992 as well, and an address in Join a Game uses it.
- [x] Windows: lobby codes in the Windows build (a recursive submodule
      checkout, `build-rtc.sh` under MSYS2, and a CA bundle shipped with the
      game), built by the `friend-mode-windows` workflow.
- [x] Optional session password; WebRTC also encrypts the connection.
      Built 2026-10-23 with the Co-Op menu (Lobby code or IP, the code or
      address to share, close to new players) and a kick X in the Tab list;
      see [the lobby](friend-mode/lobby.md). Security sweep 2026-10-24 (a
      host could write files outside a guest's game folder, now fixed;
      limits on half-open connections, password guessing and renaming):
      see [security](friend-mode/security.md).
- [x] Windows build ([Windows build plan](friend-mode/windows-build.md));
      Mac and Windows have played together, tested many times (2026-10-26).
- [x] Mac/Windows compatibility testing.
- [x] Builds for release (Mac and Windows): the first, 2026-10-26, from
      `617fa85` (an optimized, self-contained Mac disk image, arm64, ad-hoc
      signed; the Windows workflow's zip). How:
      [macOS release build](friend-mode/macos-setup.md#release-build-a-disk-image-to-share).
- [ ] Packaging, compatibility matrix, installation instructions, and
      GPL-compliant distribution.

**Exit criterion:** Two players on different home networks join with a code
and no router setup.

**Possible path (not decided, 2026-10-18): Cloudflare.**

- **Lobby:** a Cloudflare Worker with one Durable Object per code. The host
  opens a WebSocket and gets a code (`RAVEN-42`); each guest connects with
  it; the lobby passes the WebRTC offers, answers and ICE candidates, then
  steps aside. No game traffic, so it fits the free plan.
- **STUN:** Cloudflare's public `stun.cloudflare.com:3478`.
- **TURN:** Cloudflare's managed TURN service. The Worker keeps the key
  and hands out short-lived credentials when a game starts, so no secret
  ships in Exult. Check its pricing first; a lobby without TURN returns
  STUN only.
- **Exult:** libdatachannel (MPL-2.0) data channels, reliable and ordered,
  behind `Friend_connection` as `friendrtc.cc` next to `friendtcp.cc`.
  Getting it into the Mac and Windows builds is the biggest risk, so prove
  it first.
- **No hard-coded server:** the lobby address is a setting
  (`config/friend/lobby_url`), defaulting to the project's Worker. The
  Worker's source lives in the repo (`tools/lobby/`, deployed with
  `wrangler deploy`), so anyone can run their own. TURN settings come from
  the lobby, not the build. Direct IP stays for LAN, Tailscale, and when
  the lobby is down.
- **Avoided:** relaying all game traffic through a Worker over WebSockets.
  Simpler, but every small packet would count against Workers' request
  limits and add latency.
- **Until then:** Tailscale (each player installs it; the host shares the
  Mac with them; guests type its `100.x` address) gets through carrier
  NAT with no code changes, and is the way to run internet tests now.

### Phase 9 — Action controls (optional)

Optional, more Diablo-like controls: walk with WASD, pick targets from the
keyboard, and keep the mouse for attacking and using things. It does not
depend on Phase 8, so it can ship in the first update after release or be
brought forward.

Two separate Game Input options: "WASD movement" (off by default) and
"Targeting (Q, Space)" (`config/gameplay/keyboard_targeting`, on by
default, since Q had no binding and Space only acts with a target picked;
off for players who pause combat with Space or bind Q). Each machine has
its own settings. Both work in single-player too. They change engine
input code (Exult's key bindings and combat targeting), so they are part of
our Exult build; a usecode mod pack cannot add them. Keep them in their own
commits, separate from the network code, so they could be offered to Exult
upstream.

- [x] **WASD movement** (built 2026-10-18, ahead of the rest of Phase 9).
      Game Input option "WASD movement", off by default
      (`config/gameplay/wasd_movement`). Walks like the arrow keys
      (`ActionWalk*`), slowing down in combat; two keys walk diagonally
      (W+D = north-east).
- [x] With WASD on, Shift plus W, A, S or D does what the key alone did:
      Shift+W the watch, Shift+S Save/Restore (W was a conflict too, not
      only S). Ctrl+S stays quicksave. (2026-10-22: Shift+S taken out.)
- [x] Keys walk at the second fastest speed; R toggles running
      (2026-10-23, was Caps Lock, which capitalised chat too; the status
      bar style moved to Shift+R. Shift was considered and dropped, it
      already means too much).
- [ ] Single-click attack: tried 2026-10-18 and taken out again (combat
      felt stuck, partly from WASD keys that stayed held). Replaced by the
      targeting below.
- [ ] Guests: send held movement keys to the host as a steady walk stream, and
      check it feels responsive over a real internet connection, not just a
      local one. (2026-10-19: the first internet test stuttered; steps are
      now numbered so the guest ignores the host's late echoes. Retest.)
- [x] **Targeting** (built 2026-10-18; with or without WASD movement, for
      the host and every guest, so guests don't have to double-click
      moving foes). Q picks the next person or creature on screen who
      isn't in the party, nearest first (Shift+Q the previous), with a
      small arrow over its head: white, or red in combat mode (outlines
      felt like status effects). Past the last one the pick clears and the
      next Q starts again. The notice near the top says "Targeting Deer",
      "Attacking Deer" or "No target". (Follow mode moved from E to Y
      for the object key, below.)
- [x] **When the pick is attacked.** Out of combat never. In combat mode
      Q goes through hostiles first, then everyone else; a hostile pick is
      attacked at once (the Avatar's target, or the host fighting for a
      guest's companion), a peaceful one waits for Space. Turning combat
      mode on with a hostile picked attacks it.
- [x] **Space attacks the target**, peaceful or not, turning combat mode
      on. While the Avatar is already attacking the pick, Space pauses
      combat as before. Guests attack peaceful people only after confirming
      ("Again to attack", 2026-10-20; it was not allowed before). Space
      was weighed against F (with food on Shift+F), double-clicking the
      target and an on-screen button, and kept: it only acts while the
      chat line is closed, and pausing combat does nothing in "original"
      combat.
- [x] **Fighting by target.** Once the Avatar fights through targeting,
      it attacks only what is picked: its attack mode is "manual" (Exult's
      combat AI then picks no foes of its own) until combat mode ends.
      Clearing the target, or walking, stops the attack; Space takes it up
      again. Saves keep the Avatar's own mode. A guest's companion already
      fights only the guest's pick (the host ends it when the guest walks
      or clears the target). Someone who never presses Q fights as U7
      always did; the party's combat AI still drives everyone else.
- Decided 2026-10-18: this may be enough; a deeper rework of the party's
  combat AI waits until play shows a need.
- [x] **Targeting for everything** (built 2026-10-19, after the first
      internet test; not tested yet). Targeting worked so well that it
      now does more than fighting:
  - **Space** out of combat talks to the pick or uses it, as a
    double-click does (a guest gets its usual answer: merchants talk,
    others say "The Avatar must do this"). In combat mode it attacks. So
    attacking a peaceful pick takes C, then Space; Space alone no longer
    starts a fight.
  - Out of combat **Q** also cycles the party (last), so the Avatar can
    talk to a companion. Not for guests.
  - **E** cycles containers and bodies within reach (chests, crates,
    barrels, bags on the ground, the fallen; not one in the next
    building), nearest first, Shift+E back, with a green arrow; Space
    opens it. Q and E
    each clear the other's pick. Guests' follow mode moved from E to **Y**
    (E was otherwise only the host's egg cheat).
  - **Range:** anything on screen, at most 40 tiles off each way (a
    square, as Exult measures distance, and how far around a guest's
    companion the host keeps creatures awake). Screens bigger than that
    showed the limit as a faint circle (taken out 2026-10-20: it cluttered
    the view; Q and E now say "No target in range" when nothing is). The circle is a
    picture of the range, not exact: it shrinks to stay wholly in view, so
    it shows why something at the far edge of a big screen can't be
    picked. (A square frame was tried first and read as a box, not a
    range.)
  - **Things, not just containers** (built 2026-10-19; not tested yet).
    E is context-sensitive instead of adding a third key or a type key:
    with no container window open it cycles containers, bodies and loose
    items (within 18 tiles with a path, as for picking up; 6 until
    2026-10-23) nearest first; with one open (a chest, a body,
    a pack, a paperdoll) it cycles what is inside, the newest window first.
    Space by the pick:
    - a container or body on the map opens; Shift+Space picks it up;
    - a loose item, or one in someone else's container, goes into the
      pack, whole stack, then the window's next item is picked (Space
      again and again empties a chest);
    - one's own item: used if it has a use; worn or wielded ones are
      readied or put away, swapping out what is in their spot (put back
      if that fails); anything else is dropped at one's feet after
      "Drop the gem? Space again". Shift+Space always drops one's own
      item at once (a stack: how many).
    - Books, scrolls (letters and notes) and the cloth map: Space reads
      them wherever they are; Shift+Space picks them up, or drops one's
      own.
    - Not okay to take: "Steal? Space again". Shift is the
      "more" key: on a stack it asks how many; on a single item in a
      window it takes everything there (asking once if any of it isn't
      yours).
    Changed 2026-10-22 ([feedback round](friend-mode/feedback-2026-10-22.md),
    C1): on the map, things with a use (books, the gavel, the bucket) are
    used with Space and taken with Shift+Space; in someone else's window
    Space takes (books too), Shift+Space takes everything and Alt+Space
    splits a stack; Alt+Space drops some of one's own. Alt always means
    "how many".
    It all moves items the way a drag and drop does
    (`Game_window::move_object`, drag.cc: weight, theft, eggs, guests'
    lift and move to the host), so guests need nothing new.
    Changed 2026-10-23 ([feedback round](friend-mode/feedback-2026-10-23.md),
    batch G): E in windows goes in reading order as items show, starting
    again in a newly opened window; Space opening a container picks its
    first item; Escape clears the pick before closing windows or opening
    the game menu.
  - **Target System options page and groups** (built 2026-10-19).
    Game menu → **Target System**, next to Game Input (the "Targeting"
    switch moved there): Enable (the rest hides when off), the range
    circle, Q offering the party, and what E offers by group: loot (on),
    decor (off), furniture (off), doors, shutters and lamps (off), usable
    things (on; levers, switches, the bucket, gavel, paint brush and easel,
    2026-10-23, was levers and switches), signs, plaques and graves (on;
    Space reads them).
    Q and E start again at the nearest after you move. Space takes loot and decor, and uses furniture, doors, lamps and
    levers (Shift picks furniture up). The groups are our own shape lists;
    see [targeting groups](friend-mode/targeting-groups.md), with the
    borderline items to review.
  - **What Space does** (2026-10-19). Under "Targeting Spark" a second
    line says what Space and Shift+Space would do with the pick: "Talk
    (Space)", "Open (Space)  Pick up (Shift+Space)", "Take (Space)  Split
    (Shift+Space)", "Use (Space)", "Read (Space)  Take (Shift+Space)",
    "Ready (Space)", "Attack (Space)", and "Steal (Space)" for anything
    taking would be a crime (`Friend_space_hint`, worked out by the same
    rules as Space).
  - **Tidy containers** (2026-10-19). Smart pickup placement: an item
    with no spot in a container window yet (taken with Space, bought,
    given by a script) goes to the first free place in a loose grid
    instead of piling up in the top left corner (`Gump::paint`, which also
    fixes Exult's own pile of purchases). **Shift+C**, or the **Sort**
    button under the checkmark, sorts a container in someone's inventory
    (a backpack, a bag in it; chests and bodies keep their layout) by
    kind: weapons, armour, ammunition, bags, potions, food, reagents,
    keys, money and gems, things to read, the rest. Rows run across the
    whole window, then down, round again when full; look-alikes (seven
    loaves of bread) lie on each other a couple of pixels apart, as a
    stack (`Gump::sort_objects`, `Sort_button` in Gump.cc). Both are options on the Target
    System page, on by default, shown even with targeting off.
  - **Backpacks and paperdolls by key** (2026-10-19). 1 to 8 open a party
    member's paperdoll as before, and pressing it again shuts it.
    Shift+1 to Shift+8 open or shut that member's backpack; Shift+I opens
    the next backpack not open yet, the Avatar's first, and with all open
    shuts them all. The host and single player are the game master: every
    member's, even while leading a companion (2026-10-23; also
    double-clicking a member or a bag in a member's pack). A guest only
    has its own companion's. While a sign,
    plaque, tombstone, scroll or book is open, Space closes it (or turns
    the page).
  - **Doors open by walking into them** (2026-10-19), standing or on the
    move, with WASD or the mouse; a guest's go through the host. Exult
    only opened a door when a walk already under way met it. The host keeps a
    guest's foe while it is in range of the companion, not on the host's
    screen (that was the "can't attack at large resolutions" bug), and
    guests get items and creatures 3 chunks around their companion (was
    2).
- [x] Attack the selected target with a key or left-click, through the
      existing attack code. Hit chance, damage, and timing stay unchanged.
      (Space attacks the pick in combat mode, as a double-click does.)
- [ ] Each player has their own target; the host relays it, and other players
      see a small marker for who is attacking what.
- [ ] Decide when party combat AI may switch away from a target a player
      picked by hand.
- [x] Toggles in Game Input / Game Engine options, saved in the config; a
      guest's toggles apply to that guest only. (WASD movement in Game
      Input; Targeting on its own Target System page; running on R.)

**Exit criterion:** With both toggles on, a guest walks with WASD, cycles
targets with Q and attacks the selected enemy, while the host plays with
the classic mouse controls.

### Phase 10 — Serpent Isle (after release)

Friend Mode is built and tested for The Black Gate only. Both players must
run the same game (the handshake turns away a mismatch), and most of the
co-op core doesn't care which game it is: joining, the picker and
spectating, movement and the leash, combat through the host, mirrored
conversations and Suggest a reply, chat, saving and autosave, Main Menu,
WASD, targeting and the minimap. What is tied to Black Gate today:

- [ ] **What guests may use:** doors, shutters, lamps, levers, secret walls,
      food, potions, keys and lockpicks are lists of Black Gate shape
      numbers; Serpent Isle numbers many differently. Add its lists, chosen
      by game.
- [ ] **Shops and trainers for guests** are switched off outside Black Gate
      (`GAME_BG` in `Friend_mode_guest_talk_to_merchant`): find Serpent
      Isle's merchants and trainers in its scripts, as was done for Black
      Gate ([shops and trainers](friend-mode/shops-and-trainers.md)).
- [ ] **Picker descriptions** of the companions, for Serpent Isle's.
- [ ] **The test kit** (F9 and F8, Ctrl+F9) with Serpent Isle shapes and companions.
- [ ] **Its story:** Serpent Isle separates and swaps companions far more
      often (Iolo, Shamino and Dupre are parted from the Avatar early). The
      "losing a companion" handling covers the mechanics; test it through
      those scenes, and the scripted events checklist in Serpent Isle.
- [ ] Silver Seed, once Serpent Isle works.

**Exit criterion:** Host and two guests play Serpent Isle's opening on
Monitor, through the first separation of companions, with shops and the
guests' world use working.

### Co-op quality of life

Modern conveniences that fit U7's "the Avatar leads" style, decided
2026-10-17. Not tied to a phase; most are small and can land before or
after Phase 8.

- [x] **Rejoin your companion.** When a guest drops, the host holds its
      companion for a few minutes (the party's AI drives it, as in follow
      mode). The same player name reconnecting gets it straight back,
      without the picker.
- [x] **Player list.** A simple overlay: Player | Character | Health. Hold
      Tab; the companion picker moved to J.
- [ ] ~~**Arrows to other players.**~~ Built, then removed after testing
      (2026-10-18): messy on screen, and the corner minimap already shows
      where everyone is. Only the arrow back to the Avatar stays.
- [x] **Suggest a reply** (built 2026-10-18). During the Avatar's
      conversation, a guest clicks a keyword: it is drawn in that player's
      color on every screen and the log says "Marc suggests: Name". One
      suggestion per player; cleared when the Avatar picks. The Avatar
      still chooses.
- [x] **Spectator camera.** Spectators press a key to cycle the view
      between players.
- [x] **Autosave every 5 minutes** on the host, in their own rotating slots
      (for example the last 3), never over a player's own saves, with the
      oldest deleted so they don't pile up. Exult has no autosave of its own
      to build on.
- [x] **Chat log.** A small log of recent lines that fades and can be
      dismissed, for spectators (who have no body) and everyone else.
      Players with a companion show overhead and in the log. Keep it to
      player chat and a few session notices, so it never fills up like an
      old MMO chat window.
- [x] **Whispers and emotes**, Ultima Online style: `/w name text`, and
      emotes such as `/wave` or `/bow` that animate the companion.
- [ ] **Shared objective (to think about).** A short line the Avatar sets
      and keeps updated ("Find Christopher's murderer") as a reminder for
      the companions. No quest log or guide arrows.

- [ ] **World map with player dots (shelved).** U toggled a whole-world
      overview (Exult's cheat map, drawn from each chunk's ground colours,
      one pixel per chunk, made in memory) with a dot per player. Built,
      then shelved after testing (2026-10-18): with the current leash
      everyone is near the Avatar, so the corner minimap is enough. The
      code is kept, unbuilt, in
      [shelved/world-map.cc](friend-mode/shelved/world-map.cc), with how to
      bring it back.
- [x] **Corner minimap (option, off by default).** A small map of the
      ground around your character with player dots, overworld only.
      Walls and roofs are objects, so towns show as floors without walls.
      In dungeons (`Game_window::is_in_dungeon`) it hides at first; a later
      version could show only the dungeon chunks the party has visited,
      kept in the save and shared by every player.

- [x] **Return to Main Menu** (built 2026-10-18). A "Main Menu" button
      above Quit in the game menu. Exult can't tear a game down and run the
      title menu again in one process, so it closes the co-op session and
      starts Exult afresh with the same arguments, minus the co-op ones,
      skipping the intro (`--skip-intro`).
- [ ] **Keyboard command list in game** (asked for 2026-10-18, once the
      in-game features are done): every key, including Friend Mode's (J,
      L, X, Y, Q, E, Space, Shift+I, Shift+C, 1-8 and Shift+1-8, Tab, WASD,
      Shift+W/S, chat commands).
- [ ] **Feature list for the project webpage** (same time): every
      feature and enhancement Friend Mode adds, written for players.

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

### Controller support (done)

**Built, tested and merged 2026-10-26** ([controller
plan](friend-mode/controller-plan.md)). Black Gate plays from an Xbox,
PlayStation or Steam Controller pad with the keyboard and mouse still
working alongside: targeting on the shoulders (L1 people, R1 things, Cross
acts, L2 / R2 as Shift and Alt), a pad pointer on the right stick, the
skills and utilities rings, the spellbook and its slots, the item menu
(Give to...), every menu and option page, conversations, the Join screen,
and windows moved between and arranged without the pointer. The button
names follow the pad (Cross or A). It works through the Target System, so
that must be on. Chat on a pad is not planned (a chatpad attachment works;
most players use voice chat). The game menu has a Controller page
(settings and the buttons list), and Game Input a Keys... page (the keyboard's Friend
Mode keys).

### Exult in the browser

Play from a web page with nothing to install, joining by lobby code. Exult has
no web build today, but it is C++ on SDL3, which can target the browser
through Emscripten.

- [x] Spike: build unmodified single-player Exult with Emscripten.
- [x] Exult waits in its own loops (clicks, conversations, delays); a browser
      page must keep handing control back. Try Emscripten's Asyncify before
      rewriting any loops, and measure speed and download size.

**Spike results (2026-10-29).** In the `ultima7-friend-mode-web` worktree
(local branch `web-spike`; `web/build.sh` then `web/serve.sh`), with
Homebrew's Emscripten 6.0.13 and its SDL3, Ogg/Vorbis and zlib ports. Exult
compiled for the browser with no source changes except one line, and runs:
the intro, the title menu, character creation, and the opening in Trinsic
with its conversations and the world, the paperdoll, the minimap and the
skill bar.

- Every wait in Exult goes through `Delay()` and `SDL_Delay`, which with
  Asyncify hands control back to the browser, so no loop needed rewriting.
- The one change: the browser blends the WebGL canvas with the page by its
  alpha, and the scalers leave the game image's alpha at 0, so the world
  showed as black (the layers, at alpha 255, showed). Asking for a canvas
  with no alpha before the window is made fixes it
  (`SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0)`, in `imagewin.cc`).
- Build: `--with-friend-rtc=no` (no WebRTC yet), `-fexceptions` (Exult needs
  C++ exceptions), Asyncify, 8 MB stack, growable memory. The data folder's
  tools fail under Emscripten, so the data files and generated headers come
  from the native checkout.
- Size: the game is 10.6 MB of WebAssembly (7.3 MB without Asyncify),
  2.7 MB gzipped. Black Gate's `static` folder is another 39 MB (21 MB
  gzipped), which players supply themselves.
- Speed: not measured yet. The opening scene played smoothly in the
  desktop app's browser pane (Chromium) on an Apple Silicon Mac; walking
  around and combat weren't tried.
- Not tried: saving (the spike keeps saves in memory, lost on reload),
  music (MIDI is off; OGG not installed here), Serpent Isle, Friend Mode.
- Seen in the log, not looked into: "Und sched. opcode 0x00" five times at
  the start of the opening.
- [x] Moved into this repo (2026-10-30): `web/` (`build.sh`, `serve.sh`,
      `shell.html`, `pre.js`, `exult-web.cfg`). The build works in a copy
      of the sources, `web/build`, since the native build lives in the
      source folder; `web/build`, `web/pkg` and `web/out` are git-ignored.
- [x] Game data and saves in browser storage (see Licensing and game data):
      the page asks for the player's Ultima VII folder, finds the Black Gate
      STATIC folder in it, and copies it into IndexedDB (IDBFS at `/user`),
      then Start runs Exult. Saves and gamedat live there too, written back
      every 5 seconds and when the page is left; a game in progress survived
      a reload. Not tried yet: the real folder dialog (the test fed files in
      by script), the in-game save menu, browsers other than Chromium.
- [ ] What's left (saves, audio, keyboard, controller, co-op, hosting) is
      tracked in [playing in a web browser](friend-mode/web-browser.md).
- [ ] Digital (OGG) music first; MIDI synthesizers later, if at all.
- [ ] Friend Mode on the browser's own WebRTC, through the Phase 8 connection
      code.
- [ ] Hosting: browsers slow down background tabs, so a host in a hidden tab
      slows everyone. Warn the host, or keep hosting to native builds.

### Companion skills (done)

**Built, tested and tuned** (the 2026-10-22 and 10-23 rounds and since):
the Class System as described below, the skill and spell bars, the Class
AI, and leading a companion. What follows is the original idea.

Two or three skills per companion, from the class they had in Ultima IV,
so a guest playing one as their "main" has more to do in a fight than let
it swing: Iolo's songs (Battle Hymn, Song of Warding, Song of Peace),
Shamino's Volley at the three nearest hostiles, Jaana's Mass Cure and
Restoration, Dupre's Challenge, Spark's sling tricks. A small skill bar for
guests, about one skill per fight (cooldowns), and each companion's skills
unlocked by an item of their own found in their home (Iolo's lute).
Companions nobody plays use their skills on their own through a "Class AI"
(healers watch health, Shamino keeps his distance), or keep U7's AI. The
Avatar gets a spell bar instead: chosen spells cast in one press, aimed
with the Target System's pick instead of the crosshair. Host-side engine
code on stock Black Gate, built from U7's own flags, heals, missiles and
effects; an option the host sets, off by default. Plan:
[companion skills](friend-mode/companion-skills.md). Moved forward on
2026-10-20, ahead of Phase 8, since it needs balancing and changes today's
combat: the foundation (Game menu → Class System, the bars, definitions,
cooldowns, effects, the Class AI's choices) is built and not yet tested;
Class AI placing and Challenge followed the same day (untested). Next for
it: the host and a single player leading a companion, its skill bar and
the party following it, with Y to hold the party
([plan](friend-mode/leading-companions.md), 2026-10-21).

### Co-op scaling

Enemies a little harder per guest playing a companion: mostly more
effective health (the host shrinks each hit on them), a little more damage
to the party, host-only settings, on top of Exult's own combat difficulty.
Only if more testing shows combat got too easy with skills and the better
AI (2026-10-25). Plan: [co-op scaling](friend-mode/coop-scaling-plan.md).

### Offering features to Exult

Several additions don't need co-op (the Target System, moving items
without the mouse, doors opening by walking, WASD, backpack keys, health
bars). What could go upstream and what to untangle first:
[upstreaming notes](friend-mode/upstreaming.md).

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

## No mods

Decided 2026-11-01: Quest of the Companions plays Black Gate (with Forge of
Virtue) as it shipped, with no mods. Mods such as Marzo Junior's Keyring
change the game's scripts, map and items, which co-op, skills, targeting,
merchants and the opening's scenes all rely on, so any of them could break
these systems in ways that look like our bugs. Allowing mods in single
player only would leave a mod's saves unable to go co-op later; allowing
them in co-op would mean checking mods on join and testing each one. Not
worth it: every save, single player or co-op, is plain Black Gate.

- The game never looks for mods (`ModManager::gather_mods`), so Exult's
  game menu lists only the games, and `--mod` and `--installmod` are
  refused with a reason (`exult.cc`). Mods already installed stay on disk,
  untouched.
- Players who want mods, or Exult as it is, download Exult itself; the
  two can sit side by side.
- What players liked in mods comes in as our own features, built and
  tested for co-op. Using keys without hunting for them was the main
  wish, and Exult already has it: **K** (Try Keys), then click a locked
  door or chest, uses any matching key in the party, guests' own keys
  too.
- [ ] The website says so: mods are off, so every co-op player runs the
      same game; for mods, use Exult.
- [ ] Before a public release: our own settings and saves folder (for
      example `Library/Application Support/Quest of the Companions` on a
      Mac). Today it shares Exult's (`Library/Application Support/Exult`),
      so the two apps share a settings file and saves.

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

The early questions (how Exult ties input and the camera to the Avatar,
suppressing companion AI, which scripts companions can trigger, what to
replicate, saves with players in them) were answered while building Phases
1 to 7; the answers are in the [Friend Mode README](friend-mode/README.md),
[control points](friend-mode/control-points.md) and the
[interaction policy](friend-mode/interaction-policy.md). Still open:

1. How to detect incompatible Exult builds, mods and game data beyond
   today's handshake (protocol version and game).
2. Whether the host can simulate distant regions for independent
   exploration ([research](friend-mode/independent-views-research.md)).

## Next steps

(Updated 2026-10-26.) Co-op, lobby codes, the Class System, the Target
System and controller support are built and tested; Mac and Windows play
together.

1. **Release builds and packaging:** Mac and Windows builds for release,
   installation instructions, a compatibility note (Black Gate only, same
   build on every machine), and GPL-compliant distribution (source, the
   licence, no game data).
2. **A test with more players:** three or four (host and two or three
   guests), the two-people-grab-one-item check, and long internet sessions
   (Phase 6's exit criterion).
3. **The webpage:** the feature list and shortcuts in `ultima7-co-op-site`
   brought up to date (the controller, the Keys page, the item menu), the
   roadmap there (controller support is done), and a line that mods are
   off so every co-op player runs the same game (see "No mods").
4. **Fixes from play,** as they come (the latest are in
   [feedback 2026-10-25](friend-mode/feedback-2026-10-25.md), all fixed).
5. **Later, optional:** co-op scaling (only if combat proves too easy), the
   ring art for the controller, splitting `exult.cc` and the clean
   branches before a public fork ([branching](friend-mode/branching.md)),
   offering features to Exult ([upstreaming](friend-mode/upstreaming.md)),
   Serpent Isle (Phase 10), and the future ideas above.

---

**Project motto:** *One Avatar. One Britannia. Bring a friend.*
