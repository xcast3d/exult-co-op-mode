/*
 * Controller support: a gamepad's buttons, the pad pointer and the button
 * names in the hint line.  See controller.h and
 * docs/friend-mode/controller-plan.md.
 */
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "controller.h"

#include "Configuration.h"
#include "gamewin.h"
#include "Gump_manager.h"
#include "Modal_gump.h"
#include "Text_button.h"
#include "gump_utils.h"
#include "ibuf8.h"
#include "items.h"
#include "font.h"
#include "palette.h"
#include "shapeid.h"
#include "shapes/vgafile.h"
#include "mouse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

extern Configuration* config;

namespace {
// Every input a binding can go on: SDL's buttons, then the two triggers
// (axes, read as buttons).
constexpr int left_trigger  = SDL_GAMEPAD_BUTTON_COUNT;
constexpr int right_trigger = SDL_GAMEPAD_BUTTON_COUNT + 1;
constexpr int input_count   = SDL_GAMEPAD_BUTTON_COUNT + 2;

// A mouse id of our own for the pointer's pushed button events, so they
// aren't taken for the real mouse.
constexpr SDL_MouseID pad_mouse_id = 0x7AD0;
// And a keyboard id for the menus' repeated keys (a held direction).
constexpr SDL_KeyboardID pad_keyboard_id = 0x7AD0;
constexpr Uint64         repeat_first_ms = 400;    // Held this long, it repeats...
constexpr Uint64         repeat_ms       = 120;    // ...this often.

struct Input_name {
	int         input;
	const char* name;    // In the config.
};

constexpr std::array<Input_name, 22> input_names{{
		{SDL_GAMEPAD_BUTTON_SOUTH, "south"},
		{SDL_GAMEPAD_BUTTON_EAST, "east"},
		{SDL_GAMEPAD_BUTTON_WEST, "west"},
		{SDL_GAMEPAD_BUTTON_NORTH, "north"},
		{SDL_GAMEPAD_BUTTON_BACK, "back"},
		{SDL_GAMEPAD_BUTTON_START, "start"},
		{SDL_GAMEPAD_BUTTON_LEFT_STICK, "leftstick"},
		{SDL_GAMEPAD_BUTTON_RIGHT_STICK, "rightstick"},
		{SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "leftshoulder"},
		{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "rightshoulder"},
		{SDL_GAMEPAD_BUTTON_DPAD_UP, "dpup"},
		{SDL_GAMEPAD_BUTTON_DPAD_DOWN, "dpdown"},
		{SDL_GAMEPAD_BUTTON_DPAD_LEFT, "dpleft"},
		{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "dpright"},
		{SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, "leftpaddle1"},
		{SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, "rightpaddle1"},
		{SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, "leftpaddle2"},
		{SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, "rightpaddle2"},
		{SDL_GAMEPAD_BUTTON_MISC1, "misc1"},
		{SDL_GAMEPAD_BUTTON_TOUCHPAD, "touchpad"},
		{left_trigger, "lefttrigger"},
		{right_trigger, "righttrigger"},
}};

// The plan's table (by position; "swap south and east" applies after).
constexpr std::array<std::pair<int, const char*>, 16> default_map{{
		{SDL_GAMEPAD_BUTTON_LEFT_STICK, "TOGGLE_RUN"},
		{SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "TARGET_PERSON"},
		{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "TARGET_THING"},
		{left_trigger, "SHIFT"},
		{right_trigger, "ALT"},
		{SDL_GAMEPAD_BUTTON_SOUTH, "TARGET_ACT"},
		{SDL_GAMEPAD_BUTTON_WEST, "SKILLS_RING"},
		{SDL_GAMEPAD_BUTTON_EAST, "CLOSE_WINDOW"},
		{SDL_GAMEPAD_BUTTON_NORTH, "UTILITIES_RING"},
		{SDL_GAMEPAD_BUTTON_DPAD_UP, "NEXT_CHARACTER"},
		{SDL_GAMEPAD_BUTTON_DPAD_LEFT, "INVENTORY"},    // The paperdoll (I).
		{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "BACKPACK"},
		{SDL_GAMEPAD_BUTTON_DPAD_DOWN, "STATS"},
		{SDL_GAMEPAD_BUTTON_BACK, "PLAYER_LIST"},
		{SDL_GAMEPAD_BUTTON_START, "CLOSE_OR_MENU"},
		{SDL_GAMEPAD_BUTTON_RIGHT_STICK, "TOGGLE_COMBAT"},
}};

struct Action_name {
	Controller_action action;
	const char*       name;
};

constexpr std::array<Action_name, 17> action_names{{
		{Controller_action::none, "NONE"},
		{Controller_action::shift, "SHIFT"},
		{Controller_action::alt, "ALT"},
		{Controller_action::target_person, "TARGET_PERSON"},
		{Controller_action::target_thing, "TARGET_THING"},
		{Controller_action::target_act, "TARGET_ACT"},
		{Controller_action::toggle_run, "TOGGLE_RUN"},
		{Controller_action::next_character, "NEXT_CHARACTER"},
		{Controller_action::backpack, "BACKPACK"},
		{Controller_action::player_list, "PLAYER_LIST"},
		{Controller_action::skills_ring, "SKILLS_RING"},
		{Controller_action::utilities_ring, "UTILITIES_RING"},
		{Controller_action::chat_phrases, "CHAT_PHRASES"},
		{Controller_action::close_window, "CLOSE_WINDOW"},
		{Controller_action::hold_follow, "HOLD_FOLLOW"},
		{Controller_action::sort_window, "SORT_WINDOW"},
		{Controller_action::item_menu, "ITEM_MENU"},
}};

// Held actions (config/gameplay/controller/hold/<button>): a button with
// one does its own action on a tap (on release) and this one when held.
constexpr std::array<std::pair<int, const char*>, 3> default_hold_map{{
		{SDL_GAMEPAD_BUTTON_SOUTH, "ITEM_MENU"},    // Tap acts on the pick, hold: what else.
		{SDL_GAMEPAD_BUTTON_DPAD_UP, "HOLD_FOLLOW"},
		{SDL_GAMEPAD_BUTTON_EAST, "SORT_WINDOW"},    // Tap closes the window, hold tidies it.
}};
constexpr Uint64 hold_ms = 350;    // Held this long: the held action.

// A held trigger is one of these from its press to its release, whatever
// the pointer does meanwhile.
enum class Trigger_use {
	off,
	modifier,    // Shift or Alt.
	mouse,       // A mouse button (the pointer was showing).
	action       // Another action, run on press and release.
};

struct State {
	bool enabled    = true;
	bool swap_south = false;    // Swap south and east (Nintendo-style).
	int  speed      = 100;      // Pointer speed, percent.
	int  hide_ms    = 5000;     // Pointer hides after this long untouched.

	std::array<Controller_binding, input_count> map;
	std::array<Controller_binding, input_count> hold_map;    // Held actions (none: none).
	// A button with a held action, down and not yet decided (physical input).
	std::array<bool, SDL_GAMEPAD_BUTTON_COUNT>   pending{};
	std::array<bool, SDL_GAMEPAD_BUTTON_COUNT>   hold_fired{};
	std::array<Uint64, SDL_GAMEPAD_BUTTON_COUNT> down_at{};
	std::vector<SDL_JoystickID>                 pads;    // Open, oldest first; the first plays.

	Controller_runner     runner     = nullptr;
	Controller_switched   switched   = nullptr;
	Controller_window_key window_key = nullptr;

	bool last_pad = false;    // The last input was the pad's.
	bool quit     = false;    // Controller_quit: nothing opens any more.

