/**
 ** Gamewin.cc - X-windows Ultima7 map browser.
 **
 ** Written: 7/22/98 - JSF
 **/

/*
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2025  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "gamewin.h"

#include "friendmap.h"

#include "Astar.h"
#include "Audio.h"
#include "AudioMixer.h"
#include "Configuration.h"
#include "Face_stats.h"
#include "Flex.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "ItemMenu_gump.h"
#include "Notebook_gump.h"
#include "ShortcutBar_gump.h"
#include "actions.h"
#include "animate.h"
#include "audio/midi_drivers/XMidiFile.h"
#include "barge.h"
#include "cheat.h"
#include "chunks.h"
#include "chunkter.h"
#include "combat.h"
#include "combat_opts.h"
#include "contain.h"
#include "dir.h"
#include "drag.h"
#include "effects.h"
#include "egg.h"
#include "exult.h"
#include "files/U7file.h"
#include "flic/playfli.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamerend.h"
#include "items.h"
#include "keyactions.h"
#include "keys.h"
#include "mappatch.h"
#include "monsters.h"
#include "monstinf.h"
#include "mouse.h"
#include "npcnear.h"
#include "objiter.h"
#include "party.h"
#include "paths.h"
#include "scene_layer.h"
#include "schedule.h"
#include "spellbook.h"
#include "touchui.h"
#include "ucmachine.h"
#include "ucsched.h" /* Only used to flush objects. */
#include "usefuns.h"
#include "utils.h"
#include "qotcversion.h"
#include "version.h"
#include "virstone.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#ifdef USE_EXULTSTUDIO
#	include "servemsg.h"
#	include "server.h"
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::istream;
using std::make_unique;
using std::ofstream;
using std::rand;
using std::srand;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {

	class Strings {
	public:
		static auto DoesNotExist() {
			return get_text_msg(0x750 - msg_file_start);
		}

		static auto IsNotOnTheMap() {
			return get_text_msg(0x751 - msg_file_start);
		}

		static auto SearchingForShape() {
			return get_text_msg(0x752 - msg_file_start);
		}

		static auto NotFound() {
			return get_text_msg(0x753 - msg_file_start);
		}

		static auto SavingGame() {
			return get_text_msg(0x754 - msg_file_start);
		}

		static auto Oink() {
			return get_text_msg(0x6F0 - msg_file_start);
		}
	};

}    // namespace

// THE game window:
Game_window* Game_window::game_window = nullptr;

/*
 *  Provide chirping birds.
 */
class Background_noise : public Time_sensitive {
	int          repeats;       // Repeats in quick succession.
	int          last_sound;    // # of last sound played.
	Game_window* gwin;

	enum Song_states {
		Invalid,
		Outside,
		Dungeon,
		Nighttime,
		RainStorm,
		SnowStorm
	} laststate;    // Last state for SFX music tracks,

	// 1 outside, 2 dungeon, 3 nighttime, 4 rainstorm,
	// 5 snowstorm, 6 for danger nearby
public:
	Background_noise(Game_window* gw) : repeats(0), last_sound(-1), gwin(gw), laststate(Invalid) {
		gwin->get_tqueue()->add(5000, this);
	}

	~Background_noise() override {
		gwin->get_tqueue()->remove(this);
	}

	void handle_event(unsigned long curtime, uintptr udata) override;

	static bool is_combat_music(int num) {
		// Lumping music 16 as if it were a combat music in order to simplify
		// the check.
		return (num >= Audio::game_music(9) && num <= Audio::game_music(12))
			   || (num >= Audio::game_music(15) && num <= Audio::game_music(18));
	}
};

/*
 *  Play background sound.
 */

void Background_noise::handle_event(unsigned long curtime, uintptr udata) {
#ifndef COLOURLESS_REALLY_HATES_THE_BG_SFX

	unsigned long delay = 8000;
	Song_states   currentstate;
	const int     bghour    = gwin->get_clock()->get_hour();
	const int     weather   = gwin->get_effects()->get_weather();
	const bool    nighttime = bghour < 6 || bghour > 20;
	Combat_schedule::danger_music();
	if (gwin->is_in_dungeon()) {
		currentstate = Dungeon;
	} else if (weather == 2) {
		currentstate = RainStorm;
	} else if (weather == 1) {
		currentstate = SnowStorm;
	} else if (nighttime) {
		currentstate = Nighttime;
	} else {
		currentstate = Outside;
	}

	MyMidiPlayer* player = Audio::get_ptr()->get_midi();
	// The background sfx tracks only play for Digital Music, MT32emu,
	// MT32/FakeMT32 FMOpl is not sounding acceptable even though the original
	// used it.
	const bool play_bg_tracks = player && (player->get_ogg_enabled() || player->is_mt32());

	if (player) {
		delay                = 1000;    // Quickly get back to this function check
		const int curr_track = player->get_current_track();
		if ((curr_track == -1 || laststate != currentstate) && Audio::get_ptr()->is_music_enabled()) {
			// Conditions: not playing music, playing a background music
			if (curr_track == -1 || gwin->is_background_track(curr_track)
				|| ((currentstate == Dungeon) && !is_combat_music(curr_track))) {
				// Not already playing music
				int tracknum = 255;

				// Get the relevant track number.
				if (gwin->is_in_dungeon()) {
					if (play_bg_tracks) {
						tracknum = Audio::game_music(52);
					}
					laststate = Dungeon;
				} else if (weather == 1) {    // Snowstorm
					if (play_bg_tracks) {
						tracknum = Audio::game_music(5);
					}
					laststate = SnowStorm;
				} else if (weather == 2) {    // Rainstorm
					if (play_bg_tracks) {
						tracknum = Audio::game_music(4);
					}
					laststate = RainStorm;
				} else if (nighttime) {
					if (play_bg_tracks) {
						tracknum = Audio::game_music(7);
					}
					laststate = Nighttime;
				} else {
					if (play_bg_tracks) {
						tracknum = Audio::game_music(6);
					}
					laststate = Outside;
				}
				Audio::get_ptr()->start_music(tracknum, true);
			}
		}
	}

	// Tests to see if background sfx tracks are playing,
	// possible when the game has been restored
	// and the Audio option was changed from OGG/MT32 to something else
	if (player && !play_bg_tracks
		&& ((player->get_current_track() >= 4 && player->get_current_track() <= 7) || player->get_current_track() == 52)) {
		player->stop_music();
	}
	Main_actor* ava = gwin->get_main_actor();
	// Testing. When outside play birds during daytime or crickets at night
	if (ava && !gwin->is_main_actor_inside() && currentstate != Dungeon) {
		int                        sound     = -1;    // SFX #.
		static const unsigned char bgnight[] = {61, 61, 255};
		static const unsigned char bgday[]   = {82, 85, 85};
		if (repeats > 0) {    // Repeating?
			sound = last_sound;
		} else {
			if (nighttime) {
				sound = bgnight[rand() % sizeof(bgnight)];
				// only play daytime sfx when no music track is playing
			} else if (!play_bg_tracks && (!player || player->get_current_track() == -1)) {
				sound = bgday[rand() % sizeof(bgday)];
			}
			last_sound = sound;
		}
		if (sound >= 0 && sound != 255) {
			Audio::get_ptr()->play_sound_effect(Audio::game_sfx(sound), AUDIO_MAX_VOLUME - 64);
			repeats++;    // Count it.
			if (rand() % (repeats + 1) == 0) {
				// Repeat.
				delay = 500 + rand() % 1000;
			} else {
				delay   = 4000 + rand() % 3000;
				repeats = 0;
			}
		} else {
			repeats = 0;
		}
	}

	gwin->get_tqueue()->add(curtime + delay, this, udata);
#endif
}

/*
 *  Create game window.
 */

Game_window::Game_window(
		int width, int height, bool fullscreen, int gwidth, int gheight, int scale, int scaler, Image_window::FillMode fillmode,
		unsigned int fillsclr    // Window dimensions.
		)
		: dragging(nullptr), effects(new Effects_manager(this)), map(new Game_map(0)), render(new Game_render),
		  gump_man(new Gump_manager), party_man(new Party_manager), win(nullptr), npc_prox(new Npc_proximity_handler(this)),
		  pal(nullptr), tqueue(new Time_queue()), background_noise(new Background_noise(this)), usecode(nullptr), combat(false),
		  focus(true), ice_dungeon(false), painted(false), ambient_light(false), infravision_active(false), skip_above_actor(31),
		  in_dungeon(0), num_npcs1(0), std_delay(c_std_delay), time_stopped(0), special_light(0), theft_warnings(0), theft_cx(255),
		  theft_cy(255), moving_barge(nullptr), main_actor(nullptr), controlled_actor(nullptr),
		  controlled_actor_previous_schedule(-1), network_friend_control(false), network_dialogue_paused(false),
		  camera_actor(nullptr), npcs(0), bodies(0), scrolltx(0),
		  scrollty(0), dirty(0, 0, 0, 0), save_names{}, mouse3rd(false), fastmouse(false), double_click_closes_gumps(false),
		  text_bg(false), step_tile_delta(8), allow_right_pathfind(2), scroll_with_mouse(false), alternate_drop(false),
		  allow_autonotes(false), allow_enhancements(false), in_exult_menu(false), extended_intro(false), load_palette_timer(0),
		  plasma_start_color(0), plasma_cycle_range(0), skip_lift(255), paint_eggs(false), paint_egg_areas(0), armageddon(false),
		  walk_in_formation(false), debug(0), blits(0), scrolltx_l(0), scrollty_l(0), scrolltx_lp(0), scrollty_lp(0),
		  scrolltx_lo(0), scrollty_lo(0), avposx_ld(0), avposy_ld(0), lerping_enabled(0) {
	game_window = this;    // Set static ->.
	clock       = new Game_clock(tqueue);
	shape_man   = new Shape_manager();    // Create the single instance.
	maps.push_back(map);                  // Map #0.
	// Create window.
	win = new Image_window8(width, height, gwidth, gheight, scale, fullscreen, scaler, fillmode, fillsclr);
	win->set_title("Exult Ultima VII Engine");
	pal = new Palette();
	Game_singletons::init(this);    // Everything but 'usecode' exists.
	Shape_frame::set_to_render(win->get_ib8());

	string str;
	config->value("config/gameplay/textbackground", text_bg, -1);
	config->value("config/gameplay/mouse3rd", str, "no");
	if (str == "yes") {
		mouse3rd = true;
	}
	config->set("config/gameplay/mouse3rd", str, false);
	config->value("config/gameplay/fastmouse", str, "no");
	if (str == "yes") {
		fastmouse = true;
	}
	config->set("config/gameplay/fastmouse", str, false);
	config->value("config/gameplay/double_click_closes_gumps", str, "no");
	if (str == "yes") {
		double_click_closes_gumps = true;
	}
	config->set("config/gameplay/double_click_closes_gumps", str, false);
	config->value("config/gameplay/combat/difficulty", Combat::difficulty, 0);
	config->set("config/gameplay/combat/difficulty", Combat::difficulty, false);
	config->value("config/gameplay/combat/charmDifficulty", str, "normal");
	Combat::charmed_more_difficult = (str == "hard");
	config->set("config/gameplay/combat/charmDifficulty", str, false);
	config->value("config/gameplay/combat/mode", str, "original");
	if (str == "keypause") {
		Combat::mode = Combat::keypause;
	} else {
		Combat::mode = Combat::original;
	}
	config->set("config/gameplay/combat/mode", str, false);
	config->value("config/gameplay/combat/show_hits", str, "no");
	Combat::show_hits = (str == "yes");
	config->set("config/gameplay/combat/show_hits", str, false);
	config->value("config/gameplay/combat/health_bars", str, "no");
	Combat::health_bars = (str == "yes");
	config->set("config/gameplay/combat/health_bars", str, false);
	config->value("config/gameplay/minimap", str, "yes");    // Friend Mode corner minimap.
	Friend_minimap_enabled = (str == "yes");
	config->set("config/gameplay/minimap", str, false);
	config->value("config/gameplay/minimap_size", str, "small");
	Friend_minimap_large = (str == "large");
	config->set("config/gameplay/minimap_size", str, false);
	config->value("config/audio/disablepause", str, "no");
	config->set("config/audio/disablepause", str, false);

	config->value("config/gameplay/step_tile_delta", step_tile_delta, 8);
	if (step_tile_delta < 1) {
		step_tile_delta = 1;
	}
	config->set("config/gameplay/step_tile_delta", step_tile_delta, false);

	config->value("config/gameplay/allow_right_pathfind", str, "double");
	if (str == "no") {
		allow_right_pathfind = 0;
	} else if (str == "single") {
		allow_right_pathfind = 1;
	}
	config->set("config/gameplay/allow_right_pathfind", str, false);

	// New 'formation' walking?
	config->value("config/gameplay/formation", str, "yes");
	// Assume "yes" on anything but "no".
	walk_in_formation = str != "no";
	config->set("config/gameplay/formation", walk_in_formation ? "yes" : "no", false);

	config->value("config/gameplay/smooth_scrolling", lerping_enabled, 0);
	config->set("config/gameplay/smooth_scrolling", lerping_enabled, false);
	config->value("config/gameplay/alternate_drop", str, "no");
	alternate_drop = str == "yes";
	config->set("config/gameplay/alternate_drop", alternate_drop ? "yes" : "no", false);
	config->value("config/gameplay/allow_autonotes", str, "no");
	allow_autonotes = str == "yes";
	config->set("config/gameplay/allow_autonotes", allow_autonotes ? "yes" : "no", false);
	config->value("config/gameplay/enhancements", str, "yes");
	allow_enhancements = str == "yes";
	config->set("config/gameplay/enhancements", allow_enhancements ? "yes" : "no", false);
	Shape_info::set_allow_enhancements(allow_enhancements);
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
	const string default_scroll_with_mouse = "no";
	const string default_item_menu         = "yes";
	const string default_dpad_location     = "right";
	const string default_touch_pathfind    = "yes";
	const string default_shortcutbar       = "translucent";
	const string default_outline           = "black";
#else
	const string default_scroll_with_mouse = "yes";
	const string default_item_menu         = "no";
	const string default_dpad_location     = "no";
	const string default_touch_pathfind    = "no";
	const string default_shortcutbar       = "no";
	const string default_outline           = "no";
#endif

	// scroll with mouse
	config->value("config/gameplay/scroll_with_mouse", str, default_scroll_with_mouse);
	scroll_with_mouse = str == "yes";
	config->set("config/gameplay/scroll_with_mouse", scroll_with_mouse ? "yes" : "no", false);

	// Item menu
	config->value("config/touch/item_menu", str, default_item_menu);
	item_menu = str == "yes";
	config->set("config/touch/item_menu", item_menu ? "yes" : "no", false);

	// DPad location
	config->value("config/touch/dpad_location", str, default_dpad_location);
	if (str == "no") {
		dpad_location = 0;
	} else if (str == "left") {
		dpad_location = 1;
	} else {
		str           = "right";
		dpad_location = 2;
	}
	config->set("config/touch/dpad_location", str, false);

	// Touch pathfind
	config->value("config/touch/touch_pathfind", str, default_touch_pathfind);
	touch_pathfind = str == "yes";
	config->set("config/touch/touch_pathfind", touch_pathfind ? "yes" : "no", false);

	// Shortcut bar
	config->value("config/shortcutbar/use_shortcutbar", str, default_shortcutbar);
	if (str == "no") {
		use_shortcutbar = 0;
	} else if (str == "yes") {
		use_shortcutbar = 2;
	} else {
		str             = "translucent";
		use_shortcutbar = 1;
	}
	config->set("config/shortcutbar/use_shortcutbar", str, false);

	config->value("config/shortcutbar/use_outline_color", str, default_outline);
	if (str == "no") {
		outline_color = NPIXCOLORS;
	} else if (str == "green") {
		outline_color = POISON_PIXEL;
	} else if (str == "white") {
		outline_color = PROTECT_PIXEL;
	} else if (str == "yellow") {
		outline_color = CURSED_PIXEL;
	} else if (str == "blue") {
		outline_color = CHARMED_PIXEL;
	} else if (str == "red") {
		outline_color = HIT_PIXEL;
	} else if (str == "purple") {
		outline_color = PARALYZE_PIXEL;
	} else {
		str           = "black";
		outline_color = BLACK_PIXEL;
	}
	config->set("config/shortcutbar/use_outline_color", str, false);

	config->value("config/shortcutbar/hide_missing_items", str, "yes");
	sb_hide_missing = str != "no";
	config->set("config/shortcutbar/hide_missing_items", sb_hide_missing ? "yes" : "no", false);
	config->write_back();

	// default to SI extended intro
	config->value("config/gameplay/extended_intro", str, "yes");
	extended_intro = str == "yes";
	config->set("config/gameplay/extended_intro", extended_intro ? "yes" : "no", false);
}

/*
 *  Blank out screen.
 */
