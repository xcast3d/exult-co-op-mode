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

// Co-Op Mode (Friend Mode) hosting.  Set from the title menu before a game
// starts; Friend_mode_open_to_coop starts it during play.
bool Friend_mode_hosting();
void Friend_mode_set_hosting(bool on);
bool Friend_mode_open_to_coop();
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
// Read-only objects (signs, books, map, telescope...): a guest uses them on
// its own copy of the world, and only the guest sees the result.
bool Friend_mode_guest_local_shape(int shape);
// Guest: ask the host to use 'item' on 'target'.
void Friend_mode_request_item_use(Game_object* item, Game_object* target, Game_object* second = nullptr);
// Guest: use an allowed item (picking its targets); false if not allowed.
bool Friend_mode_guest_use_item(Game_object* item);
// Guest: use an item from the companion's own pack by keyboard shortcut.
// Returns false if nothing happened.
bool Friend_mode_guest_activate(Game_object* item);
// Guest: crosshair click for a key or lockpick target (nullptr on Escape).
Game_object* Friend_mode_pick_item_target();
// Guest in combat mode: ask the host to have our companion attack 'obj'.
// False if it isn't a foe (a door, say), so the usual click rules apply.
bool Friend_mode_guest_attack(Game_object* obj);

// Friend Mode music: the host's tracks play on guests too.  Called for
// every music start and stop; false if this one must not play (a guest's
// own, while the host chooses the music).
bool Friend_mode_note_music(bool start, int num, bool continuous, int force, const std::string& flex);
// Friend Mode host: show missiles and sprite effects (spells, explosions,
// blood) on guests too, as pictures only.  'item' is what a sprite follows.
void Friend_mode_note_hit(Game_object* target, int damage, int health_left);
void Friend_mode_note_projectile(const Tile_coord& from, const Tile_coord& to, int weapon, int sprite, int speed);
void Friend_mode_note_sprite(
		int num, Game_object* item, const Tile_coord& pos, int xoff, int yoff, int dx, int dy, int delay, int frame,
		int reps);
// Friend Mode shared journal (see Notebook_gump).  True while connected.
bool Friend_mode_journal_shared();
// Start writing in shared note 'index'.  False if someone else is.
bool Friend_mode_journal_begin_edit(int index);
// Finished writing in a note ('index' < 0 for a new one).  True if the note
// went to the host, which sends it back in the journal.
bool Friend_mode_journal_end_edit(
		int index, bool changed, int day, int hour, int minute, int tx, int ty, const std::string& text);
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
