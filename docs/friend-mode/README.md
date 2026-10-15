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
- [ ] Verify dialogue and quest interactions continue to use the canonical Avatar.

See [macOS setup](macos-setup.md) and [control-point notes](control-points.md).
The guest-action boundary is defined in the [interaction authority
policy](interaction-policy.md).

## Local possession prototype

Launch the modified build with `./exult --bg --nocrc`, load a save with an
active companion, and press `Control-F`. Local movement and camera focus switch
to the first eligible party companion. Press `Control-F` again to restore
Avatar control and the companion's prior AI schedule.

Saving or loading while controlling a companion automatically hands control
back to the Avatar first, preventing the temporary suspended-AI schedule from
being written into a save.

Scripted control, incapacitation, party removal, and deliberate Usecode
interactions also revoke companion control. This keeps dialogue and scripted
movement anchored to the canonical Avatar while those boundaries are tested.

The current prototype is intentionally conservative: opening a paperdoll also
returns control to the Avatar because all deliberate activations share the same
boundary. The next slice will separate safe UI and inventory actions from
story-bearing Usecode.

The `--nocrc` flag is required because the development key binding changes the
generated Exult resource archive. It does not disable validation of the
original Ultima VII game data.
