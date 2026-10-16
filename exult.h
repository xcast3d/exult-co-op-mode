/*
 *  Copyright (C) 2001-2022  The Exult Team
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

#ifndef EXULT_H
#define EXULT_H

#include "mouse.h"

#include <string>
#include <vector>

class Actor;
class Cheat;
class Configuration;
class Game_window;
class KeyBinder;
class Tile_coord;
class Paintable;
class GameManager;
class ShortcutBar_gump;
class TouchUI;
class Game_object;

// Friend Mode connection identity is collected before the guest world loads.
bool Friend_mode_collect_connection_details();
// Guest item moves, mirroring a drag on the host: picking up lifts the item
// out of the host's world too, dropping places it, and a cancelled drag (or
// the rest of a split stack) is put back.
void Friend_mode_note_drag_start(Game_object* obj);
void Friend_mode_note_drag_put_back();
// 'gump_container' is the container whose window received the drop, if any
// (a dropped stack that merged into another is left detached).
void Friend_mode_note_drag_drop(Game_object* dropped, int quantity, Game_object* gump_container, bool whole);

// This instance joined a Friend Mode host: the host owns the world, so
// monster and weather eggs (among others) do nothing here.
bool Friend_mode_is_guest();
// Host: a saved game was loaded, so guests need the new world.
void Friend_mode_note_world_loaded();

// Keep the Friend Mode connection moving (accept, handshake, send and
// receive) from loops other than the main one, such as modal gumps.  What
// arrives is queued and handled when the main loop resumes.
void Friend_mode_background_poll();
// Connect to the host and replace the guest's gamedat with the host's live world.
bool Friend_mode_download_world();
std::string Friend_mode_display_name(const Game_object* object, const std::string& character_name);

// Host: Usecode changed a world object's shape or frame; share with guests.
void Friend_mode_note_object_change(Game_object* object, int old_shape, int old_frame);
// World objects besides doors that guests may use (shutters, lamps...).
bool Friend_mode_guest_usable_shape(int shape);
// Host: while using an object for a guest, the guest's companion (not the
// Avatar) walks to it for scripts that path the Avatar there first.
Actor* Friend_mode_usecode_walker();
// Guest: ask the host to use a door or other guest-usable object.
void Friend_mode_request_object_use(Game_object* object);
// Items guests may use: food and potions on their own companion, keys and
// lockpicks on a door or chest they pick.
enum class Friend_item_use { none, on_self, on_target };
Friend_item_use Friend_mode_guest_item_use(int shape);
// Guest: ask the host to use 'item' on 'target'.
void Friend_mode_request_item_use(Game_object* item, Game_object* target);
// Guest: crosshair click for a key or lockpick target (nullptr on Escape).
Game_object* Friend_mode_pick_item_target();

// Allows world-object speech to enter the host's Friend Mode event stream.
void Friend_mode_note_speech(Game_object* source, const char* text);
void Friend_mode_note_dialogue_reset();
void Friend_mode_note_dialogue_face(int shape, int frame, int slot);
void Friend_mode_note_dialogue_face_frame(int frame, int slot);
void Friend_mode_note_dialogue_remove_face(int shape);
void Friend_mode_note_dialogue_message(const char* text);
void Friend_mode_note_dialogue_choices(const std::vector<std::string>& choices);
void Friend_mode_note_dialogue_clear_choices();

/*
 *  Get a click, or, optionally, a keyboard char.
 *
 *  Output: false if user hit ESC.
 *      Chr gets keyboard char., or 0 if it's was a mouse click.
 */
extern bool g_waiting_for_click;

extern bool Get_click(
		int& x, int& y,                                 // Location returned (if not ESC).
		Mouse::Mouse_shapes shape,                      // Mouse shape to use.
		char*               chr           = nullptr,    // Char. returned if not null.
		bool                drag_ok       = false,      // Can drag while here.
		Paintable*          paint         = nullptr,    // Paint over everything else.
		bool                rotate_colors = false       // If the palette colors should rotate.
);

/*
 *  Make a screenshot of the current screen display
 */
extern void make_screenshot(
		bool silent = false    // If false, will display a success/failure message
);

/*
 *  Wait for someone to stop walking.  If a timeout is given, at least
 *  one animation cycle will still always occur.
 */

extern void Wait_for_arrival(
		Actor*            actor,          // Whom to wait for.
		const Tile_coord& dest,           // Where he's going.
		long              maxticks = 0    // Max. # msecs. to wait, or 0.
);

extern void change_gamma(bool down);
extern void increase_scaleval();
extern void decrease_scaleval();
extern void setup_video(
		bool fullscreen, int setup_video_type, int resx = 0, int resy = 0, int gw = 0, int gh = 0, int scaleval = 0, int scaler = 0,
		Image_window::FillMode fillmode = Image_window::Fill, int fill_scaler = 0);

enum setup_video_type {
	VIDEO_INIT        = 0,    // read and write initial settings and create gwin
	TOGGLE_FULLSCREEN = 1,    // toggle fullscreen using proper settings
	MENU_INIT         = 2,    // read and set initial settings for video gump menu
	SET_CONFIG        = 3     // sets the config settings (doesn't write)
};

enum quitting_time_enum {
	QUIT_TIME_NO      = 0,
	QUIT_TIME_YES     = 1,
	QUIT_TIME_RESTART = 2
};

extern KeyBinder*     keybinder;
extern Configuration* config;
extern GameManager*   gamemanager;

extern quitting_time_enum quitting_time;

extern ShortcutBar_gump* g_shortcutBar;
extern TouchUI*          touchui;

#endif