	// The pointer, in window coordinates.
	bool   showing     = false;
	bool   centre_next = true;    // Start in the centre when it next shows.
	float  px = 0, py = 0;
	float  seen_x = -1, seen_y = -1;    // Where the mouse was last seen (ours or real).
	Uint64 last_activity    = 0;
	Uint64 pointer_activity = 0;    // The right stick or a trigger click, last.
	Uint64 last_tick        = 0;

	std::array<Trigger_use, 2> trigger{Trigger_use::off, Trigger_use::off};

	// The left stick in Exult's other screens, as the D-pad: -1, 0 or 1 on
	// each axis (x, y).
	std::array<int, 2> stick_dir{0, 0};

	// A direction held in those screens, repeating (SDLK_UNKNOWN: none).
	SDL_Keycode repeat_key        = SDLK_UNKNOWN;
	SDL_Keymod  repeat_mod        = SDL_KMOD_NONE;
	int         repeat_button     = -1;       // The D-pad button (or the stick's way).
	bool        repeat_from_stick = false;
	Uint64      repeat_next       = 0;
};

State state;

SDL_Gamepad* Primary_pad() {
	return state.pads.empty() ? nullptr : SDL_GetGamepadFromID(state.pads.front());
}

// The input whose binding a physical one uses (south and east swapped).
int Mapped_input(int input) {
	if (state.swap_south) {
		if (input == SDL_GAMEPAD_BUTTON_SOUTH) {
			return SDL_GAMEPAD_BUTTON_EAST;
		}
		if (input == SDL_GAMEPAD_BUTTON_EAST) {
			return SDL_GAMEPAD_BUTTON_SOUTH;
		}
	}
	return input;
}

const Controller_binding& Binding_of(int input) {
	return state.map[Mapped_input(input)];
}

Controller_binding Parse_binding(const std::string& text) {
	Controller_binding binding;
	binding.name = text;
	for (const auto& entry : action_names) {
		if (text == entry.name) {
			binding.action = entry.action;
			return binding;
		}
	}
	binding.action = text.empty() ? Controller_action::none : Controller_action::key_binding;
	return binding;
}

// Hide or show Exult's own cursor: hidden while the pad is in use and its
// pointer isn't showing.
void Update_cursor() {
	const bool hide = state.enabled && state.last_pad && !state.showing;
	if (Mouse::pad_hidden != hide) {
		Mouse::pad_hidden   = hide;
		Mouse::mouse_update = true;
		if (Mouse* mouse = Mouse::mouse()) {
			if (hide) {
				mouse->hide();
			} else {
				mouse->show();
			}
		}
		if (Game_window* gwin = Game_window::get_instance()) {
			gwin->set_painted();
		}
	}
}

void Set_last_input(bool pad) {
	if (pad) {
		state.last_activity = SDL_GetTicks();
	}
	if (state.last_pad != pad) {
		state.last_pad            = pad;
		Modal_gump::focus_on_open = pad && state.enabled;
		Gump::pad_in_use          = pad && state.enabled;
		Update_cursor();
		if (state.switched) {
			state.switched();
		}
	}
}

void Hide_pointer() {
	if (state.showing && state.trigger[0] != Trigger_use::mouse && state.trigger[1] != Trigger_use::mouse) {
		state.showing     = false;
		state.centre_next = true;
		Update_cursor();
	}
}

SDL_Window* Game_screen_window() {
	Game_window* gwin = Game_window::get_instance();
	return gwin && gwin->get_win() ? gwin->get_win()->get_screen_window() : nullptr;
}

void Push_mouse_button(Uint8 button, bool down) {
	SDL_Window* window = Game_screen_window();
	if (!window) {
		return;
	}
	SDL_Event event{};
	event.type            = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
	event.button.timestamp = SDL_GetTicksNS();
	event.button.windowID = SDL_GetWindowID(window);
	event.button.which    = pad_mouse_id;
	event.button.button   = button;
	event.button.down     = down;
	event.button.clicks   = 1;
	event.button.x        = state.px;
	event.button.y        = state.py;
	SDL_PushEvent(&event);
}

void Warp_pointer(SDL_Window* window, float x, float y) {
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(window, &w, &h);
	const float old_x = state.px;
	const float old_y = state.py;
	state.px          = std::clamp(x, 0.f, static_cast<float>(std::max(w - 1, 0)));
	state.py          = std::clamp(y, 0.f, static_cast<float>(std::max(h - 1, 0)));
	SDL_WarpMouseInWindow(window, state.px, state.py);
	state.seen_x = state.px;
	state.seen_y = state.py;
	// SDL's own motion from the warp doesn't know about the buttons we
	// pushed, and dragging reads them from the motion (state &
	// SDL_BUTTON_LMASK): with a trigger held as a mouse button, follow it
	// with a motion that says so.
	SDL_MouseButtonFlags held = 0;
	for (int which = 0; which < 2; ++which) {
		if (state.trigger[which] == Trigger_use::mouse) {
			const Controller_action action = Binding_of(which == 0 ? left_trigger : right_trigger).action;
			held |= action == Controller_action::shift ? SDL_BUTTON_LMASK : SDL_BUTTON_RMASK;
		}
	}
	if (held) {
		SDL_Event event{};
		event.type             = SDL_EVENT_MOUSE_MOTION;
		event.motion.timestamp = SDL_GetTicksNS();
		event.motion.windowID  = SDL_GetWindowID(window);
		event.motion.which     = pad_mouse_id;
		event.motion.state     = held;
		event.motion.x         = state.px;
		event.motion.y         = state.py;
		event.motion.xrel      = state.px - old_x;
		event.motion.yrel      = state.py - old_y;
		SDL_PushEvent(&event);
	}
}

// A trigger, read as a button with a little hysteresis.
bool Trigger_down(SDL_Gamepad* pad, SDL_GamepadAxis axis, bool was_down) {
	const float value = SDL_GetGamepadAxis(pad, axis) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
	return value > (was_down ? 0.35f : 0.5f);
}

void Trigger_changed(int which, bool down) {
	const Controller_binding& binding = Binding_of(which == 0 ? left_trigger : right_trigger);
	Trigger_use&              use     = state.trigger[which];
	const bool                modifier
			= binding.action == Controller_action::shift || binding.action == Controller_action::alt;
	if (down) {
		Set_last_input(true);
		if (modifier && state.showing) {
			use                    = Trigger_use::mouse;    // LT the left button, RT the right.
			state.pointer_activity = SDL_GetTicks();
			Push_mouse_button(binding.action == Controller_action::shift ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT, true);
		} else if (modifier) {
			use = Trigger_use::modifier;
		} else if (binding.action != Controller_action::none) {
			use = Trigger_use::action;
			if (state.runner) {
				state.runner(binding, true);
			}
		}
		return;
	}
	if (use == Trigger_use::mouse) {
		Push_mouse_button(binding.action == Controller_action::shift ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT, false);
	} else if (use == Trigger_use::action && state.runner) {
		state.runner(binding, false);
	}
	use = Trigger_use::off;
}

// Each pass of every event loop (from Delay): the triggers, the pointer
// and its timeout.  Polled, so it works in the modal loops too.
void Controller_tick() {
	const Uint64 now = SDL_GetTicks();
	const float  dt  = state.last_tick ? std::min(static_cast<float>(now - state.last_tick), 50.f) / 1000.f : 0.f;
	state.last_tick  = now;
	SDL_Gamepad* pad    = Primary_pad();
	SDL_Window*  window = Game_screen_window();
	if (!state.enabled || !window) {
		return;
	}

	// The real mouse always wins: if it moved, the pointer carries on from
	// there (and the pad is no longer the last input).
	float mx = 0;
	float my = 0;
	SDL_GetMouseState(&mx, &my);
	if (state.seen_x >= 0 && (std::fabs(mx - state.seen_x) > 2.f || std::fabs(my - state.seen_y) > 2.f)) {
		state.px          = mx;
		state.py          = my;
		state.centre_next = false;
		Set_last_input(false);
	}
	state.seen_x = mx;
	state.seen_y = my;

	if (!pad) {
		return;
	}
	if (state.repeat_key != SDLK_UNKNOWN && now >= state.repeat_next) {
		// Still held that way?
		bool held = false;
		if (state.repeat_from_stick) {
			const bool    across = state.repeat_button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
								|| state.repeat_button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
			const bool    minus  = state.repeat_button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
								|| state.repeat_button == SDL_GAMEPAD_BUTTON_DPAD_UP;
			const int     value  = SDL_GetGamepadAxis(pad, across ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_LEFTY);
			constexpr int on     = SDL_JOYSTICK_AXIS_MAX * 3 / 10;
			held                 = minus ? value < -on : value > on;
		} else {
			held = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(state.repeat_button));
		}
		if (!held) {
			state.repeat_key = SDLK_UNKNOWN;
		} else {
			SDL_Event repeat{};
			repeat.type          = SDL_EVENT_KEY_DOWN;
			repeat.key.timestamp = SDL_GetTicksNS();
			repeat.key.windowID  = SDL_GetWindowID(window);
			repeat.key.which     = pad_keyboard_id;
			repeat.key.key       = state.repeat_key;
			repeat.key.scancode  = SDL_GetScancodeFromKey(state.repeat_key, nullptr);
			repeat.key.mod       = state.repeat_mod;
			repeat.key.down      = true;
			SDL_PushEvent(&repeat);
			state.repeat_next = now + repeat_ms;
		}
	}
	const bool lt = Trigger_down(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, state.trigger[0] != Trigger_use::off);
	const bool rt = Trigger_down(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, state.trigger[1] != Trigger_use::off);
	if (lt != (state.trigger[0] != Trigger_use::off)) {
		Trigger_changed(0, lt);
	}
	if (rt != (state.trigger[1] != Trigger_use::off)) {
		Trigger_changed(1, rt);
	}

