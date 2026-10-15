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

This checkpoint does not yet synchronize world state or replicate movement.
The temporary local guest profile and copied runtime data live under
`.friend-mode-local/`, which is ignored by Git.

The next assignment slice now transfers the host's eligible companion roster,
shows the guest a selector using the companions' existing portraits and names,
and has the host validate the requested assignment. The guest then takes local
input and camera control of the accepted companion. This has been verified with
Iolo over two local instances. Moving this selector from the loaded world into
the pre-game connection flow is a later UI refinement; movement and world state
are still not synchronized at this checkpoint.

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
