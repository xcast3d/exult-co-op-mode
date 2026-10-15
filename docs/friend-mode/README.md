# Friend Mode development

Friend Mode is an experimental cooperative multiplayer extension for Exult. The
project vision and roadmap live in [the original project brief](../ultima-7-friend-mode.md).

Development starts from a verified, unmodified Exult build. Keep the story
Avatar, the human-controlled actor, and the network host as separate concepts;
the first proof of concept changes only the local controller and camera target.

## Current milestone

Phase 0: establish a reproducible macOS build and identify the smallest engine
seams needed for local party-member possession.

- [x] Import the official Exult source and history.
- [x] Configure GitHub as `upstream` and Bitbucket as `origin`.
- [x] Configure and compile an unmodified Exult binary on macOS.
- [ ] Launch with legally obtained Ultima VII data and record a baseline save.
- [ ] Trace and document input, camera, party AI, and Usecode boundaries.
- [ ] Implement the local Avatar/Iolo possession experiment.

See [macOS setup](macos-setup.md) and [control-point notes](control-points.md).