void Game_window::clear_screen(bool update) {
	win->BeginPaintIntoGuardBand(nullptr, nullptr, nullptr, nullptr);
	win->fill8(0, win->get_full_width(), win->get_full_height(), win->get_start_x(), win->get_start_y());

	win->EndPaintIntoGuardBand();
	// update screen
	if (update) {
		show(true);
	}
}

/*
 *  Deleting game window.
 */

Game_window::~Game_window() {
	gump_man->close_all_gumps(true);
	clear_world(false);    // Delete all objects, chunks.
	delete shape_man;
	delete gump_man;
	delete party_man;
	delete background_noise;
	delete tqueue;
	tqueue = nullptr;
	// Free objects that may own layers living in 'win' before the
	// window itself (the drag state and text effects each own layers).
	delete dragging;
	dragging = nullptr;
	delete effects;
	effects = nullptr;
	delete pal;
	for (auto* map : maps) {
		if (map) {
			delete map;
		}
	}
	delete usecode;
	delete win;
	delete clock;
	delete npc_prox;
	delete render;
	// The single instance is gone; clear the static so get_instance() returns
	// null (e.g. Mouse's destructor checks this before touching the window).
	game_window = nullptr;
}

/*
 *  Redirect all shape/text drawing to a buffer (e.g. an overlay layer),
 *  returning the previous target. Both the window's active buffer and the
 *  static shape render target are switched so every drawing path follows.
 */

Image_buffer8* Game_window::push_render_target(Image_buffer8* buf) {
	Image_buffer8* prev = win->set_render_buffer(buf);
	Shape_frame::set_to_render(buf);
	return prev;
}

void Game_window::pop_render_target(Image_buffer8* prev) {
	win->set_render_buffer(prev);
	Shape_frame::set_to_render(prev);
}

/*
 *  Abort.
 */

void Game_window::abort(const char* msg, ...) {
	std::va_list ap;
	va_start(ap, msg);
	char buf[512];
	vsnprintf(buf, sizeof(buf), msg, ap);    // Format the message.
	cerr << "Exult (fatal): " << buf << endl;
	delete this;
	throw quit_exception(-1);
}

bool Game_window::is_background_track(int num) const {    // ripped out of Background_noise
	// Have to do it this way because of SI.
	return num == Audio::game_music(4) || num == Audio::game_music(5) || num == Audio::game_music(6) || num == Audio::game_music(7)
		   || num == Audio::game_music(8) || num == Audio::game_music(52);
}

void Game_window::init_files(bool cycle) {
	const uint32                      menu_mouse_only = 1u << static_cast<int>(Image_window::UiLayerMousePointer);
	Game_window::Scoped_ui_layer_mask load_layers(this, cycle ? 0u : menu_mouse_only);
	// Display red plasma during load...
	if (cycle) {
		setup_load_palette();
	}

	usecode = Usecode_machine::create();
	Game_singletons::init(this);    // Everything should exist here.

	cycle_load_palette();
	shape_man->load();    // All the .vga files!
	cycle_load_palette();

	const unsigned long timer = SDL_GetTicks();
	srand(timer);    // Use time to seed rand. generator.
	// Force clock to start.
	tqueue->add(timer, clock, this);
	// Go to starting chunk
	scrolltx_lp = scrolltx_l = scrolltx = game->get_start_tile_x();
	scrollty_lp = scrollty_l = scrollty = game->get_start_tile_y();
	scrolltx_lo = scrollty_lo = 0;
	avposx_ld = avposy_ld = 0;

	// initialize keybinder
	delete keybinder;
	keybinder = new KeyBinder();

	std::string d;
	std::string keyfilename;
	d = "config/disk/game/" + Game::get_gametitle() + "/keys";
	config->value(d.c_str(), keyfilename, "(default)");
	if (keyfilename == "(default)") {
		config->set(d.c_str(), keyfilename, true);
		keybinder->LoadDefaults();
	} else {
		try {
			keybinder->LoadFromFile(keyfilename.c_str());
		} catch (file_open_exception&) {
			cerr << "Key mappings file '" << keyfilename << "' not found, falling back to default mappings." << endl;
			keybinder->LoadDefaults();
		}
	}
	keybinder->LoadFromPatch();
	cycle_load_palette();

	int fps;    // Init. animation speed.
	config->value("config/video/fps", fps, 5);
	if (fps <= 0) {
		fps = 5;
	}
	std_delay = 1000 / fps;    // Convert to msecs. between frames.
}

/*
 *  Read any map. (This is for "multimap" games, not U7.)
 */

Game_map* Game_window::get_map(int num    // Should be > 0.
) {
	if (num >= static_cast<int>(maps.size())) {
		maps.resize(num + 1);
	}
	if (maps[num] == nullptr) {
		auto* newmap = new Game_map(num);
		maps[num]    = newmap;
		maps[num]->init();
	}
	return maps[num];
}

/*
 *  Set current map to given #.
 */

void Game_window::set_map(int num) {
	map = get_map(num);
	if (!map) {
		abort("Map %d doesn't exist", num);
	}
	Game_singletons::gmap = map;
}

/*
 *  Get map patch list.
 */
Map_patch_collection& Game_window::get_map_patches() {
	return map->get_map_patches();
}

/*
 *  Set/unset barge mode.
 */

void Game_window::set_moving_barge(Barge_object* b) {
	if (b && b != moving_barge) {
		b->gather();    // Will 'gather' on next move.
		if (!b->contains(main_actor)) {
			b->set_to_gather();
		}
		landing_barge = nullptr;    // Boarding cancels any pending landing glide.
	} else if (!b && moving_barge) {
		moving_barge->done();    // No longer 'barging'.
		// Smooth scroll: prevent jerkiness on offboarding, especially the flying carpet.
		scrolltx_lp = scrolltx_l = scrolltx;
		scrollty_lp = scrollty_l = scrollty;
		landing_barge            = moving_barge;
	}
	moving_barge = b;
}

/*
 *  Is character moving?
 */

bool Game_window::is_moving() const {
	return moving_barge ? moving_barge->is_moving() : main_actor->is_moving();
}

/*
 *  Are we in dont_move mode?
 */

bool Game_window::main_actor_dont_move() const {
	return !cheat.in_map_editor() && main_actor != nullptr &&    // Not if map-editing.
		   ((main_actor->get_flag(Obj_flags::dont_move)) || (main_actor->get_flag(Obj_flags::dont_render)));
}

/*
 *  Are we in dont_move mode?
 */

bool Game_window::main_actor_can_act() const {
	return main_actor->can_act();
}

bool Game_window::main_actor_can_act_charmed() const {
	return main_actor->can_act_charmed();
}

bool Game_window::player_can_act() const {
	return (controlled_actor ? controlled_actor : main_actor)->can_act();
}

bool Game_window::player_can_act_charmed() const {
	return (controlled_actor ? controlled_actor : main_actor)->can_act_charmed();
}

bool Game_window::in_infravision() const {
	return infravision_active || cheat.in_infravision();
}

/*
 *  Add time for a light spell.
 */

void Game_window::add_special_light(int units    // Light=500, GreatLight=5000.
) {
	if (!special_light) {    // Nothing in effect now?
		special_light = clock->get_total_minutes();
		clock->set_palette();
	}
	special_light += units / 20;    // Figure ending time.
}

/*
 *  Set 'stop time' value.
 */

void Game_window::set_time_stopped(long delay    // Delay in ticks (1/1000
												 // secs.), -1 to stop
												 // indefinitely, or 0 to end.
) {
	if (delay == -1) {
		time_stopped = -1;
	} else if (!delay) {
		time_stopped = 0;
	} else {
		const long new_expire = Game::get_ticks() + delay;
		if (new_expire > time_stopped) {    // Set expiration time.
			time_stopped = new_expire;
		}
	}
}

/*
 *  Return delay to expiration (or 3000 if indefinite).
 */

long Game_window::check_time_stopped() {
	if (time_stopped == -1) {
		return 3000;
	}
	const long delay = time_stopped - Game::get_ticks();
	if (delay > 0) {
		return delay;
	}
	time_stopped = 0;    // Done.
	return 0;
}

/*
 *  Toggle combat mode.
 */

void Game_window::toggle_combat() {
	combat = !combat;
	if (Friend_mode_is_guest()) {
		// The host runs every fight.  Here combat mode only changes what a
		// double-click does (Friend_mode_guest_attack); our copies of the
		// Avatar and party must not start fights of their own.
		if (g_shortcutBar) {
			g_shortcutBar->set_changed();
		}
		if (combat) {
			Friend_mode_combat_started();    // A hostile picked with Q.
		}
		return;
	}
	// Change party member's schedules.
	int       newsched = combat ? Schedule::combat : Schedule::follow_avatar;
	const int cnt      = party_man->get_count();
	for (int i = 0; i < cnt; i++) {
		const int party_member = party_man->get_member(i);
		Actor*    person       = get_npc(party_member);
		if (!person) {
			continue;
		}
		if (!person->can_act_charmed()) {
			newsched = Schedule::combat;
		}
		const int sched = person->get_schedule_type();
		if (sched != newsched && sched != Schedule::wait && sched != Schedule::loiter) {
			person->set_schedule_type(newsched);
		}
	}
	if (!main_actor_can_act_charmed()) {
		newsched = Schedule::combat;
	}
	if (main_actor->get_schedule_type() != newsched && !Friend_mode_is_held(main_actor)) {    // Held: Friend Mode's Y.
		main_actor->set_schedule_type(newsched);
	}
	if (combat) {    // Get rid of flee modes.
		main_actor->ready_best_weapon();
		set_moving_barge(nullptr);    // And get out of barge mode.
		Actor*    all[9];
		const int cnt = get_party(all, 1);
		for (int i = 0; i < cnt; i++) {
			// Did Usecode set to flee?
			Actor* act = all[i];
			if (act->get_attack_mode() == Actor::flee && !act->did_user_set_attack()) {
				act->set_attack_mode(Actor::nearest);
			}
			// And avoid attacking party members,
			//  in case of Usecode bug.
			const Game_object* targ = act->get_target();
			if (targ && targ->get_flag(Obj_flags::in_party)) {
				act->set_target(nullptr);
			}
			if (touchui != nullptr && Combat::mode != Combat::original) {
				touchui->showPauseControls();
			}
		}
		Friend_mode_combat_started();    // A hostile picked with Q.
	} else {                 // Ending combat.
		Friend_mode_combat_ended();    // Its attack mode back, if targeting made it manual.
		Combat::resume();    // Make sure not still paused.
		if (touchui != nullptr && Combat::mode != Combat::original) {
			touchui->hidePauseControls();
		}
	}
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
}

/*
 *  Add an NPC.
 */

void Game_window::add_npc(
		Actor* npc,
		int    num    // Number. Has to match npc->num.
) {
	assert(num == npc->get_npc_num());
	assert(num <= static_cast<int>(npcs.size()));
	if (num == static_cast<int>(npcs.size())) {    // Add at end.
		npcs.push_back(std::static_pointer_cast<Actor>(npc->shared_from_this()));
	} else {
		// Better be unused.
		assert(!npcs[num] || npcs[num]->is_unused());
		npcs[num] = std::static_pointer_cast<Actor>(npc->shared_from_this());
	}
}

/*  Get desired NPD.
 */
Actor* Game_window::get_npc(long npc_num) const {
	if (npc_num < 0 || npc_num >= static_cast<int>(npcs.size())) {
		return nullptr;
	} else {
		return npcs[npc_num].get();
	}
}

/*
 *  Show desired NPC.
 */

void Game_window::locate_npc(int npc_num) {
	std::ostringstream msg;
	Actor*             npc = get_npc(npc_num);
	if (!npc) {
		msg << "NPC " << npc_num << Strings::DoesNotExist();
		effects->center_text(msg.str().c_str());
	} else if (npc->is_pos_invalid()) {
		msg << "NPC " << npc_num << Strings::IsNotOnTheMap();
		effects->center_text(msg.str().c_str());
	} else {
		// ++++WHAT IF on a different map???
		const Tile_coord pos = npc->get_tile();
		center_view(pos);
		cheat.clear_selected();
		cheat.append_selected(npc);
		msg << "NPC " << npc_num << ": '" << npc->get_npc_name() << "'.";
		const int above = pos.tz + npc->get_info().get_3d_height() - 1;
		if (skip_above_actor > above) {
			set_above_main_actor(above);
		}
		effects->center_text(msg.str().c_str());
	}
}

/*
 *  Find first unused NPC #.
 */

int Game_window::get_unused_npc() {
	int cnt = npcs.size();    // Get # in list.
	int i   = 0;
	for (; i < cnt; i++) {
		if (i >= 356 && i <= 359) {
			continue;    // Never return these.
		}
		if (!npcs[i] || npcs[i]->is_unused()) {
			return i;
		}
	}
	if (i >= 356 && i <= 359) {
		// Special, 'reserved' cases.
		i = 360;
		do {
			npcs.push_back(std::make_shared<Npc_actor>("Reserved", 0));
			auto& npc = npcs[cnt];
			npc->set_schedule_type(Schedule::wait);
			npc->set_unused(true);
		} while (++cnt < 360);
	}
	return i;    // First free one is past the end.
}

/*
 *  Resize event occurred.
 */

void Game_window::resized(
		unsigned int neww, unsigned int newh, bool newfs, unsigned int newgw, unsigned int newgh, unsigned int newsc,
		unsigned int newsclr, Image_window::FillMode newfill, unsigned int newfillsclr) {
	win->resized(neww, newh, newfs, newgw, newgh, newsc, newsclr, newfill, newfillsclr);
	pal->apply(false);
	Shape_frame::set_to_render(win->get_ib8());
	if (!main_actor) {    // In case we're before start.
		return;
	}
	center_view(main_actor->get_tile());
	paint();
	// Do the following only if in game (not for menus)
	if (!gump_man->gump_mode()) {
		char msg[80];
		snprintf(msg, sizeof(msg), "%ux%ux%u", neww, newh, newsc);
		effects->center_text(msg);
	}
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
}

/*
 *  Clear out world's contents. Should be used during a 'restore'.
 */

void Game_window::clear_world(bool restoremapedit) {
	const bool edit = cheat.in_map_editor();
	cheat.set_map_editor(false);
	Combat::resume();
	tqueue->clear();    // Remove all entries.
	clear_dirty();
	Usecode_script::clear();    // Clear out all scheduled usecode.
	// Most NPCs were deleted when the map is cleared; we have to deal with some
	// stragglers.
	for (auto& npc : npcs) {
		if (npc && npc->is_unused()) {
			npc.reset();
		}
	}
	for (auto* map : maps) {
		if (map) {
			map->clear();
		}
	}
	try {
		set_map(0);    // Back to main map.
	} catch (const quit_exception& /*f*/) {
		// Lets not allow this to go up.
	}
	Monster_actor::delete_all();    // To be safe, del. any still around.
	Notebook_gump::clear();
	main_actor   = nullptr;
	controlled_actor = nullptr;
	controlled_actor_previous_schedule = -1;
	remote_friends.clear();
	camera_actor = nullptr;
	num_npcs1    = 0;
	theft_cx = theft_cy = -1;
	combat              = false;
	npcs.resize(0);    // NPC's already deleted above.
	bodies.resize(0);
	moving_barge       = nullptr;    // Get out of barge mode.
	landing_barge      = nullptr;
	special_light      = 0;        // Clear out light spells.
	ambient_light      = false;    // And ambient lighting.
	infravision_active = false;
	effects->remove_all_effects(false);
	Schedule_change::clear();
	cheat.set_map_editor(edit && restoremapedit);
}

/*
 *  Look throughout the map for a given shape. The search starts at
 *  the first currently-selected shape, if possible.
 *
 *  Output: true if found, else 0.
 */

bool Game_window::locate_shape(
		int  shapenum,    // Desired shape.
		bool upwards,     // If true, search upwards.
		int  frnum,       // Frame.
		int  qual         // Quality/quantity.
) {
	// Get (first) selected object.
	const std::vector<Game_object_shared>& sel   = cheat.get_selected();
	Game_object*                           start = !sel.empty() ? (sel[0]).get() : nullptr;
	std::ostringstream                     msg;
	msg << Strings::SearchingForShape() << shapenum;
	effects->center_text(msg.str().c_str());
	paint();
	show();
	Game_object* obj = map->locate_shape(shapenum, upwards, start, frnum, qual);
	if (!obj) {
		effects->center_text(Strings::NotFound());
		return false;    // Not found.
	}
	effects->remove_text_effects();
	cheat.clear_selected();    // Select obj.
	cheat.append_selected(obj);
	//++++++++Got to show it.
	Game_object* owner = obj->get_outermost();    //+++++TESTING
	if (owner != obj) {
		cheat.append_selected(owner);
	}
	center_view(owner->get_tile());
	return true;
}

/*
 *  Set location to ExultStudio.
 */