	// The right stick moves the pointer: a dead zone, then speed growing
	// with the push (squared, for a slow fine-aim range near the centre).
	float sx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
	float sy = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
	const float     length    = std::sqrt(sx * sx + sy * sy);
	constexpr float dead_zone = 0.15f;
	if (length > dead_zone) {
		Set_last_input(true);
		state.pointer_activity = now;
		if (!state.showing) {
			state.showing = true;
			int w         = 0;
			int h         = 0;
			SDL_GetWindowSize(window, &w, &h);
			// A crosshair (Get_click) is where the mouse is: aim from there.
			const bool crosshair = Mouse::mouse() && Mouse::mouse()->get_shape() == Mouse::greenselect;
			if (state.centre_next && !crosshair) {
				Warp_pointer(window, w / 2.f, h / 2.f);
			} else {
				state.px = mx;
				state.py = my;
			}
			state.centre_next = false;
			Update_cursor();
		}
		const float push  = std::min((length - dead_zone) / (1.f - dead_zone), 1.f);
		int         w     = 0;
		int         h     = 0;
		SDL_GetWindowSize(window, &w, &h);
		// Full push crosses the window's width in about a second.
		const float pixels = push * push * static_cast<float>(w) * (state.speed / 100.f) * dt;
		sx /= length;
		sy /= length;
		if (pixels > 0.f) {
			Warp_pointer(window, state.px + sx * pixels, state.py + sy * pixels);
		}
	} else if (state.showing && now - state.pointer_activity > static_cast<Uint64>(state.hide_ms)) {
		// Only the pointer's own use counts: walking or targeting meanwhile
		// doesn't keep it up.  It comes back in the centre.
		Hide_pointer();
	}
}

bool Is_primary(SDL_JoystickID id) {
	return !state.pads.empty() && state.pads.front() == id;
}

const char* Type_label(SDL_Gamepad* pad, int input) {
	const SDL_GamepadType type = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
	const bool playstation = type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5;
	const bool nintendo    = type >= SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO && type <= SDL_GAMEPAD_TYPE_GAMECUBE;
	switch (input) {
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		return playstation ? "L1" : nintendo ? "L" : "LB";
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		return playstation ? "R1" : nintendo ? "R" : "RB";
	case left_trigger:
		return playstation ? "L2" : nintendo ? "ZL" : "LT";
	case right_trigger:
		return playstation ? "R2" : nintendo ? "ZR" : "RT";
	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		return playstation ? "L3" : "LS";
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
		return playstation ? "R3" : "RS";
	case SDL_GAMEPAD_BUTTON_BACK:
		return type == SDL_GAMEPAD_TYPE_PS5 ? "Create"
			 : type == SDL_GAMEPAD_TYPE_PS4 ? "Share"
			 : type == SDL_GAMEPAD_TYPE_XBOXONE ? "View"
			 : nintendo ? "-" : type == SDL_GAMEPAD_TYPE_PS3 ? "Select" : "Back";
	case SDL_GAMEPAD_BUTTON_START:
		return playstation && type != SDL_GAMEPAD_TYPE_PS3 ? "Options"
			 : type == SDL_GAMEPAD_TYPE_XBOXONE ? "Menu"
			 : nintendo ? "+" : "Start";
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		return "Up";
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		return "Down";
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		return "Left";
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		return "Right";
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return "A";
	case SDL_GAMEPAD_BUTTON_EAST:
		return "B";
	case SDL_GAMEPAD_BUTTON_WEST:
		return "X";
	case SDL_GAMEPAD_BUTTON_NORTH:
		return "Y";
	default:
		break;
	}
	for (const auto& entry : input_names) {
		if (entry.input == input) {
			return entry.name;
		}
	}
	return "?";
}

std::string Input_label(int input) {
	SDL_Gamepad* pad = Primary_pad();
	if (pad && input < SDL_GAMEPAD_BUTTON_COUNT) {
		// The face buttons as printed on the pad (SDL knows which).
		switch (SDL_GetGamepadButtonLabel(pad, static_cast<SDL_GamepadButton>(input))) {
		case SDL_GAMEPAD_BUTTON_LABEL_A:
			return "A";
		case SDL_GAMEPAD_BUTTON_LABEL_B:
			return "B";
		case SDL_GAMEPAD_BUTTON_LABEL_X:
			return "X";
		case SDL_GAMEPAD_BUTTON_LABEL_Y:
			return "Y";
		case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
			return "Cross";
		case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
			return "Circle";
		case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
			return "Square";
		case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
			return "Triangle";
		default:
			break;
		}
	}
	return Type_label(pad, input);
}
}    // namespace

void Controller_set_hints() {
	// The Bluetooth Steam Controller: off in SDL by default.
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
}

