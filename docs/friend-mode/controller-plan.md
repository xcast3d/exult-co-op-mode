# Controller support (plan)

**Status:** a plan, not started (2026-10-19; current state and Steam
Controller notes added 2026-10-23). Listed under "Possible future
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

In more detail (checked 2026-10-23, SDL 3.4.18 from Homebrew):

- **Opening.** SDL starts with `SDL_INIT_GAMEPAD`. Pads already connected
  are opened at startup (`SDL_GetJoysticks`, `SDL_IsGamepad`), and an
  `SDL_AddEventWatch` listener (`Handle_device_connection_event`) opens and
  closes them as they come and go, whichever event loop is running. The
  open pads are never stored; code finds them with `SDL_GetGamepadFromID`.
- **Walking.** Only `SDL_GAMEPAD_AXIS_LEFTX/LEFTY` are read. Dead zone 0.25
  per axis; under 0.60 of full push is slow, under 0.90 medium, above that
  fast (`Mouse::*_speed_factor`). The stick sets `joy_aim_x/y`, a point
  off the screen centre, and the frame loop keeps calling `start_actor`
  towards it while it is non-zero (exult.cc, after "joy_aim_x != 0").
  Walking is skipped in dont-move mode or when the Avatar can't act.
- **Not read:** the right stick, triggers, every button, touchpads, gyro.
  No controller hints are set, so pads SDL leaves off by default (the
  Steam Controller, below) are never seen.
- **Already done on the keyboard side:** the Target System (`Friend_mode_space`,
  `Friend_mode_cycle_target`, `Friend_mode_cycle_object`) and the
  conversation highlight (`Conversation::move_highlight`, on E / Shift+E),
  so most buttons only have to call these.
- iOS has its own virtual pad (`ios/GamePadView`) and Android a virtual
  joystick (`TouchUI_Android`); both feed SDL's gamepad events, so
  anything built here works for them too.

## Steam Controller (original, 2015)

Tested 2026-10-23 on macOS 27 with the Bluetooth (BLE) firmware: macOS
shows it as "SteamController", vendor 0x28DE, product 0x1106.

- **SDL ignores it by default.** `SDL_HINT_JOYSTICK_HIDAPI_STEAM`
  ("HIDAPI driver for Bluetooth Steam Controllers") defaults to "0".
  Without it Exult sees no gamepad, only macOS's fallback mouse and keys
  (right pad moves the pointer, triggers click). Fix: set the hint to "1"
  before `SDL_Init`, or test first with the environment variable
  `SDL_JOYSTICK_HIDAPI_STEAM=1`.
- **macOS permission.** Reading it takes Input Monitoring (System Settings,
  Privacy & Security). Steam failed with "Unable to open local device"
  until it was allowed; Exult will need the same, and should say so in
  the log when `SDL_OpenGamepad` fails.
- **Steam running.** Steam opens the controller too. Whether SDL can still
  read it while Steam is open, or only once Steam has quit, is untested.
  Launched from Steam as a non-Steam game, Steam Input may also send its
  own key and mouse layout on top; the README should say to turn Steam
  Input off for Exult, or to quit Steam.
- **Extra inputs.** SDL names the back grips `SDL_GAMEPAD_BUTTON_LEFT_PADDLE1`
  (L4) and `RIGHT_PADDLE1` (R4). Free in the map above; candidates are the
  held radial menu for skills and spells, and "more" (LB) for players who
  want the shoulders for combat. How SDL reports the two trackpads and the
  gyro for this pad is still to check (the left pad probably comes in as
  the D-pad).
- **Until then** it already works through Steam: add Exult as a non-Steam
  game and map the right pad to the mouse, triggers to clicks and the
  buttons to keys. Good enough for play, and a check of which keys the
  default map needs.

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

0. **See more pads.** Set `SDL_HINT_JOYSTICK_HIDAPI_STEAM` to "1" next to
   the other hints before `SDL_Init` (exult.cc), and keep a list of the
   open pads instead of looking them up per event, so held buttons (LB,
   held Y, held Back) can be read from anywhere.
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
9. **Pointer (stopgap).** The right stick moves the mouse pointer, RT is a
   left click and LT a right click while it is moving. Covers whatever has
   no controller path yet (the skill and spell bars before the radial menu,
   dragging, the map), and the Steam Controller's right trackpad if SDL
   reports it. Not in the button map above; it only takes the right stick
   and triggers when they are moved, and LT/RT go back to paperdolls when
   the pointer has been still for a moment.
10. **Options:** a "Controller" line on the Target System page (on when a
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

Steam Controller (Bluetooth): with Steam quit, then with Steam open, then
launched from Steam. Each time, check the log for "Game controller attached
and open", that the stick walks, and that the paddles arrive. Also a pad
plugged in and pulled out mid-game, and two pads at once (host and a local
second player are not planned, but the second pad must not also drive the
Avatar).

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
