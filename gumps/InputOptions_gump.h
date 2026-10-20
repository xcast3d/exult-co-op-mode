/*
Copyright (C) 2001-2024 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef INPUTOPTIONS_GUMP_H
#define INPUTOPTIONS_GUMP_H

#include "Modal_gump.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;
class Font;

class InputOptions_gump : public Modal_gump {
private:
	int  doubleclick;
	int  rightclick_close;
	int  right_pathfind;
	bool scroll_mouse;
	int  mouse3rd;
	int  fastmouse;
	bool item_menu;
	int  dpad_location;
	bool touch_pathfind;
	int  wasd_movement;

	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_help,
		id_cancel,
		id_first_setting,
		id_doubleclick = id_first_setting,
		id_rightclick_close,
		id_right_pathfind,
		id_scroll_mouse,
		id_mouse3rd,
		id_fastmouse,
		id_item_menu,
		id_dpad_location,
		id_touch_pathfind,
		id_wasd_movement,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

public:
	InputOptions_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	void build_buttons();

	void load_settings();
	void save_settings();
	void cancel();
	void help();

	void toggle_doubleclick(int state) {
		doubleclick = state;
	}

	void toggle_rightclick_close(int state) {
		rightclick_close = state;
	}

	void toggle_right_pathfind(int state) {
		right_pathfind = state;
	}

	void toggle_scroll_mouse(int state) {
		scroll_mouse = state;
	}

	void toggle_mouse3rd(int state) {
		mouse3rd = state;
	}

	void toggle_fastmouse(int state) {
		fastmouse = state;
	}

	void toggle_item_menu(int state) {
		item_menu = state;
	}

	void toggle_dpad_location(int state) {
		dpad_location = state;
	}

	void toggle_touch_pathfind(int state) {
		touch_pathfind = state;
	}

	void toggle_wasd_movement(int state) {
		wasd_movement = state;
	}

	Gump_button* on_button(int mx, int my) override;
};

// Friend Mode: the Targeting System options page (Q, E and Space), next to
// Game Input in the game menu.  With "Enable" off the rest is hidden.  Kept
// in this file so no build file changes.
class TargetingOptions_gump : public Modal_gump {
private:
	enum setting_ids {
		set_enabled,
		set_smart,    // These two show even with targeting off.
		set_sort,
		set_party,
		set_loot,
		set_decor,
		set_furniture,
		set_interactive,
		set_levers,
		set_signs,
		set_count
	};

	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_cancel,
		id_first_setting,
		id_count = id_first_setting + set_count
	};

	std::array<int, set_count>                         settings{};
	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	static const char* label(int setting);

public:
	TargetingOptions_gump();

	void paint() override;
	void close() override;
	void cancel();

	void set(int setting, int state);

	template <int setting>
	void toggle(int state) {
		set(setting, state);
	}

	Gump_button* on_button(int mx, int my) override;

private:
	bool shown(int id) const;    // Hidden while targeting is off.
};
#endif