void Controller_read_config() {
	config->value("config/gameplay/controller/enabled", state.enabled, true);
	config->value("config/gameplay/controller/swap_south_east", state.swap_south, false);
	config->value("config/gameplay/controller/pointer_speed", state.speed, 100);
	config->value("config/gameplay/controller/pointer_hide_ms", state.hide_ms, 5000);
	state.speed   = std::clamp(state.speed, 10, 400);
	state.hide_ms = std::max(state.hide_ms, 500);
	for (auto& binding : state.map) {
		binding = Controller_binding();
	}
	for (auto& binding : state.hold_map) {
		binding = Controller_binding();
	}
	for (const auto& [input, action] : default_map) {
		state.map[input] = Parse_binding(action);
	}
	for (const auto& [input, action] : default_hold_map) {
		state.hold_map[input] = Parse_binding(action);
	}
	for (const auto& entry : input_names) {
		const std::string key = std::string("config/gameplay/controller/buttons/") + entry.name;
		if (config->key_exists(key)) {
			std::string text;
			config->value(key, text, "");
			state.map[entry.input] = Parse_binding(text);
		}
		const std::string hold_key = std::string("config/gameplay/controller/hold/") + entry.name;
		if (config->key_exists(hold_key)) {
			std::string text;
			config->value(hold_key, text, "");
			state.hold_map[entry.input] = Parse_binding(text);
		}
	}
	Update_cursor();
}

void Controller_init() {
	Controller_read_config();
	SDL_JoystickID* joysticks = SDL_GetJoysticks(nullptr);
	if (joysticks) {
		for (int i = 0; joysticks[i]; ++i) {
			if (SDL_IsGamepad(joysticks[i])) {
				Controller_device_added(joysticks[i]);
			}
		}
		SDL_free(joysticks);
	}
	Delay_hook        = Controller_tick;
	Screen_event_hook = Controller_screen_event;
}

void Controller_quit() {
	Delay_hook        = nullptr;
	Screen_event_hook = nullptr;
	state.runner      = nullptr;
	state.switched    = nullptr;
	state.window_key  = nullptr;
	state.enabled     = false;
	state.quit        = true;
	for (const SDL_JoystickID id : state.pads) {
		if (SDL_Gamepad* pad = SDL_GetGamepadFromID(id)) {
			SDL_CloseGamepad(pad);
		}
	}
	state.pads.clear();    // So SDL_Quit's removals find nothing of ours.
}

void Controller_device_added(SDL_JoystickID id) {
	if (state.quit || std::find(state.pads.begin(), state.pads.end(), id) != state.pads.end()) {
		return;
	}
	SDL_Gamepad* pad = SDL_OpenGamepad(id);
	if (!pad) {
		std::cout << "Game controller attached, but it failed to open. Error: \"" << SDL_GetError() << '"'
#ifdef SDL_PLATFORM_MACOS
				  << " (macOS may need Input Monitoring allowed for Exult: System Settings, Privacy & Security)"
#endif
				  << std::endl;
		return;
	}
	state.pads.push_back(id);
	std::cout << "Game controller attached and open: \"" << SDL_GetGamepadName(pad) << '"'
			  << (state.pads.size() > 1 ? " (not used: the first one plays)" : "") << std::endl;
}

void Controller_device_removed(SDL_JoystickID id) {
	const auto at = std::find(state.pads.begin(), state.pads.end(), id);
	if (at == state.pads.end()) {
		return;
	}
	const bool was_primary = at == state.pads.begin();
	state.pads.erase(at);
	if (SDL_Gamepad* pad = SDL_GetGamepadFromID(id)) {
		SDL_CloseGamepad(pad);
	}
	std::cout << "Game controller detached and closed." << std::endl;
	if (was_primary) {
		// Let go of whatever the old one held.
		for (int which = 0; which < 2; ++which) {
			if (state.trigger[which] != Trigger_use::off) {
				Trigger_changed(which, false);
			}
		}
		Hide_pointer();
		Set_last_input(false);
	}
}

void Controller_set_game_hooks(Controller_runner runner, Controller_switched switched) {
	state.runner   = runner;
	state.switched = switched;
}

void Controller_set_window_hook(Controller_window_key window_key) {
	state.window_key = window_key;
}

// The keys a window in front gets for the pad's buttons, or none.
static SDL_Keycode Window_key(int input) {
	switch (Mapped_input(input)) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return SDLK_SPACE;
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		return SDLK_UP;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		return SDLK_DOWN;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		return SDLK_LEFT;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		return SDLK_RIGHT;
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		return SDLK_PAGEUP;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		return SDLK_PAGEDOWN;
	default:
		return SDLK_UNKNOWN;
	}
}

bool Controller_event(const SDL_Event& event) {
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN:
		if (event.key.which == pad_keyboard_id) {
			return true;    // A menu's repeat left over after it closed: not for the world.
		}
		Set_last_input(false);
		return false;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (event.button.which != pad_mouse_id) {
			Set_last_input(false);
		}
		return false;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP: {
		if (!state.enabled) {
			return false;
		}
		if (!Is_primary(event.gbutton.which)) {
			return true;    // A second pad is ignored.
		}
		const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
		if (down) {
			Set_last_input(true);
		}
		const int input = event.gbutton.button;
		if (state.window_key) {    // A window in front takes it first (the spellbook).
			const SDL_Keycode key = Window_key(input);
			if (key != SDLK_UNKNOWN && state.window_key(key, down)) {
				return true;
			}
		}
		const Controller_binding& binding = Binding_of(input);
		if (input < SDL_GAMEPAD_BUTTON_COUNT && state.hold_map[Mapped_input(input)].action != Controller_action::none) {
			// Tap or hold: decided on release, or by Controller_frame.
			if (down) {
				state.pending[input]    = true;
				state.hold_fired[input] = false;
				state.down_at[input]    = SDL_GetTicks();
			} else if (state.pending[input]) {
				state.pending[input] = false;
				if (state.hold_fired[input]) {
					if (state.runner) {
						state.runner(state.hold_map[Mapped_input(input)], false);
					}
				} else if (state.runner && binding.action != Controller_action::none) {
					state.runner(binding, true);    // A tap.
					state.runner(binding, false);
				}
			}
			return true;
		}
		switch (binding.action) {
		case Controller_action::none:
		case Controller_action::shift:    // Read as held (Controller_shift_held).
		case Controller_action::alt:
			return true;
		case Controller_action::target_person:
		case Controller_action::target_thing:
		case Controller_action::target_act:
			if (down) {
				Hide_pointer();    // Back to targeting.
			}
			break;
		default:
			break;
		}
		if (state.runner) {
			state.runner(binding, down);
		}
		return true;
	}
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		if (!state.enabled) {
			return false;
		}
		if (!Is_primary(event.gaxis.which)) {
			return true;
		}
		const bool walking
				= event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY;
		if (walking && std::abs(event.gaxis.value) > SDL_JOYSTICK_AXIS_MAX / 4) {
			Set_last_input(true);
		}
		// The left stick walks (Handle_event), but picks in a ring while one
		// is open; the rest is polled.
		return !walking || Controller_ring_is_open();
	}
	default:
		return false;
	}
}