inline void Send_location(Game_window* gwin) {
#ifdef USE_EXULTSTUDIO
	if (client_socket >= 0 &&    // Talking to ExultStudio?
		cheat.in_map_editor()) {
		unsigned char  data[50];
		unsigned char* ptr = &data[0];
		little_endian::Write4(ptr, gwin->get_scrolltx());
		little_endian::Write4(ptr, gwin->get_scrollty());
		little_endian::Write4(ptr, gwin->get_width() / c_tilesize);
		little_endian::Write4(ptr, gwin->get_height() / c_tilesize);
		little_endian::Write4(ptr, gwin->get_win()->get_scale_factor());
		Exult_server::Send_data(client_socket, Exult_server::view_pos, &data[0], ptr - data);
	}
#else
	ignore_unused_variable_warning(gwin);
#endif
}

/*
 *  Send current location to ExultStudio.
 */

void Game_window::send_location() {
	Send_location(this);
}

bool Game_window::rotatecolours() {
	Uint32        ticks       = SDL_GetTicks();
	static Uint32 last_rotate = 0;
	// Rotate less often if scaling and
	//   not paletized.
	const int rot_speed = 100 << (win->fast_palette_rotate() ? 0 : 1);

	if (ticks > last_rotate + rot_speed) {
		// (Blits in simulated 8-bit mode.)
		win->rotate_colors(0xfc, 3, 0);
		win->rotate_colors(0xf8, 4, 0);
		win->rotate_colors(0xf4, 4, 0);
		win->rotate_colors(0xf0, 4, 0);
		win->rotate_colors(0xe8, 8, 0);
		win->rotate_colors(0xe0, 8, 1);
		while (ticks > last_rotate + rot_speed) {
			last_rotate += rot_speed;
		}
		// Non palettized needs explicit blit.
		if (!win->is_palettized()) {
			set_painted();
		}
		return true;
	}
	return false;
}

/*
 *  Set the scroll position to a given tile.
 */

void Game_window::set_scrolls(int newscrolltx, int newscrollty) {
	scrolltx = newscrolltx;
	scrollty = newscrollty;
	// Set scroll box.
	// Let's try 2x2 tiles.
	scroll_bounds.w = scroll_bounds.h = 2;
	scroll_bounds.x                   = scrolltx + (get_width() / c_tilesize - scroll_bounds.w) / 2;
	// OFFSET HERE
	scroll_bounds.y = scrollty + ((get_height()) / c_tilesize - scroll_bounds.h) / 2;

	Barge_object* old_active_barge = moving_barge;
	map->read_map_data();    // This pulls in objects.
	// Found active barge?
	if (!old_active_barge && moving_barge) {
		// Do it right.
		Barge_object* b = moving_barge;
		moving_barge    = nullptr;
		set_moving_barge(b);
	}
	// Set where to skip rendering.
	const int  cx    = camera_actor->get_cx();
	const int  cy    = camera_actor->get_cy();
	Map_chunk* nlist = map->get_chunk(cx, cy);
	nlist->setup_cache();
	const int tx = camera_actor->get_tx();
	const int ty = camera_actor->get_ty();
	set_above_main_actor(nlist->is_roof(tx, ty, camera_actor->get_lift()));
	set_in_dungeon(nlist->has_dungeon() ? nlist->is_dungeon(tx, ty) : 0);
	set_ice_dungeon(nlist->is_ice_dungeon(tx, ty));
	send_location();    // Tell ExultStudio.
}

/*
 *  Set the scroll position so that a given tile is centered. (Used by
 *  center_view.)
 */

void Game_window::set_scrolls(Tile_coord cent    // Want center here.
) {
	// Figure in tiles.
	// OFFSET HERE
	const int tw = get_width() / c_tilesize;
	const int th = (get_height()) / c_tilesize;
	set_scrolls(DECR_TILE(cent.tx, tw / 2), DECR_TILE(cent.ty, th / 2));
}

/*
 *  Center view around a given tile. This is called during a 'restore'
 *  to init. stuff as well.
 */

void Game_window::center_view(const Tile_coord& t) {
	set_scrolls(t);
	set_all_dirty();
}

/*
 *  Set actor to center view around.
 */

void Game_window::set_camera_actor(Actor* a) {
	if (a == main_actor &&    // Setting back to main actor?
		camera_actor &&       // Change in chunk?
		(camera_actor->get_cx() != main_actor->get_cx() || camera_actor->get_cy() != main_actor->get_cy())) {
		// Cache out temp. objects.
		emulate_cache(camera_actor->get_chunk(), main_actor->get_chunk());
	}
	camera_actor       = a;
	const Tile_coord t = a->get_tile();
	set_scrolls(t);    // Set scrolling around position,
	// and read in map there.
	set_all_dirty();
}

/*
 * Toggle the Phase 1 Friend Mode prototype between the story Avatar and the
 * first active party companion. Actor identity and party membership are not
 * changed; only local input and camera ownership move.
 */
bool Game_window::toggle_friend_mode_control() {
	Actor* current = get_controlled_actor();
	if (current != main_actor) {
		return release_friend_mode_control();
	}

	const int count = party_man->get_count();
	for (int i = 0; i < count; ++i) {
		if (take_friend_mode_control(party_man->get_member(i))) {
			return true;
		}
	}

	cout << "Friend Mode: no eligible party companion" << endl;
	return false;
}

/*
 * Give the local player control of one specific active party companion.  The
 * network host selects by stable NPC number; the guest resolves that actor in
 * its copy of the loaded world.
 */
Actor* Game_window::get_action_target() const {
	const Game_object_shared target = action_target.lock();
	return target ? target->as_actor() : nullptr;
}

void Game_window::set_action_target(Actor* target) {
	action_target = weak_from_obj(target);
}

bool Game_window::take_friend_mode_control(int npc_num, bool network_guest) {
	Actor* companion = get_npc(npc_num);
	// The host decides for a network guest, which may get back a companion
	// that is asleep or paralyzed (see enforce_friend_mode_control_safety).
	if (!companion || companion == main_actor || !companion->is_in_party() || companion->is_dead()
		|| (!network_guest
			&& (companion->get_flag(Obj_flags::asleep) || companion->get_flag(Obj_flags::paralyzed)))) {
		return false;
	}
	if (controlled_actor && controlled_actor != main_actor) {
		release_friend_mode_control();
	}
	companion->stop();
	controlled_actor_previous_schedule = companion->get_schedule_type();
	companion->set_schedule_type(Schedule::wait);
	controlled_actor = companion;
	network_friend_control = network_guest;
	set_camera_actor(companion);
	cout << "Friend Mode: controlling " << companion->get_npc_name() << endl;
	return true;
}

int Game_window::friend_leash_distance(const Tile_coord& t) const {
	return main_actor ? t.distance_2d(main_actor->get_tile()) : 0;
}

bool Game_window::friend_leash_allows(const Tile_coord& from, const Tile_coord& to) const {
	const int dist = friend_leash_distance(to);
	return dist <= friend_leash_max_tiles || dist < friend_leash_distance(from);
}

void Game_window::set_network_host_paused(bool paused) {
	network_host_paused = paused;
	if (paused && controlled_actor) {
		controlled_actor->stop();
	}
}

void Game_window::set_network_spectating(bool spectating) {
	network_spectating = spectating;
	if (spectating) {
		main_actor->stop();
		set_camera_actor(main_actor);
	}
}

void Game_window::set_network_dialogue_paused(bool paused) {
	network_dialogue_paused = paused;
	if (paused && network_friend_control && controlled_actor) {
		controlled_actor->stop();
	}
}

bool Game_window::near_remote_friend(const Game_object* obj) const {
	if (!obj) {
		return false;
	}
	for (const Remote_friend& each : remote_friends) {
		if (obj != each.actor && obj->get_map() == each.actor->get_map()
			&& obj->distance(each.actor) <= 1 + c_screen_tile_size) {
			return true;
		}
	}
	return false;
}

bool Game_window::is_remote_friend(const Game_object* obj) const {
	return obj && std::any_of(remote_friends.begin(), remote_friends.end(), [obj](const Remote_friend& each) {
			   return each.actor == obj;
		   });
}

int Game_window::friend_party_schedule(const Remote_friend& each) const {
	if (combat) {
		return Schedule::combat;
	}
	const int previous = each.previous_schedule;
	// A companion saved while a guest held it (before saves knew better)
	// comes back waiting; it should follow like the rest of the party.
	return previous < 0 || previous == Schedule::wait || previous == Schedule::loiter ? Schedule::follow_avatar
																						 : previous;
}

bool Game_window::reserve_friend_mode_actor(int npc_num) {
	Actor* companion = get_npc(npc_num);
	if (!companion || companion == main_actor || !companion->is_in_party() || companion->is_dead()
		|| is_remote_friend(companion)) {
		return false;
	}
	companion->stop();
	remote_friends.push_back({companion, companion->get_schedule_type()});
	companion->set_schedule_type(Schedule::wait);
	cout << "Friend Mode: reserved " << companion->get_name() << " for remote control" << endl;
	return true;
}

void Game_window::release_remote_friend_mode_actor(int npc_num) {
	for (auto it = remote_friends.begin(); it != remote_friends.end(); ++it) {
		Actor* actor = it->actor;
		if (actor->get_npc_num() != npc_num) {
			continue;
		}
		actor->stop();
		if (actor->is_in_party() && !actor->is_dead()) {
			actor->set_schedule_type(friend_party_schedule(*it));    // Back with the party.
		}
		remote_friends.erase(it);
		return;
	}
}

/*
 * Return local control to the Avatar and restore a possessed companion's AI.
 * This is deliberately safe to call when the Avatar is already controlled.
 */
bool Game_window::release_friend_mode_control() {
	Actor* current = get_controlled_actor();
	if (!current || current == main_actor) {
		return false;
	}

	current->stop();
	current->set_target(nullptr);
	if (controlled_actor_previous_schedule >= 0) {
		int sched = controlled_actor_previous_schedule;
		// A companion led by the host goes back into the fight in combat mode.
		if (!network_friend_control && combat && sched == Schedule::follow_avatar) {
			sched = Schedule::combat;
		}
		current->set_schedule_type(sched);
	}
	controlled_actor = main_actor;
	controlled_actor_previous_schedule = -1;
	network_friend_control = false;
	network_dialogue_paused = false;
	set_camera_actor(main_actor);
	cout << "Friend Mode: controlling the Avatar" << endl;
	return true;
}

/*
 * Scripted movement, incapacitation, and party removal take precedence over
 * local companion possession. Return control to the Avatar before either actor
 * enters a state that could fight the local controller.  A network guest
 * keeps its companion while it is asleep (or knocked out) or paralyzed: the
 * host refuses its moves meanwhile, and frees it only if it dies or leaves
 * the party.
 */
// A script has 'actor' in hand: in_usecode_control, but not counting the
// scripts of an item it is using itself (a bucket it fills, led by the
// host), which were made for it (Usecode_script::get_friend_walker).
static bool Friend_story_controls(const Actor* actor) {
	if (actor->get_flag(Obj_flags::dont_render) || actor->get_flag(Obj_flags::dont_move)) {
		return true;
	}
	Usecode_script* scr = nullptr;
	while ((scr = Usecode_script::find_active(actor, scr)) != nullptr) {
		if (!scr->is_no_halt() && scr->get_friend_walker() != actor) {
			return true;
		}
	}
	return false;
}

void Game_window::enforce_friend_mode_control_safety() {
	Actor* current = get_controlled_actor();
	if (!current || current == main_actor) {
		return;
	}
	const bool incapacitated = current->get_flag(Obj_flags::asleep) || current->get_flag(Obj_flags::paralyzed);
	// A knocked-out (or sleeping, paralyzed) Avatar's own script, its fall
	// to the ground (Actor::lay_down), is no story taking over: the led
	// companion plays on (2026-10-28; it handed control back to the
	// Avatar lying there).  Waking ends that script.
	const bool avatar_down = main_actor->get_flag(Obj_flags::asleep) || main_actor->get_flag(Obj_flags::paralyzed)
							 || main_actor->is_knocked_out();
	if ((!network_friend_control
		 && (main_actor_dont_move() || (!avatar_down && main_actor->in_usecode_control())
			 || Friend_story_controls(current) || incapacitated))
		|| !current->is_in_party() || current->is_dead()) {
		release_friend_mode_control();
	}
}

/*
 *  Scroll if necessary.
 *
 *  Output: 1 if scrolled (screen updated).
 */

bool Game_window::scroll_if_needed(Tile_coord t) {
	if (suppress_barge_recenter) {
		// Change lift of a barge without recentering the view,
		// afterwards the next move recenters.
		return false;
	}
	bool scrolled = false;
	// 1 lift = 1/2 tile.
	const int tx = DECR_TILE(t.tx, t.tz / 2);
	const int ty = DECR_TILE(t.ty, t.tz / 2);
	if (Tile_coord::gte(DECR_TILE(scroll_bounds.x), tx)) {
		view_left();
		scrolled = true;
	} else if (Tile_coord::gte(tx, (scroll_bounds.x + scroll_bounds.w) % c_num_tiles)) {
		view_right();
		scrolled = true;
	}
	if (Tile_coord::gte(DECR_TILE(scroll_bounds.y), ty)) {
		view_up();
		scrolled = true;
	} else if (Tile_coord::gte(ty, (scroll_bounds.y + scroll_bounds.h) % c_num_tiles)) {
		view_down();
		scrolled = true;
	}
	return scrolled;
}

/*
 * Keep roof and dungeon rendering tied to the actor driving the camera.  This
 * is normally the Avatar, but Friend Mode can temporarily use a companion.
 */
bool Game_window::scroll_if_needed(const Actor* a, const Tile_coord& t) {
	if (a != camera_actor) {
		return false;
	}

	const int  cx    = t.tx / c_tiles_per_chunk;
	const int  cy    = t.ty / c_tiles_per_chunk;
	const int  tx    = t.tx % c_tiles_per_chunk;
	const int  ty    = t.ty % c_tiles_per_chunk;
	Map_chunk* nlist = map->get_chunk(cx, cy);
	const bool ice   = nlist->is_ice_dungeon(tx, ty);
	bool       changed = set_above_main_actor(nlist->is_roof(tx, ty, t.tz));
	changed            = set_in_dungeon(nlist->has_dungeon() ? nlist->is_dungeon(tx, ty) : 0) || changed;
	if (ice_dungeon != ice) {
		set_ice_dungeon(ice);
		changed = true;
	}
	if (changed) {
		set_all_dirty();
	}
	return scroll_if_needed(t);
}

/*
 *  Change lift of a barge without recentering the view.
 */

void Game_window::barge_lift_move(Game_object* obj, const Tile_coord& t) {
	if (!obj->as_barge()) {
		obj->move(t);
		return;
	}
	suppress_barge_recenter = true;
	obj->move(t);
	suppress_barge_recenter = false;
}

/*
 *  Show the absolute game location, where each coordinate is of the
 *  8x8 tiles clicked on.
 */

void Game_window::show_game_location(
		int x, int y    // Point on screen.
) {
	x = get_scrolltx() + x / c_tilesize;
	y = get_scrollty() + y / c_tilesize;
	cout << "Game location is (" << x << ", " << y << ")" << endl;
}

/*
 *  Get screen area used by object.
 */

TileRect Game_window::get_shape_rect(const Game_object* obj) const {
	if (!obj->get_chunk()) {    // Not on map?
		Gump* gump = gump_man->find_gump(obj);
		if (gump) {
			return gump->get_shape_rect(obj);
		} else {
			return TileRect(0, 0, 0, 0);
		}
	}
	Shape_frame* s = obj->get_shape();
	if (!s) {
		// This is probably fatal.
#ifdef DEBUG
		std::cerr << "DEATH! get_shape() returned a nullptr pointer: " << __FILE__ << ":" << __LINE__ << std::endl;
		std::cerr << "Betcha it's a little doggie." << std::endl;
#endif
		return TileRect(0, 0, 0, 0);
	}
	Tile_coord t      = obj->get_tile();    // Get tile coords.
	const int  lftpix = (c_tilesize * t.tz) / 2;
	t.tx += 1 - get_scrolltx();
	t.ty += 1 - get_scrollty();
	// Watch for wrapping.
	if (t.tx < -c_num_tiles / 2) {
		t.tx += c_num_tiles;
	}
	if (t.ty < -c_num_tiles / 2) {
		t.ty += c_num_tiles;
	}
	return get_shape_rect(s, t.tx * c_tilesize - 1 - lftpix, t.ty * c_tilesize - 1 - lftpix);
}

/*
 *  Get screen location of given tile.
 */

