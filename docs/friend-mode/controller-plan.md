# Controller support (plan)

**Status:** a plan, not started (2026-10-19). Listed under "Possible future
projects" in the [project brief](../ultima-7-friend-mode.md).

Goal: play Black Gate, alone or in Co-Op Mode, with a gamepad and no
keyboard or mouse. The [Target System](targeting-groups.md) (Q, E, Space)
already turns most mouse work into a few keys; a controller mostly needs
those keys on buttons, plus a way through conversations, menus and chat.

## What Exult has today

- **Left stick walks.** Exult opens any controller SDL knows
  (`Open_game_controller`, exult.cc) and walks with the left stick at three
  speeds (`SDL_EVENT_GAMEPAD_AXIS_MOTION` in `Handle_event`).
- **No buttons.** No `SDL_EVENT_GAMEPAD_BUTTON_*` is handled anywhere.
- The "D-Pad screen location" option (Game Input) is the iOS and Android
  on-screen touch pad (`touchui`), not a physical controller.

## Button map (first version)

Names as on an Xbox pad (SDL's south/east/west/north on others).

| Button | Does | Same as |
| --- | --- | --- |
| Left stick | walk | the mouse / WASD |
| A (south) | act on the pick: talk, open, take, use, attack | Space |
| B (east) | back: close the top window, cancel a prompt, clear the pick | Escape |
| X (west) | next thing | E |
| Y (north) | next person or creature | Q |
| LB (left shoulder) | "more": with A, how many / take all / pick up | Shift |
| RB (right shoulder) | combat mode on or off | C |
| LT / RT | previous / next party member's paperdoll | 1-8 |
| D-pad up / down | previous / next (in the current list) | Shift+Q, Shift+E |
| D-pad left / right | previous / next backpack | Shift+I |
| Start | game menu | Escape (no window open) |
| Back / Select | journal; held: player list | N, Tab |
| Right stick click | chat quick phrases (below) | Enter |

Guests get the same map; on a guest Y (north) held for a second is
"follow the Avatar" (Y on the keyboard).

The map lives in the config (`config/gameplay/controller/...`, button name
to action name, the same action names as the key bindings in
`keyactions.cc`), with the table above as the default, so it can be
changed without a rebuild.

## Implementation

1. **Buttons to actions.** In `Handle_event`, turn
   `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP` into the existing actions instead of
   fake key presses: the Target System calls (`Friend_mode_cycle_target`,
   `Friend_mode_cycle_object`, `Friend_mode_space`), and Exult's key
   actions (`ActionCombat`, `ActionInventory`, `ActionCloseOrMenu`...) by
   name through the keybinder's action table. LB is held state passed in
   where Shift is read today (`Friend_mode_space(shift)`, the cycles'
   step).
2. **Modal loops.** Conversations, signs, books and number prompts wait
   in their own loops (`Get_click`, `Gump_manager::do_modal_gump`), outside
   `Handle_event`. Each needs A as a click (Space already closes signs and
   turns book pages there) and B as Escape.
3. **Conversation choices (the main gap).** Choices are picked with the
   mouse or 1-9/a-z (`Usecode_internal::get_user_choice_num`). Add a
   highlighted choice:
   - when choices appear, the first is highlighted;
   - D-pad up/down (and X/Y, the cycle buttons) move the highlight, wrapping
     round;
   - A picks it; B picks "bye" if offered (as Escape does today).
   The highlight is drawn the way Suggest a reply draws a guest's
   suggestion (`Conversation::suggest_choice`, a choice in a palette
   color), in a color of its own. The same keys work on the keyboard:
   Q/E move it and Space picks, so conversations need no mouse either.
   Keyboard part built 2026-10-22 (`Conversation::move_highlight`, white;
   E and Shift+E, Space; nothing highlighted until the first E). A
   controller only has to send those keys.
   Guests watching the Avatar's conversation use the same highlight to
   pick their suggestion: A sends it (the "Suggest a reply" message).
4. **NPC text that waits for a click** ("click to continue") takes A as a
   click; it already takes any key.
5. **Menus and option pages.** The game menu and the options pages are
   lists of buttons (`Gamemenu_gump`, `*Options_gump`). Add a focused
   button: D-pad up/down moves it (drawn with a highlight box), left/right
   changes a Yes/No or list setting, A presses it, B cancels. Start in
   the title menu works the same (it is already keyboard-driven).
6. **Number prompts** ("How many?", the quantity slider): left/right
   change by one, LB+left/right by ten, A accepts, B cancels.
7. **Chat.** No keyboard: the right stick click opens a short list of
   quick phrases ("Follow me", "Wait here", "Over here", "Ready", "Thanks"),
   picked like conversation choices; plus the emotes (`/wave`, `/bow`...).
   Typing stays on the keyboard.
8. **Items between packs and equipment** already work without dragging:
   A takes into the pack, readies, puts away or drops
   (`Game_window::move_object`). Still missing: giving an item to another
   party member (the Avatar only). A "give to" list on LB+A in an open
   pack window could follow.
9. **Options:** a "Controller" line on the Target System page (on when a
   controller is connected) and the remapping in the config.

## Order

1. Buttons to the Target System and key actions, A/B in signs and prompts.
2. Conversation choice highlight (keyboard and controller at once).
3. Menus and option pages.
4. Chat quick phrases.
5. Remapping in the config; then a "Controller" options page if needed.

Steps 1 and 2 already make the game playable with a pad, except menus and
chat.

## Testing

A controller on the Mac (any SDL-known pad: Xbox, PlayStation, Switch
Pro). Black Gate's opening in Trinsic without touching the keyboard or
mouse: walk, talk to Finnigan through every choice, open the stables'
chests, take and ready a weapon, fight a rat, use the game menu to save.
Then a guest doing the same with the host on the keyboard.

## For upstream Exult

Nothing here depends on Co-Op Mode. Steps 1 to 6 would suit Exult itself
(see [upstreaming notes](upstreaming.md)); the Co-Op parts (guests'
suggestions, chat phrases) stay ours.

## Skills and spells (Class System)

Decided 2026-10-20: the Class System's skill bar (a guest's companion) and
the Avatar's spell bar are used with the mouse; a controller gets its own
way in, such as a radial menu on a held button listing the skills or
spells, rather than keyboard keys mapped to buttons. See
[companion skills](companion-skills.md).