void Controller_screen_event(SDL_Event& event, bool menu) {
	if (event.type == SDL_EVENT_KEY_DOWN) {
		if (event.key.which != pad_keyboard_id) {
			Set_last_input(false);    // The keyboard, in these screens too.
		}
		return;
	}
	bool from_stick = false;
	// The left stick: pushed past half way, a press of the D-pad that way
	// (back under a third lets go).
	if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && state.enabled && Is_primary(event.gaxis.which)
		&& (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)) {
		const bool    across = event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX;
		int&          dir    = state.stick_dir[across ? 0 : 1];
		const int     was    = dir;
		const int     value  = event.gaxis.value;
		constexpr int on     = SDL_JOYSTICK_AXIS_MAX * 6 / 10;
		constexpr int off    = SDL_JOYSTICK_AXIS_MAX * 3 / 10;
		if (value > on) {
			dir = 1;
		} else if (value < -on) {
			dir = -1;
		} else if (std::abs(value) < off) {
			dir = 0;
		}
		if (dir == 0 && was != 0 && state.repeat_from_stick) {
			state.repeat_key = SDLK_UNKNOWN;    // Let go: no more repeats.
		}
		if (dir == was || dir == 0) {
			return;
		}
		from_stick = true;
		SDL_Event press{};
		press.type              = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
		press.gbutton.timestamp = event.gaxis.timestamp;
		press.gbutton.which     = event.gaxis.which;
		press.gbutton.button    = static_cast<Uint8>(
				   across ? (dir < 0 ? SDL_GAMEPAD_BUTTON_DPAD_LEFT : SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
							 : (dir < 0 ? SDL_GAMEPAD_BUTTON_DPAD_UP : SDL_GAMEPAD_BUTTON_DPAD_DOWN));
		press.gbutton.down = true;
		event              = press;
	}
	if (!state.enabled || (event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.type != SDL_EVENT_GAMEPAD_BUTTON_UP)
		|| !Is_primary(event.gbutton.which)) {
		return;
	}
	const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
	if (down) {
		Set_last_input(true);
	}
	SDL_Keycode key = SDLK_UNKNOWN;
	SDL_Keymod  mod = SDL_KMOD_NONE;
	switch (Mapped_input(event.gbutton.button)) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		key = menu ? SDLK_RETURN : SDLK_SPACE;
		break;
	case SDL_GAMEPAD_BUTTON_EAST:
		key = SDLK_ESCAPE;    // Back, close, cancel (the crosshair, the map; "bye").
		break;
	case SDL_GAMEPAD_BUTTON_START:
		key = SDLK_ESCAPE;
		break;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		key = SDLK_LEFT;
		break;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		key = SDLK_RIGHT;
		break;
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: {
		const bool dpad = event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP;
		key = menu ? (dpad ? SDLK_UP : SDLK_PAGEUP) : SDLK_E;
		mod = menu ? SDL_KMOD_NONE : SDL_KMOD_LSHIFT;
		break;
	}
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: {
		const bool dpad = event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN;
		key = menu ? (dpad ? SDLK_DOWN : SDLK_PAGEDOWN) : SDLK_E;
		break;
	}
	default:
		break;
	}
	if (key == SDLK_UNKNOWN) {
		return;
	}
	// A held direction repeats (Controller_tick), till it's let go.
	const int button = event.gbutton.button;
	const bool way   = button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN
					|| button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
	if (way && down) {
		state.repeat_key        = key;
		state.repeat_mod        = mod;
		state.repeat_button     = button;
		state.repeat_from_stick = from_stick;
		state.repeat_next       = SDL_GetTicks() + repeat_first_ms;
	} else if (way && button == state.repeat_button && !state.repeat_from_stick) {
		state.repeat_key = SDLK_UNKNOWN;
	}
	SDL_Window* window = Game_screen_window();
	SDL_Event   key_event{};
	key_event.type          = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	key_event.key.timestamp = event.gbutton.timestamp;
	key_event.key.windowID  = window ? SDL_GetWindowID(window) : 0;
	key_event.key.key       = key;
	key_event.key.scancode  = SDL_GetScancodeFromKey(key, nullptr);
	key_event.key.mod       = mod;
	key_event.key.down      = down;
	event                   = key_event;
}

void Controller_frame() {
	state.repeat_key = SDLK_UNKNOWN;    // The world: no menu repeats (they'd walk).
	SDL_Gamepad* pad = Primary_pad();
	if (!state.enabled || !pad) {
		return;
	}
	const Uint64 now = SDL_GetTicks();
	for (int input = 0; input < SDL_GAMEPAD_BUTTON_COUNT; ++input) {
		if (!state.pending[input] || state.hold_fired[input]) {
			continue;
		}
		if (!SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(input))) {
			state.pending[input] = false;    // Let go while another screen had the events.
			continue;
		}
		if (now - state.down_at[input] >= hold_ms) {
			state.hold_fired[input] = true;
			if (state.runner) {
				state.runner(state.hold_map[Mapped_input(input)], true);
			}
		}
	}
}

bool Controller_left_stick_centred() {
	SDL_Gamepad* pad = Primary_pad();
	if (!pad) {
		return true;
	}
	constexpr int dead = SDL_JOYSTICK_AXIS_MAX / 4;
	return std::abs(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX)) <= dead
		   && std::abs(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY)) <= dead;
}

bool Controller_last_input_is_pad() {
	return state.enabled && state.last_pad && Primary_pad();
}

static bool Modifier_held(Controller_action action) {
	SDL_Gamepad* pad = Primary_pad();
	if (!state.enabled || !pad) {
		return false;
	}
	for (int which = 0; which < 2; ++which) {
		if (state.trigger[which] == Trigger_use::modifier
			&& Binding_of(which == 0 ? left_trigger : right_trigger).action == action) {
			return true;
		}
	}
	for (int input = 0; input < SDL_GAMEPAD_BUTTON_COUNT; ++input) {
		if (Binding_of(input).action == action && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(input))) {
			return true;
		}
	}
	return false;
}

bool Controller_shift_held() {
	return Modifier_held(Controller_action::shift);
}

bool Controller_alt_held() {
	return Modifier_held(Controller_action::alt);
}

bool Controller_pointer_showing() {
	return state.showing;
}

void Controller_aim_pointer(float wx, float wy) {
	SDL_Window* window = Game_screen_window();
	if (!window) {
		return;
	}
	Warp_pointer(window, wx, wy);
	state.pointer_activity = SDL_GetTicks();
	state.centre_next      = false;
	if (!state.showing) {
		state.showing = true;
		Update_cursor();
	}
}

std::string Controller_label(Controller_action action, const char* key_name) {
	for (const auto& entry : input_names) {
		const Controller_binding& binding = Binding_of(entry.input);
		if (binding.action == action
			&& (action != Controller_action::key_binding || (key_name && binding.name == key_name))) {
			return Input_label(entry.input);
		}
	}
	return {};
}

