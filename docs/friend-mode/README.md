# Friend Mode development

Friend Mode is an experimental cooperative multiplayer extension for Exult. The
project vision and roadmap live in [the original project brief](../ultima-7-friend-mode.md).

Development starts from a verified, unmodified Exult build. Keep the story
Avatar, the human-controlled actor, and the network host as separate concepts;
the first proof of concept changes only the local controller and camera target.

## Current milestone

Phase 1: harden the working local party-member possession prototype.

- [x] Import the official Exult source and history.
- [x] Configure GitHub as `upstream` and Bitbucket as `origin`.
- [x] Configure and compile an unmodified Exult binary on macOS.
- [x] Launch with legally obtained Ultima VII data and record a baseline save.
- [x] Trace the initial input, camera, and party-AI control points.
- [x] Implement and manually verify local Avatar/Iolo possession.
- [x] Restore Avatar control automatically across save/load.
- [x] Restore Avatar control automatically across unsafe scripted transitions.
- [x] Verify dialogue interactions return to and use the canonical Avatar.
- [x] Allow companion-owned paperdolls, bags, ordinary containers, and item
  dragging while preserving companion control.
- [x] Restrict companion controllers to their own inventory while retaining
  Avatar access to all party inventories.

See [macOS setup](macos-setup.md) and [control-point notes](control-points.md).
The guest-action boundary is defined in the [interaction authority
policy](interaction-policy.md).

## Phase 2 checkpoint

The first loopback networking slice is in place. A host can listen on a local
TCP port and a guest can use a dedicated **Connect to Host** title-menu entry.
The peers exchange a versioned handshake and reject incompatible game variants.
This has been verified with two isolated Exult instances on one Mac.

The temporary local guest profile and copied runtime data live under
`.friend-mode-local/`, which is ignored by Git.

The next assignment slice now transfers the host's eligible companion roster,
shows the guest a **Select Your Character** carousel using the companions'
existing framed portraits, names, short descriptions, and Exult arrow artwork,
and has the host validate the requested assignment. The guest then takes local
input and camera control of the accepted companion. This has been verified with
Iolo over two local instances. Moving connection-address entry and this selector
into one uninterrupted pre-game flow remains a later UI refinement.

The current synchronization prototype has also been verified for Avatar and
companion position/facing, nearby NPC and creature position/facing, companion
movement across building thresholds, client-specific roofs, and disconnect
notification. The guest camera and movement cursor remain anchored to the
assigned companion. Guest-side NPC simulation is suspended so it does not fight
host snapshots.

Inspection labels from single clicks remain local to the client that generated
them. Network guests can use their own inventory and ordinary containers, but
NPC/sign activations that could start Avatar-owned story Usecode are currently
blocked without revoking companion control.

Host-generated NPC remarks and animal barks are now replicated to the guest.
Host-started conversations also place the guest in a temporary spectator
state: movement and interaction are paused, portraits, text, and response
choices are mirrored, and only the host can choose a response. The guest
resumes control of the same companion when the conversation ends. All four
behaviors have been verified together over two local instances.

## Phase 3 checkpoint: player identity and overhead chat

**Connect to Host** now asks for a player name (up to 20 characters) and a host
address before loading. The host uses the Avatar's name. Both peers exchange
names after the handshake, and clicking a player-controlled actor or its party
portrait shows `Character (Player)`.

While connected, press **Enter** to open the `Say:` line, **Enter** again to
send, or **Escape** to cancel. Messages are limited to 80 printable ASCII
characters and are filtered again on receipt. Text appears centred above the
speaker in the shadowed font, wraps by pixel width (long words are split), and
stays up longer for longer messages. Up to three recent messages per speaker
stack upward; older ones are dropped. Players without an actor (spectators)
appear near the top of the view as `Name: text`.

Chat packets carry the sender's NPC number. The host ignores the guest's claim
and uses its own assignment; guests trust the host. Chat never runs Usecode.
The protocol version is now 5. Relaying chat between multiple guests waits on
multi-peer sessions.

### Joining with the host's world

After **Connect to Host**, the guest connects from the title menu and asks for
the host's world. The host writes its live game to a temporary save (the
normal save code, about 190 KB) and sends it as one variable-length message:
a header packet with the length, then raw bytes. The guest shows download
progress, unpacks the save into its gamedat, and loads it, so party, quest
flags, items, doors and NPCs match the host at the moment of joining. Protocol
version is now 7.

### Doors, shutters and lamps