inline void Get_shape_location(Tile_coord t, int scrolltx, int scrollty, int& x, int& y) {
	const int lft = 4 * t.tz;
	t.tx += 1 - scrolltx;
	t.ty += 1 - scrollty;
	// Watch for wrapping.
	if (t.tx < -c_num_tiles / 2) {
		t.tx += c_num_tiles;
	}
	if (t.ty < -c_num_tiles / 2) {
		t.ty += c_num_tiles;
	}
	x = t.tx * c_tilesize - 1 - lft;
	y = t.ty * c_tilesize - 1 - lft;
}

/*
 *  Get screen loc. of object which MUST be on the map (no owner).
 */

void Game_window::get_shape_location(const Game_object* obj, int& x, int& y) {
	const Tile_coord t = obj->get_tile();
	Get_shape_location(t, scrolltx, scrollty, x, y);
	bool apply_avpos = false;
	// Smooth scroll barges but only if the Avatar is on board,
	// otherwise the barge could be jerking during automatic boarding.
	if (moving_barge && moving_barge->contains(get_main_actor())) {
		if (moving_barge->is_grouped_member(obj)) {
			apply_avpos = true;
		}
	} else {
		// Smooth scroll the avatar
		const Actor* actor = obj->as_actor();
		if (obj == get_camera_actor()) {
			apply_avpos = true;
		} else if (actor && actor->is_in_party() && lerping_enabled) {
			// Apply the same lerping offset to party members
			apply_avpos = true;
		} else if (landing_barge && lerping_enabled && landing_barge->is_grouped_member(obj)) {
			// Keep party and barge in sync during landing.
			apply_avpos = true;
		}
	}
	if (apply_avpos) {
		x += avposx_ld;
		y += avposy_ld;
	}
	x -= scrolltx_lo;
	y -= scrollty_lo;
}

void Game_window::get_shape_location(const Tile_coord& t, int& x, int& y) {
	Get_shape_location(t, scrolltx, scrollty, x, y);
	x -= scrolltx_lo;
	y -= scrollty_lo;
}

/*
 *  Put the actor(s) in the world.
 */

void Game_window::init_actors() {
	if (main_actor) {    // Already done?
		Game::clear_avname();
		Game::clear_avsex();
		Game::clear_avskin();
		return;
	}
	read_npcs();    // Read in all U7 NPC's.
	int avsched = combat ? Schedule::combat : Schedule::follow_avatar;
	if (!main_actor_can_act_charmed()) {
		avsched = Schedule::combat;
	}
	main_actor->set_schedule_type(avsched);

	// Was a name, sex or skincolor set in Game
	// this bascially detects
	bool changed = false;

	if (Game::get_avsex() == 0 || Game::get_avsex() == 1 || Game::get_avname()
		|| (Game::get_avskin() >= 0 && Game::get_avskin() <= 2)) {
		changed = true;
	}

	Game::clear_avname();
	Game::clear_avsex();
	Game::clear_avskin();

	// Update gamedat if there was a change
	if (changed) {
		schedule_npcs(6, false);
		write_npcs();
	}
}

// In gamemgr/modmgr.cc because it is also needed by ES.
string get_game_identity(const char* savename, const string& title);

/*
 *  Create initial 'gamedat' directory if needed
 *
 */

bool Game_window::init_gamedat(bool create) {
	// Create gamedat files 1st time.
	if (create) {
		cout << "Creating 'gamedat' files." << endl;
		if (is_system_path_defined("<PATCH>") && U7exists(PATCH_INITGAME)) {
			restore_gamedat(PATCH_INITGAME);
		} else {
			// Flag that we're reading U7 file.
			Game::set_new_game();
			restore_gamedat(INITGAME);
		}
		// Editing, and no IDENTITY?
		if (Game::is_editing() && !U7exists(IDENTITY)) {
			auto pOut = U7open_out(IDENTITY);
			if (!pOut) {
				return false;
			}
			auto& out = *pOut;
			out << Game::get_gametitle() << endl;
		}

		// log version of exult that was used to start this game
		auto out = U7open_out(GNEWGAMEVER);
		if (out) {
			QOTC_write_version(*out);
			getVersionInfo(*out);
		}
	}
	//++++Maybe just test for IDENTITY+++:
	else if ((U7exists(U7NBUF_DAT) || !U7exists(NPC_DAT)) && !Game::is_editing()) {
		return false;
	} else {
		auto pIdentity_file = U7open_in(IDENTITY);
		if (!pIdentity_file) {
			return false;
		}
		auto& identity_file = *pIdentity_file;
		char  gamedat_identity[256];
		identity_file.read(gamedat_identity, 256);
		char* ptr = gamedat_identity;
		for (; (*ptr != 0x1a && *ptr != 0x0d && *ptr != 0x0a); ptr++)
			;
		*ptr = 0;
		cout << "Gamedat identity " << gamedat_identity << endl;
		const string static_identity = get_game_identity(INITGAME, Game::get_gametitle());
		if (static_identity != gamedat_identity) {
			return false;
		}
		// scroll coords.
	}
	read_save_names();    // Read in saved-game names.
	return true;
}

/*
 *  Save game by writing out to the 'gamedat' directory.
 *
 *  Output: 0 if error, already reported.
 */

void Game_window::write(bool nopaint) {
	// Never serialize the temporary AI schedule used while a companion is
	// locally possessed.  A companion the host or a single player leads
	// (Friend Mode) stays led, and is written following, as a guest's
	// companion is below; any other possession is handed back here.
	Actor* led = controlled_actor != main_actor && !network_friend_control ? controlled_actor : nullptr;
	if (!led) {
		release_friend_mode_control();
	}

	// Lets just show a nice message on screen first

	const int width       = get_width();
	const int centre_x    = width / 2;
	const int height      = get_height();
	const int centre_y    = height / 2;
	const int text_height = shape_man->get_text_height(0);
	const int text_width  = shape_man->get_text_width(0, Strings::SavingGame());

	if (!nopaint) {
		win->fill_translucent8(0, width, height, 0, 0, shape_man->get_xform(8));
		shape_man->paint_text(0, Strings::SavingGame(), centre_x - text_width / 2, centre_y - text_height);
		show(true);
	}
	Friend_mode_saving(true);    // Items guests hold count as where they came from.
	for (auto* map : maps) {
		if (map) {
			map->write_ireg();    // Write ireg files.
		}
	}
	// A guest's companion waits for its guest on the host; it must not be
	// saved waiting, or it stays put for good once the save is loaded.
	std::vector<int> held_schedules;
	for (const Remote_friend& each : remote_friends) {
		held_schedules.push_back(each.actor->get_schedule_type());
		each.actor->set_schedule_type(friend_party_schedule(each));
	}
	const int led_schedule = led ? led->get_schedule_type() : -1;
	if (led) {
		const int prev = controlled_actor_previous_schedule;
		led->set_schedule_type(prev >= 0 && prev != Schedule::wait ? prev : Schedule::follow_avatar);
	}
	write_npcs();              // Write out npc.dat.
	if (led && led->get_schedule_type() != led_schedule) {
		led->set_schedule_type(led_schedule);
	}
	for (size_t i = 0; i < remote_friends.size(); ++i) {
		if (remote_friends[i].actor->get_schedule_type() != held_schedules[i]) {
			remote_friends[i].actor->set_schedule_type(held_schedules[i]);
		}
	}
	Friend_mode_saving(false);
	usecode->write();          // Usecode.dat (party, global flags).
	Notebook_gump::write();    // Write out journal.
	write_gwin();              // Write our data.
	write_saveinfo(!nopaint);
}

/*
 *  Restore game by reading in 'gamedat'.
 *
 *  Output: 0 if error, already reported.
 */

void Game_window::read() {
	Game_window::Scoped_ui_layer_mask load_layers(this, 0);
	Audio::get_ptr()->cancel_streams();
	// Display red plasma during load...
	setup_load_palette();

	clear_world(true);    // Wipe clean.

	// Re-add background noise to queue after clear_world() removed it
	if (background_noise) {
		tqueue->add(Game::get_ticks() + 5000, background_noise);
	}

	read_gwin();    // Read our data.
	// DON'T do anything that might paint()
	// before calling read_npcs!!
	setup_game(cheat.in_map_editor());    // Read NPC's, usecode.
	Mouse::mouse()->set_speed_cursor();
	Friend_mode_note_world_loaded();
}

/*
 *  Write data for the game.
 *
 *  Output: 0 if error.
 */

void Game_window::write_gwin() {
	OFileDataSource gout(GWINDAT);
	// Start with scroll coords (in tiles).
	gout.write2(get_scrolltx());
	gout.write2(get_scrollty());
	// Write clock.
	gout.write2(clock->get_day());
	gout.write2(clock->get_hour());
	gout.write2(clock->get_minute());
	gout.write4(special_light);    // Write spell expiration minute.
	MyMidiPlayer* player = Audio::get_ptr()->get_midi();
	if (player) {
		gout.write4(static_cast<uint32>(player->get_current_track()));
		gout.write4(static_cast<uint32>(player->is_repeating()) | player->get_egg_count() << 16);
	} else {
		gout.write4(static_cast<uint32>(-1));
		gout.write4(0);
	}
	gout.write1(armageddon ? 1 : 0);
	gout.write1(ambient_light ? 1 : 0);
	gout.write1(combat ? 1 : 0);
	gout.write1(infravision_active ? 1 : 0);
	if (!gout.good()) {
		throw file_write_exception(GWINDAT);
	}
}

/*
 *  Read data for the game.
 *
 *  Output: 0 if error.
 */

void Game_window::read_gwin() {
	if (!clock->in_queue()) {    // Be sure clock is running.
		tqueue->add(Game::get_ticks(), clock, this);
	}

	IFileDataSource gin(GWINDAT);
	if (!gin.good()) {
		return;
	}

	// Start with scroll coords (in tiles).
	scrolltx_lp = scrolltx_l = scrolltx = gin.read2();
	scrollty_lp = scrollty_l = scrollty = gin.read2();
	scrolltx_lo = scrollty_lo = 0;
	avposx_ld = avposy_ld = 0;
	// Read clock.
	clock->reset();
	clock->set_day(gin.read2());
	clock->set_hour(gin.read2());
	clock->set_minute(gin.read2());
	if (!gin.good()) {    // Next ones were added recently.
		throw file_read_exception(GWINDAT);
	}
	special_light = gin.read4();
	armageddon    = false;    // Old saves may not have this yet.

	if (!gin.good()) {
		special_light = 0;
		return;
	}

	const int track_num = gin.read4();
	const int repeat    = gin.read4();
	if (!gin.good()) {
		Audio::get_ptr()->stop_music();
		return;
	}
	MyMidiPlayer* midi = Audio::get_ptr()->get_midi();
	if (!is_background_track(track_num) || (midi && (midi->get_ogg_enabled() || midi->is_mt32()))) {
		Audio::get_ptr()->start_music(track_num, repeat & 0x1);
		if (midi) {
			midi->set_egg_count(static_cast<uint16>(repeat >> 16));
		}
	}
	armageddon = gin.read1() == 1;
	if (!gin.good()) {
		armageddon = false;
	}

	ambient_light = gin.read1() == 1;
	if (!gin.good()) {
		ambient_light = false;
	}

	combat = gin.read1() == 1;
	if (!gin.good()) {
		combat = false;
	}

	infravision_active = gin.read1() == 1;
	if (!gin.good()) {
		infravision_active = false;
	}
}

/*
 *  Was any map modified?
 */

bool Game_window::was_map_modified() {
	if (Game_map::was_chunk_terrain_modified()) {
		return true;
	}
	for (auto* map : maps) {
		if (map && map->was_map_modified()) {
			return true;
		}
	}
	return false;
}

/*
 *  Write out map data (IFIXxx, U7CHUNKS, U7MAP) to static, and also
 *  save 'gamedat' to <PATCH>/initgame.dat.
 *
 *  Note: This is for map-editing.
 *
 *  Output: Errors are reported.
 */

void Game_window::write_map() {
	for (auto* map : maps) {
		if (map && map->was_map_modified()) {
			map->write_static();    // Write ifix, map files.
		}
	}
	write();    // Write out to 'gamedat' too.
	save_gamedat(PATCH_INITGAME, "Saved map");
}

/*
 *  Reinitialize game from map.
 */

void Game_window::read_map() {
	init_gamedat(true);    // Unpack 'initgame.dat'.
	read();                // This does the whole restore.
}

/*
 *  Reload (patched) usecode.
 */

void Game_window::reload_usecode() {
	// Get custom usecode functions.
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_USECODE)) {
		auto pFile = U7open_in(PATCH_USECODE);
		if (!pFile) {
			return;
		}
		auto& file = *pFile;
		usecode->read_usecode(file, true);
	}
}

/*
 *  Shift view by one tile.
 */

