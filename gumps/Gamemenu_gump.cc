/*
Copyright (C) 2001-2025 The Exult Team

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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Gamemenu_gump.h"

#include "AudioOptions_gump.h"
#include "File_gump.h"
#include "GameDisplayOptions_gump.h"
#include "GameEngineOptions_gump.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "InputOptions_gump.h"
#include "Newfile_gump.h"
#include "Text_button.h"
#include "UIOptions_gump.h"
#include "VideoOptions_gump.h"
#include "Yesno_gump.h"
#include "exult.h"
#include "controller.h"
#include "friendclass.h"
#include "friendbattle.h"
#include "gameclk.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"
#include "qotcupdates.h"

#include <algorithm>
#include <string>
#include <vector>

using std::string;

class Strings : public GumpStrings {
public:
	static auto LoadSaveGame() {
		return get_text_msg(0x5E0 - msg_file_start);
	}

	static auto VideoOptions() {
		return get_text_msg(0x5E1 - msg_file_start);
	}

	static auto AudioOptions() {
		return get_text_msg(0x5E2 - msg_file_start);
	}

	static auto UIOptions() {
		return get_text_msg(0x5E7 - msg_file_start);
	}

	static auto GameDisplay() {
		return get_text_msg(0x5E3 - msg_file_start);
	}

	static auto GameEngine() {
		return get_text_msg(0x5E4 - msg_file_start);
	}

	static auto GameInput() {
		return get_text_msg(0x5E5 - msg_file_start);
	}

	static auto Quit() {
		return get_text_msg(0x5E6 - msg_file_start);
	}
};

using Gamemenu_button = CallbackTextButton<Gamemenu_gump>;

namespace {
// Co-Op Mode in the game menu: hosting on or off, how guests join, what to
// give them, whether new players can join now, and the password (a box
// to click and type in, like the chat line).
class Coop_options_gump : public Modal_gump {
	enum button_ids { id_ok, id_hosting, id_type, id_joining, id_copy, id_count };

	// The settings, then one row each for the code, or the IP and port.
	enum rows { row_hosting, row_type, row_joining, row_password, row_details };

	static constexpr int width       = 190;
	static constexpr int field_x     = 92;    // The password box, from the left.
	static constexpr int field_width = 90;

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;
	std::vector<std::string>                           details;
	bool                                               editing = false;    // Typing a password.
	std::string                                        password;

	using Button = CallbackTextButton<Coop_options_gump>;
	using Toggle = CallbackToggleTextButton<Coop_options_gump>;

	bool shown(int id) const {
		const bool hosting = Friend_mode_coop_hosting_now();
		switch (id) {
		case id_joining:
			return hosting;
		case id_copy:
			return hosting && !details.empty();
		default:
			return true;
		}
	}

	void set_hosting(int state) {
		Friend_mode_set_coop_hosting(state == 1);
	}

	void set_type(int state) {
		Friend_mode_set_coop_by_lobby(state == 0);
	}

	void set_joining(int state) {
		Friend_mode_set_coop_joining_open(state == 1);
	}

	void copy() {
		Friend_mode_copy_coop_details();
	}

	int detail_rows() const {
		return std::max<int>(1, static_cast<int>(details.size()));
	}

	// OK and the background follow the number of detail rows.
	void layout() {
		const int ok_row = row_details + detail_rows() + 1;
		SetProceduralBackground(TileRect(0, 0, width, yForRow(ok_row + 1)), -1, false);
		buttons[id_ok]->set_pos((width - 50) / 2, yForRow(ok_row));
		buttons[id_copy]->set_pos(width - 40 - label_margin, yForRow(row_details));
	}

	TileRect password_box() const {
		return TileRect(x + field_x, y + yForRow(row_password) - 1, field_width, 11);
	}

	void start_editing() {
		editing  = true;
		password = Friend_mode_current_password();
		SDL_StartTextInput(gwin->get_win()->get_screen_window());
		paint();
	}

	void stop_editing(bool keep) {
		if (!editing) {
			return;
		}
		editing = false;
		SDL_StopTextInput(gwin->get_win()->get_screen_window());
		if (keep) {
			Friend_mode_set_password(password);
		}
		paint();
	}

public:
	Coop_options_gump() : Modal_gump(nullptr, -1) {
		details = Friend_mode_coop_detail_rows();
		SetProceduralBackground(TileRect(0, 0, width, yForRow(row_details + 3)), -1);
		buttons[id_ok]      = std::make_unique<Button>(this, &Coop_options_gump::close, GumpStrings::OK(), 0, 0, 50);
		buttons[id_hosting] = std::make_unique<Toggle>(this, &Coop_options_gump::set_hosting,
				std::vector<std::string>{"Off", "On"}, Friend_mode_coop_hosting_now() ? 1 : 0, field_x,
				yForRow(row_hosting), field_width);
		buttons[id_type]    = std::make_unique<Toggle>(this, &Coop_options_gump::set_type,
                std::vector<std::string>{"Lobby code", "IP address"}, Friend_mode_coop_by_lobby() ? 0 : 1, field_x,
                yForRow(row_type), field_width);
		buttons[id_joining] = std::make_unique<Toggle>(this, &Coop_options_gump::set_joining,
				std::vector<std::string>{"Closed", "Open"}, Friend_mode_coop_joining_open() ? 1 : 0, field_x,
				yForRow(row_joining), field_width);
		buttons[id_copy]    = std::make_unique<Button>(this, &Coop_options_gump::copy, "Copy", 0, 0, 40);
		layout();
		set_pos();    // Centred.
	}

	void close() override {
		stop_editing(true);
		done = true;
	}

	bool mouse_down(int mx, int my, MouseButton button) override {
		if (button == MouseButton::Left && password_box().has_point(mx, my)) {
			if (!editing) {
				start_editing();
			}
			return true;
		}
		if (editing) {
			stop_editing(true);    // A click elsewhere keeps what was typed.
		}
		return Modal_gump::mouse_down(mx, my, button);
	}

	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override {
		if (!editing) {
			return Modal_gump::key_down(chr, unicode);
		}
		if (chr == SDLK_RETURN || chr == SDLK_KP_ENTER) {
			stop_editing(true);
		} else if (chr == SDLK_ESCAPE) {
			stop_editing(false);
		} else if (chr == SDLK_BACKSPACE) {
			if (!password.empty()) {
				password.pop_back();
				paint();
			}
		} else if (unicode >= ' ' && unicode < 127 && password.size() < 32) {
			// With text input on, SDL sends a key and the text for each
			// character: take the text only.
			if (chr == SDLK_UNKNOWN || !SDL_TextInputActive(gwin->get_win()->get_screen_window())) {
				password.push_back(static_cast<char>(unicode));
				paint();
			}
		}
		return true;
	}

	Gump_button* on_button(int mx, int my) override {
		for (int id = 0; id < id_count; ++id) {
			auto& btn   = buttons[id];
			auto  found = btn && shown(id) ? btn->on_button(mx, my) : nullptr;
			if (found) {
				return found;
			}
		}
		return Modal_gump::on_button(mx, my);
	}

	void paint() override {
		Modal_gump::paint();
		for (int id = 0; id < id_count; ++id) {
			if (buttons[id] && shown(id)) {
				buttons[id]->paint();
			}
		}
		Image_window8* iwin  = gwin->get_win();
		auto           label = [&](int row, const std::string& text) {
            font->paint_text(iwin->get_ib8(), text.c_str(), x + label_margin, y + yForRow(row) + 1);
		};
		label(row_hosting, "Hosting:");
		label(row_type, "Type:");
		if (Friend_mode_coop_hosting_now()) {
			label(row_joining, "New players:");
		}
		label(row_password, "Password:");
		const TileRect box = password_box();
		iwin->get_ib8()->draw_box(box.x, box.y, box.w, box.h, 1, editing ? 138 : 142, editing ? 15 : 0);
		const std::string shown_password
				= editing ? std::string(password.size(), '*')
						  : (Friend_mode_has_password() ? std::string("Set (click)") : std::string("None (click)"));
		font->paint_text(iwin->get_ib8(), shown_password.c_str(), box.x + 3, box.y + 2);
		if (editing) {
			iwin->get_ib8()->fill8(
					15, 1, box.h - 4, box.x + 4 + font->get_text_width(shown_password.c_str()), box.y + 2);
		}
		for (std::size_t i = 0; i < details.size(); ++i) {
			label(row_details + static_cast<int>(i), details[i]);
		}
		paint_focus();    // Controller support: the highlight.
		gwin->set_painted();
	}

	// Controller support: the highlight.
	void focus_widgets(std::vector<Gump_widget*>& list) override {
		for (int id = 0; id < id_count; ++id) {
			if (buttons[id] && shown(id)) {
				list.push_back(buttons[id].get());
			}
		}
	}

	// The code (or the internet address) arrives a moment after opening,
	// and the rows change with the type: keep the page current.
	bool run() override {
		auto now = Friend_mode_coop_detail_rows();
		if (now != details) {
			details = std::move(now);
			layout();
			return true;    // Paint again.
		}
		return Modal_gump::run();
	}
};
}    // namespace

Gamemenu_gump::Gamemenu_gump() : Modal_gump(nullptr, -1) {
	createButtons();
}

void Gamemenu_gump::createButtons() {
	int y                      = 0;
	int margin                 = 4;
	int preferred_button_width = 108;
	// clear the buttons array
	for (auto& button : buttons) {
		button.reset();
	}

	// Resize the gump to default
	SetProceduralBackground(TileRect(0, 0, 100, yForRow(8)), -1);
	// Friend Mode guests are along for the ride: the host saves and loads.
	if (!gwin->is_in_exult_menu() && !Friend_mode_is_guest()) {
		buttons[id_load_save] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::loadsave, Strings::LoadSaveGame(), margin, yForRow(y++), preferred_button_width, 11);
	}
	buttons[id_video_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::video_options, Strings::VideoOptions(), margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_ui_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::ui_options, Strings::UIOptions(), margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_audio_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::audio_options, Strings::AudioOptions(), margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_game_engine_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::game_engine_options, Strings::GameEngine(), margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_game_display_options] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::game_display_options, Strings::GameDisplay(), margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_input] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::input_options, Strings::GameInput(), margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_targeting] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::targeting_options, "Target System", margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_class_system] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::class_options, "Class System", margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_combat_system] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::combat_options, "Combat System", margin, yForRow(y++), preferred_button_width, 11);
	buttons[id_controller] = std::make_unique<Gamemenu_button>(
			this, &Gamemenu_gump::controller_page, "Controller", margin, yForRow(y++), preferred_button_width, 11);

	// The host's page; a browser only joins (from the title menu).
#ifndef __EMSCRIPTEN__
	if (!gwin->is_in_exult_menu() && !Friend_mode_is_guest()) {
		buttons[id_open_coop] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::open_coop, "Co-Op Mode", margin, yForRow(y++), preferred_button_width, 11);
	}
#endif
#ifdef QOTC_UPDATES
	if (QOTC_updates_on()) {
		buttons[id_updates] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::check_for_updates, "Check Updates", margin, yForRow(y++),
				preferred_button_width, 11);
	}
#endif
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	if (!gwin->is_in_exult_menu()) {
		buttons[id_main_menu] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::quit_to_menu, "Main Menu", margin, yForRow(y++), preferred_button_width, 11);
		buttons[id_quit] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::quit_exult, Strings::Quit(), margin, yForRow(y++), preferred_button_width, 11);
	}
#endif

	// Resize to fit the combined height of the buttons
	SetProceduralBackground(TileRect(0, yForRow(0) - margin, 100, yForRow(y) + margin), -1, true);

	// Resize to fit width of buttons
	ResizeWidthToFitWidgets(tcb::span(buttons.data(), buttons.size()), margin);

	// Centre all the buttons
	for (auto& button : buttons) {
		if (button) {
			HorizontalArrangeWidgets(tcb::span(&button, 1), 0);
		}
	}
}

// Quit Exult, or (Main Menu) go back to the title menu: Exult starts
// again there once this game has shut down.
void Gamemenu_gump::quit(bool return_to_menu) {
#ifdef __EMSCRIPTEN__
	return_to_menu = true;    // In a browser, Quit goes back to the title menu too.
#endif
	if (!Yesno_gump::ask(return_to_menu ? "Return to the main menu?" : Strings::Doyoureallywanttoquit_())) {
		return;
	}
	if (return_to_menu) {
		Friend_mode_request_main_menu();
	}
	quitting_time = QUIT_TIME_YES;
	done          = true;
}

//+++++ implement actual functionality and option screens
void Gamemenu_gump::loadsave() {
	// File_gump *fileio = new File_gump();
	auto* fileio = new Newfile_gump();
	gumpman->do_modal_gump(fileio, Mouse::hand);
	if (fileio->restored_game()) {
		done = true;
	}
	delete fileio;
}

void Gamemenu_gump::video_options() {
	auto* vid_opts = new VideoOptions_gump();
	gumpman->do_modal_gump(vid_opts, Mouse::hand);

	// resolution could have changed, so recenter & repaint menu.
	set_pos();
	gwin->paint();
	gwin->show();
	delete vid_opts;
}

void Gamemenu_gump::audio_options() {
	auto* aud_opts = new AudioOptions_gump();
	gumpman->do_modal_gump(aud_opts, Mouse::hand);
	delete aud_opts;
}

void Gamemenu_gump::ui_options() {
	auto* ui_opts = new UIOptions_gump();
	gumpman->do_modal_gump(ui_opts, Mouse::hand);
	delete ui_opts;
}

void Gamemenu_gump::game_display_options() {
	auto* gp_opts = new GameDisplayOptions_gump();
	gumpman->do_modal_gump(gp_opts, Mouse::hand);
	delete gp_opts;
	// Language might have changed so recreate buttons
	createButtons();
}

void Gamemenu_gump::game_engine_options() {
	auto* cbt_opts = new GameEngineOptions_gump();
	gumpman->do_modal_gump(cbt_opts, Mouse::hand);
	delete cbt_opts;
}

void Gamemenu_gump::open_coop() {
	Coop_options_gump coop;
	gumpman->do_modal_gump(&coop, Mouse::hand);
}

// The updater's own window says what it found; the menu closes so it
// isn't left behind it.
void Gamemenu_gump::check_for_updates() {
#ifdef QOTC_UPDATES
	QOTC_check_for_updates();
#endif
	done = true;
}

void Gamemenu_gump::input_options() {
	InputOptions_gump input_opts;
	gumpman->do_modal_gump(&input_opts, Mouse::hand);
}

void Gamemenu_gump::targeting_options() {
	TargetingOptions_gump targeting_opts;
	gumpman->do_modal_gump(&targeting_opts, Mouse::hand);
}

void Gamemenu_gump::class_options() {
	Friend_class_options();
}

void Gamemenu_gump::combat_options() {
	Friend_combat_options();
}

void Gamemenu_gump::controller_page() {
	Controller_options_page();    // Its Buttons... shows the list.
}

void Gamemenu_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	paint_focus();    // Controller support: the highlighted button.
	gwin->set_painted();
}

void Gamemenu_gump::focus_widgets(std::vector<Gump_widget*>& list) {
	add_focus(list, buttons);
}

void Gamemenu_gump::do_exult_menu() {
	// Need to do a very small init of game data... palette, mouse, gumps
	Gamemenu_gump gmenu;
	// Does not return until gump can be deleted:
	Game_window::get_instance()->get_gump_man()->do_modal_gump(&gmenu, Mouse::hand);
}

Gump_button* Gamemenu_gump::on_button(int mx, int my) {
	for (auto& btn : buttons) {
		auto found = btn ? btn->on_button(mx, my) : nullptr;
		if (found) {
			return found;
		}
	}
	return Modal_gump::on_button(mx, my);
}
