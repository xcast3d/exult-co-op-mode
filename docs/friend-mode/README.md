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
- [ ] Restore Avatar control automatically across save/load and unsafe scripted transitions.
- [ ] Verify dialogue and quest interactions continue to use the canonical Avatar.

See [macOS setup](macos-setup.md) and [control-point notes](control-points.md).

## Local possession prototype

Launch the modified build with `./exult --bg --nocrc`, load a save with an
active companion, and press `Control-F`. Local movement and camera focus switch
to the first eligible party companion. Press `Control-F` again to restore
Avatar control and the companion's prior AI schedule.

The `--nocrc` flag is required because the development key binding changes the
generated Exult resource archive. It does not disable validation of the
original Ultima VII game data.
