# Initial control-point notes

This is the Phase 0 map for the local possession prototype. It records likely
seams, not a final design.

## Actor identity

- `Game_window::main_actor` is the canonical Avatar and is exposed by
  `get_main_actor()` in `gamewin.h`. Story and engine code use this identity in
  many places, so the possession prototype should not replace it.
- Party membership and ordering are managed by `Party_manager` in `party.cc`.
  `get_party(..., 1)` includes the Avatar alongside companions.

## Input target

- The main event loop in `exult.cc` already reads `get_camera_actor()` for
  movement direction, but action gating and several movement operations still
  call `get_main_actor()` or `main_actor_can_act*()`.
- `Game_window::start_actor*()` in `gamewin.cc` directly moves `main_actor`.
  This is the first seam to separate into a human-controlled actor without
  changing the story Avatar.
- Key dispatch gates in `keys.cc` are also Avatar-specific and will need an
  explicit policy: physical-action eligibility should follow the controlled
  actor, while story actions remain Avatar-only.

## Camera target

- `Game_window` already has a separate `camera_actor` plus
  `set_camera_actor()` and `get_camera_actor()` in `gamewin.h`/`gamewin.cc`.
- That existing separation should be reused for Phase 1. It also contains cache
  handoff behavior when returning the camera to the Avatar.

## Companion AI

- Party following is centered in `Party_manager::move_followers()` in
  `party.cc` and the `Schedule::follow_avatar` paths in `actors.cc`.
- A possessed companion needs a narrow, reversible suppression mechanism so
  normal follow behavior pauses without changing party identity or deleting the
  actor's schedule state.

## First implementation boundary

Introduce a controller-facing actor reference that defaults to `main_actor`.
Route movement and physical input through it, point `camera_actor` at it, and
temporarily suppress follow AI only for that actor. Keep dialogue, quest
identity, Usecode provenance, saves, and party membership tied to
`main_actor` until each boundary is tested explicitly.