void Game_window::view_right() {
	const int w = get_width();
	const int h = get_height();
	// Get current rightmost chunk.
	const int old_rcx = ((scrolltx + (w - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	scrolltx          = INCR_TILE(scrolltx);
	scroll_bounds.x   = INCR_TILE(scroll_bounds.x);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	// Shift image to left.
	win->copy(c_tilesize, 0, w - c_tilesize, h, 0, 0);
	// Paint 1 column to right.
	paint(w - c_tilesize, 0, c_tilesize, h);
	dirty.x -= c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_rcx = ((scrolltx + (w - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	if (new_rcx != old_rcx) {
		Send_location(this);
	}
}

void Game_window::view_left() {
	const int old_lcx = (scrolltx / c_tiles_per_chunk) % c_num_chunks;
	// Want to wrap.
	scrolltx        = DECR_TILE(scrolltx);
	scroll_bounds.x = DECR_TILE(scroll_bounds.x);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	win->copy(0, 0, get_width() - c_tilesize, get_height(), c_tilesize, 0);
	const int h = get_height();
	paint(0, 0, c_tilesize, h);
	dirty.x += c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_lcx = (scrolltx / c_tiles_per_chunk) % c_num_chunks;
	if (new_lcx != old_lcx) {
		Send_location(this);
	}
}

void Game_window::view_down() {
	const int w = get_width();
	const int h = get_height();
	// Get current bottomost chunk.
	const int old_bcy = ((scrollty + (h - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	scrollty          = INCR_TILE(scrollty);
	scroll_bounds.y   = INCR_TILE(scroll_bounds.y);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	win->copy(0, c_tilesize, w, h - c_tilesize, 0, 0);
	paint(0, h - c_tilesize, w, c_tilesize);
	dirty.y -= c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_bcy = ((scrollty + (h - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	if (new_bcy != old_bcy) {
		Send_location(this);
	}
}

void Game_window::view_up() {
	const int old_tcy = (scrollty / c_tiles_per_chunk) % c_num_chunks;
	// Want to wrap.
	scrollty        = DECR_TILE(scrollty);
	scroll_bounds.y = DECR_TILE(scroll_bounds.y);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	const int w = get_width();
	win->copy(0, 0, w, get_height() - c_tilesize, 0, c_tilesize);
	paint(0, 0, w, c_tilesize);
	dirty.y += c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_tcy = (scrollty / c_tiles_per_chunk) % c_num_chunks;
	if (new_tcy != old_tcy) {
		Send_location(this);
	}
}

/*
 *  Get gump being dragged.
 */

Gump* Game_window::get_dragging_gump() {
	return dragging ? dragging->gump : nullptr;
}

/*
 *  Alternative start actor function
 *  Placed in an alternative function to prevent breaking barges
 */
/*
 *  Friend Mode: walking into a closed door opens it, from standing as well
 *  as on the move.  Exult opened doors only when a walk already under way
 *  met one, so from in front of a door the walker slid sideways or
 *  stopped.  A guest asks the host, which opens it for everyone.  True if
 *  'tile' has a closed door (opened now, asked for, or locked).
 */

// A door opened by walking into it (WASD, arrow keys): closed again once
// the walker is through it, as a mouse walk does (Path_walking_actor_action).
namespace {
struct Door_behind {
	Game_object_weak door;
	Game_object_weak walker;
	Tile_coord       tile;          // The door's tile when closed.
	TileRect         foot;          // Its footprint when closed.
	bool             horizontal = false;
	int              side       = 0;    // The walker's side when it opened: -1 or 1.
	uint32           opened     = 0;
};

Door_behind door_behind;

// Which side of the door a tile is on: -1 or 1, or 0 in the doorway.
int Door_side(const Door_behind& d, const Tile_coord& t) {
	const int at    = d.horizontal ? t.ty : t.tx;
	const int start = d.horizontal ? d.foot.y : d.foot.x;
	const int size  = d.horizontal ? d.foot.h : d.foot.w;
	return at < start ? -1 : (at >= start + size ? 1 : 0);
}

// Toggle a door as NPCs do: quality 0, so only the door's own toggle runs;
// a guest asks the host.
void Toggle_door(Game_window* gwin, Actor* actor, Game_object* door) {
	if (gwin->has_network_friend_control() && actor != gwin->get_main_actor()) {
		Friend_mode_request_object_use(door);
		return;
	}
	const int savequal = door->get_quality();
	door->set_quality(0);
	door->activate();
	door->set_quality(savequal);
}
}    // namespace

void Friend_close_door_behind() {
	if (!door_behind.side) {
		return;
	}
	auto   walker = door_behind.walker.lock();
	Actor* actor  = walker ? walker->as_actor() : nullptr;
	if (!actor || SDL_GetTicks() - door_behind.opened > 20000
		|| actor->get_tile().distance_2d(door_behind.tile) > 8) {
		door_behind = Door_behind();    // Gone the other way, or away.
		return;
	}
	if (Door_side(door_behind, actor->get_tile()) != -door_behind.side) {
		return;    // Not through yet.
	}
	// Not on anyone standing in the doorway.
	Actor_vector near;
	Game_object::find_nearby_actors(near, door_behind.tile, c_any_shapenum, 4);
	for (const auto* other : near) {
		const Tile_coord t = other->get_tile();
		if (!other->is_dead() && door_behind.foot.has_point(t.tx, t.ty)) {
			return;
		}
	}
	Game_object_shared door = door_behind.door.lock();
	if (!door) {
		// A guest's world may have been refreshed from the host's: the
		// open door nearest where it was.
		Game_object_vector found;
		Game_object::find_nearby(found, door_behind.tile, c_any_shapenum, 2, 0);
		for (auto* obj : found) {
			if (obj->get_info().is_door() && !obj->is_closed_door() && !obj->get_owner()) {
				door = obj->shared_from_this();
				break;
			}
		}
	}
	door_behind = Door_behind();
	if (door && door->get_info().is_door() && !door->is_closed_door()) {
		Toggle_door(Game_window::get_instance(), actor, door.get());
	}
}

static bool Open_door_ahead(Game_window* gwin, Actor* actor, const Tile_coord& tile) {
	Game_object* door = Game_object::find_door(tile);
	if (!door || !door->is_closed_door()) {
		return false;
	}
	// Once per door every 1.5 seconds: opening toggles it, and a guest's
	// door opens only when the host's answer comes back.
	static Game_object_weak last_door;
	static uint32           last_time = 0;
	const uint32            now       = SDL_GetTicks();
	if (last_door.lock().get() == door && now - last_time < 1500) {
		return true;
	}
	last_door = weak_from_obj(door);
	last_time = now;
	if (door->get_framenum() % 4 >= 2) {
		gwin->get_effects()->add_text("Locked", door);
		return true;
	}
	// Remember it, to close it behind the walker.
	door_behind.door       = weak_from_obj(door);
	door_behind.walker     = weak_from_obj(actor);
	door_behind.tile       = door->get_tile();
	door_behind.foot       = door->get_footprint();
	door_behind.horizontal = door_behind.foot.w > door_behind.foot.h;
	door_behind.side       = Door_side(door_behind, actor->get_tile());
	door_behind.opened     = now;
	Toggle_door(gwin, actor, door);
	return true;
}

void Game_window::start_actor_alt(
		int winx, int winy,    // Mouse position to aim for.
		int speed              // Msecs. between frames.
) {
	if (network_input_blocked()) {
		return;
	}
	Actor* actor = get_controlled_actor();
	// Avatar can move, don't sync the barge anymore.
	if (actor == main_actor) {
		landing_barge = nullptr;
	}
	int  ax;
	int  ay;
	bool blocked[8];
	get_shape_location(actor, ax, ay);

	Tile_coord start = actor->get_tile();
	int        dir;
	const bool checkdrop = (actor->get_type_flags() & MOVE_LEVITATE) == 0;

	for (dir = 0; dir < 8; dir++) {
		Tile_coord dest = start.get_neighbor(dir);
		blocked[dir]    = actor->is_blocked(dest, &start, actor->get_type_flags());
		if (checkdrop && abs(start.tz - dest.tz) > 1) {
			blocked[dir] = true;
		}
	}

	dir = Get_direction_NoWrap(ay - winy, winx - ax);

	if (blocked[dir] && !cheat.in_map_editor() && Open_door_ahead(this, actor, start.get_neighbor(dir))) {
		return;    // Walks through once it is open.
	}
	if (blocked[dir] && !blocked[(dir + 1) % 8]) {
		dir = (dir + 1) % 8;
	} else if (blocked[dir] && !blocked[(dir + 7) % 8]) {
		dir = (dir + 7) % 8;
	} else if (blocked[dir]) {
		Game_object* block = actor->is_moving() ? nullptr : actor->find_blocking(start.get_neighbor(dir), dir);
		// We already know the blocking object isn't the avatar, so don't
		// double check it here.
		if (!block || !block->move_aside(actor, dir)) {
			stop_actor();
			if (actor->get_lift() % 5) {    // Up on something?
				// See if we're stuck in the air.
				const int savetz = start.tz;
				if (!Map_chunk::is_blocked(start, 1, MOVE_WALK, 100) && start.tz < savetz) {
					actor->move(start.tx, start.ty, start.tz);
				}
			}
			return;
		}
	}

	const int delta = step_tile_delta * c_tilesize;    // Bigger # here avoids jerkiness,
	// but causes probs. with followers.
	switch (dir) {
	case north:
		// cout << "NORTH" << endl;
		ay -= delta;
		break;

	case northeast:
		// cout << "NORTH EAST" << endl;
		ay -= delta;
		ax += delta;
		break;

	case east:
		// cout << "EAST" << endl;
		ax += delta;
		break;

	case southeast:
		// cout << "SOUTH EAST" << endl;
		ay += delta;
		ax += delta;
		break;

	case south:
		// cout << "SOUTH" << endl;
		ay += delta;
		break;

	case southwest:
		// cout << "SOUTH WEST" << endl;
		ay += delta;
		ax -= delta;
		break;

	case west:
		// cout << "WEST" << endl;
		ax -= delta;
		break;

	case northwest:
		// cout << "NORTH WEST" << endl;
		ay -= delta;
		ax -= delta;
		break;
	}

	const int lift       = actor->get_lift();
	const int liftpixels = 4 * lift;    // Figure abs. tile.
	int       tx         = get_scrolltx() + (ax + liftpixels) / c_tilesize;
	int       ty         = get_scrollty() + (ay + liftpixels) / c_tilesize;
	// Wrap:
	tx = (tx + c_num_tiles) % c_num_tiles;
	ty = (ty + c_num_tiles) % c_num_tiles;
	actor->walk_to_tile(tx, ty, lift, speed, 0);
	if (actor != main_actor) {
		if (actor == get_party_leader()) {
			// A companion one leads: in formation, as the Avatar would.
			if (walk_in_formation && actor->get_action()) {
				actor->get_action()->set_get_party(true);
			} else {
				get_leader_followers();
			}
		}
		return;
	}
	if (walk_in_formation && main_actor->get_action()) {
		//++++++In this case, may need to set schedules back to
		// follow_avatar after, say, sitting.++++++++++
		main_actor->get_action()->set_get_party(true);
	} else {    // "Traditional" Exult walk:-)
		main_actor->get_followers();
	}
}

/*
 *  Start the actor.
 */

void Game_window::start_actor(
		int winx, int winy,    // Mouse position to aim for.
		int speed              // Msecs. between frames.
) {
	if (network_input_blocked()) {
		return;
	}
	Actor* actor = get_controlled_actor();
	if (actor->get_flag(Obj_flags::asleep)) {
		return;    // Zzzzz....
	}
	if (!cheat.in_map_editor() && (actor->in_usecode_control() || actor->get_flag(Obj_flags::paralyzed))) {
		return;
	}
	if (gump_man->gump_mode() && !gump_man->gumps_dont_pause_game()) {
		return;
	}
	//	teleported = 0;
	if (moving_barge && actor == main_actor) {
		// Want to move center there.
		const int lift       = main_actor->get_lift();
		const int liftpixels = 4 * lift;    // Figure abs. tile.
		int       tx         = get_scrolltx() + (winx + liftpixels) / c_tilesize;
		int       ty         = get_scrollty() + (winy + liftpixels) / c_tilesize;
		// Wrap:
		tx                     = (tx + c_num_tiles) % c_num_tiles;
		ty                     = (ty + c_num_tiles) % c_num_tiles;
		const Tile_coord atile = moving_barge->get_center();
		const Tile_coord btile = moving_barge->get_tile();
		// Go faster than walking.
		moving_barge->travel_to_tile(Tile_coord(tx + btile.tx - atile.tx, ty + btile.ty - atile.ty, btile.tz), speed / 2);
	} else {
		/*
		main_actor->walk_to_tile(tx, ty, lift, speed, 0);
		main_actor->get_followers();
		*/
		// Set schedule.
		const int sched = actor->get_schedule_type();
		if (actor != main_actor) {
			start_actor_alt(winx, winy, speed);
			return;
		}
		if (sched != Schedule::follow_avatar && sched != Schedule::combat && !main_actor->get_flag(Obj_flags::asleep)) {
			main_actor->set_schedule_type(Schedule::follow_avatar);
			// If the avatar was *not* in combat or follow avatar schedules,
			// then we need to reset the schedules of party members when the
			// Avatar moves.
			const int cnt = party_man->get_count();
			for (int i = 0; i < cnt; i++) {
				Actor* npc = get_npc(party_man->get_member(i));
				// Not sure how much of this is needed, but in any case...
				if (npc == nullptr || npc->get_flag(Obj_flags::asleep) || npc->is_dead()) {
					continue;
				}
				npc->set_schedule_type(Schedule::follow_avatar);
			}
		}
		// Going to use the alternative function for this at the moment
		start_actor_alt(winx, winy, speed);
	}
}

/*
 *  Find path to where user double-right-clicked.
 */

void Game_window::start_actor_along_path(
		int winx, int winy,    // Mouse position to aim for.
		int speed              // Msecs. between frames.
) {
	if (network_input_blocked()) {
		return;
	}
	Actor* actor = get_controlled_actor();
	if (actor->get_flag(Obj_flags::asleep) || actor->get_flag(Obj_flags::paralyzed)
		|| actor->get_schedule_type() == Schedule::sleep || (moving_barge && actor == main_actor)) {    // For now, don't do barges.
		return;                                                                     // Zzzzz....
	}
	// Animation in progress?
	if (!cheat.in_map_editor() && actor->in_usecode_control()) {
		return;
	}
	//	teleported = 0;
	const int        lift       = actor->get_lift();
	const int        liftpixels = 4 * lift;    // Figure abs. tile.
	const Tile_coord dest(
			get_scrolltx() + (winx + liftpixels) / c_tilesize, get_scrollty() + (winy + liftpixels) / c_tilesize, lift);
	if (!actor->walk_path_to_tile(dest, speed)) {
		cout << "Couldn't find path for controlled actor." << endl;
		if (touch_pathfind) {
			Mouse::mouse()->flash_shape(Mouse::blocked);
		}
	} else {
		if (touch_pathfind) {
			get_effects()->add_effect(std::make_unique<Sprites_effect>(18, dest, 0, 0, 0, 0));
		}
		if (actor == main_actor) {
			main_actor->get_followers();
		} else if (actor == get_party_leader()) {
			get_leader_followers();
		}
	}
}

Actor* Game_window::get_party_leader() const {
	if (controlled_actor && controlled_actor != main_actor && !network_friend_control) {
		return controlled_actor;
	}
	return main_actor;
}

void Game_window::get_leader_followers() {
	Actor* leader = get_party_leader();
	if (leader == main_actor) {
		main_actor->get_followers();
		return;
	}
	// The party and the Avatar, as Main_actor::get_followers does for the
	// Avatar.
	auto get = [leader](Actor* npc) {
		if (!npc || npc == leader || npc->get_flag(Obj_flags::asleep) || npc->is_dead()) {
			return;
		}
		const int sched = npc->get_schedule_type();
		if (sched == Schedule::combat || sched == Schedule::wait || sched == Schedule::loiter) {
			return;
		}
		if (sched != Schedule::follow_avatar) {
			npc->set_schedule_type(Schedule::follow_avatar);
		} else {
			npc->follow(leader);
		}
	};
	for (int i = 0; i < party_man->get_count(); ++i) {
		get(get_npc(party_man->get_member(i)));
	}
	get(main_actor);
}

/*
 *  Stop the actor.
 */

void Game_window::stop_actor() {
	Actor* actor = get_controlled_actor();
	if (moving_barge && actor == main_actor) {
		moving_barge->stop();
	} else {
		actor->stop();    // Stop and set resting state.
		if (!gump_man->gump_mode() || gump_man->gumps_dont_pause_game()) {
			//+++++++The following line is for testing:
			//			if (!walk_in_formation)
			if (actor == main_actor) {
				main_actor->get_followers();
			} else if (actor == get_party_leader()) {
				get_leader_followers();
			}
		}
	}
}

/*
 *  Teleport the party.
 */

void Game_window::teleport_party(
		const Tile_coord& t,            // Where to go.
		bool              skip_eggs,    // Don't activate eggs at dest.
		int               newmap,       // New map #, or -1 for same map.
		bool              no_status_check) {
	const Tile_coord oldpos = main_actor->get_tile();
	main_actor->set_action(nullptr);    // Definitely need this, or you may
	//   step back to where you came from.
	moving_barge  = nullptr;    // Calling 'done()' could be risky...
	landing_barge = nullptr;
	int       i;
	const int cnt = party_man->get_count();
	if (!skip_eggs) {
		main_actor->get_chunk()->unhatch_eggs(main_actor, t.tx, t.ty, t.tz, oldpos.tx, oldpos.ty);
	}
	if (newmap != -1) {
		set_map(newmap);
	}
	main_actor->move(t.tx, t.ty, t.tz, newmap);    // Move Avatar.
	// Fixes a rare crash when moving between maps and teleporting:
	newmap = main_actor->get_map_num();
	center_view(t);    // Bring pos. into view, and insure all
	clock->reset_palette();
	//   objs. exist.
	for (i = 0; i < cnt; i++) {
		const int party_member = party_man->get_member(i);
		Actor*    person       = get_npc(party_member);
		// A Friend Mode guest's companion waits on the host but still travels,
		// as does the companion the host leads.
		if (person && !person->is_dead()
			&& (person->get_schedule_type() != Schedule::wait || is_remote_friend(person) || person == controlled_actor
				|| Friend_mode_is_held(person))
			&& (person->can_act() || no_status_check)) {
			person->set_action(nullptr);
			const Tile_coord t1 = Map_chunk::find_spot(t, 8, person->get_shapenum(), person->get_framenum(), 1);
			if (t1.tx != -1) {
				person->move(t1, newmap);
				person->change_frame(person->get_dir_framenum(Actor::standing));
			}
		}
	}
	main_actor->get_followers();
	if (!skip_eggs) {    // Check all eggs around new spot.
		Map_chunk::try_all_eggs(main_actor, t.tx, t.ty, t.tz, oldpos.tx, oldpos.ty);
	}
	//	teleported = 1;
}

/*
 *  Get party members.
 *
 *  Output: Number returned in 'list'.
 */

int Game_window::get_party(
		Actor** a_list,       // Room for 9.
		int     avatar_too    // 1 to include Avatar too.
) {
	int n = 0;
	if (avatar_too && main_actor) {
		a_list[n++] = main_actor;
	}
	const int cnt = party_man->get_count();
	for (int i = 0; i < cnt; i++) {
		const int party_member = party_man->get_member(i);
		Actor*    person       = get_npc(party_member);
		if (person) {
			a_list[n++] = person;
		}
	}
	return n;    // Return # actually stored.
}

/*
 *  Find a given shaped item amongst the party, and 'activate' it.  This
 *  is used, for example, by the 'f' command to feed.
 *
 *  Output: True if the item was found, else false.
 */

bool Game_window::activate_item(
		int shnum,    // Desired shape.
		int frnum,    // Desired frame
		int qual      // Desired quality
) {
	if (Friend_mode_is_guest()) {
		// Keyboard shortcuts (food, watch, lockpicks...) use the guest's own
		// companion's items, through the host like a double-click.
		Actor* own = network_friend_control ? controlled_actor : nullptr;
		Game_object* obj = own && own != main_actor ? own->find_item(shnum, qual, frnum) : nullptr;
		return obj && Friend_mode_guest_activate(obj);
	}
	Actor*    party[9];    // Get party.
	const int cnt = get_party(party, 1);
	for (int i = 0; i < cnt; i++) {
		Actor*       person = party[i];
		Game_object* obj    = person->find_item(shnum, qual, frnum);
		if (obj) {
			obj->activate();
			return true;
		}
	}
	// Special case: Archwizard mode spellbook - create a temporary one with
	// all spells.
	if (shnum == 761 && cheat.in_wizard_mode()) {
		static unsigned char all_spells[9]    = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		static auto          wizard_spellbook = std::make_unique<Spellbook_object>(761, 0, 0, 0, 0, all_spells, 255);
		wizard_spellbook->activate();
		return true;
	}
	return false;
}

/*
 *  Find the top object that can be selected, dragged, or activated.
 *  The one returned is the 'highest'.
 *
 *  Output: ->object, or null if none.
 */

Game_object* Game_window::find_object(
		int x, int y    // Pos. on screen.
) {
#ifdef DEBUG
	cout << "Clicked at tile (" << get_scrolltx() + x / c_tilesize << ", " << get_scrollty() + y / c_tilesize << ")" << endl;
#endif
	const int not_above = get_render_skip_lift();
	// Figure chunk #'s.
	const int start_cx = ((scrolltx + x / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	const int start_cy = ((scrollty + y / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	// Check 1 chunk down & right too.
	const int stop_cx = (2 + (scrolltx + (x + 4 * not_above) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	const int stop_cy = (2 + (scrollty + (y + 4 * not_above) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;

	Game_object* best  = nullptr;    // Find 'best' one.
	bool         trans = true;       // Try to avoid 'transparent' objs.
	// Go through them.
	for (int cy = start_cy; cy != stop_cy; cy = INCR_CHUNK(cy)) {
		for (int cx = start_cx; cx != stop_cx; cx = INCR_CHUNK(cx)) {
			Map_chunk* olist = map->get_chunk(cx, cy);
			if (!olist) {
				continue;
			}
			Object_iterator next(olist->get_objects());
			Game_object*    obj;
			while ((obj = next.get_next()) != nullptr) {
				if (obj->get_lift() >= not_above || !get_shape_rect(obj).has_world_point(x, y) || !obj->is_findable()) {
					continue;
				}
				// Check the shape itself.
				Shape_frame* s = obj->get_shape();
				int          ox;
				int          oy;
				get_shape_location(obj, ox, oy);
				if (!s->has_point(x - ox, y - oy)) {
					continue;
				}
				// Fixes key under rock in BG at [915, 2434, 0]; need to
				// know if there are side effects.
				if (!best || best->lt(*obj) != 0 || trans) {
					bool ftrans = obj->get_info().is_transparent();
					// TODO: Fix this properly, instead of with an ugly hack.
					// This fixes clicking the Y shapes instead of the Y
					// depression in SI. This has the effect of also making
					// the Y depressions not draggable.
					if (GAME_SI && obj->get_shapenum() == 0xd1 && obj->get_framenum() == 17) {
						ftrans = true;    // Pretend it is transparent
					}
					if (!ftrans || trans) {
						best  = obj;
						trans = ftrans;
					}
				}
			}
		}
	}
	return best;
}

void Game_window::find_nearby_objects(Game_object_map_xy& mobjxy, int x, int y, Gump* gump) {
	// Find object at each pixel
	for (int iy = y - 10; iy < (y + 10); iy++) {
		for (int ix = x - 10; ix < (x + 10); ix++) {
			Game_object* iobj;
			if (gump) {
				iobj = gump->find_object(ix, iy);
			} else {
				iobj = find_object(ix, iy);
			}

			if (iobj) {
				const Shape_info& info = iobj->get_info();
				auto              cl   = info.get_shape_class();
				// Filter list a bit.
				if (cl == Shape_info::unusable || cl == Shape_info::building) {
					continue;
				}
				if ((cl == Shape_info::hatchable || cl == Shape_info::barge || info.is_barge_part()) && !cheat.in_map_editor()) {
					continue;
				}
				if (info.is_transparent()) {
					continue;
				}
				mobjxy[iobj] = Position2d{ix, iy};
			}
		}
	}
}

static inline string Get_object_name(const Game_object* obj) {
	string name;
	if (obj == Game_window::get_instance()->get_main_actor()) {
		if (GAME_BG) {
			name = get_misc_name(0x42);
		} else if (GAME_SI) {
			name = get_misc_name(0x4e);
		} else {
			name = obj->get_name();
		}
	} else {
		name = obj->get_name();
	}
	return Friend_mode_display_name(obj, name);
}

/*
 *  Show the name of the item the mouse is clicked on.
 */

void Game_window::show_items(
		int x, int y,    // Coords. in window.
		bool ctrl        // Control key is pressed.
) {
	// Look for obj. in open gump.
	Gump* gump = gump_man->find_gump(x, y);
	int   gx;
	int   gy;
	gump_man->map_game_to_gump(gump, x, y, gx, gy);
	Game_object* obj;    // What we find.
	bool         found_in_gump = false;
	if (gump) {
		obj           = gump->find_object(gx, gy);
		found_in_gump = (obj != nullptr);
		if (!obj) {
			obj = gump->get_cont_or_actor(gx, gy);
		}
	} else {    // Search rest of world.
		obj = find_object(x, y);
	}

	if (item_menu) {
		Game_object_map_xy mobjxy;
		find_nearby_objects(mobjxy, x, y, gump);
		if (!mobjxy.empty() && Notebook_gump::get_instance() == nullptr) {
			// Make sure menu is visible on the screen
			Itemmenu_gump itemgump(&mobjxy, x, y);
			Game_window::get_instance()->get_gump_man()->do_modal_gump(&itemgump, Mouse::hand, itemgump.get_outline_painter());
			obj = nullptr;
		}
	}

	// Map-editing?
	if (obj && cheat.in_map_editor()) {
		if (ctrl) {    // Control?  Toggle.
			cheat.toggle_selected(obj);
		} else {
			// In normal mode, sel. just this one.
			cheat.clear_selected();
			if (cheat.get_edit_mode() == Cheat::move) {
				cheat.append_selected(obj);
			}
		}
	} else {    // All other cases:  unselect.
		cheat.clear_selected();
	}

	// Do we have an NPC?
	Actor* npc = obj ? obj->as_actor() : nullptr;
	if (npc && cheat.number_npcs() && (npc->get_npc_num() > 0 || npc == main_actor)) {
		char              str[64];
		const std::string namestr = Get_object_name(obj);
		snprintf(str, sizeof(str), "(%i) %s", npc->get_npc_num(), namestr.c_str());
		effects->add_text(str, obj);
	} else if (obj) {
		// Show name.  If clicking empty space in a gump with a custom name,
		// use that instead of the container object's name.
		std::string namestr;
		const char* gname = (gump && !found_in_gump) ? gump->get_click_name() : nullptr;
		if (gname) {
			namestr = gname;
		} else {
			namestr = Get_object_name(obj);
		}
		if (Game_window::get_instance()->failed_copy_protection() && (npc == main_actor || !npc)) {    // Avatar and items
			namestr = Strings::Oink();
		}
		// Combat and an NPC?
		if (in_combat() && Combat::mode != Combat::original && npc) {
			char buf[128];
			snprintf(buf, sizeof(buf), " (%d)", npc->get_property(Actor::health));
			namestr += buf;
		}
		effects->add_text(namestr.c_str(), obj);
	} else if (cheat.in_map_editor() && skip_lift > 0) {
		// Show flat, but not when editing ter.
		const ShapeID id = get_flat(x, y);
		char          str[20];
		snprintf(str, sizeof(str), "Flat %d:%d", id.get_shapenum(), id.get_framenum());
		effects->add_text(str, x, y);
	}
	// If it's an actor and we want to grab the actor, grab it.
	if (npc && cheat.grabbing_actor() && (npc->get_npc_num() || npc == main_actor)) {
		cheat.set_grabbed_actor(npc);
	}

#ifdef DEBUG
	int shnum;
	if (obj) {
		shnum                   = obj->get_shapenum();
		const int         frnum = obj->get_framenum();
		const Shape_info& info  = obj->get_info();
		cout << "Object " << shnum << ':' << frnum << " has 3d tiles (x, y, z): " << info.get_3d_xtiles(frnum) << ", "
			 << info.get_3d_ytiles(frnum) << ", " << info.get_3d_height();
		Actor* npc = obj->as_actor();
		if (npc) {
			cout << ", sched = " << npc->get_schedule_type() << ", real align = " << npc->get_alignment()
				 << ", eff. align = " << npc->get_effective_alignment() << ", npcnum = " << npc->get_npc_num();
		}
		cout << endl;
		if (obj->get_chunk()) {
			const Tile_coord t = obj->get_tile();
			cout << "tx = " << t.tx << ", ty = " << t.ty << ", tz = " << t.tz << ", ";
		}
		cout << "quality = " << obj->get_quality() << ", okay_to_take = " << obj->get_flag(Obj_flags::okay_to_take)
			 << ", flag0x1d = " << obj->get_flag(0x1d) << ", hp = " << obj->get_obj_hp() << ", weight = " << obj->get_weight()
			 << ", volume = " << obj->get_volume() << endl;
		cout << "obj = " << obj << endl;
		if (obj->get_flag(Obj_flags::asleep)) {
			cout << "ASLEEP" << endl;
		}
		if (obj->is_egg()) {    // Show egg info. here.
			obj->as_egg()->print_debug();
		}
	} else {    // Obj==0
		const ShapeID id = get_flat(x, y);
		shnum            = id.get_shapenum();
		cout << "Clicked on flat shape " << shnum << ':' << id.get_framenum() << endl;

#	ifdef CHUNK_OBJ_DUMP
		Map_chunk* chunk = map->get_chunk_safely(x / c_tiles_per_chunk, y / c_tiles_per_chunk);
		if (chunk) {
			Object_iterator it(chunk->get_objects());
			Game_object*    each;
			cout << "Chunk Contents: " << endl;
			while ((each = it.get_next()) != nullptr) {
				cout << "    " << each->get_name() << ":" << each->get_shapenum() << ":" << each->get_framenum() << endl;
			}
		}
#	endif
		if (id.is_invalid()) {
			return;
		}
	}
	const Shape_info& info = ShapeID::get_info(shnum);
	cout << "TFA[1][0-6]= " << (info.get_tfa(1) & 127) << endl;
	cout << "TFA[0][0-1]= " << (info.get_tfa(0) & 3) << endl;
	cout << "TFA[0][3-4]= " << ((info.get_tfa(0) >> 3) & 3) << endl;
	if (info.is_animated()) {
		cout << "Object is ANIMATED" << endl;
	}
	if (info.has_translucency()) {
		cout << "Object has TRANSLUCENCY" << endl;
	}
	if (info.is_transparent()) {
		cout << "Object is TRANSPARENT" << endl;
	}
	if (info.is_light_source()) {
		cout << "Object is LIGHT_SOURCE" << endl;
	}
	if (info.is_door()) {
		cout << "Object is a DOOR" << endl;
	}
	if (info.is_solid()) {
		cout << "Object is SOLID" << endl;
	}
#endif
}

/*
 *  Handle right click when combat is paused.  The user can right-click
 *  on a party member, then on an enemy to attack.
 */

void Game_window::paused_combat_select(
		int x, int y    // Coords in window.
) {
	Gump* gump = gump_man->find_gump(x, y);
	if (gump) {
		return;    // Ignore if clicked on gump.
	}
	Game_object* obj = find_object(x, y);
	Actor*       npc = obj ? obj->as_actor() : nullptr;
	if (!npc || !npc->is_in_party() || npc->get_flag(Obj_flags::asleep) || npc->is_dead() || npc->get_flag(Obj_flags::paralyzed)
		|| !npc->can_act_charmed()) {
		return;    // Want an active party member.
	}
	npc->paint_outline(PROTECT_PIXEL);
	show(true);    // Flash white outline.
	SDL_Delay(100);
	npc->add_dirty();
	paint_dirty();
	show();
	// Pick a spot.
	if (!Get_click(x, y, Mouse::greenselect, nullptr, true)) {
		return;
	}
	obj = find_object(x, y);    // Find it.
	if (!obj) {                 // Nothing?  Walk there.
		// Needs work if lift > 0.
		const int        lift       = npc->get_lift();
		const int        liftpixels = 4 * lift;
		const Tile_coord dest(scrolltx + (x + liftpixels) / c_tilesize, scrollty + (y + liftpixels) / c_tilesize, lift);
		// Aim within 1 tile.
		if (!npc->walk_path_to_tile(dest, std_delay, 0, 1)) {
			Mouse::mouse()->flash_shape(Mouse::blocked);
		} else {    // Make sure he's in combat mode.
			npc->set_target(nullptr, true);
		}
		return;
	}
	Actor* target = obj->as_actor();
	// Don't attack party or body.
	if ((target && target->is_in_party()) || obj->get_info().is_body_shape()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		return;
	}
	npc->set_target(obj, true);
	obj->paint_outline(HIT_PIXEL);    // Flash red outline.
	show(true);
	SDL_Delay(100);
	add_dirty(obj);
	paint_dirty();
	show();
	if (npc->get_schedule_type() != Schedule::combat) {
		npc->set_schedule_type(Schedule::combat);
	}
}

/*
 *  Get the 'flat' that a screen point is in.
 */

ShapeID Game_window::get_flat(
		int x, int y    // Window point.
) {
	int       tx     = (get_scrolltx() + x / c_tilesize) % c_num_tiles;
	int       ty     = (get_scrollty() + y / c_tilesize) % c_num_tiles;
	const int cx     = tx / c_tiles_per_chunk;
	const int cy     = ty / c_tiles_per_chunk;
	tx               = tx % c_tiles_per_chunk;
	ty               = ty % c_tiles_per_chunk;
	Map_chunk* chunk = map->get_chunk(cx, cy);
	ShapeID    id    = chunk->get_flat(tx, ty);
	return id;
}

/*
 *  Handle a double-click.
 */

void Game_window::double_clicked(
		int x, int y    // Coords in window.
) {
	if (network_input_blocked()) {
		return;
	}
	// Animation in progress?
	if (main_actor_dont_move()) {
		return;
	}
	// Nothing going on?
	// Look for obj. in open gump.
	Game_object* obj  = nullptr;
	const bool   gump = gump_man->double_clicked(x, y, obj);
	// If gump manager didn't handle it, we search the world for an object
	if (!gump) {
		obj = find_object(x, y);
	}
	double_clicked_object(obj, gump);
}

/*
 *  Act on an object as a double-click does, without the mouse: 'gump' says
 *  it is in an open window.  double_clicked() finds the object under the
 *  mouse and calls this; the Target System's Space calls it directly.
 */

void Game_window::double_clicked_object(Game_object* obj, bool gump) {
	if (network_input_blocked() || main_actor_dont_move()) {
		return;
	}
	const bool avatar_can_act = player_can_act();    // Friend Mode: the one played.
	if (!gump) {
		if (!avatar_can_act && obj && obj->as_actor() && obj->as_actor() == main_actor->as_actor()) {
			ActionFileGump(nullptr);
			return;
		}
		Actor* controlled = get_controlled_actor();
		if (obj && controlled && controlled != main_actor && !obj->as_actor()) {
			Container_game_object* container = obj->as_container();
			if (container) {
				if (!cheat.in_hack_mover() && !Fast_pathfinder_client::is_grabable(controlled, obj)) {
					Mouse::mouse()->flash_shape(Mouse::blocked);
					return;
				}
				// Only the gump-only container path is guest-safe.  Containers
				// requiring Usecode fall through to the Avatar-owned path below.
				if (container->show_gump()) {
					effects->remove_text_effects();
					return;
				}
			}
		}
		// Check path, except if an NPC, sign, or if editing.  Reach is
		// measured from whoever is being controlled, not the Avatar.
		if (obj && !obj->as_actor() && !cheat.in_hack_mover() &&
			//! Is_sign(obj->get_shapenum()) &&
			!Fast_pathfinder_client::is_grabable(controlled ? controlled : main_actor, obj)) {
			Mouse::mouse()->flash_shape(Mouse::blocked);
			return;
		}
	}
	if (!obj || !avatar_can_act) {
		return;    // Nothing found or avatar disabled.
	}
	if (network_friend_control && controlled_actor && controlled_actor != main_actor) {
		// Guest in combat: the host fights for our companion.  Anything
		// that isn't a foe follows the guest's usual rules below.
		if (combat && !gump && !Combat::is_paused() && Friend_mode_guest_attack(obj)) {
			return;
		}
	} else if (combat && !gump &&    // In combat?
		!Combat::is_paused() && player_can_act_charmed() && (!gump_man->gump_mode() || gump_man->gumps_dont_pause_game())) {
		Actor*                        npc  = obj->as_actor();
		const Shape_info&             info = obj->get_info();
		const Shape_info::Shape_class cls  = info.get_shape_class();
		// But don't attack party members.
		if ((!npc || !npc->is_in_party()) &&
			// Or bodies.
			!obj->get_info().is_body_shape()) {
			// And don't attack unlocked doors
			if ((info.is_door() && obj->get_framenum() % 4 < 2) ||
				// or unlocked containers
				(cls == Shape_info::container && !info.is_container_locked())) {
				obj->activate();
				return;
			} else if (controlled_actor && controlled_actor != main_actor) {
				// Leading a companion (Shift+Tab): it fights, not the Avatar;
				// a peaceful person after confirming, as for a guest.
				if (npc) {
					Friend_mode_attack_target(npc);
				}
				return;
			} else {
				// In combat mode.
				// Want everyone to be in combat.
				combat = false;
				main_actor->set_target(obj);
				toggle_combat();
				return;
			}
		}
	}
	Actor* controlled = get_controlled_actor();
	if (gump && controlled && controlled != main_actor && !obj->as_actor()) {
		Container_game_object* container = obj->as_container();
		// A guest's own packs; the host leading a companion, any party
		// member's (the game master).
		Actor* holder = obj->get_outermost()->as_actor();
		if (container && (holder == controlled || (!network_friend_control && holder && holder->is_in_party()))
			&& container->show_gump()) {
			effects->remove_text_effects();
			return;
		}
	}
	Actor* clicked_actor = obj->as_actor();
	if (!gump && controlled && controlled != main_actor && clicked_actor && clicked_actor->is_in_party()) {
		// A guest can inspect only its own paperdoll.  The host (and single
		// player), leading a companion or not, is the game master: all of them.
		if (clicked_actor == controlled || !network_friend_control) {
			effects->remove_text_effects();
			clicked_actor->show_inventory();
		} else {
			Mouse::mouse()->flash_shape(Mouse::redx);
		}
		return;
	}
	if (controlled && controlled != main_actor && network_friend_control) {
		// Food, potions, keys and lockpicks: the host runs them for us.
		const Friend_item_use item_use = clicked_actor ? Friend_item_use::none
													   : Friend_mode_guest_item_use(obj->get_shapenum(), obj->get_framenum());
		if (item_use != Friend_item_use::none) {
			Actor* holder = obj->get_outermost()->as_actor();
			if (holder && holder != controlled) {
				Mouse::mouse()->flash_shape(Mouse::redx);    // Someone else's pack.
				return;
			}
			effects->remove_text_effects();
			Friend_mode_guest_use_item(obj);
			return;
		}
		// Doors, shutters, lamps and similar world toggles are the Usecode
		// objects guests may use: the host uses them and shares the result.
		if (!clicked_actor && !obj->get_owner() && obj->get_info().is_door()) {
			if (obj->get_framenum() % 4 >= 2) {
				effects->add_text("Locked", obj);
			} else {
				Friend_mode_request_object_use(obj);
			}
			return;
		}
		if (!clicked_actor && !obj->get_owner() && Friend_mode_guest_usable_shape(obj->get_shapenum())) {
			Friend_mode_request_object_use(obj);
			return;
		}
		if (!clicked_actor && obj->get_info().is_container_locked()) {
			effects->add_text("Locked", obj);    // A key or lockpick opens it.
			return;
		}
		if (!clicked_actor && Friend_mode_guest_local_shape(obj->get_shapenum())) {
			effects->remove_text_effects();    // Only shows something: run it here.
			usecode->init_conversation();
			// Our companion stands in for the Avatar (the spittoon's "ptui!").
			Actor* const before = Friend_mode_set_usecode_walker(controlled);
			obj->activate();
			Friend_mode_set_usecode_walker(before);
			return;
		}
		// Merchants talk to a guest's companion on the guest's own copy;
		// purchases go to the host (Friend_mode_guest_talk_to_merchant).
		if (clicked_actor && Friend_mode_guest_talk_to_merchant(clicked_actor)) {
			return;
		}
		// Network guests cannot start Avatar-owned usecode or storyline
		// dialogue. Guest-safe containers and their own inventory returned
		// through the dedicated paths above.  Anything else with a script
		// (people, story objects) is the Avatar's to use.
		if (clicked_actor || usecode->function_exists(obj->get_usecode())) {
			effects->add_text("The Avatar must do this", obj);
		} else {
			Mouse::mouse()->flash_shape(Mouse::redx);
		}
		return;
	}
	if (controlled && controlled != main_actor) {
		// Leading a companion (Shift+Tab, the host or a single player): the
		// guests' rules (docs/friend-mode/interaction-policy.md), carried out
		// here, where the world is: merchants and trainers talk to it; other
		// people and story objects are the Avatar's.
		const int shape = obj->get_shapenum();
		if (!clicked_actor && !obj->get_owner() && (obj->get_info().is_door() || Friend_mode_guest_usable_shape(shape))) {
			if (obj->get_info().is_door() && obj->get_framenum() % 4 >= 2) {
				effects->add_text("Locked", obj);
			} else {
				effects->remove_text_effects();
				Friend_mode_lead_use_object(obj, controlled);
			}
			return;
		}
		if (!clicked_actor && obj->get_info().is_container_locked()) {
			effects->add_text("Locked", obj);    // A key or lockpick opens it.
			return;
		}
		const Friend_item_use lead_use
				= clicked_actor ? Friend_item_use::none : Friend_mode_guest_item_use(shape, obj->get_framenum());
		if (!clicked_actor && (lead_use != Friend_item_use::none || Friend_mode_guest_local_shape(shape))) {
			// Food, potions, keys and lockpicks, the gavel, the bucket and
			// pools of water; signs, books and the like.  What a guest uses
			// on itself is used by the one led (a drink from a pool).
			effects->remove_text_effects();
			usecode->init_conversation();
			if (lead_use == Friend_item_use::on_self) {
				usecode->intercept_click_on_item(controlled);
			}
			// The one led stands in for the Avatar in the item's script
			// (walks to the well, fills the bucket, spits in the spittoon).
			Actor* const before = Friend_mode_set_usecode_walker(controlled);
			obj->activate();
			Friend_mode_set_usecode_walker(before);
			if (lead_use == Friend_item_use::on_self) {
				usecode->intercept_click_on_item(nullptr);    // In case it went unused.
			}
			return;
		}
		// Merchants and trainers talk to the companion, as to a guest's.
		if (clicked_actor && Friend_mode_lead_talk_to_merchant(clicked_actor)) {
			return;
		}
		if (clicked_actor || usecode->function_exists(obj->get_usecode())) {
			effects->add_text("The Avatar must do this", obj);
		} else {
			Mouse::mouse()->flash_shape(Mouse::redx);
		}
		return;
	}
	effects->remove_text_effects();    // Remove text msgs. from screen.
#ifdef DEBUG
	cout << "Object name is " << obj->get_name() << endl;
#endif
	usecode->init_conversation();
	// Check if CTRL is pressed to open basic properties instead
	const bool ctrl_pressed = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
	obj->activate(ctrl_pressed ? 2 : 1);
	npc_prox->wait(4);    // Delay "barking" for 4 secs.
}

/*
 *  Add an NPC to the 'nearby' list.
 */

void Game_window::add_nearby_npc(Npc_actor* npc) {
	if (!npc->is_nearby()) {
		npc->set_nearby();
		npc_prox->add(Game::get_ticks(), npc);
	}
}

/*
 *  Remove an NPC from the nearby list.
 */

void Game_window::remove_nearby_npc(Npc_actor* npc) {
	if (npc->is_nearby()) {
		npc_prox->remove(npc);
	}
}

/*
 *  Add all nearby NPC's to the given list.
 */

void Game_window::get_nearby_npcs(Actor_vector& a_list) const {
	npc_prox->get_all(a_list);
}

/*
 *  Tell all npc's to update their schedules at a new 3-hour period.
 */

void Game_window::schedule_npcs(
		int  hour,    // 24 hour
		bool repaint) {
	// Go through npc's, skipping Avatar.
	for (auto& npc : npcs) {
		if (!npc) {
			continue;
		}
		// Don't want companions leaving.
		if (npc->get_schedule_type() != Schedule::wait
			&& (npc->get_schedule_type() != Schedule::combat || npc->get_target() == nullptr)) {
			npc->update_schedule(hour / 3, hour % 3 == 0 ? -1 : 0);
		}
	}

	if (repaint) {
		paint();    // Repaint all.
	}
}

/*
 *  Tell all npc's to restore some of their HP's and/or mana on the hour.
 */

void Game_window::mend_npcs() {
	// Go through npc's.
	for (auto& npc : npcs) {
		if (npc) {
			npc->mend_wounds(true);
		}
	}
}

/*
 *  Get guard shape.
 */

int Game_window::get_guard_shape() {
	// Base on avatar's position.
	const Tile_coord pos = Game_window::get_instance()->main_actor->get_tile();
	if (!GAME_SI) {    // Default (BG).
		return 0x3b2;
	}
	// Moonshade?
	if (pos.tx >= 2054 && pos.ty >= 1698 && pos.tx < 2590 && pos.ty < 2387) {
		return 0x103;    // Ranger.
	}
	// Fawn?
	if (pos.tx >= 895 && pos.ty >= 1604 && pos.tx < 1173 && pos.ty < 1960) {
		return 0x17d;    // Fawn guard.
	}
	if (pos.tx >= 670 && pos.ty >= 2430 && pos.tx < 1135 && pos.ty < 2800) {
		return 0xe4;    // Pikeman.
	}
	return -1;    // No local guard.
}

/*
 *  Find a witness to the Avatar's thievery.
 *
 *  Output: ->witness, or nullptr.
 *      closest_npc = closest one that's nearby.
 */

Actor* Game_window::find_witness(
		Actor*& closest_npc,    // Closest one returned.
		int     align,          // Desired alignment.
		Actor*  thief           // Who is seen; the Avatar if null.
) {
	Actor* who = thief ? thief : main_actor;
	Actor_vector nearby;    // See if someone is nearby.
	who->find_nearby_actors(nearby, c_any_shapenum, 12, 0x28);
	closest_npc                    = nullptr;    // Look for closest NPC.
	int       closest_dist         = 5000;
	Actor*    witness              = nullptr;    // And closest facing us.
	int       closest_witness_dist = 5000;
	const int gshape               = get_guard_shape();
	for (auto& npc : nearby) {
		// Want non-party intelligent and not disabled NPCs only.
		if (npc->is_in_party() || !npc->is_sentient() || !npc->can_act()) {
			continue;
		}
		// Witnesses must either match desired alignment or they must be
		// local guards and the alignment must be neutral. This makes guards
		// assist neutral and chaotic NPCs.
		if (npc->get_effective_alignment() != align || (npc->get_shapenum() == gshape && align != Actor::neutral)) {
			continue;
		}
		const int dist = npc->distance(who);
		if (dist >= closest_witness_dist || !Fast_pathfinder_client::is_grabable(npc, who)) {
			continue;
		}
		// Looking toward Avatar?
		auto      sched   = static_cast<Schedule::Schedule_types>(npc->get_schedule_type());
		const int dir     = npc->get_direction(who);
		const int facing  = npc->get_dir_facing();
		const int dirdiff = (dir - facing + 8) % 8;
		if (dirdiff < 3 || dirdiff > 5 || sched == Schedule::hound) {
			// Yes.
			witness              = npc;
			closest_witness_dist = dist;
		} else if (dist < closest_dist) {
			closest_npc  = npc;
			closest_dist = dist;
		}
	}
	return witness;
}

/*
 *  Handle theft.
 */

void Game_window::theft(Actor* thief) {
	Actor* who = thief ? thief : main_actor;
	// See if in a new location.
	const int cx = who->get_cx();
	const int cy = who->get_cy();
	if (cx != theft_cx || cy != theft_cy) {
		theft_cx       = cx;
		theft_cy       = cy;
		theft_warnings = 0;
	}
	Actor* closest_npc;
	Actor* witness = find_witness(closest_npc, Actor::neutral, who);
	if (!witness) {
		if (closest_npc && rand() % 2) {
			if (closest_npc->is_goblin()) {
				closest_npc->say(goblin_heard_something);
			} else if (closest_npc->can_speak()) {
				closest_npc->say(heard_something);
			}
		}
		return;    // Didn't get caught.
	}
	const bool avainvisible = who->get_flag(Obj_flags::invisible);
	if (avainvisible && !witness->can_see_invisible()) {
		if (witness->is_goblin()) {
			witness->say(goblin_invis_theft);
		} else if (witness->can_speak()) {
			witness->say(first_invis_theft, last_invis_theft);
		}
		return;    // Didn't get caught because was invisible.
	}
	const int dir = witness->get_direction(who);
	// Face the thief.
	witness->change_frame(witness->get_dir_framenum(dir, Actor::standing));
	// If not in combat, change to hound schedule.
	auto sched = static_cast<Schedule::Schedule_types>(witness->get_schedule_type());
	if (sched != Schedule::combat && sched != Schedule::hound) {
		witness->set_schedule_type(Schedule::hound);
	}
	theft_warnings++;
	if (theft_warnings < 2 + rand() % 3) {
		// Just a warning this time.
		if (witness->is_goblin()) {
			witness->say(goblin_theft);
		} else if (witness->can_speak()) {
			witness->say(first_theft, last_theft);
		}
		return;
	}
	gump_man->close_all_gumps();    // Get gumps off screen.
	call_guards(witness, true);
}

/*
 *  Create a guard to arrest the Avatar.
 */

void Game_window::call_guards(
		Actor* witness,    // ->witness, or nullptr to find one.
		bool   theft       // called from Game_window::theft
) {
	Actor* closest;
	if (armageddon || in_dungeon) {
		return;
	}
	const int gshape = get_guard_shape();
	const int align  = witness ? witness->get_effective_alignment() : Actor::neutral;
	if (witness || (witness = find_witness(closest, align)) != nullptr) {
		if (witness->is_goblin()) {
			if (gshape < 0) {
				witness->say(goblin_need_help);
			} else if (theft) {
				witness->say(goblin_call_guards_theft);
			} else {
				witness->say(first_goblin_call_guards, last_goblin_call_guards);
			}
		} else if (witness->can_speak()) {
			if (gshape < 0) {
				witness->say(first_need_help, last_need_help);
			} else if (theft) {
				witness->say(first_call_guards_theft, last_call_guards_theft);
			} else {
				witness->say(first_call_guards, last_call_guards);
			}
		}
	}
	if (gshape < 0) {    // No local guards; lets forward to attack_avatar.
		attack_avatar(0, align);
		return;
	}

	const Tile_coord dest = Map_chunk::find_spot(main_actor->get_tile(), 5, gshape, 0, 1);
	if (dest.tx != -1) {
		// Show guard running up.
		// Create it off-screen.
		const int        numguards = 1 + rand() % 3;
		const Tile_coord offscreen(main_actor->get_tile() + Tile_coord(128, 128, 0));
		// Start in combat if avatar is fighting.
		// FIXME: Disabled for now, as causes guards to attack
		// avatar if you break glass, when they should arrest
		// instead.
		const Schedule::Schedule_types sched = /*combat ? Schedule::combat
											  :*/
				Schedule::arrest_avatar;
		for (int i = 0; i < numguards; i++) {
			const Game_object_shared new_guard = Monster_actor::create(gshape, offscreen, sched, Actor::chaotic);
			auto*                    guard     = static_cast<Monster_actor*>(new_guard.get());
			add_nearby_npc(guard);
			guard->approach_another(main_actor);
		}
		// Guaranteed way to do it.
		if (!Background_noise::is_combat_music(Audio::get_ptr()->current_music_track())) {
			Audio::get_ptr()->start_music(Audio::game_music(10), true);
		}
	}
}

/*
 *  Makes guards stop trying to arrest the avatar. Based on what it seems to
 *  do in SI, as well as what usecode seems to want to do.
 */

void Game_window::stop_arresting() {
	if (armageddon || in_dungeon) {
		return;
	}
	const int gshape = get_guard_shape();
	if (gshape < 0) {    // No one to calm down.
		return;
	}

	Actor_vector nearby;    // See if someone is nearby.
	main_actor->find_nearby_actors(nearby, gshape, 20, 0x28);
	for (auto& npc : nearby) {
		if (!npc->is_in_party() && npc->get_schedule_type() == Schedule::arrest_avatar) {
			npc->set_schedule_type(Schedule::wander);
			// Prevent guard from becoming hostile.
			npc->set_alignment(Actor::neutral);
		}
	}
}

/*
 *  Have nearby residents attack the Avatar.
 */

void Game_window::attack_avatar(
		int create_guards,    // # of extra guards to create.
		int align) {
	if (armageddon) {
		return;
	}
	const int gshape = get_guard_shape();
	if (gshape >= 0 && !in_dungeon) {
		while (create_guards--) {
			// Create it off-screen.
			const Game_object_shared new_guard = Monster_actor::create(
					gshape, main_actor->get_tile() + Tile_coord(128, 128, 0), Schedule::combat, Actor::chaotic);
			auto* guard = static_cast<Monster_actor*>(new_guard.get());
			add_nearby_npc(guard);
			guard->set_target(main_actor, true);
			guard->approach_another(main_actor);
		}
	}

	int          numhelpers = 0;
	const int    maxhelpers = 3;
	Actor_vector nearby;    // See if someone is nearby.
	main_actor->find_nearby_actors(nearby, c_any_shapenum, 20, 0x28);
	for (auto& npc : nearby) {
		if (npc->can_act() && !npc->is_in_party() && npc->is_sentient()
			&& ((npc->get_shapenum() == gshape && !in_dungeon) || align == npc->get_effective_alignment()) &&
			// Only if can get there.
			Fast_pathfinder_client::is_grabable(npc, main_actor)) {
			npc->set_target(main_actor, true);
			if (++numhelpers >= maxhelpers) {
				break;
			}
		}
	}
	// Guaranteed way to do it.
	if (!Background_noise::is_combat_music(Audio::get_ptr()->current_music_track())) {
		Audio::get_ptr()->start_music(Audio::game_music(10), true);
	}
}

/*
 *  Gain/lose focus.
 */

void Game_window::get_focus() {
	cout << "Game resumed" << endl;
	Audio::get_ptr()->resume_audio();
	focus = true;
	tqueue->resume(Game::get_ticks());
}

void Game_window::lose_focus() {
	if (!focus) {
		return;    // Fixes SDL bug.
	}
	cout << "Game paused" << endl;

	string str;
	config->value("config/audio/disablepause", str, "no");
	if (str == "no") {
		Audio::get_ptr()->pause_audio();
	}

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
	// Quick saving to make sure no game progress gets lost
	// when the app goes into background.
	write();
#endif

	focus = false;
	tqueue->pause(Game::get_ticks());
}

/*
 *  Prepare for game
 */

void Game_window::setup_game(bool map_editing) {
	// Map 0 always exists.
	get_map(0)->init();
	if (is_system_path_defined("<PATCH>")) {
		// There is a patch dir; search for other maps.
		for (int i = 1; i <= 0xFF; i++) {
			char fname[128];
			if (U7exists(Get_mapped_name(PATCH_U7MAP, i, fname))) {
				get_map(i)->init();
			}
		}
	}
	// Init. current 'tick'.
	Game::set_ticks(SDL_GetTicks());
	Face_stats::RemoveGump();    // it tries to update when reading actors so
								 // delete it until finished loading
	init_actors();               // Set up actors if not already done.
	// This also sets up initial
	// schedules and positions.

	cycle_load_palette();

	Notebook_gump::initialize();    // Read in journal.

	// read autonotes
	std::string d;
	std::string autonotesfilename;
	d = "config/disk/game/" + Game::get_gametitle() + "/autonotes";
	config->value(d.c_str(), autonotesfilename, "(default)");
	if (autonotesfilename == "(default)") {
		config->set(d.c_str(), autonotesfilename, true);
		Notebook_gump::read_auto_text();
	} else {
		try {
			Notebook_gump::read_auto_text_file(autonotesfilename.c_str());
		} catch (file_open_exception&) {
			cerr << "Autonotes file '" << autonotesfilename << "' not found, falling back to default autonotes." << endl;
			Notebook_gump::read_auto_text();
		}
	}

	usecode->read();    // Read the usecode flags
	cycle_load_palette();

	if (GAME_BG && !map_editing) {
		string yn;    // Override from config. file.
		// Skip intro. scene?
		config->value("config/gameplay/skip_intro", yn, "no");
		if (yn == "yes") {
			usecode->set_global_flag(Usecode_machine::did_first_scene, true);
		}

		// Should Avatar be visible?
		if (usecode->get_global_flag_bool(Usecode_machine::did_first_scene)) {
			main_actor->clear_flag(Obj_flags::bg_dont_render);
		} else {
			main_actor->set_flag(Obj_flags::bg_dont_render);
		}
	}

	cycle_load_palette();

	// Fade out & clear screen before palette change
	pal->fade_out(c_fade_out_time);
	clear_screen(true);
	load_palette_timer = 0;
	load_screen_layer.reset();

	// note: we had to stop the plasma here already, because init_readied
	// and activate_eggs may update the screen through usecode functions
	// (Helm of Light, for example)
	Actor*    party[9];
	const int cnt = get_party(party, 1);    // Get entire party.
	for (int i = 0; i < cnt; i++) {         // Init. rings.
		party[i]->init_readied();
	}
	// Ensure time is stopped if map-editing.
	time_stopped = map_editing ? -1 : 0;
	//+++++The below wasn't prev. done by ::read(), so maybe it should be
	//+++++controlled by a 'first-time' flag.
	// Want to activate first egg.
	Map_chunk* olist = main_actor->get_chunk();
	olist->setup_cache();

	const Tile_coord t = main_actor->get_tile();
	// Do them immediately.
	if (!map_editing) {    // (Unless map-editing.)
		olist->activate_eggs(main_actor, t.tx, t.ty, t.tz, -1, -1, true);
	}
	// Force entire repaint.
	set_all_dirty();
	painted = true;                     // Main loop uses this.
	gump_man->close_all_gumps(true);    // Kill gumps.
	Face_stats::load_config(config);
	if (using_shortcutbar()) {
		g_shortcutBar = new ShortcutBar_gump(0, 0);
	}
	if (touchui != nullptr) {
		touchui->showButtonControls();
		touchui->showGameControls();
	}
	// During the first scene do not show the UI elements,
	// unless the Avatar is free to move (possibly in mods).
	if ((GAME_BG && !usecode->get_global_flag_bool(Usecode_machine::did_first_scene))
		|| (GAME_SI && !usecode->get_global_flag_bool(Usecode_machine::si_did_first_scene))
		|| (main_actor_dont_move() && !main_actor_can_act() && !main_actor_can_act_charmed())) {
		if (touchui != nullptr) {
			touchui->hideGameControls();
		}
		if (Face_stats::Visible()) {
			Face_stats::HideGump();
		}
		if (ShortcutBar_gump::Visible()) {
			ShortcutBar_gump::HideGump();
		}
	}

	// Set palette for time-of-day.
	clock->reset_palette();
	pal->fade(6, 1, -1);    // Fade back in.
}

void Game_window::plasma(int w, int h, int x, int y, int startc, int endc) {
	win->BeginPaintIntoGuardBand(&x, &y, &w, &h);
	Image_buffer8* ibuf = get_win()->get_ib8();

	ibuf->fill8(startc, w, h, x, y);

	for (int i = 0; i < w * h; i++) {
		const Uint8 pc = startc + rand() % (endc - startc + 1);
		const int   px = x + rand() % w;
		const int   py = y + rand() % h;

		for (int j = 0; j < 6; j++) {
			const int px2 = px + rand() % 17 - 8;
			const int py2 = py + rand() % 17 - 8;
			ibuf->fill8(pc, 3, 1, px2 - 1, py2);
			ibuf->fill8(pc, 1, 3, px2, py2 - 1);
		}
	}
	win->EndPaintIntoGuardBand();
	painted = true;
}

/*
 *  Chunk caching emulation: swap out chunks which are now at least
 *  3 chunks away.
 */
void Game_window::emulate_cache(Map_chunk* olist, Map_chunk* nlist) {
	if (!olist) {
		return;    // Seems like there's nothing to do.
	}
	// Cancel weather from eggs that are
	// far away.
	effects->remove_weather_effects(120);
	const int newx = nlist->get_cx();
	const int newy = nlist->get_cy();
	const int oldx = olist->get_cx();
	const int oldy = olist->get_cy();
	Game_map* omap = olist->get_map();
	Game_map* nmap = nlist->get_map();
	// Cancel scripts 4 chunks from this.
	Usecode_script::purge(Tile_coord(newx * c_tiles_per_chunk, newy * c_tiles_per_chunk, 0), 4 * c_tiles_per_chunk);
	// Temporary objects (monster bodies, spawned items) in chunks we left
	// behind are deleted, as in the original.  Friend Mode keeps them one
	// chunk further out (3 instead of 2), and also around a guest's
	// companion.  A guest's world follows the host's, so it deletes nothing.
	constexpr int keep_chunks = 3;
	if (!Friend_mode_is_guest()) {
		auto chunk_dist = [](int a, int b) {
			const int d = a > b ? a - b : b - a;
			return d < c_num_chunks - d ? d : c_num_chunks - d;
		};
		auto near_chunk = [&](int cx, int cy, int ax, int ay) {
			return chunk_dist(cx, ax) <= keep_chunks && chunk_dist(cy, ay) <= keep_chunks;
		};
		auto near_guest = [&](int cx, int cy) {
			for (const Remote_friend& each : remote_friends) {
				const Actor* guest = each.actor;
				if (guest->get_map() == omap && !guest->is_pos_invalid()
					&& near_chunk(cx, cy, guest->get_cx(), guest->get_cy())) {
					return true;
				}
			}
			return false;
		};
		Game_object_vector removes;
		for (int dy = -keep_chunks; dy <= keep_chunks; dy++) {
			for (int dx = -keep_chunks; dx <= keep_chunks; dx++) {
				const int cx = (oldx + dx + c_num_chunks) % c_num_chunks;
				const int cy = (oldy + dy + c_num_chunks) % c_num_chunks;
				if (nmap == omap && near_chunk(cx, cy, newx, newy)) {
					continue;    // Still near.
				}
				if (near_guest(cx, cy)) {
					continue;    // Near a guest's companion.
				}
				Map_chunk* list = omap->get_chunk_safely(cx, cy);
				if (!list) {
					continue;
				}
				Object_iterator it(list->get_objects());
				Game_object*    each;
				while ((each = it.get_next()) != nullptr) {
					Egg_object* egg = each->as_egg();
					if (egg != nullptr && egg->reset()) {
						continue;
					} else if (each->get_flag(Obj_flags::is_temporary)) {
						removes.push_back(each);
					}
				}
			}
		}
		for (auto* remove : removes) {
#ifdef DEBUG
			const Tile_coord t = remove->get_tile();
			cout << "Culling object: " << remove->get_name() << '(' << remove << ")@" << t.tx << "," << t.ty << "," << t.tz
				 << endl;
#endif
			remove->delete_contents();    // first delete item's contents
			remove->remove_this(nullptr);
		}
	}

	if (omap == nmap) {
		omap->cache_out(newx, newy);
	} else {                        // Going to new map?
		omap->cache_out(-1, -1);    // Cache out whole of old map.
	}
}

// Tests to see if a move goes out of range of the actors superchunk
bool Game_window::emulate_is_move_allowed(int tx, int ty) {
	const int ax = camera_actor->get_cx() / c_chunks_per_schunk;
	const int ay = camera_actor->get_cy() / c_chunks_per_schunk;
	tx /= c_tiles_per_schunk;
	ty /= c_tiles_per_schunk;

	int difx = ax - tx;
	int dify = ay - ty;

	if (difx < 0) {
		difx = -difx;
	}
	if (dify < 0) {
		dify = -dify;
	}

	// Is it within 1 superchunk range?
	return (!difx || difx == 1 || difx == c_num_schunks || difx == c_num_schunks - 1)
		   && (!dify || dify == 1 || dify == c_num_schunks || dify == c_num_schunks - 1);
}

// create mini-screenshot (96x60) for use in savegames
unique_ptr<Shape_file> Game_window::create_mini_screenshot() {
	set_all_dirty();
	render->paint_map(0, 0, get_width(), get_height());

	unique_ptr<unsigned char[]> img(win->mini_screenshot());
	unique_ptr<Shape_file>      sh;
	if (img) {
		sh = make_unique<Shape_file>(make_unique<Shape_frame>(std::move(img), 96, 60, 0, 0, true));
	}

	set_all_dirty();
	paint();
	return sh;
}

#define BG_PLASMA_START_COLOR 128
#define BG_PLASMA_CYCLE_RANGE 80

#define SI_PLASMA_START_COLOR 16
#define SI_PLASMA_CYCLE_RANGE 96

void Game_window::setup_load_palette() {
	if (load_palette_timer != 0) {
		return;
	}

	if (GAME_BG) {
		plasma_start_color = BG_PLASMA_START_COLOR;
		plasma_cycle_range = BG_PLASMA_CYCLE_RANGE;
	} else {    // Default: if (GAME_SI)
		plasma_start_color = SI_PLASMA_START_COLOR;
		plasma_cycle_range = SI_PLASMA_CYCLE_RANGE;
	}

	// Put up the plasma to the screen
	plasma(win->get_full_width(), win->get_full_height(), win->get_start_x(), win->get_start_y(), plasma_start_color,
		   plasma_start_color + plasma_cycle_range - 1);

	// Load the palette
	if (GAME_BG) {
		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 2);
	} else if (GAME_SI) {
		Shape_frame* loadscreen = game->get_menu_shapes()->get_shape(0, 0);

		// Show the loadscreen on a full screen scene layer
		if (loadscreen) {
			load_screen_layer = std::make_unique<Scene_layer>(320, 200);
			load_screen_layer->begin_frame();
			loadscreen->paint(0, 0);
			load_screen_layer->end_frame();
		}

		// SI Beta doesn't have the load screen palette so create an appropriate one for the plasma
		if (GAME_SIB) {
			// The needed colour ranges are created by interpolating between these colours
			uint8 colours[][3] = {
					{00,   00, 0xFC}, 
                    {00, 0x2C, 0xFC},
                    {00, 0x5C, 0xFC},
                    {00, 0x90, 0xFC},
                    {00, 0x90, 0xFC},
                    {00, 0x44, 0xFC},
			}, rgb[3] = {};

			for (int i = 0; i < SI_PLASMA_CYCLE_RANGE; i++) {
				int range = std::min<int>(std::size(colours) - 1, i / 16);

				// Next range is valid to interpolate the colour
				if (range < 5) {
					for (int c = 0; c < 3; c++) {
						int a  = colours[range][c];
						int b  = colours[range + 1][c];
						rgb[c] = ((b << 4) + (a - b) * (i & 0xF)) >> 4;
					}
				}
				// next range is not valid so just use this ranges start colour
				else {
					for (int c = 0; c < 3; c++) {
						rgb[c] = colours[range][c];
					}
				}
				pal->set_color(i + SI_PLASMA_START_COLOR, (rgb[0] >> 2) & 0x3F, (rgb[1] >> 2) & 0x3F, (rgb[2] >> 2) & 0x3F);
			}

		} else {
			pal->load(MAINSHP_FLX, PATCH_MAINSHP, 1);
		}
	}

	pal->apply();
	load_palette_timer = SDL_GetTicks();
}

void Game_window::cycle_load_palette() {
	if (load_palette_timer == 0) {
		return;
	}
	const uint32 ticks = SDL_GetTicks();
	if (ticks > load_palette_timer + 75) {
		for (int i = 0; i < 4; ++i) {
			get_win()->rotate_colors(plasma_start_color, plasma_cycle_range, 1);
		}
		show(true);

		// We query the timer here again, as the blit can take easily 50 ms and
		// more depending on the chosen scaler and the overall system speed
		load_palette_timer = SDL_GetTicks();
	}
}

bool Game_window::is_hostile_nearby() const {
	/* If there is a hostile NPC nearby, the avatar isn't allowed to
	 * move very fast
	 * Note that the range at which this occurs in the original is
	 * less than the "potential target" range- that is, if I go into
	 * combat mode, even when I'm allowed to run at full speed,
	 * I'll sometime charge off to kill someone "too far away"
	 * to affect a speed limit.
	 * I don't know whether this is taken into account by
	 * get_nearby_npcs, but on the other hand, its a negligible point.
	 */
	Actor_vector nearby;
	get_nearby_npcs(nearby);

	bool nearby_hostile = false;
	for (auto& actor : nearby) {
		if (!actor->is_dead() && actor->get_schedule() && actor->get_effective_alignment() >= Actor::evil
			&& ((actor->get_schedule_type() == Schedule::combat
				 && static_cast<Combat_schedule*>(actor->get_schedule())->has_started_battle())
				|| actor->get_schedule_type() == Schedule::arrest_avatar)) {
			/* TODO- I think invisibles still trigger the
			 * slowdown, verify this. */
			nearby_hostile = true;
			break; /* No need to bother checking the rest :P */
		}
	}
	return nearby_hostile;
}

bool Game_window::failed_copy_protection() {
	const bool confused    = main_actor->get_flag(Obj_flags::confused);
	const bool failureFlag = usecode->get_global_flag_bool(Usecode_machine::failed_copy_protect);
	return (GAME_SI && confused) || (GAME_BG && failureFlag);
}

void Game_window::got_bad_feeling(int odds) {
	if (!(GAME_BG || GAME_SI)) {
		return;
	}
	if ((rand() % odds) == 0) {
		const char* badfeeling = get_misc_name(GAME_BG ? 0x44 : 0x50);
		effects->remove_text_effect(main_actor);
		effects->add_text(badfeeling, main_actor);
	}
}

void Game_window::set_shortcutbar(uint8 s) {
	if (use_shortcutbar == s) {
		return;
	}
	if (using_shortcutbar() && s > 0) {
		use_shortcutbar = s;
		return;
	}
	use_shortcutbar = s;
	if (!is_in_exult_menu()) {
		if (using_shortcutbar()) {
			g_shortcutBar = new ShortcutBar_gump(0, 0);
		} else {
			gump_man->close_gump(g_shortcutBar);
			g_shortcutBar = nullptr;
		}
	}
}
