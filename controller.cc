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
#include "Modal_gump.h"
#include "gump_utils.h"
#include "ibuf8.h"
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
		{SDL_GAMEPAD_BUTTON_WEST, "TOGGLE_COMBAT"},
		{SDL_GAMEPAD_BUTTON_EAST, "SKILLS_RING"},
		{SDL_GAMEPAD_BUTTON_NORTH, "UTILITIES_RING"},
		{SDL_GAMEPAD_BUTTON_DPAD_UP, "NEXT_CHARACTER"},
		{SDL_GAMEPAD_BUTTON_DPAD_LEFT, "INVENTORY"},    // The paperdoll (I).
		{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "BACKPACK"},
		{SDL_GAMEPAD_BUTTON_DPAD_DOWN, "STATS"},
		{SDL_GAMEPAD_BUTTON_BACK, "PLAYER_LIST"},
		{SDL_GAMEPAD_BUTTON_START, "CLOSE_OR_MENU"},
		{SDL_GAMEPAD_BUTTON_RIGHT_STICK, "NONE"},    // Suggested: CHAT_PHRASES.
}};

struct Action_name {
	Controller_action action;
	const char*       name;
};

constexpr std::array<Action_name, 13> action_names{{
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
}};

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
	std::vector<SDL_JoystickID>                 pads;    // Open, oldest first; the first plays.

	Controller_runner   runner   = nullptr;
	Controller_switched switched = nullptr;

	bool last_pad = false;    // The last input was the pad's.

	// The pointer, in window coordinates.
	bool   showing     = false;
	bool   centre_next = true;    // Start in the centre when it next shows.
	float  px = 0, py = 0;
	float  seen_x = -1, seen_y = -1;    // Where the mouse was last seen (ours or real).
	Uint64 last_activity = 0;
	Uint64 last_tick     = 0;

	std::array<Trigger_use, 2> trigger{Trigger_use::off, Trigger_use::off};
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
			use = Trigger_use::mouse;    // LT the left button, RT the right.
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
	} else if (state.showing && now - state.last_activity > static_cast<Uint64>(state.hide_ms)) {
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
	for (const auto& [input, action] : default_map) {
		state.map[input] = Parse_binding(action);
	}
	for (const auto& entry : input_names) {
		const std::string key = std::string("config/gameplay/controller/buttons/") + entry.name;
		if (config->key_exists(key)) {
			std::string text;
			config->value(key, text, "");
			state.map[entry.input] = Parse_binding(text);
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

void Controller_device_added(SDL_JoystickID id) {
	if (std::find(state.pads.begin(), state.pads.end(), id) != state.pads.end()) {
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

bool Controller_event(const SDL_Event& event) {
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN:
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
		const Controller_binding& binding = Binding_of(event.gbutton.button);
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
		return !walking;    // The left stick walks (Handle_event); the rest is polled.
	}
	default:
		return false;
	}
}

void Controller_screen_event(SDL_Event& event, bool menu) {
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
		key = menu ? SDLK_ESCAPE : SDLK_UNKNOWN;
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
