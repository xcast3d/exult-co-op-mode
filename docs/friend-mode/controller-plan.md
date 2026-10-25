# Controller support (plan)

**Status:** steps 0 to 2 built on `feature/controller` (2026-10-25) and
played with a PS4 pad: more pads, buttons to actions, the pad names in
the hint line, the pad pointer. Then (2026-10-25, untested with a pad):
the pad in Exult's other screens as the keys they take (the title menu,
windows, the "How many?" slider with left / right and LB / RB by ten,
Yes/No, signs and conversation choices, which covers most of steps 3 to
5 and 8); the face buttons drawn as glyphs in the hint line (the
PlayStation shapes, the Xbox letters). After a pad test: Cross skips the
intro and scenes as Space does, the left stick drives menus as the D-pad,
and step 7's highlight is on the game menu and every page it opens
(Video, Audio and its Mixer, Game Engine, Game Display, Input, Target
System, Class System, Co-Op Mode, UI and its layer page, Load/Save: up /
down pick a save, LB / RB the buttons, a new save named by date). Still
pointer-only: Audio's Advanced page and character creation. Step 6
built (2026-10-25, untested with a pad): both rings, with placeholder
plates drawn in code (a dark disc, gold when highlighted) until the art
in the Art section is made; the utilities ring has the spellbook where
chat quick phrases will go. Steps 9 to 11 are still a plan. (Plan 2026-10-19; current state and Steam Controller notes added
2026-10-23; button map and direction from a Steam Input test,
2026-10-25.) Listed under "Possible future projects" in the
[project brief](../ultima-7-friend-mode.md).

Goal: play Black Gate, alone or in Co-Op Mode, with a gamepad and no
keyboard or mouse. The [Target System](targeting-groups.md) (Q, E, Space)
already turns most mouse work into a few keys; a controller mostly needs
those keys on buttons, plus a pointer, two radial menus, and a way through
conversations, menus and chat.

**Branch:** built on a branch of its own (such as `feature/controller`,
from `feature/friend-mode`), merged back when a step is playable. Not
started until the current round of test fixes is in.

## Direction (decided 2026-10-25)

- **Made for a standard pad.** Xbox or PlayStation layout: two sticks (each
  also a button), D-pad, four face buttons, two shoulders, two triggers,
  Select (Back, Share) and Start (Menu, Options). Nothing depends on
  extras such as paddles, trackpads or gyro; a pad that has them can map
  them in the config.
- **The pad adds, never replaces.** When a controller is connected, the
  keyboard and mouse keep working exactly as they do now, at the same time.
  Nothing is switched off or hidden because a pad is present.
- **Controller-only screens where buttons run out.** A pad has too few
  buttons for every key Exult and Friend Mode use. Instead of chords, the
  rest goes into controller UI: two held radial menus (skills, utilities),
  a stick-driven pointer, and focus highlights in conversations and
  menus. These appear only in response to the pad.
- **The targeting line shows the pad's buttons.** The "Targeting Deer"
  notice at the top and the hint line under it stay as they are, but
  when the pad was the last thing used the hint names its buttons:
  "Take (A)  Split (LT+A)" instead of "Take (Space)  Split
  (Shift+Space)". Back to keys as soon as a key is pressed. Labels come
  from SDL (`SDL_GetGamepadButtonLabel`: A/B/X/Y, or Cross/Circle/Square/
  Triangle on PlayStation pads), not a fixed list.

## Tested with Steam Input (2026-10-25)

Before writing any code, the map below was tried with Steam Input sending
keys and mouse to Exult (Steam Controller, macOS). It played well: walking,
targeting, acting, fighting and the party windows all needed no keyboard.

Steam setup that worked, for anyone using that route today:

- Add Exult as a non-Steam game and launch it from Steam.
- Pick a **keyboard and mouse** template (such as "Keyboard (WASD) and
  Mouse") for Exult. With no layout, Steam falls back to its gamepad
  template ("Controller 0 uses xinput : true" in Steam's
  `logs/controller.txt`); on macOS Steam can't present a virtual gamepad to
  other apps, so nothing reaches Exult, not even the pointer.
- macOS needs Input Monitoring allowed for Steam (and possibly
  Accessibility, for Steam to move the pointer).

What it showed:

- The tested buttons are the default map below, except two changes made
  after: Space moved from east to south, and the triggers became Shift and
  Alt, mouse buttons only while the pointer shows (the test had them as
  clicks at all times).
- Not enough buttons: skills (three slots), eating, the watch and the map
  were left over with only two face buttons free. Hence the radial menus.
- Mouse-only jobs (dragging items, the skill bar, odd buttons in windows)
  were fine with a pad pointer and triggers as mouse buttons. Hence the
  pointer stays in the native version.

## Button map (default)

Names: Xbox / PlayStation, then SDL's name. The "Same as" column is the key
or mouse action the Steam test used.

| Button | Does | Same as |
| --- | --- | --- |
| Left stick | walk | WASD / arrows |
| Left stick click (L3) | run or walk, toggled | R |
| Right stick | move the pointer (below) | mouse |
| Right stick click (R3) | combat mode on or off (moved from west, 2026-10-25) | C |
| LB / L1 (left shoulder) | next person or creature | Q |
| RB / R1 (right shoulder) | next thing | E |
| LT / L2 (left trigger), held | Shift; with the pointer showing: left mouse button (held: drag) | Shift / left click |
| RT / R2 (right trigger), held | Alt; with the pointer showing: right mouse button | Alt / right click |
| A / Cross (south) | act on the pick: talk, open, take, use, attack; held: item menu (below) | Space |
| X / Square (west), held | skills radial (below; moved from east, 2026-10-25) | |
| B / Circle (east) | close the newest window; with none, clear the pick; held: sort it (2026-10-25) | Shift+C |
| Y / Triangle (north), held | utilities radial (below) | |
| D-pad up | tap: switch to the next character; held: hold / follow (2026-10-25) | Shift+Tab / Y |
| D-pad left | paperdoll | |
| D-pad right | inventory | I |
| D-pad down | stats | Z |
| Select / Back | player list | Tab |
| Start / Menu | game menu, or close the top window | Escape |

Notes:

- **South confirms** (decided 2026-10-25). The Steam test had Space on
  east; the native map moves it to south, the usual confirm button on
  Xbox and PlayStation (the skills ring went to east, then to west). SDL names buttons by
  position, so on a Nintendo-style pad south is B; the config can swap
  south and east for players who want that.
- **Triggers are Shift and Alt, held.** Every Shift and Alt version of a
  Target System key works on the pad without its own button:
  LT + south is Shift+Space (take all, split), RT + south is Alt+Space
  (drop some of one's own), LT + LB / RB is Shift+Q / Shift+E (cycle
  backwards). Only while the pointer is showing are the triggers mouse
  buttons instead (see Pointer).
- **Targeting first, pointer for the rest.** Nearly everything usable in
  the world is reached with LB / RB and south. The pointer is kept for
  what the Target System doesn't cover: dragging an item to a chosen
  place or person, buttons on windows (the paperdoll's disk and combat
  buttons), the map. So the triggers spend most of the time as Shift and
  Alt.
- **Guests** get the same map. "Follow the Avatar" (Y on the keyboard)
  goes in the utilities radial.
- **Two pads** at once: only the first drives the player; a second is
  ignored (no local second player is planned).

The map lives in the config (`config/gameplay/controller/...`, button name
to action name, the same action names as the key bindings in
`keyactions.cc`), with the table above as the default, so it can be
changed without a rebuild.

## Pad test notes (2026-10-25)

From playing with a PS4 pad, after the menus and prompts were in:

1. **Stick walking ignored run / walk.** The stick went by how far it
   was pushed, up to full speed. Now it is no faster than the keys: R's
   run or walk, and slower in combat. (Fixed.)
2. **Load/Save mixed its axes.** The saves are a column, the buttons a
   row: up / down pick a save, left / right (and LB / RB) the buttons.
   On the keyboard left / right still move the cursor in the name.
   (Fixed.)
3. **Yes/No had no highlight.** It now has one, moved with left /
   right; Cross presses it. (Fixed.)
4. **The pointer didn't go away.** Any pad input kept it up, walking
   included; now only the right stick and trigger clicks do, and after
   the hide delay it goes, coming back in the centre. (Fixed.)
5. **East felt like "close".** Circle now closes the newest window (or
   clears the pick with none); the skills ring moved to Square (west),
   combat to R3. Chat quick phrases need a new place (the utilities
   ring). (Done.)
6. **Y (hold / follow) had no button.** D-pad up held (about a third of
   a second) does it; a tap still switches character, now on release.
   Guests get follow / take over the same way. Held actions are in the
   config too (`config/gameplay/controller/hold/<button>`). (Done.)
7. **The Hold / Follow button under the minimap** hides while the pad is
   in use (it comes back with the mouse or a key). (Done.)
8. **A list of the pad's buttons** is needed in the game, not only in
   this plan. Built: a "Controller" page in the game menu (the start of
   step 11) that lists the live map with the pad's own names, a page at
   a time (left / right); the same list in the README. The settings
   (swap, pointer speed, hide delay) join it later. (Done.)
10. **Sort a window** (Shift+C) had no button: Circle held sorts the
    newest window, as a tap closes it. (Done.)
11. **The window's Sort and Take all buttons** (under a container's
    checkmark) hide while the pad is in use, as the minimap's Hold
    button does: Circle held sorts, L2+Cross takes all. (Done.)