// The Controller page (the game menu): the live map, a page of buttons at a
// time; left / right (or LB / RB, or the arrows) turn the pages.
namespace {
using Page_row = std::pair<std::string, std::string>;    // The button, what it does.

std::string Describe(const Controller_binding& binding) {
	switch (binding.action) {
	case Controller_action::shift:
		return "Shift; pointer up: left click, drag";
	case Controller_action::alt:
		return "Alt; pointer up: right click";
	case Controller_action::target_person:
		return "Next person or creature";
	case Controller_action::target_thing:
		return "Next thing";
	case Controller_action::target_act:
		return "Act on the pick";
	case Controller_action::toggle_run:
		return "Run or walk";
	case Controller_action::next_character:
		return "Next character";
	case Controller_action::backpack:
		return "Next backpack";
	case Controller_action::player_list:
		return "Player list (co-op, held)";
	case Controller_action::skills_ring:
		return "Skills ring (held)";
	case Controller_action::utilities_ring:
		return "Utilities ring (held)";
	case Controller_action::chat_phrases:
		return "Chat phrases (coming)";
	case Controller_action::close_window:
		return "Close the newest window";
	case Controller_action::hold_follow:
		return "The party holds or follows";
	case Controller_action::sort_window:
		return "Sort the newest window";
	case Controller_action::item_menu:
		return "Item menu: give, put in, drop...";
	case Controller_action::key_binding:
		if (binding.name == "TOGGLE_COMBAT") {
			return "Combat on or off";
		}
		if (binding.name == "INVENTORY") {
			return "Paperdoll";
		}
		if (binding.name == "STATS") {
			return "Stats";
		}
		if (binding.name == "CLOSE_OR_MENU") {
			return "Clear the pick, close, menu";
		}
		{
			std::string text = binding.name;    // Another key action: its name.
			std::replace(text.begin(), text.end(), '_', ' ');
			return text;
		}
	default:
		return {};
	}
}

// Every page: the world's buttons (from the map), then the menus'.
std::vector<std::vector<Page_row>> Page_rows() {
	constexpr int        per_page = 10;
	std::vector<Page_row> world{{"Left stick", "Walk"}, {"Right stick", "Pointer"}};
	static const int order[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST,
			SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
			left_trigger, right_trigger, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
			SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
			SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START,
			SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
			SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_MISC1, SDL_GAMEPAD_BUTTON_TOUCHPAD};
	for (const int input : order) {
		const std::string label = Input_label(input);
		const std::string tap   = Describe(Binding_of(input));
		if (!tap.empty()) {
			world.emplace_back(label, tap);
		}
		const std::string held = Describe(state.hold_map[Mapped_input(input)]);
		if (!held.empty()) {
			world.emplace_back(label + ", held", held);
		}
	}
	std::vector<std::vector<Page_row>> pages;
	for (size_t at = 0; at < world.size(); at += per_page) {
		pages.emplace_back(world.begin() + at, world.begin() + std::min(world.size(), at + per_page));
	}
	const std::string south  = Input_label(Mapped_input(SDL_GAMEPAD_BUTTON_SOUTH));
	const std::string east   = Input_label(Mapped_input(SDL_GAMEPAD_BUTTON_EAST));
	const std::string start  = Input_label(SDL_GAMEPAD_BUTTON_START);
	const std::string lb     = Input_label(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
	const std::string rb     = Input_label(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
	pages.push_back({
			{"Menus:", ""},
			{"Up / Down, left stick", "Move the highlight"},
			{south, "Choose"},
			{east + " or " + start, "Back"},
			{"Left / Right", "Change a setting"},
			{lb + " / " + rb, "By ten; Load/Save buttons"},
			{"Conversations:", ""},
			{lb + " / " + rb + ", Up / Down", "Pick a reply"},
			{south, "Say it; skip the intro"},
			{start, "Bye"},
	});
	return pages;
}

class Controller_buttons_gump : public Modal_gump {
	using Button = CallbackTextButton<Controller_buttons_gump>;

	std::vector<std::vector<Page_row>>   pages;
	int                                  page = 0;
	int                                  label_w = 0;
	std::array<std::unique_ptr<Gump_button>, 3> buttons;    // Previous, next, OK.

	void turn(int dir) {
		page = (page + dir + static_cast<int>(pages.size())) % static_cast<int>(pages.size());
		paint();
	}

	void previous() {
		turn(-1);
	}

	void next() {
		turn(1);
	}

public:
	Controller_buttons_gump() : Modal_gump(nullptr, -1), pages(Page_rows()) {
		int desc_w = 0;
		for (const auto& rows : pages) {
			for (const auto& [label, desc] : rows) {
				label_w = std::max(label_w, font->get_text_width(label.c_str()));
				desc_w  = std::max(desc_w, font->get_text_width(desc.c_str()));
			}
		}
		const int width = std::max(160, label_margin * 2 + label_w + 10 + desc_w);
		SetProceduralBackground(TileRect(0, 0, width, yForRow(13)), -1);
		const int margin = label_margin;    // (Passed on by reference: a copy.)
		buttons[0] = std::make_unique<Button>(this, &Controller_buttons_gump::previous, "<", margin, yForRow(12), 20);
		buttons[1] = std::make_unique<Button>(this, &Controller_buttons_gump::next, ">", margin + 24, yForRow(12), 20);
		buttons[2] = std::make_unique<Button>(
				this, &Controller_buttons_gump::close, GumpStrings::OK(), width - 50 - margin, yForRow(12), 50);
	}

	void paint() override {
		Modal_gump::paint();
		for (auto& button : buttons) {
			button->paint();
		}
		Image_buffer8*    ib    = gwin->get_win()->get_ib8();
		const std::string title = "Controller  (page " + std::to_string(page + 1) + " of "
								  + std::to_string(pages.size()) + ")";
		font->paint_text(ib, title.c_str(), x + label_margin, y + yForRow(0) + 1);
		int row = 1;
		for (const auto& [label, desc] : pages[page]) {
			font->paint_text(ib, label.c_str(), x + label_margin, y + yForRow(row) + 1);
			font->paint_text(ib, desc.c_str(), x + label_margin + label_w + 10, y + yForRow(row) + 1);
			++row;
		}
		gwin->set_painted();
	}

	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override {
		switch (chr) {
		case SDLK_LEFT:
		case SDLK_PAGEUP:
			turn(-1);
			return true;
		case SDLK_RIGHT:
		case SDLK_PAGEDOWN:
			turn(1);
			return true;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			close();
			return true;
		default:
			return Modal_gump::key_down(chr, unicode);
		}
	}

	Gump_button* on_button(int mx, int my) override {
		for (auto& button : buttons) {
			if (Gump_button* found = button->on_button(mx, my)) {
				return found;
			}
		}
		return Modal_gump::on_button(mx, my);
	}
};
}    // namespace

void Controller_buttons_page() {
	Controller_buttons_gump page;
	Game_window::get_instance()->get_gump_man()->do_modal_gump(&page, Mouse::hand);
}

// The radial menus.
namespace {
struct Ring_state {
	bool                                                open = false;
	std::string                                         title;
	std::vector<Controller_ring_entry>                  entries;
	std::function<std::vector<Controller_ring_entry>()> refresh;
	int                                                 highlight = -1;
	int                                                 layer     = -1;
	std::string                                         painted;    // What the layer shows now.
	int                                                 dest[4]   = {0, 0, 0, 0};
};

Ring_state ring;

constexpr int ring_radius = 44;                                   // Centre to a plate's centre.
constexpr int ring_plate  = 28;                                   // A plate's size (the art: 28x28).
constexpr int ring_size   = 2 * ring_radius + ring_plate + 4;     // The ring, square.
constexpr int ring_w      = ring_size + 200;                      // The layer: room for text entries.

// An entry's angle, clockwise from the top (radians): up to four make an
// arc over the title, 60 degrees apart (three: up-left, up, up-right);
// more go all round.
float Ring_angle(int index, int count) {
	constexpr float two_pi = 6.2831853f;
	if (count <= 4) {
		constexpr float step = two_pi / 6;
		return (index - (count - 1) / 2.f) * step;
	}
	return two_pi * index / count;
}

// Which entry the left stick points at (the nearest, within half the gap
// to the next), or -1 near the centre or away from the arc.
int Ring_pick() {
	SDL_Gamepad* pad   = Primary_pad();
	const int    count = static_cast<int>(ring.entries.size());
	if (!pad || count == 0) {
		return -1;
	}
	const float sx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
	const float sy = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
	if (std::sqrt(sx * sx + sy * sy) < 0.5f) {
		return -1;
	}
	constexpr float two_pi = 6.2831853f;
	const float     angle  = std::atan2(sx, -sy);    // 0 at the top, clockwise.
	const float     reach  = count <= 4 ? two_pi / 12 + 0.35f : two_pi / count / 2;
	int             best   = -1;
	float           least  = reach;
	for (int i = 0; i < count; ++i) {
		// The difference wrapped to -pi..pi (left of the top is negative).
		const float off = std::fabs(std::remainder(angle - Ring_angle(i, count), two_pi));
		if (off <= least) {
			least = off;
			best  = i;
		}
	}
	return best;
}

// 'across': the sideways radius (wider for text entries, so their boxes
// don't meet).
void Ring_plate_centre(int index, int count, int& cx, int& cy, int across = ring_radius) {
	const float angle = Ring_angle(index, count);
	cx                = ring_w / 2 + static_cast<int>(std::lround(across * std::sin(angle)));
	cy                = ring_size / 2 - static_cast<int>(std::lround(ring_radius * std::cos(angle)));
}

// A placeholder plate (until the art comes): a dark disc with a black rim,
// gold and lighter when highlighted.
void Ring_paint_plate(Image_buffer8* buf, int cx, int cy, bool lit) {
	Palette*            pal  = Game_window::get_instance()->get_pal();
	const unsigned char body = static_cast<unsigned char>(lit ? pal->find_color(30, 22, 12) : pal->find_color(14, 10, 6));
	const unsigned char rim  = static_cast<unsigned char>(lit ? pal->find_color(63, 50, 16) : pal->find_color(34, 26, 14));
	const int           r    = ring_plate / 2;
	for (int dy = -r; dy < r; ++dy) {
		for (int dx = -r; dx < r; ++dx) {
			const int d = (2 * dx + 1) * (2 * dx + 1) + (2 * dy + 1) * (2 * dy + 1);    // (2 * distance)^2
			if (d < (2 * r - 4) * (2 * r - 4)) {
				buf->put_pixel8(body, cx + dx, cy + dy);
			} else if (d < (2 * r - 1) * (2 * r - 1)) {
				buf->put_pixel8(lit ? rim : body, cx + dx, cy + dy);
			} else if (d < (2 * r) * (2 * r) + 2 * r) {
				buf->put_pixel8(lit ? rim : 0, cx + dx, cy + dy);
			}
		}
	}
}

// Text with a black outline (the hint line's way), its top left at (x, y),
// word by word: this font draws a space as a solid black box.
void Ring_text(Image_buffer8* buf, const std::shared_ptr<Font>& font, const std::string& text, int x, int y) {
	unsigned char black[256];
	std::fill(std::begin(black), std::end(black), static_cast<unsigned char>(0));
	const int space = font->get_text_width(" ");
	for (int pass = 0; pass < 2; ++pass) {
		int wx = x;
		for (size_t start = 0; start <= text.size();) {
			const size_t      end  = std::min(text.find(' ', start), text.size());
			const std::string word = text.substr(start, end - start);
			if (!word.empty()) {
				if (pass == 0) {
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							if (dx || dy) {
								font->draw_text(buf, wx + dx, y + dy, word.c_str(), black);
							}
						}
					}
				} else {
					font->draw_text(buf, wx, y, word.c_str());
				}
				wx += font->get_text_width(word.c_str());
			}
			wx += space;
			start = end + 1;
		}
	}
}

// A plate nobody can use: everything on it darkened, as Exult greys things.
void Ring_dim_plate(Image_buffer8* buf, int cx, int cy) {
	const auto& dim = Shape_manager::get_instance()->get_xform(8);
	const int   r   = ring_plate / 2;
	for (int dy = -r; dy <= r; ++dy) {
		for (int dx = -r; dx <= r; ++dx) {
			if (dx * dx + dy * dy <= r * r) {
				const unsigned char pix = buf->get_pixel8(cx + dx, cy + dy);
				if (pix != 255) {
					buf->put_pixel8(dim[pix], cx + dx, cy + dy);
				}
			}
		}
	}
}

// A text entry: its name in a dark box, the rim gold when highlighted,
// darker when it can't be used.
void Ring_paint_label(
		Image_buffer8* buf, const std::shared_ptr<Font>& font, const std::string& text, int cx, int cy, bool lit,
		bool usable) {
	Palette*            pal  = Game_window::get_instance()->get_pal();
	const unsigned char body = static_cast<unsigned char>(lit ? pal->find_color(30, 22, 12) : pal->find_color(14, 10, 6));
	const unsigned char rim  = static_cast<unsigned char>(lit ? pal->find_color(63, 50, 16) : 0);
	const int           w    = font->get_text_width(text.c_str()) + 10;
	const int           h    = font->get_text_height() + 6;
	const int           x    = cx - w / 2;
	const int           y    = cy - h / 2;
	buf->draw_box(x, y, w, h, 1, body, rim);
	Ring_text(buf, font, text, x + 5, y + 3);
	if (!usable) {
		const auto& dim = Shape_manager::get_instance()->get_xform(8);
		for (int py = y; py < y + h; ++py) {
			for (int px = x; px < x + w; ++px) {
				const unsigned char pix = buf->get_pixel8(px, py);
				if (pix != 255) {
					buf->put_pixel8(dim[pix], px, py);
				}
			}
		}
	}
}

void Ring_hide() {
	Game_window* gwin = Game_window::get_instance();
	if (ring.layer >= 0 && gwin->get_win()->layer_is_visible(ring.layer)) {
		gwin->layer_set_visible(ring.layer, false);
		gwin->set_painted();
	}
}
}    // namespace

