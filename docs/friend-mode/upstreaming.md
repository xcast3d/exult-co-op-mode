# Upstreaming notes

Friend Mode is a fork of [Exult](https://github.com/exult/exult) (the
`upstream` remote). Several of its additions have nothing to do with
co-op and could be offered to Exult on their own. This page says what they
are, where the code is, and what to untangle first.

Everything here is GPL, like Exult. Upstream Exult formats with its
`.clang-format` and wants one feature per pull request.

## Candidates

| Feature | What it does | Where | Co-op ties |
| --- | --- | --- | --- |
| **Target System** (Q, E, Space) | Keyboard targeting: Q cycles people and creatures, E things (containers, bodies, signs, loot, and what is in open windows), Space talks, attacks, opens, uses, reads, takes, readies or drops. Range circle on big screens. Options page. | exult.cc, "The Target System" section (`Friend_mode_cycle_target`, `Friend_mode_cycle_object`, `Friend_mode_space`, `Friend_thing_group_of`, the paint functions); `TargetingOptions_gump` in gumps/InputOptions_gump.*; `Game_window::set_action_target` | `Friend_mode_is_guest()` branches send a guest's attack or use to the host; drop them. Notices use Friend Mode's pinned notice line (`Friend_mode_target_notice`). |
| **Moving items without the mouse** | `Game_window::move_object`: a drag and drop for a given object (take into a pack, ready, put away, drop at the feet, part of a stack), with the drag's weight, theft and egg rules. | drag.cc (`Dragging_info(obj, gump)`, `lift`, `place`), gamewin.h | The drag hooks `Friend_mode_note_drag_*` are already in Exult's drag code here; upstream would not have them. |
| **Act on an object without the mouse** | `Game_window::double_clicked_object`: double-click logic split from the mouse lookup. | gamewin.cc | Guest branches inside are the existing Friend Mode ones. |
| **Doors open by walking into them** | From standing as well as mid-walk (Exult only opened them when a walk under way met one). | gamewin.cc, `Open_door_ahead` in `start_actor_alt` | A guest's door goes through the host (`Friend_mode_request_object_use`). |
| **WASD movement** | W, A, S, D walk (diagonals with two), read by keyboard position each frame; Shift+key does the key's old binding. Game Input option. | exult.cc (`Friend_wasd_movement`, the main loop's WASD block, `Handle_event`), gumps/InputOptions_gump.* | None. |
| **Backpack and paperdoll keys** | 1-8 close an open paperdoll; Shift+1-8 open or close a member's backpack; Shift+I steps through the party's backpacks. | keyactions.cc (`ActionInventory`), exult.cc (`Friend_mode_backpack_key`) | A guest only has its companion's. |
| **Space closes signs and books** | Space as a click while a sign, plaque, scroll or book waits. | exult.cc, `Get_click` (`get_click_space_continues`) | None. |
| **Enemy health bars** | Optional small bars over hurt foes (Game Engine option). | gamerend.cc (`paint_health_bars`), combat.cc (`Combat::health_bars`) | Guests get health from the host. |
| **Font 0 asterisk** | Draws the '*' Black Gate's overhead font lacks. | shapes/font.cc (`Font::add_missing_asterisk`) | None. |
| **Corner minimap** | A small ground map with dots, in a HUD-scaled layer. | friendmap.cc | Player dots come from the co-op player list. |
| **Return to Main Menu** | Restarts Exult at the title menu from the game menu. | exult.cc (`Friend_mode_restart_at_menu`), gumps/Gamemenu_gump.* | Strips the co-op arguments. |

Probably not: everything about the network, guests, the leash, the shared
journal, merchant conversations for guests (they rely on the host), and
the item groups' shape lists as they stand (Black Gate only; Serpent Isle
needs its own).

## Before offering one

1. **Names.** Keyboard features carry the `Friend_` / `Friend_mode_`
   prefix because they were written here. Upstream would want plain names
   (`Targeting_...`, a `Targeting` class) and config keys without "friend".
2. **Own files.** The Target System lives in exult.cc next to the network
   code. Move it to its own file (say `targeting.cc`) with a small header,
   keeping the guest branches behind one hook.
3. **Notices.** It shows "Targeting Deer" on Friend Mode's pinned notice
   line; upstream would use Exult's own text effects or a small overlay.
4. **Strings.** Our labels are plain English strings; upstream's options
   pages use translated messages (`exultmsg.txt`).
5. **Docs.** Exult's docs/ReadMe.html lists keys and options; add the new
   ones there.
6. **Tests.** Single player only: the
   [test checklist](test-checklist.md) sections for targeting, minus the
   guest lines.

## Keeping in step with upstream

Merge `upstream/master` into `feature/friend-mode` now and then; the
keyboard features touch Exult's input, drag and key-action code, which is
where conflicts will show first.