9. **Xbox:** every Xbox pad has View where the PS4 has Share (SDL's
   Back); Series pads add a Share button (SDL's MISC1), unmapped.

## Radial menus

Hold the button: a ring of up to eight entries opens around the Avatar.
Push the **left stick** towards an entry to highlight it (the Avatar
doesn't walk while a ring is open); **let go** of the button to use it.
Let go with the stick centred to cancel. Tapping the button without
choosing does nothing, so a short press can't fire anything by accident.

- **Skills (west, held):** slots 1, 2 and 3 of the
  [Class System](companion-skills.md) skill bar, with the bar's 24px icons
  and cooldowns. The Avatar's spell bar the same way. A skill that needs a
  target uses the current pick, as the keys do; with no pick it shows the
  crosshair, aimed with the right stick and confirmed with south.
- **Utilities (north, held):** feed (F), watch (W), map / location,
  journal, lockpick (P), keys (K), follow the Avatar (guests), chat quick
  phrases (if not on R3). Same order every time, so it becomes muscle
  memory.
- Drawn as a gump in the screen's centre with a name under the
  highlighted entry. In single player the game keeps running while a ring
  is open, as it does for the skill bar (open question: pause it in single
  player only).
- Written so other rings can be added (spells by circle, party orders).

## Pointer

- **Right stick moves Exult's pointer.** It starts hidden in the centre of
  the screen and shows when the right stick moves. Speed grows with how
  far the stick is pushed, with a small dead zone and a slow fine-aim
  range near the centre.
- **While it shows, the triggers are the mouse buttons.** LT down / up is
  the left button down / up, so holding LT while moving the right stick
  drags an item; RT is the right button. While it's hidden they are Shift
  and Alt. The pointer on screen is the sign of which, so there is no
  mode to remember.
- **Hides after about 5 seconds** with no controller input, and goes back
  to the centre for next time. LB, RB or south hide it at once, since
  they mean the player is back to targeting.
- **The real mouse always wins.** Moving the mouse shows the normal
  pointer at the mouse's position; the pad pointer then carries on from
  there.
- Built by sending ordinary mouse events (`SDL_WarpMouseInWindow` and
  pushed `SDL_EVENT_MOUSE_*` events), so every place that reads the mouse,
  including the modal loops (`Get_click`, dragging, gumps), works with no
  change of its own.

## Items and equipment (decided 2026-10-25)

What Space does with items today (`Friend_mode_space`,
`Friend_mode_equip`, exult.cc), checked 2026-10-25:

- **Swapping works for one's own character.** Space on plate boots in
  one's pack readies them and puts the leather boots that were on into
  the pack; with no room, the swap is undone ("No room"). Two-handed
  weapons free both hands the same way.
- **Unequipping works:** Space on a worn thing puts it in the pack.
- **Only for the controlled character.** The Avatar in Iolo's paperdoll or
  pack *takes* the item into the Avatar's own pack, as from any other
  container. Equipping Iolo today means switching to him first (D-pad up).
- **No split unequip.** Alt+Space on worn arrows drops some at one's feet.

Changes, keyboard and pad alike:

- **Space acts for the window's owner.** In a party member's paperdoll or
  pack, the Avatar's Space readies, swaps or puts away on *that member*,
  as it already does for one's own (the Avatar is the party
  administrator). Taking it into one's own pack moves to Shift+Space
  (LT + south). Containers outside the party work as today.
- **Split unequip.** On a worn stack, Alt+Space (RT + south) puts *some*
  back in the pack (how many). Dropping some of a worn stack moves to the
  item menu.
- **Item menu: hold south** on a picked item. Tap still does Space; hold
  opens a short list over the pick, moved with the D-pad, south to choose,
  Start to cancel:
  - Use / Ready / Put away (whichever Space would do)
  - Give to... (below)
  - Put in... (an open container, or a chest picked on the map)
  - Split (how many)
  - Drop
  Only the entries that apply are shown. South's tap has to wait for the
  release to tell it from a hold (about a quarter second). Exult's touch
  item menu (`Itemmenu_gump`: Use, Pick up, Move) is a `Modal_gump`, so it
  can be the base and gets the menu focus of step 7 for free. On the
  keyboard the same menu opens on a key (to choose; Ctrl+Space is a
  candidate).
- **Give to...** The party's portraits along the bottom (`Face_stats`)
  get a highlight: Q / E (LB / RB) move it, Space (south) gives, Escape
  (Start) cancels. It starts on the owner of the other open paperdoll if
  there is one, else the next party member. The item goes into the
  receiver's pack, the whole stack, or how many with Alt (RT); then Space
  in their window readies it. Both must be within reach ("Too far away"),
  and a full pack says "No room". The hint line adds "Give" when the pick
  can be given. A keyboard key of its own as well: **G** (moving Black
  Gate's abacus key, which is hardly used) or **U** (free since the world
  map was shelved); to decide.
- **Guests don't give** (decided 2026-10-25). As today, a guest only
  drops things and picks things up into their own inventory. Their item
  menu has no Give to... and Put in... only for their own bags; sharing
  gear goes through the floor or the Avatar.

## What still needs the pointer

After the item changes above, the pointer (right stick, triggers) is left
for:

1. **"Use X on Y"**: a key on a door, a lockpick on a chest, food on a
   companion, the bucket on a well. These show a crosshair (`Get_click`).
   Better than the pointer: while the crosshair is up, LB / RB step
   through likely targets (as Q / E do), south confirms, Start cancels;
   the right stick still aims freely for anything else.
2. **Pushing or placing things in the world** (crates, puzzle pieces) and
   dropping on a chosen spot.
3. **The map**, and arranging things inside a pack (Sort covers most of it).
4. Anything not covered yet while the steps below are built.

Window buttons are mostly covered: the paperdoll's disk button is Start,
its combat button west.

## Built so far (steps 0 to 2)

- `controller.cc` / `controller.h`: the open pads (the first plays), the
  map from `config/gameplay/controller/buttons/<button>` (`south`, `east`,
  `leftshoulder`, `lefttrigger`, `dpup`...) = an action (`TARGET_PERSON`,
  `TARGET_THING`, `TARGET_ACT`, `TOGGLE_RUN`, `NEXT_CHARACTER`, `BACKPACK`,
  `PLAYER_LIST`, `SHIFT`, `ALT`, `SKILLS_RING`, `UTILITIES_RING`,
  `CHAT_PHRASES`, `NONE`, or any key binding's name such as
  `TOGGLE_COMBAT`), and `enabled`, `swap_south_east`, `pointer_speed`
  (percent), `pointer_hide_ms`.
- The pointer and the triggers are polled from `Delay()` (a hook in
  `gump_utils.h`), so they work in every loop, the modal ones included.
  While a trigger is held as a mouse button, each pointer move also pushes
  a motion event carrying that button, which dragging needs.
- Exult's own cursor hides while the pad is in use and its pointer isn't
  showing (`Mouse::pad_hidden`); a crosshair still shows.
- The D-pad as built: up the next character (Shift+Tab; a guest asks for
  the next free companion), left the paperdolls (`INVENTORY`, the I key:
  in this fork I opens paperdolls), right the next backpack (Shift+I),
  down stats. Start clears the pick first, as Escape does, then closes
  windows or opens the menu.
- East, north and R3 only say "not built yet" until step 6 and step 9.

## Art (checked 2026-10-25)

For the controller screens, what the game and Friend Mode already have,
and what is missing. Sizes are the frames' own. The skill bar draws 24x24
pictures, so new art is 24x24 to match (the radial entries sit in 24px
sockets, so the smaller shortcut bar icons centre in them).

Already there, reused as is:

| For | Art | Size |
| --- | --- | --- |
| Skills ring: skills 1-3 | `data/friend/skills/skill_icons_colour.shp` / `_parchment.shp`, one frame per skill | 24x24 |
| Skills ring: the Avatar's spells | the spell bar's pictures (spellbook gump spell icons) | as the spell bar |
| Utilities: feed | `shortcutbar.vga` 6 (food) | 14x14 |
| Utilities: map | `shortcutbar.vga` 8 (maps) | 16x16 |
| Utilities: journal | `shortcutbar.vga` 9 (notebook) | 12x12 |
| Utilities: keys | `shortcutbar.vga` 3 (keyring), or 7 (key) | 16x13 / 9x16 |
| Utilities: watch | BG `shapes.vga` 159 (pocketwatch) | 9x10 |
| Utilities: lockpick | BG `shapes.vga` 627, frame 7 | 12x13 |
| Pointer, "use X on Y" crosshair | `pointers.shp` frame 2 (greenselect) | 11x11 |
| Item menu | `Itemmenu_gump` (the touch item menu: text buttons) | - |
| Give to...: portraits | `Face_stats` faces along the bottom | - |
| Menu focus (step 7), Give highlight, ring label | drawn boxes and the game font, as the conversation highlight is drawn | - |

Missing, to be made (24x24, U7 palette, transparent ground, in the style
of the skill icons):

1. **Follow the Avatar** (utilities ring; guests): nothing in the game
   fits (the hold / follow button is text).
2. **Chat quick phrases** (utilities ring or R3): a speech scroll or
   bubble; Black Gate has none.
3. **Ring socket**: one entry's round plate, 28x28, with a **highlighted**
   frame of the same size (the entry under the stick). Eight of them make
   the ring.
4. **Ring centre plate** (optional), about 40x40, under the name of the
   highlighted entry; without it the name is drawn on a plain box.
5. **Watch and lockpick at 24x24** (optional): the item shapes above work
   but are small next to the skill icons.
6. **Button glyphs** (optional, only if the hint line should show
   pictures instead of "A", "LT+A"): A, B, X, Y; Cross, Circle, Square,
   Triangle; LB, RB, LT, RT; D-pad; Start, Back; about 12x12 each.

## Text entry

Typing (chat, the journal, save names) stays on the keyboard. Options for
no keyboard, in order of effort:

1. Chat quick phrases (below) cover most of chat.
2. Steam's on-screen keyboard, when Exult is launched from Steam (Big
   Picture) or on a Steam Deck. Exult only has to start SDL text input
   (`SDL_StartTextInput`) where it already takes text.
