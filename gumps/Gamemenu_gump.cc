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
#include "friendclass.h"
#include "gameclk.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"

#include <string>

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
// Co-Op Mode in the game menu: how guests join, what to give them, and
// whether new players can join now.  The password is set on the title
// menu's Co-Op Mode (it needs typing).
class Coop_options_gump : public Modal_gump {
	enum button_ids { id_ok, id_open, id_type, id_copy, id_joining, id_count };

	enum rows { row_hosting, row_type, row_details, row_joining, row_password, row_count };

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	using Button = CallbackTextButton<Coop_options_gump>;
	using Toggle = CallbackToggleTextButton<Coop_options_gump>;

	bool shown(int id) const {
		const bool hosting = Friend_mode_hosting();
		switch (id) {
		case id_open:
			return !hosting;
		case id_copy:
			return hosting;
		case id_joining:
			return hosting;
		default:
			return true;
		}
	}

	void set_type(int state) {
		Friend_mode_set_coop_by_lobby(state == 0);
	}

	void set_joining(int state) {
		Friend_mode_set_coop_joining_open(state == 1);
	}

	void open_now() {
		Friend_mode_open_to_coop();
	}

	void copy() {
		Friend_mode_copy_coop_details();
	}

public:
	Coop_options_gump() : Modal_gump(nullptr, -1) {
		SetProceduralBackground(TileRect(0, 0, 190, yForRow(row_count + 2)), -1);
		buttons[id_ok]   = std::make_unique<Button>(this, &Coop_options_gump::close, GumpStrings::OK(), 70,
                                                  yForRow(row_count + 1), 50);
		buttons[id_open] = std::make_unique<Button>(this, &Coop_options_gump::open_now, "Open to Co-Op",
				get_button_pos_for_label("Hosting:"), yForRow(row_hosting), 80);
		buttons[id_type] = std::make_unique<Toggle>(this, &Coop_options_gump::set_type,
				std::vector<std::string>{"Lobby code", "IP address"}, Friend_mode_coop_by_lobby() ? 0 : 1,
				get_button_pos_for_label("Type:"), yForRow(row_type), 80);
		buttons[id_copy] = std::make_unique<Button>(this, &Coop_options_gump::copy, "Copy",
				get_button_pos_for_label("Type:"), yForRow(row_details), 40);
		buttons[id_joining] = std::make_unique<Toggle>(this, &Coop_options_gump::set_joining,
				std::vector<std::string>{"Closed", "Open"}, Friend_mode_coop_joining_open() ? 1 : 0,
				get_button_pos_for_label("New players:"), yForRow(row_joining), 80);
		RightAlignWidgets(tcb::span(buttons.data() + id_open, id_count - id_open));
	}

	void close() override {
		done = true;
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
		label(row_hosting, Friend_mode_hosting() ? "Hosting: on" : "Hosting:");
		label(row_type, "Type:");
		label(row_details, Friend_mode_coop_details());
		if (Friend_mode_hosting()) {
			label(row_joining, "New players:");
		}
		label(row_password, Friend_mode_has_password() ? "Password: set (title menu)" : "Password: none (title menu)");
		gwin->set_painted();
	}

	// The code arrives a moment after opening: keep the page current.
	bool run() override {
		const std::string details = Friend_mode_coop_details();
		if (details != last_details) {
			last_details = details;
			return true;    // Paint again.
		}
		return Modal_gump::run();
	}

private:
	std::string last_details;
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
	if (!gwin->is_in_exult_menu() && !Friend_mode_is_guest()) {
		buttons[id_open_coop] = std::make_unique<Gamemenu_button>(
				this, &Gamemenu_gump::open_coop, "Co-Op Mode", margin, yForRow(y++), preferred_button_width, 11);
	}
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

void Gamemenu_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}
	gwin->set_painted();
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
