/*
 * Controller support: a gamepad's buttons, the pad pointer and the button
 * names in the hint line (docs/friend-mode/controller-plan.md, steps 0 to
 * 2).  The pad adds to the keyboard and mouse, never replaces them.
 *
 * The map is config/gameplay/controller/buttons/<button> = <action>, with
 * the plan's table as the default.  Buttons are named by position (south,
 * east, west, north, leftshoulder, lefttrigger, dpup...), actions by the
 * key bindings' names where there is one (TOGGLE_COMBAT, INVENTORY, STATS,
 * CLOSE_OR_MENU) and by the names below for the rest.
 */
#ifndef CONTROLLER_H
#define CONTROLLER_H

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <string>

enum class Controller_action {
	none,
	shift,             // Held: Shift (pointer showing: left mouse button).
	alt,               // Held: Alt (pointer showing: right mouse button).
	target_person,     // Q: the next person or creature.
	target_thing,      // E: the next thing.
	target_act,        // Space: act on the pick.
	toggle_run,        // R: run or walk.
	next_character,    // Shift+Tab: lead (a guest: play) the next companion.
	backpack,          // Shift+I: the next backpack.
	player_list,       // Held Tab: who plays whom.
	skills_ring,       // Held: the skills radial (step 6, not built yet).
	utilities_ring,    // Held: the utilities radial (step 6, not built yet).
	chat_phrases,      // Chat quick phrases (step 9, not built yet).
	close_window,      // The newest window shuts; with none, the pick clears.
	hold_follow,       // Y: the party holds or follows (a guest: follow or take over).
	sort_window,       // Shift+C: tidy the newest window.
	key_binding        // One of Exult's key actions, by name.
};

// What a button does, from the config.
struct Controller_binding {
	Controller_action action = Controller_action::none;
	std::string       name;    // The action's name (for key_binding, Exult's).
};

// Before SDL_Init: the hints for pads SDL leaves off by default (the
// Bluetooth Steam Controller).
void Controller_set_hints();
// After SDL_Init: read the config, open the pads already connected and
// hook into every event loop (Delay).
void Controller_init();
// A pad came or went (from the device watch, any thread's loop).
void Controller_device_added(SDL_JoystickID id);
void Controller_device_removed(SDL_JoystickID id);
// Read the config again (the options page, later).
void Controller_read_config();

// The main loop's pass each frame: a button with a held action runs it
// once held long enough (D-pad up held: hold / follow).
void Controller_frame();

// The game's side: what a button does.  'down' false on release (held
// actions: the player list).  Set by exult.cc.
using Controller_runner = void (*)(const Controller_binding& binding, bool down);
// Called when the last input switches between the pad and the keyboard or
// mouse, to redraw the hint line.
using Controller_switched = void (*)();
void Controller_set_game_hooks(Controller_runner runner, Controller_switched switched);

// Handle_event's first look at an event.  True if it was the pad's and
// is done with; false for everything else (which it may only note).
bool Controller_event(const SDL_Event& event);

// Is the left stick let go?  (Walking stops if it was let go while
// another screen had the events.)
bool Controller_left_stick_centred();
// Is the pad the last thing used (for the hint line's names)?
bool Controller_last_input_is_pad();
// Held triggers as Shift and Alt (only while the pad pointer is hidden).
bool Controller_shift_held();
bool Controller_alt_held();
// Is the pad pointer showing (the triggers are then mouse buttons)?
bool Controller_pointer_showing();

// The game menu's Controller page: every button and what it does, in the
// pad's own names (Cross or A...), from the live map.
void Controller_buttons_page();

// Exult's other screens, which wait in loops of their own (menus, the
// "How many?" slider, Yes/No, signs, conversations): the pad's buttons
// become the keys those screens already take.  'menu' (the title menu and
// the game's windows): the D-pad the arrows, south Return, east and Start
// Escape, the shoulders Page Up / Down.  Otherwise (Get_click: signs,
// conversations, "click to continue"): south Space, Start Escape, the
// shoulders and D-pad up / down E and Shift+E (the conversation highlight),
// D-pad left / right the arrows.  The left stick, pushed, as the D-pad.
// Rewrites 'event' in place; others are left alone.
void Controller_screen_event(SDL_Event& event, bool menu);

// The name on the first button doing 'action' ("A", "Cross", "LB",
// "LT"...), or empty with none.  'key_name' for key_binding actions.
std::string Controller_label(Controller_action action, const char* key_name = nullptr);

#endif