3. An on-screen keyboard of our own (a grid of letters picked with the
   D-pad and south) if 2 isn't enough. Later, and only if needed.

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
shows it as "SteamController", vendor 0x28DE, product 0x1106. Not the
target pad (see Direction), but the one at hand, so native support should
see it.

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
  Launched from Steam, Steam Input sends its own layout on top; with
  native support the README should say to turn Steam Input off for Exult,
  or to quit Steam.
- **Extra inputs.** SDL names the back grips `SDL_GAMEPAD_BUTTON_LEFT_PADDLE1`
  (L4) and `RIGHT_PADDLE1` (R4). Not in the default map; mappable in the
  config. How SDL reports the two trackpads and the gyro for this pad is
  still to check (the left pad probably comes in as the D-pad; the right
  pad would suit the pointer).

## Implementation

0. **See more pads.** Set `SDL_HINT_JOYSTICK_HIDAPI_STEAM` to "1" next to
   the other hints before `SDL_Init` (exult.cc), and keep a list of the
   open pads instead of looking them up per event, so held buttons (east,
   north, the triggers) can be read from anywhere. New code in its own file
   (`controller.cc`), with only hooks in `exult.cc` (see
   [branching](branching.md)).
1. **Buttons to actions.** In `Handle_event`, turn
   `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP` into the existing actions instead of
   fake key presses: the Target System calls (`Friend_mode_cycle_target`,
   `Friend_mode_cycle_object`, `Friend_mode_space`), and Exult's key
   actions (`ActionCombat`, `ActionInventory`, `ActionCloseOrMenu`...) by
   name through the keybinder's action table. L3 toggles running the way
   R does. Held LT / RT are passed in where Shift and Alt are read today
   (`Friend_mode_space(shift, alt)`, the cycles' step).
1b. **Hint line.** Remember whether the last input was the pad or the
   keyboard, and have `Friend_space_hint` (and the other "(Key)" hints)
   name buttons for the pad. Redraw the line when that switches.
2. **Pointer.** Right stick and triggers as above. Comes early: with it,
   everything that has no controller path yet is still reachable.
3. **Modal loops.** Conversations, signs, books and number prompts wait
   in their own loops (`Get_click`, `Gump_manager::do_modal_gump`), outside
   `Handle_event`. Each needs south as a click (Space already closes signs
   and turns book pages there) and Start as Escape.
4. **Conversation choices.** Choices are picked with the mouse or 1-9/a-z
   (`Usecode_internal::get_user_choice_num`). The keyboard highlight is
   built (2026-10-22: `Conversation::move_highlight`, white; E and Shift+E,
   Space; nothing highlighted until the first E). On a pad: RB / LB move it
   (the same buttons as E / Q), D-pad up / down too, south picks, Start
   picks "bye" if offered (as Escape does). Guests watching the Avatar's
   conversation use the same highlight to pick their suggestion: south sends
   it (the "Suggest a reply" message).
5. **NPC text that waits for a click** ("click to continue") takes south
   as a click; it already takes any key.
6. **Radial menus.** The skills and utilities rings above: a ring gump
   with highlight and labels, opened on a held button, read from the left
   stick, used on release.
7. **Menus and option pages.** The pause menu (Start) and every page it
   opens are driven by the pad, sub menus included:
   - **A highlighted item.** D-pad up / down moves it, wrapping round,
     drawn with a highlight box. The first item is highlighted when a page
     opens, and returning from a sub menu highlights the item that opened
     it.
   - **South presses it**, as a click would: a button runs, a sub menu
     opens with its own first item highlighted.
   - **Left / right change a setting** on the highlighted line (Yes/No,
     a list such as the scaler, a slider by one step), without opening
     the drop-down.
   - **Start or east goes back** one level, as Cancel does; on the pause
     menu itself it closes it.
   - Covers the pause menu (`Gamemenu_gump`) and its pages: Load/Save
     (`Newfile_gump`, the save list scrolling with the highlight), the
     options pages (`VideoOptions_gump`, `AudioOptions_gump`,
     `GameEngineOptions_gump`, `GameDisplayOptions_gump`,
     `InputOptions_gump`, `TargetingOptions_gump`, `UIOptions_gump`,
     `AdvancedOptions_gump`), the Mixer, and Yes/No prompts
     (`Yesno_gump`). The title menu works the same (it is already
     keyboard-driven).
   - **Built once, in `Modal_gump`,** which all of these derive from: it
     keeps the page's buttons in screen order and a focus index, and
     handles up / down / confirm / back. Each page only has to say which
     of its widgets take left / right. The same keys on the keyboard
     (arrows, Enter, Escape) do the same, so the menus need no mouse
     either way.
   - Typing fields (a save's name, the password box) take text from the
     keyboard or the on-screen keyboard (see Text entry); with the pad,
     a new save gets a name filled in (the date and place) so it can be
     saved without typing.
8. **Number prompts** ("How many?", the quantity slider): D-pad left /
   right change by one, LB / RB by ten, south accepts, Start cancels.
9. **Chat.** No keyboard: a short list of quick phrases ("Follow me", "Wait
   here", "Over here", "Ready", "Thanks") and the emotes (`/wave`,
   `/bow`...), on R3 or in the utilities ring, picked like conversation
   choices. Typing: see Text entry.
10. **Items and equipment.** The changes under Items and equipment: Space
    for the window's owner, split unequip, the item menu on a held south,
    Give to... with the face highlight, and the crosshair stepping through
    targets with LB / RB. Mostly Target System work in exult.cc, useful on
    the keyboard as well.
11. **Options:** a "Controller" page: on or off (on by default), swap
    south and east, pointer speed, pointer hide delay. Remapping in the
    config.

## Order

1. Steps 0 to 2: more pads, buttons and the hint line, pointer. The game
   is playable with a pad from here (it is what the Steam test played).
2. Steps 3 to 5: modal loops and conversations, so no mouse work is needed
   in dialogue.
3. Step 6: radial menus (skills, utilities).
4. Steps 7 and 8: menus, option pages, number prompts.
5. Step 10: items and equipment (Space for the owner, item menu, Give).
6. Step 9 and Text entry.
7. Step 11: options page and remapping.

## Testing

An Xbox or PlayStation pad on the Mac (the target), then the Steam
Controller. Black Gate's opening in Trinsic without touching the keyboard
or mouse: walk and run, talk to Finnigan through every choice, open the
stables' chests, take and ready a weapon, drag an item between packs with
the pointer, fight a rat, use a skill and eat from the radials, use the
game menu to save. Then the same with the mouse and keyboard used in
between, to check neither stops the other. Then a guest doing the same
with the host on the keyboard.

Steam Controller (Bluetooth): with Steam quit, then with Steam open, then
launched from Steam. Each time, check the log for "Game controller attached
and open" and that the stick walks. Also a pad plugged in and pulled out
mid-game, and two pads at once (the second must not also drive the
Avatar).

## For upstream Exult

Nothing in steps 0 to 5, 7 and 8 depends on Co-Op Mode or the Class
System, and they would suit Exult itself (see
[upstreaming notes](upstreaming.md)). The skills ring needs the Class
System; the Co-Op parts (guests' suggestions, chat phrases, follow) stay
ours. The map would then have to fall back to Exult's own keys where the
Target System isn't there.
