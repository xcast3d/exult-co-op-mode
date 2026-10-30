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

#include <functional>
#include <string>
#include <vector>

class Image_buffer8;

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
	item_menu,         // Held south: what can be done with the pick (Ctrl+Space).
	next_window,       // The oldest open window to the front (Back tapped).
	arrange_windows,   // The open windows laid out side by side (Start held).
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
// Quitting: the pads closed and every hook let go, before the game window
// is destroyed (SDL_Quit, at exit, reports the pads removed after that).
void Controller_quit();
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
// A window that takes the pad while it is open and in front (the Avatar's
// spellbook): given the D-pad as the arrows, LB / RB as Page Up / Down and
// south as Space, it says whether it took them.  Set by exult.cc.
using Controller_window_key = bool (*)(SDL_Keycode key, bool down);
void Controller_set_window_hook(Controller_window_key window_key);

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
// Where the pad pointer is, in window coordinates; false while it's hidden.
bool Controller_pointer_at(float& wx, float& wy);
// Move the pointer to window point (wx, wy) and show it (the crosshair
// aimed at the Target System's pick).
void Controller_aim_pointer(float wx, float wy);

// The radial menus (controller-plan.md, step 6): held Square (skills) and
// Triangle (utilities) open a ring of up to eight entries round the
// Avatar; the left stick highlights one (the Avatar doesn't walk
// meanwhile) and letting go of the button uses it.  Let go with the stick
// centred to cancel.  The game runs on meanwhile.
// How an entry is drawn: a round plate with its picture; the picture alone
// (pictures that carry their own tile: the parchment skill icons); or its
// name as text in a box (spells, whose pictures don't read at this size).
enum class Controller_ring_look { plate, bare, text };

struct Controller_ring_entry {
	std::string name;
	// The picture, centred on (cx, cy) in 'buf'; none: the plate is blank.
	std::function<void(Image_buffer8* buf, int cx, int cy)> paint;
	std::string           corner;           // Small text on the plate (seconds left).
	bool                  usable = true;    // Dimmed when not (its use then says why).
	std::function<void()> use;
	Controller_ring_look  look = Controller_ring_look::plate;
};
// 'title' shows in the middle until an entry is highlighted.  Entries are
// read again each frame with 'refresh' (cooldowns), if given.
void Controller_ring_open(
		const std::string& title, std::vector<Controller_ring_entry> entries,
		std::function<std::vector<Controller_ring_entry>()> refresh = {});
// Letting go of the button: the highlighted entry is used (if usable).
void Controller_ring_close();
bool Controller_ring_is_open();
// Each frame, from the game's overlays: the ring centred on screen point
// (sx, sy), or hidden.
void Controller_ring_paint(int sx, int sy);

// A short note for an open window (the spellbook's "Turn the page"), its
// top centre at screen point (sx, sy); empty text hides it.  Each frame.
void Controller_paint_note(const std::string& text, int sx, int sy);
// A pad button's name as printed on the pad (SDL_GAMEPAD_BUTTON_...).
std::string Controller_button_name(int button);

// A short list to choose from (the item menu, Give to...): a window with
// 'title' and a button per entry, the first highlighted; up / down and
// Return (Cross) choose, Escape (Circle, Start) cancels.  The entry's
// index, or -1.  Waits (a modal window).
int Controller_choose(const std::string& title, const std::vector<std::string>& entries);

// The game menu's Controller page: the settings (on or off, swap south and
// east, pointer speed, pointer hide delay; saved in the config), with a
// Buttons... button for the list below.
void Controller_options_page();
// Every button and what it does, in the pad's own names (Cross or A...),
// from the live map.
void Controller_buttons_page();
// A list of (key, what it does) rows, a page at a time (left / right turn
// them): the buttons list, the game menu's Keys page.
void Controller_show_pages(
		const std::string& title, const std::vector<std::vector<std::pair<std::string, std::string>>>& pages);

// Exult's other screens, which wait in loops of their own (menus, the
// "How many?" slider, Yes/No, signs, conversations): the pad's buttons
// become the keys those screens already take.  'menu' (the title menu and
// the game's windows): the D-pad the arrows, south Return, east and Start
// Escape, the shoulders Page Up / Down.  Otherwise (Get_click: signs,
// conversations, the crosshair, the map): south Space, east and Start Escape, the
// shoulders and D-pad up / down E and Shift+E (the conversation highlight),
// D-pad left / right the arrows.  The left stick, pushed, as the D-pad.
// Rewrites 'event' in place; others are left alone.
void Controller_screen_event(SDL_Event& event, bool menu);

// The name on the first button doing 'action' ("A", "Cross", "LB",
// "LT"...), or empty with none.  'key_name' for key_binding actions.
std::string Controller_label(Controller_action action, const char* key_name = nullptr);

#endif