void Controller_ring_open(
		const std::string& title, std::vector<Controller_ring_entry> entries,
		std::function<std::vector<Controller_ring_entry>()> refresh) {
	ring.open      = true;
	ring.title     = title;
	ring.entries   = std::move(entries);
	ring.refresh   = std::move(refresh);
	ring.highlight = -1;
	ring.painted.clear();
}

void Controller_ring_close() {
	if (!ring.open) {
		return;
	}
	ring.open = false;
	Ring_hide();
	const int index = ring.highlight;
	if (index >= 0 && index < static_cast<int>(ring.entries.size()) && ring.entries[index].use) {
		std::cout << "Controller: " << ring.title << ": " << ring.entries[index].name << std::endl;
		const auto use = ring.entries[index].use;    // It may open another ring.
		use();    // Unavailable ones say why ("No watch").
	}
}

bool Controller_ring_is_open() {
	return ring.open;
}

void Controller_ring_paint(int sx, int sy) {
	Game_window*  gwin = Game_window::get_instance();
	Image_window* iwin = gwin->get_win();
	auto          font = fontManager.get_font("MENU_FONT");
	if (!ring.open || !font) {
		Ring_hide();
		return;
	}
	if (ring.refresh) {
		ring.entries = ring.refresh();
	}
	ring.highlight = Ring_pick();
	// Paint again only when what it shows changed.
	std::string shows = std::to_string(ring.highlight);
	for (const auto& entry : ring.entries) {
		shows += '|' + entry.name + '|' + entry.corner + (entry.usable ? "+" : "-");
	}
	if (ring.layer < 0) {
		constexpr int text_layer_z = (1 << 18) + (1 << 16);    // As the hint line's.
		ring.layer                 = gwin->create_layer("Radial menu", ring_w, ring_size, 255, 0, text_layer_z);
		if (ring.layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(ring.layer, Image_window::UiLayerTextEffects);
		ring.painted.clear();
	}
	if (shows != ring.painted) {
		Image_buffer8* buf = gwin->get_layer_ibuf(ring.layer);
		if (!buf) {
			return;
		}
		buf->clear_clip();
		buf->fill8(255);    // Transparent.
		Image_buffer8* prev  = gwin->push_render_target(buf);
		const int      count = static_cast<int>(ring.entries.size());
		// Text entries: spread sideways by the widest box, so neighbours on
		// the same row (the arc's left and right) keep a gap between them.
		int widest = 0;
		for (const auto& entry : ring.entries) {
			if (entry.look == Controller_ring_look::text) {
				widest = std::max(widest, font->get_text_width(entry.name.c_str()) + 10);
			}
		}
		const int max_across = ring_w / 2 - widest / 2 - 2;
		const int across     = std::clamp(widest * 2 / 3 + 8, ring_radius, std::max(ring_radius, max_across));
		for (int i = 0; i < count; ++i) {
			const auto& entry = ring.entries[i];
			int         cx    = 0;
			int         cy    = 0;
			Ring_plate_centre(i, count, cx, cy, across);
			const bool lit = i == ring.highlight;
			if (entry.look == Controller_ring_look::text) {
				Ring_paint_label(buf, font, entry.name, cx, cy, lit, entry.usable);
				continue;
			}
			if (entry.look == Controller_ring_look::plate) {
				Ring_paint_plate(buf, cx, cy, lit);
			}
			if (entry.paint) {
				// The picture, kept inside its plate.
				buf->set_clip(cx - ring_plate / 2 + 2, cy - ring_plate / 2 + 2, ring_plate - 4, ring_plate - 4);
				entry.paint(buf, cx, cy);
				buf->clear_clip();
			}
			if (!entry.usable) {
				Ring_dim_plate(buf, cx, cy);
			}
			if (entry.look == Controller_ring_look::bare && lit) {
				// No plate to light: a gold frame round the picture.
				Palette*            pal  = Game_window::get_instance()->get_pal();
				const unsigned char gold = static_cast<unsigned char>(pal->find_color(63, 50, 16));
				buf->draw_box(cx - ring_plate / 2, cy - ring_plate / 2, ring_plate, ring_plate, 1, 0xFF, gold);
			}
			if (!entry.corner.empty()) {
				Ring_text(buf, font, entry.corner, cx + ring_plate / 2 - font->get_text_width(entry.corner.c_str()) - 1,
						  cy + ring_plate / 2 - font->get_text_height());
			}
		}
		// The highlighted entry's name in the middle (else the ring's title;
		// a text entry already shows its name).
		const bool        named = ring.highlight >= 0 && ring.highlight < count
							  && ring.entries[ring.highlight].look != Controller_ring_look::text;
		const std::string name  = named ? ring.entries[ring.highlight].name : ring.title;
		Ring_text(buf, font, name, (ring_w - font->get_text_width(name.c_str())) / 2,
				  (ring_size - font->get_text_height()) / 2);
		gwin->pop_render_target(prev);
		gwin->layer_set_dirty(ring.layer);
		ring.painted = shows;
	}
	const float f       = iwin->get_ui_scale_factor(Image_window::UiLayerTextEffects);
	const int   dw      = static_cast<int>(ring_w * f);
	const int   dh      = static_cast<int>(ring_size * f);
	const int   dest[4] = {sx - dw / 2, sy - dh / 2, dw, dh};
	if (!std::equal(dest, dest + 4, ring.dest) || !iwin->layer_is_visible(ring.layer)) {
		std::copy(dest, dest + 4, ring.dest);
		gwin->layer_set_dest(ring.layer, dest[0], dest[1], dest[2], dest[3]);
		gwin->layer_set_visible(ring.layer, true);
		gwin->set_painted();
	}
}

// A note under an open window.
void Controller_paint_note(const std::string& text, int sx, int sy) {
	static int         layer = -1;
	static std::string painted;
	static int         w       = 0;
	static int         h       = 0;
	static int         shown[4] = {0, 0, 0, 0};
	Game_window*       gwin    = Game_window::get_instance();
	Image_window*      iwin    = gwin->get_win();
	auto               font    = fontManager.get_font("MENU_FONT");
	if (text.empty() || !font) {
		if (layer >= 0 && iwin->layer_is_visible(layer)) {
			gwin->layer_set_visible(layer, false);
			gwin->set_painted();
		}
		return;
	}
	if (layer < 0 || text != painted) {
		if (layer >= 0) {
			gwin->destroy_layer(layer);
		}
		w                          = font->get_text_width(text.c_str()) + 4;
		h                          = font->get_text_height() + 4;
		constexpr int text_layer_z = (1 << 18) + (1 << 16);
		layer                      = gwin->create_layer("Window note", w, h, 255, 0, text_layer_z);
		if (layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(layer, Image_window::UiLayerTextEffects);
		Image_buffer8* buf = gwin->get_layer_ibuf(layer);
		if (!buf) {
			return;
		}
		buf->clear_clip();
		buf->fill8(255);
		Image_buffer8* prev = gwin->push_render_target(buf);
		Ring_text(buf, font, text, 2, 2);
		gwin->pop_render_target(prev);
		gwin->layer_set_dirty(layer);
		painted  = text;
		shown[2] = -1;    // Place it again.
	}
	constexpr float smaller = 0.8f;    // As the hint line.
	const float     f       = iwin->get_ui_scale_factor(Image_window::UiLayerTextEffects) * smaller;
	const int       dw      = static_cast<int>(w * f);
	const int       dh      = static_cast<int>(h * f);
	const int       dest[4] = {sx - dw / 2, sy, dw, dh};
	if (!std::equal(dest, dest + 4, shown) || !iwin->layer_is_visible(layer)) {
		std::copy(dest, dest + 4, shown);
		gwin->layer_set_dest(layer, dest[0], dest[1], dest[2], dest[3]);
		gwin->layer_set_visible(layer, true);
		gwin->set_painted();
	}
}

std::string Controller_button_name(int button) {
	return Input_label(button);
}

// A short list to choose from.
namespace {
class Choice_button : public Text_button {
	std::function<void()> on_choose;

public:
	Choice_button(Gump_Base* parent, const std::string& text, int x, int y, int w, std::function<void()> choose)
			: Text_button(parent, text, x, y, w, 11), on_choose(std::move(choose)) {}

	bool activate(MouseButton button) override {
		if (button != MouseButton::Left) {
			return false;
		}
		on_choose();
		return true;
	}
};

class Choice_gump : public Modal_gump {
	std::string                               title;
	std::vector<std::unique_ptr<Gump_button>> buttons;
	int                                       chosen = -1;

public:
	Choice_gump(std::string heading, const std::vector<std::string>& entries)
			: Modal_gump(nullptr, -1), title(std::move(heading)) {
		int widest = font->get_text_width(title.c_str());
		for (const auto& entry : entries) {
			widest = std::max(widest, font->get_text_width(entry.c_str()) + 12);
		}
		const int width = std::max(100, widest + 2 * label_margin);
		const int count = static_cast<int>(entries.size());
		SetProceduralBackground(TileRect(0, 0, width, yForRow(count + 1) + 2), -1);
		const int margin = label_margin;    // (Passed on by reference: a copy.)
		for (int i = 0; i < count; ++i) {
			buttons.push_back(std::make_unique<Choice_button>(
					this, entries[i], margin, yForRow(i + 1), width - 2 * margin, [this, i]() {
						chosen = i;
						done   = true;
					}));
		}
		focus_first();
	}

	int get_chosen() const {
		return chosen;
	}

	void paint() override {
		Modal_gump::paint();
		for (auto& button : buttons) {
			button->paint();
		}
		font->paint_text(gwin->get_win()->get_ib8(), title.c_str(), x + label_margin, y + yForRow(0) + 1);
		paint_focus();
		gwin->set_painted();
	}

	void focus_widgets(std::vector<Gump_widget*>& list) override {
		add_focus(list, buttons);
	}

	Gump_button* on_button(int mx, int my) override {
		for (auto& button : buttons) {
			if (Gump_button* found = button->on_button(mx, my)) {
				return found;
			}
		}
		return Modal_gump::on_button(mx, my);
	}
};
}    // namespace

int Controller_choose(const std::string& title, const std::vector<std::string>& entries) {
	if (entries.empty()) {
		return -1;
	}
	Choice_gump choice(title, entries);
	Game_window::get_instance()->get_gump_man()->do_modal_gump(&choice, Mouse::hand);
	return choice.get_chosen();
}