The host shares every change a game script makes to a world object's shape or
frame, hooked in `Usecode_internal::set_item_frame` and `set_item_shape`. That
covers doors opened by the Avatar, NPCs or scripts, shutters, lamps, levers and
so on. Decorative animation (torches, fountains) uses `change_frame` directly
and is not sent. Ultima VII opens doors by swapping shape (closed 270/376 to
open 432/433), so an object is identified by tile plus old shape and frame,
falling back to any door at that tile.

Guests may use doors and a short list of world toggles (Black Gate only for
now): shutters 290/291/322/372, light sources 336/338, sconces 435/481,
curtains 657/678, levers 787, switches 788 and lamp posts (889 unlit, which swaps to 526 lit lamp when turned on). The guest's double-click asks the host,
which checks the shape and reach (the normal double-click reach rule) and then
uses it: doors the way NPCs do (quality zeroed so only the door's own toggle
runs), the others with a normal activation. Locked doors show "Locked". Levers
and switches are allowed so guests can help with puzzles; their scripts can
open gates or trigger effects elsewhere, which is accepted. Protocol version is 9.

### Party mirroring and spectating

The host sends its party list when a guest joins and whenever the party
changes, and the guest updates its local party to match before taking control.
This lets a guest control a companion recruited after the guest's copy of the
world was made (for example Spark). A guest without a companion is a
spectator: movement and double-clicks are blocked so it cannot move its local
copy of the Avatar, and the camera follows the host's Avatar. Protocol version
is now 6.

### Leash

Guest companions stay near the Avatar, which keeps them inside the area the
host simulates (see the [independent-views research](independent-views-research.md)).
Distance is measured horizontally from the Avatar:

- Beyond one screen (20 tiles): "Straying from the Avatar", and a green arrow
  at the screen edge points back to the Avatar.
- At 32 tiles the guest cannot step further ("You must return to the Avatar").
  The guest checks before stepping, and the host refuses such moves and resends
  the true position. Steps back toward the Avatar are always allowed.
- If the Avatar moves away and leaves the companion beyond 32 tiles, the guest
  sees "The Avatar needs you"; after 5 seconds the host moves the companion
  beside the Avatar ("You are beckoned by the Avatar"). A change of map moves
  it at once.

Raising the radius later is the path toward free roaming.

### Session notices

The host shows notices near the top of the view when a guest joins, enters the
world as a companion, and leaves (the companion returns to party AI). The guest
sees a notice when it joins the host's world. Connections that are not Friend
Mode clients, such as local port probes, are logged and closed.

## Local possession prototype

Launch the modified build with `./exult --bg --nocrc`, load a save with an
active companion, and press `Control-F`. Local movement and camera focus switch
to the first eligible party companion. Press `Control-F` again to restore
Avatar control and the companion's prior AI schedule.

Saving or loading while controlling a companion automatically hands control
back to the Avatar first, preventing the temporary suspended-AI schedule from
being written into a save.

Scripted control, incapacitation, party removal, and story-bearing Usecode
interactions revoke companion control. Dialogue therefore remains anchored to
the canonical Avatar.

Safe local interactions are now separated from that boundary. A controlled
companion can open only its own paperdoll and nested bags, open ordinary
non-scripted containers, and move items using its own reach. The Avatar remains
the party administrator and can inspect every party member. Pressing `I` while
controlling a companion opens that companion directly; Avatar control retains
normal inventory cycling.

The `--nocrc` flag is required because the development key binding changes the
generated Exult resource archive. It does not disable validation of the
original Ultima VII game data.

## Dialogue authority

Network dialogue is host-authoritative. When the Avatar begins a conversation,
connected guests temporarily enter a spectator state: movement and world
interaction pause, the host's conversation text and portraits are mirrored,
and only the host may choose responses. Guest control resumes on the same
companion when dialogue ends.

Inspection labels, including single-click object names, remain local to each
client and are not replicated. Spoken world text—including scheduled NPC
remarks, animal barks, and scripted item speech—is host-authoritative and is
replicated to guests.

## Known prototype gaps

- Guests start from the host's world, and door changes are shared, but item
  and container changes after joining are not synced yet; see the
  [world sync plan](world-sync-plan.md).
- Creature identity currently relies on deterministic loaded-world ordering;
  spawned/despawned creature replication needs a durable network identity.
- The companion selector still appears after loading rather than in the
  pre-game connection flow.
- Exult has no in-game return to the title menu, so a guest whose host
  disconnects sees a notice and Exult closes after a few seconds.
- Sessions have a single peer, so chat is not yet relayed among several
  guests.
