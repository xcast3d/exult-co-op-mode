/**
 ** Exult.cc - Multiplatform Ultima 7 game engine
 **
 ** Written: 7/22/98 - JSF
 **/
/*
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

#include "exult.h"

#include "Audio.h"
#include "AudioMixer.h"
#include "Configuration.h"
#include "Face_stats.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Scroll_gump.h"
#include "ShortcutBar_gump.h"
#include "U7file.h"
#include "U7fileman.h"
#include "VideoOptions_gump.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "args.h"
#include "barge.h"
#include "cheat.h"
#include "chunks.h"
#include "combat_opts.h"
#include "contain.h"
#include "crc.h"
#include "dir.h"
#include "drag.h"
#include "effects.h"
#include "exult_bg_flx.h"
#include "exult_flx.h"
#include "exult_si_flx.h"
#include "exultmenu.h"
#include "fnames.h"
#include "font.h"
#include "font_map.h"
#include "friendnet.h"
#include "game.h"
#include "gamemap.h"
#include "gamemgr/modmgr.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "istring.h"
#include "items.h"
#include "keyactions.h"
#include "keys.h"
#include "menulist.h"
#include "mouse.h"
#include "monsters.h"
#include "palette.h"
#include "party.h"
#include "paths.h"
#include "schedule.h"
#include "shapeid.h"
#include "sdlrwopsistream.h"
#include "sdlrwopsostream.h"
#include "touchui.h"
#include "u7drag.h"
#include "usecode/conversation.h"
#include "ucmachine.h"
#include "utils.h"
#include "verify.h"
#include "version.h"
#ifdef HAVE_ZIP_SUPPORT
#	include "files/zip/unzip.h"
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <unordered_set>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
static const SDL_MouseID EXULT_TOUCH_MOUSEID = SDL_TOUCH_MOUSEID;
static const SDL_TouchID EXSDL_MOUSE_TOUCHID = SDL_MOUSE_TOUCHID;
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#ifdef USE_EXULTSTUDIO /* Only needed for communication with exult studio */
#	if HAVE_SYS_TIME_H
#		include <sys/time.h>
#	endif
#	include "chunks.h"
#	include "chunkter.h"
#	include "objserial.h"
#	include "servemsg.h"
#	include "server.h"
#endif

#if (defined(USECODE_DEBUGGER) && defined(XWIN))
#	include <csignal>
#endif

using namespace Pentagram;

#ifdef SDL_PLATFORM_IOS
#	include "ios_utils.h"
#elif defined(ANDROID)
#	include "TouchUI_Android.h"
#endif

using std::atexit;
using std::atof;
using std::cerr;
using std::cout;
using std::endl;
using std::exit;
using std::string;
using std::toupper;
using std::vector;

Configuration* config      = nullptr;
KeyBinder*     keybinder   = nullptr;
GameManager*   gamemanager = nullptr;

/*
 *  Globals:
 */
Game_window*       gwin          = nullptr;
quitting_time_enum quitting_time = QUIT_TIME_NO;

bool intrinsic_trace = false;    // Do we trace Usecode-intrinsics?
int  usecode_trace   = 0;        // Do we trace Usecode-instructions?
// 0 = no, 1 = short, 2 = long
bool combat_trace = false;    // show combat messages?

// Save game compression level
int  save_compression = 1;
bool ignore_crc       = false;

TouchUI* touchui = nullptr;

bool              g_waiting_for_click = false;
ShortcutBar_gump* g_shortcutBar       = nullptr;

#ifdef USECODE_DEBUGGER
bool        usecode_debugging = false;    // Do we enable the usecode debugger?
extern void initialise_usecode_debugger();
#endif

int current_scaleval = 1;

/*
 *  Local functions:
 */
static int  exult_main(const char* runpath);
static void Init();
static int  Play();
static bool Get_click(int& x, int& y, char* chr, bool drag_ok, bool rotate_colors = false);
static void set_scaleval(int new_scaleval);
#ifdef USE_EXULTSTUDIO
static void Move_dragged_shape(int shape, int frame, int x, int y, int prevx, int prevy, bool show);
static void Move_dragged_combo(
		int xtiles, int ytiles, int tiles_right, int tiles_below, int x, int y, int prevx, int prevy, bool show);
static void Drop_dragged_shape(int shape, int frame, int x, int y);
static void Drop_dragged_chunk(int chunknum, int x, int y);
static void Drop_dragged_npc(int npcnum, int x, int y);
static void Drop_dragged_combo(int cnt, U7_combo_data* combo, int x, int y);
static void Move_grid(int x, int y, int prevx, int prevy, bool ireg, int xtiles, int ytiles, int tiles_right, int tiles_below);
static int  drag_prevx = -1, drag_prevy = -1;
static int  drag_shfile = -1;
static int  drag_shpnum = -1, drag_shfnum = -1;
static int  drag_cbcnt    = -1;
static int  drag_cbxtiles = -1, drag_cbytiles = -1;
static int  drag_cbrtiles = -1, drag_cbbtiles = -1;
static int  drag_npcnum = -1;
static int  drag_cnknum = -1;
#endif
static void BuildGameMap(BaseGameInfo* game, int mapnum);
static void Handle_events();
static void Handle_event(SDL_Event& event);
static bool Friend_actor_is_eligible(uint32 npc_num);
static int  Select_friend_actor(const std::vector<uint32>& roster);

static int Friend_creature_index(const Actor* actor) {
	int index = 0;
	for (Monster_actor* creature = Monster_actor::get_first_in_world(); creature;
			 creature = creature->get_next_in_world(), ++index) {
		if (creature == actor) {
			return index;
		}
	}
	return -1;
}

static Monster_actor* Friend_creature_by_index(uint32 wanted) {
	uint32 index = 0;
	for (Monster_actor* creature = Monster_actor::get_first_in_world(); creature;
			 creature = creature->get_next_in_world(), ++index) {
		if (index == wanted) {
			return creature;
		}
	}
	return nullptr;
}

/*
 *  Statics:
 */
static bool   run_bg       = false;        // skip menu and run bg
static bool   run_si       = false;        // skip menu and run si
static bool   run_fov      = false;        // skip menu and run fov
static bool   run_ss       = false;        // skip menu and run ss
static bool   run_sib      = false;        // skip menu and run sib
static string arg_gamename = "default";    // cmdline arguments
static string arg_modname  = "default";    // cmdline arguments
static string arg_configfile;
static int    arg_buildmap     = -1;
static int    arg_mapnum       = -1;
static bool   arg_nomenu       = false;
static bool   arg_edit_mode    = false;    // Start up ExultStudio.
static bool   arg_write_xml    = false;    // Write out game's config. as XML.
static bool   arg_reset_video  = false;    // Resets the video setings.
static bool   arg_verify_files = false;    // Verify a game's files.
static bool   arg_friend_host  = false;
static string arg_friend_join;
static int    arg_friend_port = 1987;
static string friend_player_name = "Player";
static Friend_mode_session friend_session;
static bool friend_host_dialogue_active = false;
static std::string friend_host_peer_name;    // Connected guest, for leave notice.
static std::vector<uint32> friend_sent_party;    // Party last sent to the guest.
// Grace period before a companion left beyond the leash returns to the Avatar.
static constexpr uint32 friend_leash_grace_ms  = 5000;
static uint32           friend_leash_return_at = 0;    // Host: ticks to return, or 0.
static bool friend_chat_active = false;
static std::string friend_chat_text;

// The actor this machine's player speaks through, or null while spectating.
static Actor* Friend_mode_local_speaker() {
	if (friend_session.mode() == Friend_mode_session::Mode::guest) {
		return gwin->has_network_friend_control() ? gwin->get_controlled_actor() : nullptr;
	}
	return gwin->get_main_actor();
}

// Resolve the speaker of incoming chat.  The host trusts only its own
// assignment for the guest; guests trust the host's NPC number.
static Actor* Friend_mode_remote_speaker(uint32 claimed) {
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		return gwin->get_remote_friend_actor();
	}
	return claimed == Friend_mode_session::chat_spectator ? nullptr : gwin->get_npc(static_cast<int>(claimed));
}

static void Friend_mode_show_chat(Actor* speaker, const std::string& sender, const std::string& text) {
	if (text.empty()) {
		return;
	}
	if (speaker) {
		gwin->get_effects()->add_chat_text(text.c_str(), speaker);
	} else {
		const std::string line = (sender.empty() ? std::string("Spectator") : sender) + ": " + text;
		gwin->get_effects()->add_chat_text(line.c_str(), nullptr);
	}
}

// Session status shown near the top of the view, like spectator chat.
static void Friend_mode_notice(const std::string& text) {
	cout << "Friend Mode: " << text << endl;
	gwin->get_effects()->add_chat_text(text.c_str(), nullptr);
}

static std::string Friend_mode_actor_name(int npc_num) {
	Actor* actor = gwin->get_npc(npc_num);
	return actor ? actor->get_name() : std::string("a companion");
}

// Guest leash state, shown locally from the synced Avatar position.
enum class Friend_leash_zone { near, straying, beyond };
static Friend_leash_zone friend_leash_zone = Friend_leash_zone::near;

// While straying, an arrow at the screen edge points back to the Avatar.
static void Friend_mode_paint_leash_arrow() {
	if (friend_session.mode() != Friend_mode_session::Mode::guest || friend_leash_zone == Friend_leash_zone::near) {
		return;
	}
	Actor*      actor  = gwin->get_controlled_actor();
	Main_actor* avatar = gwin->get_main_actor();
	if (!actor || actor == avatar) {
		return;
	}
	const Tile_coord from = actor->get_tile();
	const Tile_coord to   = avatar->get_tile();
	const int        dx   = to.tx - from.tx;
	const int        dy   = to.ty - from.ty;
	if (dx == 0 && dy == 0) {
		return;
	}
	Mouse*       mouse = Mouse::mouse();
	Shape_frame* arrow = mouse->get_pointer_frame(mouse->get_long_arrow(Get_direction(-dy, dx)));
	if (!arrow) {
		return;
	}
	// Walk from the screen centre toward the Avatar until near the edge.
	const int   w      = gwin->get_width();
	const int   h      = gwin->get_height();
	const int   margin = 16;
	const float len    = std::sqrt(static_cast<float>(dx * dx + dy * dy));
	const float ux     = dx / len;
	const float uy     = dy / len;
	const float sx     = ux != 0 ? (w / 2 - margin) / std::abs(ux) : 1e9f;
	const float sy     = uy != 0 ? (h / 2 - margin) / std::abs(uy) : 1e9f;
	const float scale  = std::min(sx, sy);
	const int   x      = w / 2 + static_cast<int>(ux * scale);
	const int   y      = h / 2 + static_cast<int>(uy * scale);
	arrow->paint_rle(gwin->get_win()->get_ib8(), x, y);
	gwin->set_painted();
}

static void Friend_mode_update_leash_zone(Actor* actor) {
	Friend_leash_zone zone = Friend_leash_zone::near;
	if (actor && actor != gwin->get_main_actor()) {
		const int dist = gwin->friend_leash_distance(actor->get_tile());
		if (dist > Game_window::friend_leash_max_tiles) {
			zone = Friend_leash_zone::beyond;
		} else if (dist > Game_window::friend_leash_warn_tiles) {
			zone = Friend_leash_zone::straying;
		}
	}
	if (zone != friend_leash_zone) {
		if (zone == Friend_leash_zone::straying && friend_leash_zone == Friend_leash_zone::near) {
			gwin->get_effects()->add_chat_text("Straying from the Avatar", actor);
		} else if (zone == Friend_leash_zone::beyond) {
			gwin->get_effects()->add_chat_text("The Avatar needs you", actor);
		}
		gwin->set_all_dirty();
		friend_leash_zone = zone;
	}
}

void Friend_mode_note_object_change(Game_object* object, int old_shape, int old_frame) {
	if (!object || friend_session.mode() != Friend_mode_session::Mode::host || !friend_session.connected()
		|| !object->get_chunk() || object->get_owner() || object->as_actor()) {
		return;
	}
	const Tile_coord t = object->get_tile();
	friend_session.send_object_change(
			t.tx, t.ty, t.tz, old_shape, object->get_shapenum(), old_frame, object->get_framenum());
}

bool Friend_mode_guest_usable_shape(int shape) {
	if (!GAME_BG) {
		return false;    // Serpent Isle shapes not reviewed yet; doors only.
	}
	switch (shape) {
	case 290:    // Closed shutters.
	case 291:
	case 322:    // Open shutters.
	case 372:
	case 336:    // Light source / lit light source.
	case 338:
	case 435:    // Lit sconce / sconce.
	case 481:
	case 657:    // Curtains.
	case 678:
	case 787:    // Lever.
	case 788:    // Switch.
	case 889:    // Lamp post (unlit).
	case 526:    // Lit lamp (a lit lamp post swaps to this shape).
		return true;
	default:
		return false;
	}
}

void Friend_mode_request_object_use(Game_object* object) {
	if (!object || friend_session.mode() != Friend_mode_session::Mode::guest || !friend_session.connected()) {
		return;
	}
	const Tile_coord t = object->get_tile();
	friend_session.send_door_use(t.tx, t.ty, t.tz, object->get_shapenum(), object->get_framenum());
}

// Find a world object at exactly this tile with this shape (and frame, if
// not c_any_framenum).
static Game_object* Friend_find_object(const Tile_coord& tile, int shape, int frame) {
	Game_object_vector found;
	Game_object::find_nearby(found, tile, shape, 1, 0xb0, c_any_qual, frame);
	for (Game_object* obj : found) {
		if (obj->get_tile() == tile && !obj->get_owner()) {
			return obj;
		}
	}
	return nullptr;
}

// Find a world object at this tile: the given shape first, then (for doors,
// which swap shape when opened: 270/376 <-> 432/433) any door shape.
static Game_object* Friend_find_door(const Tile_coord& tile, int shape, int frame) {
	if (Game_object* obj = Friend_find_object(tile, shape, frame)) {
		return obj;
	}
	if (Game_object* obj = Friend_find_object(tile, shape, c_any_framenum)) {
		return obj;
	}
	Game_object_vector found;
	Game_object::find_nearby(found, tile, c_any_shapenum, 1, 0xb0);
	for (Game_object* obj : found) {
		if (obj->get_tile() == tile && !obj->get_owner() && obj->get_info().is_door()) {
			return obj;
		}
	}
	return Game_object::find_door(tile);
}

// Apply a host shape/frame change to a guest's copy of a world object.
static void Friend_apply_object_change(Game_object* obj, int shape, int frame) {
	// Lighting is refigured only by a full paint, as Usecode does.
	const bool was_light = obj->get_info().is_light_source();
	if (obj->get_shapenum() != shape) {
		Map_chunk* chunk = obj->get_chunk();
		if (chunk) {
			const Game_object_shared keep = obj->shared_from_this();
			gwin->add_dirty(obj);
			chunk->remove(obj);    // Re-add so the chunk cache updates.
			obj->set_shape(shape, frame);
			chunk->add(obj);
			gwin->add_dirty(obj);
		}
	}
	if (obj->get_framenum() != frame) {
		obj->change_frame(frame);
	}
	if (was_light != obj->get_info().is_light_source()) {
		gwin->paint();
	}
}

static Actor* friend_usecode_walker = nullptr;

Actor* Friend_mode_usecode_walker() {
	return friend_usecode_walker;
}

// Host: use a door or other guest-usable object for a guest.  Doors are
// toggled the way NPCs do it (quality zeroed so only the door's own toggle
// runs, not story Usecode); shutters, lamps and the like are activated.
static void Friend_mode_use_object_for_guest(const Friend_mode_session::Object_frame_event& request, Actor* actor) {
	const Tile_coord tile(request.tx, request.ty, request.tz);
	Game_object*     obj = Friend_find_door(tile, request.shape, request.old_frame);
	if (!obj || !actor) {
		cout << "Friend Mode: refused object use (nothing at " << tile.tx << ',' << tile.ty << ')' << endl;
		return;
	}
	const bool door = obj->get_info().is_door();
	if (!door && !Friend_mode_guest_usable_shape(obj->get_shapenum())) {
		cout << "Friend Mode: refused object use (shape " << obj->get_shapenum() << " not allowed)" << endl;
		return;
	}
	if (door && obj->get_framenum() % 4 >= 2) {
		cout << "Friend Mode: refused door use (locked)" << endl;
		return;
	}
	// Same reach rule as the guest's double-click (and the Avatar's).
	if (!Fast_pathfinder_client::is_grabable(actor, obj)) {
		cout << "Friend Mode: refused object use (out of reach)" << endl;
		return;
	}
	friend_usecode_walker = actor;
	if (door) {
		const int savequal = obj->get_quality();
		obj->set_quality(0);
		obj->activate();
		obj->set_quality(savequal);
	} else {
		obj->activate();
	}
	friend_usecode_walker = nullptr;
	cout << "Friend Mode: used shape " << obj->get_shapenum() << " for guest at " << tile.tx << ',' << tile.ty << endl;
}

// Host debug aid (F9): build a Friend Mode test kit around the Avatar with
// one of each object guests can use, plus containers with items.  Black
// Gate shapes.  Spawn before a guest joins: new objects are not synced yet,
// but they are part of the world a joining guest downloads.
static Game_object_shared Friend_test_kit_create(int shape, int frame) {
	const Shape_info&        info = ShapeID::get_info(shape);
	const Game_object_shared obj  = gwin->get_map()->create_ireg_object(info, shape, frame, 0, 0, 0);
	obj->set_flag(Obj_flags::okay_to_take);
	return obj;
}

static bool Friend_test_kit_place(int shape, int frame, int dx, int dy, Game_object_shared* placed = nullptr) {
	const Tile_coord avatar = gwin->get_main_actor()->get_tile();
	const Tile_coord want(avatar.tx + dx, avatar.ty + dy, avatar.tz);
	const Tile_coord pos = Map_chunk::find_spot(want, 3, shape, frame, 1);
	if (pos.tx < 0) {
		cerr << "Friend Mode: no room for test shape " << shape << endl;
		return false;
	}
	const Game_object_shared obj = Friend_test_kit_create(shape, frame);
	obj->move(pos);
	if (placed) {
		*placed = obj;
	}
	return true;
}

static void Friend_test_kit_fill(const Game_object_shared& container_obj, int shape, int frame, int quantity) {
	Container_game_object* container = container_obj ? container_obj->as_container() : nullptr;
	if (!container) {
		return;
	}
	const Game_object_shared item = Friend_test_kit_create(shape, frame);
	if (quantity > 1) {
		item->modify_quantity(quantity - 1);
	}
	container->add(item.get(), true);
}

static void Friend_mode_spawn_test_kit() {
	if (!GAME_BG) {
		Friend_mode_notice("The test kit uses Black Gate shapes");
		return;
	}
	Friend_test_kit_place(270, 0, -6, -4);    // Vertical door.
	Friend_test_kit_place(376, 0, -2, -6);    // Horizontal door.
	Friend_test_kit_place(290, 0, 3, -6);     // Closed shutters.
	Friend_test_kit_place(889, 0, 7, -4);     // Lamp post.
	Friend_test_kit_place(481, 0, 7, 0);      // Sconce.
	Friend_test_kit_place(336, 0, 7, 3);      // Light source.
	Friend_test_kit_place(787, 0, -7, 0);     // Lever.
	Game_object_shared chest;
	if (Friend_test_kit_place(800, 0, -4, 5, &chest)) {
		Friend_test_kit_fill(chest, 377, 0, 1);      // Food.
		Friend_test_kit_fill(chest, 644, 0, 25);     // Gold coins.
		Friend_test_kit_fill(chest, 616, 0, 1);      // Bottle.
		Friend_test_kit_fill(chest, 595, 0, 1);      // Torch.
		Friend_test_kit_fill(chest, 641, 0, 1);      // Key.
	}
	Game_object_shared bag;
	if (Friend_test_kit_place(802, 0, 2, 5, &bag)) {
		Friend_test_kit_fill(bag, 377, 0, 2);    // Food.
	}
	gwin->set_all_dirty();
	Friend_mode_notice("Friend Mode test kit placed");
}

// Temporary save used to hand the host's live world to a joining guest.
static constexpr const char* friend_world_file = "<SAVEGAME>/friend_join.sav";

static bool Friend_mode_read_file(const char* fname, std::vector<unsigned char>& data) {
	auto in = U7open_in(fname);
	if (!in) {
		return false;
	}
	data.assign(std::istreambuf_iterator<char>(*in), std::istreambuf_iterator<char>());
	return in->good() || in->eof();
}

// Host: write the live game to a temporary save and send it to the guest.
static void Friend_mode_send_world() {
	gwin->write(true);
	gwin->save_gamedat(friend_world_file, "Friend Mode join");
	std::vector<unsigned char> data;
	if (Friend_mode_read_file(friend_world_file, data)) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::world_save, data);
		cout << "Friend Mode: sent world (" << data.size() << " bytes)" << endl;
	} else {
		cerr << "Friend Mode: could not read " << friend_world_file << endl;
	}
	U7remove(friend_world_file);
}

static void Friend_mode_show_status(const std::string& line1, const std::string& line2) {
	auto font = fontManager.get_font("MENU_FONT");
	if (!font) {
		return;
	}
	gwin->clear_screen(true);
	Image_buffer8* buffer  = gwin->get_win()->get_ib8();
	const int      centerx = gwin->get_width() / 2;
	const int      top     = std::max(20, gwin->get_height() / 3);
	font->center_text(buffer, centerx, top, line1.c_str());
	if (!line2.empty()) {
		font->center_text(buffer, centerx, top + 18, line2.c_str());
	}
	font->center_text(buffer, centerx, top + 50, "Esc cancels");
	gwin->show(true);
}

bool Friend_mode_download_world() {
	friend_session.set_local_player_name(friend_player_name.empty() ? "Player" : friend_player_name);
	if (!friend_session.start_guest(arg_friend_join, arg_friend_port, Game::Get_unique_gamecode())) {
		Friend_mode_show_status("Could not connect to host", arg_friend_join);
		SDL_Delay(2500);
		return false;
	}
	bool         requested = false;
	const uint32 deadline  = SDL_GetTicks() + 60000;
	for (;;) {
		friend_session.poll();
		if (friend_session.connected() && !requested) {
			friend_session.request_world();
			requested = true;
		}
		Friend_mode_session::Blob_kind kind;
		std::vector<unsigned char>     data;
		if (friend_session.take_blob(kind, data) && kind == Friend_mode_session::Blob_kind::world_save) {
			auto out = U7open_out(friend_world_file);
			if (!out) {
				Friend_mode_show_status("Could not store the host's world", "");
				SDL_Delay(2500);
				friend_session.stop();
				return false;
			}
			out->write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
			out.reset();
			cout << "Friend Mode: received world (" << data.size() << " bytes)" << endl;
			gwin->restore_gamedat(friend_world_file);
			U7remove(friend_world_file);
			return true;
		}
		if (!friend_session.peer_open() || SDL_GetTicks() > deadline) {
			Friend_mode_show_status(
					friend_session.peer_open() ? "The host did not respond" : "The host refused the connection",
					"Check that both use the same Exult build and game");
			SDL_Delay(3000);
			friend_session.stop();
			return false;
		}
		const uint32 remaining = friend_session.blob_bytes_remaining();
		if (remaining > 0) {
			Friend_mode_show_status("Downloading the host's world", std::to_string(remaining / 1024) + " KB to go");
		} else {
			Friend_mode_show_status(friend_session.connected() ? "Waiting for the host's world" : "Connecting to host",
					arg_friend_join);
		}
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				friend_session.stop();
				return false;
			}
			if (event.type == SDL_EVENT_QUIT) {
				friend_session.stop();
				throw quit_exception();
			}
		}
		Delay();
	}
}

static void Friend_mode_paint_chat_input() {
	if (!friend_chat_active) {
		return;
	}
	auto font = fontManager.get_font("MENU_FONT");
	if (!font) {
		return;
	}
	Image_buffer8* buffer = gwin->get_win()->get_ib8();
	const int x = 8;
	const int h = font->get_text_height() + 8;
	const int y = gwin->get_height() - h - 8;
	const int w = std::max(120, gwin->get_width() - 16);
	buffer->draw_box(x, y, w, h, 1, 0, 15);
	std::string shown = "Say: " + friend_chat_text + "_";
	while (shown.size() > 6 && font->get_text_width(shown.c_str()) > w - 10) {
		shown.erase(5, 1);
	}
	font->draw_text(buffer, x + 5, y + 4, shown.c_str());
	gwin->set_painted();
}

std::string Friend_mode_display_name(const Game_object* object, const std::string& character_name) {
	const Actor* actor = object ? object->as_actor() : nullptr;
	if (!actor || friend_session.mode() == Friend_mode_session::Mode::off) {
		return character_name;
	}

	// Guests see the host's Avatar by role, not as "yourself".
	const bool  guest_sees_avatar = friend_session.mode() == Friend_mode_session::Mode::guest
								   && actor == gwin->get_main_actor();
	const std::string shown_name = guest_sees_avatar ? std::string("Avatar") : character_name;

	std::string player_name;
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		if (actor == gwin->get_main_actor()) {
			player_name = friend_player_name;
		} else if (actor == gwin->get_remote_friend_actor()) {
			player_name = friend_session.remote_player_name_value();
		}
	} else {
		if (actor == gwin->get_main_actor()) {
			player_name = friend_session.remote_player_name_value();
		} else if (gwin->has_network_friend_control() && actor == gwin->get_controlled_actor()) {
			player_name = friend_player_name;
		}
	}

	return player_name.empty() ? shown_name : shown_name + " (" + player_name + ")";
}

bool Friend_mode_collect_connection_details() {
	auto font = fontManager.get_font("MENU_FONT");
	if (!font) {
		return true;
	}
	std::array<std::string, 2> values{friend_player_name, arg_friend_join.empty() ? "127.0.0.1" : arg_friend_join};
	int active = 0;
	SDL_Window* window = gwin->get_win()->get_screen_window();
	SDL_StartTextInput(window);
	for (;;) {
		gwin->clear_screen(true);
		Image_buffer8* buffer = gwin->get_win()->get_ib8();
		const int centerx = gwin->get_width() / 2;
		const int top = std::max(20, gwin->get_height() / 4);
		font->center_text(buffer, centerx, top, "Connect to Host");
		const std::array<const char*, 2> labels{"Player name", "Host address"};
		for (int i = 0; i < 2; ++i) {
			const int y = top + 28 + i * 36;
			font->draw_text(buffer, centerx - 120, y, labels[i]);
			const uint8 background = i == active ? 138 : 142;
			buffer->draw_box(centerx - 25, y - 4, 145, 20, 1, background, i == active ? 15 : 0);
			font->draw_text(buffer, centerx - 18, y, values[i].c_str());
			if (i == active) {
				const int cursor_x = centerx - 18 + font->get_text_width(values[i].c_str());
				buffer->fill8(15, 1, font->get_text_height(), cursor_x + 1, y);
			}
		}
		font->center_text(buffer, centerx, top + 104, "Tab changes field");
		font->center_text(buffer, centerx, top + 118, "Enter connects   Esc cancels");
		gwin->show(true);

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_TEXT_INPUT) {
				const std::size_t limit = active == 0 ? 20 : 128;
				for (const char* ch = event.text.text; *ch && values[active].size() < limit; ++ch) {
					if (static_cast<unsigned char>(*ch) >= 32 && static_cast<unsigned char>(*ch) < 127) {
						values[active].push_back(*ch);
					}
				}
			} else if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_ESCAPE) {
					SDL_StopTextInput(window);
					return false;
				}
				if (event.key.key == SDLK_TAB || event.key.key == SDLK_UP || event.key.key == SDLK_DOWN) {
					active = 1 - active;
				} else if (event.key.key == SDLK_BACKSPACE && !values[active].empty()) {
					values[active].pop_back();
				} else if ((event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
						   && !values[0].empty() && !values[1].empty()) {
					friend_player_name = values[0];
					arg_friend_join = values[1];
					SDL_StopTextInput(window);
					return true;
				}
			} else if (event.type == SDL_EVENT_QUIT) {
				SDL_StopTextInput(window);
				throw quit_exception();
			}
		}
		Delay();
	}
}

static void Friend_mode_ensure_dialogue() {
	if (friend_session.mode() == Friend_mode_session::Mode::host && friend_session.connected()
			&& !friend_host_dialogue_active) {
		friend_host_dialogue_active = true;
		friend_session.send_dialogue_begin();
	}
}

void Friend_mode_note_speech(Game_object* source, const char* text) {
	if (!source || !text || friend_session.mode() != Friend_mode_session::Mode::host || !friend_session.connected()) {
		return;
	}
	Actor* actor = source->as_actor();
	if (!actor) {
		return;
	}
	uint32 source_id = 0;
	bool creature = false;
	if (actor->get_npc_num() >= 0) {
		source_id = static_cast<uint32>(actor->get_npc_num());
	} else {
		const int index = Friend_creature_index(actor);
		if (index < 0) {
			return;
		}
		source_id = static_cast<uint32>(index);
		creature = true;
	}
	friend_session.send_speech(source_id, creature, text);
	// Conversations and scripted speech may wait in nested input loops, so do
	// not rely on the outer game loop to flush the event.
	friend_session.poll();
}

void Friend_mode_note_dialogue_reset() {
	if (friend_session.mode() == Friend_mode_session::Mode::host && friend_session.connected()
			&& friend_host_dialogue_active) {
		friend_session.send_dialogue_end();
		friend_session.poll();
		friend_host_dialogue_active = false;
	}
}

void Friend_mode_note_dialogue_face(int shape, int frame, int slot) {
	Friend_mode_ensure_dialogue();
	if (friend_host_dialogue_active) {
		friend_session.send_dialogue_face(shape, frame, slot);
		friend_session.poll();
	}
}

void Friend_mode_note_dialogue_face_frame(int frame, int slot) {
	if (friend_host_dialogue_active) {
		friend_session.send_dialogue_face_frame(frame, slot);
		friend_session.poll();
	}
}

void Friend_mode_note_dialogue_remove_face(int shape) {
	if (friend_host_dialogue_active) {
		friend_session.send_dialogue_remove_face(shape);
		friend_session.poll();
	}
}

void Friend_mode_note_dialogue_message(const char* text) {
	Friend_mode_ensure_dialogue();
	if (friend_host_dialogue_active && text) {
		friend_session.send_dialogue_message(text);
		friend_session.poll();
	}
}

void Friend_mode_note_dialogue_choices(const std::vector<std::string>& choices) {
	Friend_mode_ensure_dialogue();
	if (friend_host_dialogue_active) {
		friend_session.send_dialogue_choices(choices);
		friend_session.poll();
	}
}

void Friend_mode_note_dialogue_clear_choices() {
	if (friend_host_dialogue_active) {
		friend_session.send_dialogue_clear_choices();
		friend_session.poll();
	}
}

static std::vector<uint32> Friend_party_members() {
	std::vector<uint32> members;
	Party_manager*      party = gwin->get_party_man();
	for (int i = 0; i < party->get_count(); ++i) {
		members.push_back(static_cast<uint32>(party->get_member(i)));
	}
	return members;
}

// Make the guest's local party match the host's, without running Usecode.
static void Friend_apply_party_members(const std::vector<uint32>& members) {
	Party_manager* party = gwin->get_party_man();
	for (int i = party->get_count() - 1; i >= 0; --i) {
		const uint32 npc_num = static_cast<uint32>(party->get_member(i));
		if (std::find(members.begin(), members.end(), npc_num) == members.end()) {
			party->remove_from_party(gwin->get_npc(static_cast<int>(npc_num)));
		}
	}
	for (uint32 npc_num : members) {
		Actor* actor = gwin->get_npc(static_cast<int>(npc_num));
		if (actor && actor != gwin->get_main_actor() && !actor->is_in_party()) {
			party->add_to_party(actor);
		}
	}
}

static bool Friend_actor_is_eligible(uint32 npc_num) {
	Actor* actor = gwin->get_npc(npc_num);
	if (!actor || actor == gwin->get_main_actor() || actor->is_dead() || actor->get_flag(Obj_flags::asleep)
		|| actor->get_flag(Obj_flags::paralyzed)) {
		return false;
	}
	Party_manager* party = gwin->get_party_man();
	for (int i = 0; i < party->get_count(); ++i) {
		if (party->get_member(i) == static_cast<int>(npc_num)) {
			return true;
		}
	}
	return false;
}

static std::string Friend_actor_description(const Actor* actor) {
	if (!actor) {
		return "Available party companion";
	}
	const std::string& name = actor->get_name();
	if (name == "Iolo") {
		return "Bard and archer\nLongtime friend of the Avatar";
	}
	if (name == "Shamino") {
		return "Ranger and tracker\nTrusted companion";
	}
	if (name == "Dupre") {
		return "Knight and swordsman\nLoyal friend";
	}
	if (name == "Jaana") {
		return "Healer\nDevoted companion";
	}
	if (name == "Julia") {
		return "Tinker and skilled mechanic";
	}
	if (name == "Katrina") {
		return "Shepherd\nSteadfast ally";
	}
	if (name == "Sentri") {
		return "Trainer\nAccomplished fighter";
	}
	if (name == "Spark") {
		return "Young adventurer from Trinsic";
	}
	return "Available party companion";
}

class Friend_actor_card final : public MenuObject {
	Actor*                actor;
	Shape_frame*          portrait;
	std::shared_ptr<Font> font;
	std::shared_ptr<Font> hot_font;
	std::string           description;

public:
	Friend_actor_card(Actor* npc, Shape_frame* face, std::shared_ptr<Font> normal, std::shared_ptr<Font> hot,
			int centerx, int top, int width, int height)
			: actor(npc), portrait(face), font(std::move(normal)), hot_font(std::move(hot)),
			  description(Friend_actor_description(npc)) {
		x = centerx;
		y = y1 = top;
		x1 = centerx - width / 2;
		x2 = x1 + width;
		y2 = y1 + height;
		selected = false;
		dirty = true;
	}

	void paint(Game_window* window) override {
		if (!dirty) {
			return;
		}
		dirty = false;
		Image_buffer8* buffer = window->get_win()->get_ib8();
		const uint8 background = selected ? 138 : 142;
		buffer->draw_box(x1, y1, x2 - x1, y2 - y1, 1, background, 0);
		buffer->draw_beveled_box(x1 + 2, y1 + 2, x2 - x1 - 4, y2 - y1 - 4, 2, background,
				background - 4, background - 2, background + 2, background + 4);

		const int portrait_x = x1 + 10;
		const int portrait_y = y1 + ((y2 - y1) - portrait->get_height()) / 2;
		Shape_manager::get_instance()->paint_shape(
				portrait_x + portrait->get_xleft(), portrait_y + portrait->get_yabove(), portrait);
		const int text_left = portrait_x + portrait->get_width() + 10;
		const int text_width = x2 - text_left - 8;
		auto name_font = selected ? hot_font : font;
		name_font->center_text(buffer, text_left + text_width / 2, y1 + 13, actor->get_name().c_str());
		int line_y = y1 + 34;
		std::size_t line_start = 0;
		while (line_start <= description.size()) {
			const std::size_t line_end = description.find('\n', line_start);
			const std::string line = description.substr(line_start, line_end - line_start);
			std::vector<std::string> words;
			std::istringstream line_stream(line);
			std::string word;
			int line_width = 0;
			const int space_width = font->get_text_width(" ");
			while (line_stream >> word) {
				if (!words.empty()) {
					line_width += space_width;
				}
				line_width += font->get_text_width(word.c_str());
				words.push_back(word);
			}
			int word_x = text_left + (text_width - line_width) / 2;
			for (const std::string& current_word : words) {
				font->paint_text(buffer, current_word.c_str(), word_x, line_y);
				word_x += font->get_text_width(current_word.c_str()) + space_width;
			}
			line_y += font->get_text_height() + 2;
			if (line_end == std::string::npos) {
				break;
			}
			line_start = line_end + 1;
		}
		window->get_win()->show(x1, y1, x2 - x1, y2 - y1);
	}

	bool handle_event(SDL_Event& event) override {
		return (event.type == SDL_EVENT_KEY_DOWN && (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER))
			   || event.type == SDL_EVENT_MOUSE_BUTTON_UP;
	}
};

static int Select_friend_actor(const std::vector<uint32>& roster) {
	std::vector<uint32> choices;
	for (uint32 npc_num : roster) {
		if (gwin->get_npc(npc_num)) {
			choices.push_back(npc_num);
		}
	}
	if (choices.empty()) {
		return -1;
	}

	auto font = fontManager.get_font("MENU_FONT");
	auto hot_font = fontManager.get_font("HOT_FONT");
	if (!hot_font) {
		hot_font = font;
	}
	const int centerx = gwin->get_width() / 2;
	const int title_y = std::max(12, gwin->get_height() / 7);
	std::size_t selected = 0;

	for (;;) {
		Actor* actor = gwin->get_npc(choices[selected]);
		if (!actor) {
			return -1;
		}
		gwin->clear_screen(true);
		font->center_text(gwin->get_win()->get_ib8(), centerx, title_y, "Select Your Character");
		const std::string availability = std::to_string(choices.size()) + " travel companion"
				+ (choices.size() == 1 ? " available" : "s available");
		font->center_text(gwin->get_win()->get_ib8(), centerx, title_y + 14, availability.c_str());

		MenuList menu;
		Shape_frame* portrait = ShapeID(actor->get_face_shapenum(), 0, SF_FACES_VGA).get_shape();
		const int card_width = std::min(280, gwin->get_width() - 52);
		const int card_height = std::max(88, portrait->get_height() + 16);
		auto* card = new Friend_actor_card(actor, portrait, font, hot_font, centerx, title_y + 36, card_width, card_height);
		card->set_id(static_cast<int>(choices[selected]));
		menu.add_entry(card);

		Shape_frame* left_shape = ShapeID(EXULT_FLX_SCROLL_LEFT_SHP, choices.size() > 1 ? 1 : 0, SF_EXULT_FLX).get_shape();
		Shape_frame* right_shape = ShapeID(EXULT_FLX_SCROLL_RIGHT_SHP, choices.size() > 1 ? 1 : 0, SF_EXULT_FLX).get_shape();
		auto* previous = new MenuEntry(left_shape, left_shape, centerx - card_width / 2 - 14, title_y + 67);
		previous->set_id(-2);
		menu.add_entry(previous);
		auto* next = new MenuEntry(right_shape, right_shape, centerx + card_width / 2 + 14, title_y + 67);
		next->set_id(-3);
		menu.add_entry(next);
		menu.set_cancel(-1);
		menu.set_selection(0);
		const int choice = menu.handle_events(gwin);
		if (choice == -2) {
			if (choices.size() > 1) {
				selected = selected == 0 ? choices.size() - 1 : selected - 1;
			}
			continue;
		}
		if (choice == -3) {
			if (choices.size() > 1) {
				selected = (selected + 1) % choices.size();
			}
			continue;
		}
		gwin->set_all_dirty();
		return choice;
	}
}

static string arg_installmod  = {};
static string arg_installdata = {};

static bool                 dragging      = false;    // Object or gump being moved.
static bool                 dragged       = false;    // Flag for when obj. moved.
static bool                 right_on_gump = false;    // Right clicked on gump?
static int                  show_items_x = 0, show_items_y = 0;
static unsigned int         show_items_time    = 0;
static bool                 show_items_clicked = false;
static int                  left_down_x = 0, left_down_y = 0;
static int                  joy_aim_x = 0, joy_aim_y = 0;
Mouse::Avatar_Speed_Factors joy_speed_factor  = Mouse::medium_speed_factor;
static Uint32               last_speed_cursor = 0;    // When we last updated the mouse cursor
#if defined _WIN32
void do_cleanup_output() {
	cleanup_output("std");
}
#endif

/*
 *  Main program.
 */

#ifdef HAVE_PNG_H
#	define EXULT_IMAGE_SUFFIX "png"
#else
#	define EXULT_IMAGE_SUFFIX "pcx"
#endif    // HAVE_PNG_H

int main(int argc, char* argv[]) {
	bool needhelp    = false;
	bool showversion = false;
	int  result;
	Args parameters;

	// Use SDL_IOStream for file I/O in the main game engine for better
	// cross-platform support.  Standalone utilities continue to default
	// to the default std::fstream-based file I/O to avoid taking an SDL
	// dependency.
	U7set_istream_factory([](const char* s, std::ios_base::openmode mode) -> std::unique_ptr<std::istream> {
		auto file = std::make_unique<std::ifstream>(s, mode);
		if (file->good()) {
			return file;
		}
		return std::make_unique<SdlRwopsIstream>(s, mode);
	});
	U7set_ostream_factory([](const char* s, std::ios_base::openmode mode) -> std::unique_ptr<std::ostream> {
		auto file = std::make_unique<std::ofstream>(s, mode);
		if (file->good()) {
			return file;
		}
		return std::make_unique<SdlRwopsOstream>(s, mode);
	});
#ifdef SDL_PLATFORM_IOS
	const char* launchFlag = iOS_GetLaunchGameFlag();

	if (launchFlag) {
		// Convert to appropriate flags
		if (strcmp(launchFlag, "--bg") == 0) {
			run_bg = true;
		} else if (strcmp(launchFlag, "--si") == 0) {
			run_si = true;
		} else if (strcmp(launchFlag, "--fov") == 0) {
			run_fov = true;
		} else if (strcmp(launchFlag, "--ss") == 0) {
			run_ss = true;
		} else if (strcmp(launchFlag, "--sib") == 0) {
			run_sib = true;
		}

		// Clear the launch flag
		iOS_ClearLaunchGameFlag();
	}
#endif
	// Declare everything from the commandline that we're interested in.
	parameters.declare("-h", &needhelp, true);
	parameters.declare("--help", &needhelp, true);
	parameters.declare("/?", &needhelp, true);
	parameters.declare("/h", &needhelp, true);
	parameters.declare("--bg", &run_bg, true);
	parameters.declare("--si", &run_si, true);
	parameters.declare("--fov", &run_fov, true);
	parameters.declare("--ss", &run_ss, true);
	parameters.declare("--sib", &run_sib, true);
	parameters.declare("--nomenu", &arg_nomenu, true);
	parameters.declare("-v", &showversion, true);
	parameters.declare("--version", &showversion, true);
	parameters.declare("--game", &arg_gamename, "default");
	parameters.declare("--mod", &arg_modname, "default");
	parameters.declare("--buildmap", &arg_buildmap, -1);
	parameters.declare("--mapnum", &arg_mapnum, -1);
	parameters.declare("--nocrc", &ignore_crc, true);
	parameters.declare("-c", &arg_configfile, "");
	parameters.declare("--edit", &arg_edit_mode, true);
	parameters.declare("--write-xml", &arg_write_xml, true);
	parameters.declare("--reset-video", &arg_reset_video, true);
	parameters.declare("--verify-files", &arg_verify_files, true);
	parameters.declare("--friend-host", &arg_friend_host, true);
	parameters.declare("--friend-join", &arg_friend_join, "");
	parameters.declare("--friend-port", &arg_friend_port, 1987);
	parameters.declare("--installdata", &arg_installdata);
	parameters.declare("--installmod", &arg_installmod);
#if defined _WIN32
	bool portable = false;
	parameters.declare("-p", &portable, true);
#endif
	// Process the args
	parameters.process(argc, argv);
	add_system_path("<alt_cfg>", arg_configfile);
#if defined _WIN32
	if (portable) {
		add_system_path("<HOME>", ".");
	}
	setup_program_paths();
	redirect_output("std");
	std::atexit(do_cleanup_output);
#endif

	if (needhelp) {
		cerr << "Usage: exult [--help|-h] [-v|--version] [-c configfile]" << endl
			 << "             [--bg|--fov|--si|--ss|--sib|--game <game>] "
				"[--mod <mod>]"
			 << endl
			 << "             [--nomenu] [--buildmap 0|1|2] [--mapnum <num>]" << endl
			 << "             [--nocrc] [--edit] [--write-xml] [--reset-video]" << endl
			 << "             [--friend-host|--friend-join <address>] [--friend-port <port>]" << endl
			 << "--help\t\tShow this information" << endl
			 << "--version\tShow version info" << endl
			 << " -c configfile\tSpecify alternate config file" << endl
			 << "--bg\t\tSkip menu and run Black Gate (prefers original game)" << endl
			 << "--fov\t\tSkip menu and run Black Gate with Forge of Virtue "
				"expansion"
			 << endl
			 << "--si\t\tSkip menu and run Serpent Isle (prefers original game)" << endl
			 << "--ss\t\tSkip menu and run Serpent Isle with Silver Seed "
				"expansion"
			 << endl
			 << "--sib\t\tSkip menu and run Serpent Isle Beta" << endl
			 << "--nomenu\tSkip BG/SI game menu" << endl
			 << "--game <game>\tSkip menu and run the game with key '<game>' "
				"in Exult.cfg"
			 << endl
			 << "--mod <mod>\tMust be used together with '--bg', '--fov', "
				"'--si', '--ss', '--sib' or"
			 << endl
			 << "\t\t'--game <game>'; runs the specified game using the mod "
				"with"
			 << endl
			 << "\t\ttitle equal to '<mod>'" << endl
			 << "--buildmap <N>\tCreate a fullsize map of the game world in "
				"u7map??."
			 << EXULT_IMAGE_SUFFIX << endl
			 << "\t\t(N=0: all roofs, 1: no level 2 roofs, 2: no roofs)" << endl
			 << "\t\tOnly valid if used together with '--bg', '--fov', '--si', "
				"'--ss', '--sib'"
			 << endl
			 << "\t\tor '--game <game>'; you may optionally specify a mod with" << endl
			 << "\t\t'--mod <mod>' (WARNING: requires big amounts of RAM, HD" << endl
			 << "\t\tspace and time!)" << endl
			 << "--mapnum <N>\tThis must be used with '--buildmap'. Selects "
				"which map"
			 << endl
			 << "\t\t(for multimap games or mods) whose map is desired" << endl
			 << "--nocrc\t\tDon't check crc's of .flx files" << endl
			 << "--verify-files\tVerifies that the files in static dir are not "
				"corrupt"
			 << endl
			 << "\t\tOnly valid if used together with '--bg', '--fov', '--si', "
				"'--ss', '--sib'"
			 << endl
			 << "\t\tor '--game <game>'; you cannot specify a mod with this "
				"flag"
			 << endl
			 << "--edit\t\tStart in map-edit mode" << endl;
#if defined _WIN32
		cerr << " -p\t\tMakes the home path the Exult directory (old Windows "
				"way)"
			 << endl;
#endif
		cerr << "--write-xml\tWrite 'patch/exultgame.xml'" << endl
			 << "--reset-video\tResets to the default video settings" << endl
			 << "--friend-host\tStart an experimental Friend Mode listen server" << endl
			 << "--friend-join <address>\tConnect to an experimental Friend Mode host" << endl
			 << "--friend-port <port>\tFriend Mode TCP port (default 1987)" << endl;

		exit(1);
	}
	const unsigned gameparam = static_cast<unsigned>(run_bg) + static_cast<unsigned>(run_si) + static_cast<unsigned>(run_fov)
							   + static_cast<unsigned>(run_ss) + static_cast<unsigned>(run_sib)
							   + static_cast<unsigned>(arg_gamename != "default");
	if (gameparam > 1) {
		cerr << "Error: You may only specify one of --bg, --fov, --si, --ss, "
				"--sib or --game!"
			 << endl;
		exit(1);
	} else if (arg_buildmap >= 0 && gameparam == 0) {
		cerr << "Error: --buildmap requires one of --bg, --fov, --si, --ss, "
				"--sib or --game!"
			 << endl;
		exit(1);
	} else if (arg_verify_files && gameparam == 0) {
		cerr << "Error: --verify-files requires one of --bg, --fov, --si, "
				"--ss, --sib or --game!"
			 << endl;
		exit(1);
	}
#ifndef HAVE_ZIP_SUPPORT
	else if (!arg_installmod.empty()) {
		cerr << "Error: --installmod unsupported in this build of Exult. Zip "
				"file support not compiled in."
			 << endl;
		exit(-1);
	} else if (!arg_installdata.empty()) {
		cerr << "Error: --installdata unsupported in this build of Exult. Zip "
				"file support not compiled in."
			 << endl;
		exit(-1);
	}
#endif

	if (arg_mapnum >= 0 && arg_buildmap < 0) {
		cerr << "Error: '--mapnum' requires '--buildmap'!" << endl;
		exit(1);
	} else if (arg_mapnum < 0) {
		arg_mapnum = 0;    // Sane default.
	}
	if (arg_friend_host && !arg_friend_join.empty()) {
		cerr << "Error: --friend-host and --friend-join cannot be used together" << endl;
		exit(1);
	}
	if (arg_friend_port < 1 || arg_friend_port > 65535) {
		cerr << "Error: --friend-port must be between 1 and 65535" << endl;
		exit(1);
	}

	if (arg_modname != "default" && !gameparam) {
		cerr << "Error: You must also specify the game to be used!" << endl;
		exit(1);
	} else if (arg_verify_files && arg_modname != "default") {
		cerr << "Error: You cannot combine --mod with --verify-files!" << endl;
		exit(1);
	} else if (arg_verify_files && (!arg_installmod.empty() || !arg_installdata.empty())) {
		cerr << "Error: You cannot combine  --verify-files with --installmod "
				"or --installdata!"
			 << endl;
		exit(1);
	} else if (!arg_installmod.empty() && !arg_installdata.empty()) {
		cerr << "Error: You cannot combine  --installmod with --installdata!" << endl;
		exit(1);
	} else if (!arg_installmod.empty() && arg_modname != "default") {
		cerr << "Error: You cannot combine  --installmod with --mod!" << endl;
		exit(1);
	}

	if (showversion) {
		getVersionInfo(cerr);
		return 0;
	}

	try {
		result = exult_main(argv[0]);
	} catch (const quit_exception& /*e*/) {
		// Your basic "shutup Valgrind" code.
		Free_text();
		fontManager.reset();
		delete gamemanager;
		delete Game_window::get_instance();
		delete game;
		Audio::Destroy();    // Deinit the sound system.
		delete config;
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_Quit();
		result = 0;
	} catch (const exult_exception& e) {
		cerr << "============================" << endl
			 << "An exception occured: " << endl
			 << e.what() << endl
			 << "errno: " << e.get_errno() << endl;
		if (e.get_errno() != 0) {
			perror("Error Description");
		}
		if (e.sourcefile()) {
			cout << "SourceFile: " << e.sourcefile() << endl;
			cout << "line: " << e.line() << endl;
		}
		cerr << "============================" << endl;

		// Verify Game Files before we exit just incase the unhandled exception
		// was caused by a bad file
		verify_files(nullptr);

		// make an emergency save
		if (gwin && gwin->get_main_actor()) {
			gwin->MakeEmergencySave();
		}
		result = e.get_errno();
	}

	return result;
}

/*
 *  Main program.
 */

int exult_main(const char* runpath) {
	string music_path;
	// output version info
	getVersionInfo(cout);

#ifndef _WIN32
	setup_program_paths();
#endif
	// Load font_map.txt from exult.flx and register the UTF-8 -> font
	// glyph translator used by Text_msg_file_reader.
	init_font_map();
	// Read in configuration file
	config = new Configuration;

	if (!arg_configfile.empty()) {
		config->read_abs_config_file(arg_configfile);
	} else {
		config->read_config_file(USER_CONFIGURATION_FILE);
	}

#if defined _WIN32
	// Install the crash handler after we've loaded config
	extern void WindowsInstallCrashHandler();
	WindowsInstallCrashHandler();
#endif
	// reset-video command line option
	if (arg_reset_video) {
		config->set("config/video/display/width", 1024, false);
		config->set("config/video/display/height", 768, false);
		config->set("config/video/game/width", 320, false);
		config->set("config/video/game/height", 200, false);
		config->set("config/video/scale", 2, false);
		config->set("config/video/scale_method", "point", false);
		config->set("config/video/fill_mode", "fit", false);
		config->set("config/video/fill_scaler", "point", false);
		config->set("config/video/share_video_settings", "yes", false);
		config->set("config/video/fullscreen", "no", false);
		// [SDL 3] force_bpp is set to 32 in imagewin/imagewin.cc
		// discard the config/video/force_bpp

		config->write_back();
	}
	if (config->key_exists("config/gameplay/allow_double_right_move")) {
		string str;
		config->value("config/gameplay/allow_double_right_move", str, "yes");
		if (str == "no") {
			config->value("config/gameplay/allow_right_pathfind", str, "no");
			config->set("config/gameplay/allow_right_pathfind", str, false);
		}
		config->remove("config/gameplay/allow_double_right_move", false);
	}

	// Setup virtual directories
	string data_path;
	config->value("config/disk/data_path", data_path, EXULT_DATADIR);
	setup_data_dir(data_path, runpath);

	const std::string default_music = get_system_path("<DATA>/music");
	config->value("config/disk/music_path", music_path, default_music.c_str());

	add_system_path("<MUSIC>", music_path);
	add_system_path("<STATIC>", "static");
	add_system_path("<GAMEDAT>", "gamedat");
	add_system_path("<PATCH>", "patch");
	//	add_system_path("<SAVEGAME>", "savegame");
	add_system_path("<SAVEGAME>", ".");
	add_system_path("<MODS>", "mods");

	std::cout << "Exult path settings:" << std::endl;
#if defined(MACOSX) || defined(SDL_PLATFORM_IOS)
	if (is_system_path_defined("<APP_BUNDLE_RES>")) {
		std::cout << "Bundled Data  : " << get_system_path("<BUNDLE>") << std::endl;
	}
#endif
	std::cout << "Data          : " << get_system_path("<DATA>") << std::endl;
	std::cout << "Digital music : " << get_system_path("<MUSIC>") << std::endl;
	std::cout << std::endl;

	// Check CRCs of our .flx files
	bool         crc_ok   = true;
	const char*  flexname = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	const uint32 crc      = crc32(flexname);
	if (crc != EXULT_FLX_CRC32) {
		crc_ok = false;
		cerr << "exult.flx has a wrong checksum!" << endl;
	}
	flexname = BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX);
	if (crc32(flexname) != EXULT_BG_FLX_CRC32) {
		crc_ok = false;
		cerr << "exult_bg.flx has a wrong checksum!" << endl;
	}
	flexname = BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX);
	if (crc32(flexname) != EXULT_SI_FLX_CRC32) {
		crc_ok = false;
		cerr << "exult_si.flx has a wrong checksum!" << endl;
	}

	bool config_ignore_crc;
	config->value("config/disk/no_crc", config_ignore_crc);
	ignore_crc |= config_ignore_crc;

	if (!ignore_crc && !crc_ok) {
		cerr << "This usually means the file(s) mentioned above are "
			 << "from a different version" << endl
			 << "of Exult than this one. Please re-install Exult" << endl
			 << endl
			 << "(Note: if you modified the .flx files yourself, "
			 << "you can skip this check" << endl
			 << "by passing the --nocrc parameter.)" << endl;

		return 1;
	}

	// Convert from old format if needed
	const vector<string> vs = config->listkeys("config/disk/game", false);
	if (vs.empty() && config->key_exists("config/disk/u7path")) {
		// Convert from the older format
		string data_directory;
		config->value("config/disk/u7path", data_directory, "./blackgate");
		config->remove("config/disk/u7path", false);
		config->set("config/disk/game/blackgate/path", data_directory, true);
	}

	// Enable tracing of intrinsics?
	config->value("config/debug/trace/intrinsics", intrinsic_trace);

	// Enable tracing of UC-instructions?
	string uctrace;
	config->value("config/debug/trace/usecode", uctrace, "no");
	to_uppercase(uctrace);
	if (uctrace == "YES") {
		usecode_trace = 1;
	} else if (uctrace == "VERBOSE") {
		usecode_trace = 2;
	} else {
		usecode_trace = 0;
	}

	config->value("config/debug/trace/combat", combat_trace);

	// Save game compression level
	config->value("config/disk/save_compression_level", save_compression, 1);
	if (save_compression < 0 || save_compression > 2) {
		save_compression = 1;
	}
	config->set("config/disk/save_compression_level", save_compression, false);
#ifdef USECODE_DEBUGGER
	// Enable usecode debugger
	config->value("config/debug/debugger/enable", usecode_debugging);
	initialise_usecode_debugger();
#endif

#if (defined(USECODE_DEBUGGER) && defined(XWIN))
	signal(SIGUSR1, SIG_IGN);
#endif

	cheat.init();

#ifdef SDL_PLATFORM_IOS
	iOS_SetupQuickActions();
	touchui = new TouchUI_iOS();
#elif defined(ANDROID)
	touchui = new TouchUI_Android();
#endif
	Init();    // Create main window.

	cheat.finish_init();
	cheat.set_map_editor(arg_edit_mode);    // Start in map-edit mode?
	if (arg_write_xml) {
		game->write_game_xml();
	}

	Mouse mouse(gwin);
	Mouse::mouse()->set_shape(Mouse::hand);

	const int result = Play();    // start game

#ifdef USE_EXULTSTUDIO
	// Currently, leaving the game results in destruction of the window.
	//  Maybe sometime in the future, there is an option like "return to
	//  main menu and select another scenario". Becaule DnD isn't registered
	//  until you really enter the game, we remove it here to prevent possible
	//  bugs invilved with registering DnD a second time over an old variable.
	Server_close();
#endif

	return result;
}

namespace ExultIcon {
#include "exulticon.h"
}

static void SetIcon() {
#ifndef MACOSX    // Don't set icon on OS X; the external icon is *much* nicer
	SDL_Color iconpal[256];
	for (int i = 0; i < 256; ++i) {
		iconpal[i].r = ExultIcon::header_data_cmap[i][0];
		iconpal[i].g = ExultIcon::header_data_cmap[i][1];
		iconpal[i].b = ExultIcon::header_data_cmap[i][2];
	}
	SDL_Surface* iconsurface = SDL_CreateSurface(ExultIcon::width, ExultIcon::height, SDL_GetPixelFormatForMasks(32, 0, 0, 0, 0));
	if (iconsurface == nullptr) {
		cout << "Error creating icon surface: " << SDL_GetError() << std::endl;
		return;
	}
	const SDL_PixelFormatDetails* iconsurface_format  = SDL_GetPixelFormatDetails(iconsurface->format);
	SDL_Palette*                  iconsurface_palette = SDL_GetSurfacePalette(iconsurface);
	for (int y = 0; y < static_cast<int>(ExultIcon::height); ++y) {
		for (int x = 0; x < static_cast<int>(ExultIcon::width); ++x) {
			const int    idx = ExultIcon::header_data[(y * ExultIcon::height) + x];
			const Uint32 pix = SDL_MapRGB(iconsurface_format, iconsurface_palette, iconpal[idx].r, iconpal[idx].g, iconpal[idx].b);
			const SDL_Rect destRect = {x, y, 1, 1};
			SDL_FillSurfaceRect(iconsurface, &destRect, pix);
		}
	}
	SDL_SetSurfaceColorKey(
			iconsurface, true, SDL_MapRGB(iconsurface_format, iconsurface_palette, iconpal[0].r, iconpal[0].g, iconpal[0].b));
	SDL_SetWindowIcon(gwin->get_win()->get_screen_window(), iconsurface);
	SDL_DestroySurface(iconsurface);
#endif
}

void Open_game_controller(SDL_JoystickID joystick_index) {
	SDL_Gamepad* input_device = SDL_OpenGamepad(joystick_index);
	if (input_device) {
		SDL_GetGamepadJoystick(input_device);
		std::cout << "Game controller attached and open: \"" << SDL_GetGamepadName(input_device) << '"' << std::endl;
	} else {
		std::cout << "Game controller attached, but it failed to open. Error: \"" << SDL_GetError() << '"' << std::endl;
	}
}

bool Handle_device_connection_event(void* userdata, SDL_Event* event) {
	ignore_unused_variable_warning(userdata);
	// Make sure that game-controllers are opened and closed, as they
	// become connected or disconnected.
	switch (event->type) {
	case SDL_EVENT_GAMEPAD_ADDED: {
		if (!SDL_GetGamepadFromID(event->gdevice.which)) {
			Open_game_controller(event->gdevice.which);
		}
		break;
	}
	case SDL_EVENT_GAMEPAD_REMOVED: {
		SDL_Gamepad* input_device = SDL_GetGamepadFromID(event->gdevice.which);
		if (input_device) {
			SDL_CloseGamepad(input_device);
			input_device = nullptr;
			std::cout << "Game controller detached and closed." << std::endl;
		}
		break;
	}
	}

	// Returning true will tell SDL3, which can invoke this via a callback
	// setup through SDL_AddEventWatch, to make sure the event gets posted
	// to its event-queue (rather than dropping it).
	return true;
}

/*
 *  Initialize and create main window.
 */
static void Init() {
	const Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD;
	// Let SDL3 choose its own audio/video drivers until proved otherwise.
	// Remove SDL_HINT_AUDIO_DRIVER set as "DirectSound"
	// Remove SDL_HINT_VIDEO_DRIVER set as "wayland,x11"
	const Uint32 joyinit = 0;
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "Landscape");
	Mouse::use_touch_input = true;
	// Remove SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE set as "0"
#	if defined(SDL_PLATFORM_IOS)
	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
#	endif
#	if defined(ANDROID)
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "8192");
#	endif
#endif
	// SDL3 lost the pointer when switching Windowed <-> Fullscreen in Wayland
	//   This was fixed
	//     1- By disabling SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP
	//        here in Exult.
	//        [ That hint was generalized to other video backends than Wayland
	//          as SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE. ]
	//     2- By SDL 3 disabling MOUSE_RELATIVE_MODE_WARP at
	//        Windowed <-> Fullscreen transitions on Wayland or XWayland.
	//        [ Resetting that hint in Exult then became useless. ]
	// Remove SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE set as "0"
	// Do not confuse that Hint with :
	//        SDL_HINT_MOUSE_RELATIVE_MODE_WARP         set as "0"
	if (!SDL_Init(init_flags | joyinit)) {
		cerr << "Unable to initialize SDL: " << SDL_GetError() << endl;
		exit(-1);
	}
	std::atexit(SDL_Quit);

	// KBD repeat should be nice.
	SDL_HideCursor();

	// Open any connected game controllers.
	SDL_JoystickID* joysticks = SDL_GetJoysticks(nullptr);
	if (joysticks) {
		for (int i = 0; joysticks[i]; ++i) {
			if (SDL_IsGamepad(joysticks[i])) {
				Open_game_controller(joysticks[i]);
			}
		}
		SDL_free(joysticks);
	}
	// Listen for game controller device connection and disconnection
	// events. Registering a listener allows these events to be received
	// and processed via any event-processing loop, of which Exult has
	// many, without needing to modify each individual loop, and to
	// make sure that SDL_Gamepad objects are always ready.
	SDL_AddEventWatch(Handle_device_connection_event, nullptr);

	// Load games and mods; also stores system paths:
	gamemanager = new GameManager();

	if (arg_buildmap < 0 && !arg_verify_files && arg_installdata.empty() && arg_installmod.empty()) {
		string gr;
		string gg;
		string gb;
		config->value("config/video/gamma/red", gr, "1.0");
		config->value("config/video/gamma/green", gg, "1.0");
		config->value("config/video/gamma/blue", gb, "1.0");
		Image_window8::set_gamma(atof(gr.c_str()), atof(gg.c_str()), atof(gb.c_str()));
		string fullscreenstr;    // Check config. for fullscreen mode.
		config->value("config/video/fullscreen", fullscreenstr, "no");
		const bool fullscreen = (fullscreenstr == "yes");
		config->set("config/video/fullscreen", fullscreen ? "yes" : "no", false);

		int border_red;
		int border_green;
		int border_blue;
		config->value("config/video/game/border/red", border_red, 0);
		if (border_red < 0) {
			border_red = 0;
		} else if (border_red > 255) {
			border_red = 255;
		}
		config->set("config/video/game/border/red", border_red, false);

		config->value("config/video/game/border/green", border_green, 0);
		if (border_green < 0) {
			border_green = 0;
		} else if (border_green > 255) {
			border_green = 255;
		}
		config->set("config/video/game/border/green", border_green, false);

		config->value("config/video/game/border/blue", border_blue, 0);
		if (border_blue < 0) {
			border_blue = 0;
		} else if (border_blue > 255) {
			border_blue = 255;
		}
		config->set("config/video/game/border/blue", border_blue, false);

		Palette::set_border(border_red, border_green, border_blue);
		bool disable_fades;
		config->value("config/video/disable_fades", disable_fades, false);

		setup_video(fullscreen, VIDEO_INIT);
		SetIcon();
		Audio::Init();
		gwin->get_pal()->set_fades_enabled(!disable_fades);
		gwin->set_in_exult_menu(false);
	}

	SDL_SetEventFilter(nullptr, nullptr);
	// Show the banner
	game = nullptr;

	do {
		reset_system_paths();
		fontManager.reset();
		U7FileManager::get_ptr()->reset();

		if (game) {
			delete game;
			game = nullptr;
		}

		ModManager* basegame = nullptr;
		if (run_bg) {
			basegame     = gamemanager->get_bg();
			arg_gamename = CFG_BG_NAME;
			run_bg       = false;
		} else if (run_fov) {
			basegame     = gamemanager->get_fov();
			arg_gamename = CFG_FOV_NAME;
			run_fov      = false;
		} else if (run_si) {
			basegame     = gamemanager->get_si();
			arg_gamename = CFG_SI_NAME;
			run_si       = false;
		} else if (run_ss) {
			basegame     = gamemanager->get_ss();
			arg_gamename = CFG_SS_NAME;
			run_ss       = false;
		} else if (run_sib) {
			basegame     = gamemanager->get_sib();
			arg_gamename = CFG_SIB_NAME;
			run_sib      = false;
		}
		BaseGameInfo* newgame = nullptr;
		if (basegame != nullptr || arg_gamename != "default") {
			if (basegame == nullptr) {
				basegame = gamemanager->find_game(arg_gamename);
			}
			if (basegame != nullptr) {
				if (arg_modname != "default") {
					// Prints error messages:
					newgame = basegame->get_mod(arg_modname);
				} else {
					newgame = basegame;
				}
				arg_modname = "default";
			} else {
				cerr << "Game '" << arg_gamename << "' not found." << endl;
				newgame = nullptr;
			}
			// Prevent game from being reloaded in case the player
			// tries to return to the main menu:
			arg_gamename = "default";
		}
#ifdef HAVE_ZIP_SUPPORT
		if (!arg_installmod.empty()) {
			if (newgame != nullptr) {
				newgame->setup_game_paths();
			}
			std::cout << "\nInstallmod: want to install mods in zip " << arg_installmod << std::endl;
			int code = ModManager::InstallModZip(arg_installmod, dynamic_cast<ModManager*>(newgame), gamemanager);

			if (code < 0) {
				std::cerr << "InstallMod: Failed to install one or more mods "
							 "in zip file "
						  << arg_installmod << std::endl
						  << "Error code:" << code << std::endl;
			} else {
				std::cerr << "InstallMod: Sucesssfully installed all mods in "
							 "zip \""
						  << arg_installmod << "\"" << std::endl;
			}

			exit(code);
		}
		if (!arg_installdata.empty()) {
			std::cout << "\nInstallData: want to extract zip \"" << arg_installdata << "\" to <DATA>" << std::endl;

			IFileDataSource ds(arg_installdata);
			if (!ds.good()) {
				std::cerr << "InstallData: Failed to open file \"" << arg_installdata << "\" for reading" << std::endl;
				exit(-2);
			}

			unzFile unzipfile = unzOpen(&ds);
			if (!unzipfile) {
				std::cerr << "InstallData: Failed to open \"" << arg_installdata << "\" as a zip file" << std::endl;
				exit(-3);
			}

			int error = unzExtractAllToPath(unzipfile, "<DATA>");
			if (error < 0) {
				std::cerr << "InstallData: Failed to extract zip file \"" << arg_installdata << "\" to <DATA>. Error code:" << error
						  << std::endl;
			} else {
				std::cerr << "InstallData: Sucesssfully installed \"" << arg_installdata << "\" into <DATA>" << std::endl;
			}

			exit(error);
		}
#endif
		if (!newgame) {
			ExultMenu exult_menu(gwin);
			newgame = exult_menu.run();
		}
		assert(newgame != nullptr);

		if (arg_buildmap >= 0) {
			BuildGameMap(newgame, arg_mapnum);
			exit(0);
		}
		if (arg_verify_files) {
			newgame->setup_game_paths();
			exit(verify_files(newgame));
		}

#ifdef DEBUG
		{
			int ix;
			int compVers = SDL_VERSION, linkVers = SDL_GetVersion();

			cout << "SDL Initialized, Exult compiled against SDL " << SDL_VERSIONNUM_MAJOR(compVers) << "."
				 << SDL_VERSIONNUM_MINOR(compVers) << "." << SDL_VERSIONNUM_MICRO(compVers) << ", Exult running against SDL "
				 << SDL_VERSIONNUM_MAJOR(linkVers) << "." << SDL_VERSIONNUM_MINOR(linkVers) << "." << SDL_VERSIONNUM_MICRO(linkVers)
				 << endl;
			cout << "SDL Video :" << endl;
			cout << "    Hint VIDEO_DRIVER is '"
				 << (SDL_GetHint(SDL_HINT_VIDEO_DRIVER) ? SDL_GetHint(SDL_HINT_VIDEO_DRIVER) : "(null)")
				 << "', Current Video Driver is '" << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "(null)") << "'"
				 << endl;
			for (ix = 0; ix < SDL_GetNumVideoDrivers(); ix++) {
				cout << "    [ " << ix << " ] Video Driver is '" << SDL_GetVideoDriver(ix) << "'" << endl;
			}
			SDL_DisplayID* display_ids = SDL_GetDisplays(nullptr);
			for (ix = 0; display_ids[ix]; ix++) {
				const SDL_DisplayMode* currMode = SDL_GetCurrentDisplayMode(display_ids[ix]);
				int                    nbpp;
				Uint32                 Rmask;
				Uint32                 Gmask;
				Uint32                 Bmask;
				Uint32                 Amask;
				SDL_GetMasksForPixelFormat(currMode->format, &nbpp, &Rmask, &Gmask, &Bmask, &Amask);
				cout << "    [ " << ix << " ] Video Display Mode [ ID is " << display_ids[ix] << " ] is '" << currMode->w << "x"
					 << currMode->h << "' at '" << currMode->refresh_rate << "Hz' for " << nbpp << " bpp" << endl;
			}
			SDL_free(display_ids);
			const char* rname = SDL_GetRendererName(SDL_GetRenderer(gwin->get_win()->get_screen_window()));
			cout << "    Hint RENDER_DRIVER is '"
				 << (SDL_GetHint(SDL_HINT_RENDER_DRIVER) ? SDL_GetHint(SDL_HINT_RENDER_DRIVER) : "(null)")
				 << "', Current Render Driver is '" << (rname ? rname : "(null)") << "'" << endl;
			for (ix = 0; ix < SDL_GetNumRenderDrivers(); ix++) {
				cout << "    [ " << ix << " ] Video Renderer is '" << (SDL_GetRenderDriver(ix) ? SDL_GetRenderDriver(ix) : "(null)")
					 << "'" << endl;
			}
			cout << "SDL Audio :" << endl;
			cout << "    Hint AUDIO_DRIVER is '"
				 << (SDL_GetHint(SDL_HINT_AUDIO_DRIVER) ? SDL_GetHint(SDL_HINT_AUDIO_DRIVER) : "(null)")
				 << "', Current Audio Driver is '" << (SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "(null)") << "'"
				 << endl;
			for (ix = 0; ix < SDL_GetNumAudioDrivers(); ix++) {
				cout << "    [ " << ix << " ] Audio Driver is '" << SDL_GetAudioDriver(ix) << "'" << endl;
			}
			SDL_AudioDeviceID* devices;
			int                num_devices;
			devices = SDL_GetAudioRecordingDevices(&num_devices);
			for (ix = 0; ix < num_devices; ix++) {
				SDL_AudioSpec as;
				int           frames = 0;
				cout << "    [ " << ix << " ] Audio Device [ ID is " << devices[ix] << " ] is [recording] '"
					 << (SDL_GetAudioDeviceName(devices[ix]) ? SDL_GetAudioDeviceName(devices[ix]) : "(null)") << "'";
				if (!SDL_GetAudioDeviceFormat(devices[ix], &as, &frames)) {
					cout << ", " << static_cast<int>(as.channels) << " channel(s) at '" << as.freq << "Hz', " << frames << " frames"
						 << endl;
				} else {
					cout << endl;
				}
			}
			SDL_free(devices);
			devices = SDL_GetAudioPlaybackDevices(&num_devices);
			for (ix = 0; ix < num_devices; ix++) {
				SDL_AudioSpec as;
				int           frames = 0;
				cout << "    [ " << ix << " ] Audio Device [ ID is " << devices[ix] << " ] is [playback] '"
					 << (SDL_GetAudioDeviceName(devices[ix]) ? SDL_GetAudioDeviceName(devices[ix]) : "(null)") << "'";
				if (!SDL_GetAudioDeviceFormat(devices[ix], &as, &frames)) {
					cout << ", " << static_cast<int>(as.channels) << " channel(s) at '" << as.freq << "Hz', " << frames << " frames"
						 << endl;
				} else {
					cout << endl;
				}
			}
			SDL_free(devices);
		}
#endif
		Game::create_game(newgame);
		Audio* audio = Audio::get_ptr();
		audio->Init_sfx();
		MyMidiPlayer* midi = audio->get_midi();

		Game::setup_text();

		// Skip splash screen?
		bool skip_splash;
		config->value("config/gameplay/skip_splash", skip_splash);

		// Check for mod overrides
		bool force_skip_splash   = false;
		bool force_digital_music = false;
		if (gamemanager != nullptr) {
			ModManager* current_game_mgr = gamemanager->find_game(Game::get_gametitle());
			if (current_game_mgr != nullptr && !Game::get_modtitle().empty()) {
				ModInfo* mod_info = current_game_mgr->get_mod(Game::get_modtitle(), false);
				if (mod_info != nullptr) {
					if (mod_info->has_force_skip_splash_set()) {
						force_skip_splash = mod_info->get_force_skip_splash();
					}
					if (mod_info->has_force_digital_music_set()) {
						force_digital_music = mod_info->get_force_digital_music();
					}
				}
			}
		}

		// Digital music if forced by mod
		if (force_digital_music) {
			if (midi != nullptr) {
				midi->set_ogg_enabled(true);
			}
		}

		// Make sure we have a proper palette before playing the intro.
		gwin->get_pal()->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULT0_PAL);
		gwin->get_pal()->apply();
		if ((!skip_splash && !force_skip_splash) && (Game::get_game_type() != EXULT_DEVEL_GAME || U7exists(INTRO_DAT))) {
			if (midi) {
				midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_INTRO);
			}
			game->play_intro();
			std::cout << "played intro" << std::endl;
		}

		if (midi) {
			if (arg_nomenu) {
				midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_INTRO);
			} else {
				midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_MAINMENU);
			}
		}
	} while (!game || !game->show_menu(arg_nomenu, !arg_friend_join.empty()));

	// Should not be needed anymore:
	delete gamemanager;
	gamemanager = nullptr;

	Audio*        audio = Audio::get_ptr();
	MyMidiPlayer* midi  = audio->get_midi();
	if (midi) {
		midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GAME);
	}

	gwin->init_files();
	gwin->read_gwin();
	gwin->setup_game(arg_edit_mode);    // This will start the scene.
	if (arg_friend_host) {
		friend_player_name = gwin->get_main_actor()->get_name();
		if (friend_player_name.empty()) {
			friend_player_name = "Host";
		}
		friend_session.set_local_player_name(friend_player_name);
		friend_session.start_host(arg_friend_port, Game::Get_unique_gamecode());
	} else if (!arg_friend_join.empty() && friend_session.mode() == Friend_mode_session::Mode::off) {
		// Normally already connected from the title menu while downloading
		// the host's world.
		friend_session.set_local_player_name(friend_player_name.empty() ? "Player" : friend_player_name);
		friend_session.start_guest(arg_friend_join, arg_friend_port, Game::Get_unique_gamecode());
	}
										// Get scale factor for mouse.
#ifdef USE_EXULTSTUDIO
	Server_init();    // Initialize server (for map-editor).
	SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
	SDL_SetEventEnabled(SDL_EVENT_DROP_TEXT, true);
#endif
}

/*
 *  Play game.
 */

static int Play() {
	do {
		quitting_time = QUIT_TIME_NO;
		Handle_events();
		if (quitting_time == QUIT_TIME_RESTART) {
			Mouse::mouse()->hide();    // Turn off mouse.
			gwin->read();              // Restart
									   /////gwin->setup_game();
									   //// setup_game is already being called from inside
									   //// of gwin->read(), so no need to call it here, I hope...
		}
	} while (quitting_time == QUIT_TIME_RESTART);

	delete gwin;

	Audio::Destroy();    // Deinit the sound system.
	delete keybinder;
	delete config;
	Free_text();
	fontManager.reset();
	delete game;
	return 0;
}

#ifdef USE_EXULTSTUDIO    // Shift-click means 'paint'.
/*
 *  Add a shape while map-editing.
 */

static void Paint_with_shape(
		SDL_Event& event,
		bool       dragging    // Painting terrain.
) {
	static int lasttx = -1;
	static int lastty = -1;
	// int scale = gwin->get_win()->get_scale();
	// int x = event.button.x/scale, y = event.button.y/scale;
	int x;
	int y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);

	const int tx = (gwin->get_scrolltx() + x / c_tilesize);
	const int ty = (gwin->get_scrollty() + y / c_tilesize);
	if (dragging) {    // See if moving to a new tile.
		if (tx == lasttx && ty == lastty) {
			return;
		}
	}
	lasttx                 = tx;
	lastty                 = ty;
	const int        shnum = cheat.get_edit_shape();
	int              frnum;
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {    // ALT?  Pick random frame.
		const ShapeID id(shnum, 0);
		frnum = std::rand() % id.get_num_frames();
	} else if (mod & SDL_KMOD_CTRL) {    // Cycle through frames.
		frnum = cheat.get_edit_frame();
		const ShapeID id(shnum, 0);
		const int     nextframe = (frnum + 1) % id.get_num_frames();
		cheat.set_edit_shape(shnum, nextframe);
	} else {
		frnum = cheat.get_edit_frame();
	}
	Drop_dragged_shape(shnum, frnum, event.button.x, event.button.y);
}

/*
 *  Set a complete chunk while map-editing.
 */

static void Paint_with_chunk(
		SDL_Event& event,
		bool       dragging    // Painting terrain.
) {
	static int lastcx = -1;
	static int lastcy = -1;
	int        x;
	int        y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);
	const int cx = (gwin->get_scrolltx() + x / c_tilesize) / c_tiles_per_chunk;
	const int cy = (gwin->get_scrollty() + y / c_tilesize) / c_tiles_per_chunk;
	if (dragging) {    // See if moving to a new chunk.
		if (cx == lastcx && cy == lastcy) {
			return;
		}
	}
	lastcx          = cx;
	lastcy          = cy;
	const int chnum = cheat.get_edit_chunknum();
	Drop_dragged_chunk(chnum, event.button.x, event.button.y);
}

/*
 *  Select chunks.
 */

static void Select_chunks(
		SDL_Event& event,
		bool       dragging,    // Painting terrain.
		bool       toggle) {
	static int lastcx = -1;
	static int lastcy = -1;
	int        x;
	int        y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);
	const int cx = (gwin->get_scrolltx() + x / c_tilesize) / c_tiles_per_chunk;
	const int cy = (gwin->get_scrollty() + y / c_tilesize) / c_tiles_per_chunk;
	if (dragging) {    // See if moving to a new chunk.
		if (cx == lastcx && cy == lastcy) {
			return;
		}
	}
	lastcx           = cx;
	lastcy           = cy;
	Map_chunk* chunk = gwin->get_map()->get_chunk(cx, cy);
	if (toggle) {
		if (!chunk->is_selected()) {
			cheat.add_chunksel(chunk);
		} else {
			chunk->set_selected(false);
		}
	} else {
		cheat.add_chunksel(chunk);
	}
	gwin->set_all_dirty();
}

/*
 *  Select for combo and edit
 */
static void Select_for_combo(
		SDL_Event& event,
		bool       dragging,    // Dragging to select.
		bool       toggle) {
	static Game_object* last_obj = nullptr;
	int                 x;
	int                 y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);
	// int tx = (gwin->get_scrolltx() + x/c_tilesize)%c_num_tiles;
	// int ty = (gwin->get_scrollty() + y/c_tilesize)%c_num_tiles;
	Game_object* obj = gwin->find_object(x, y);
	if (obj) {
		if (dragging && obj == last_obj) {
			return;
		}
		last_obj = obj;
	} else {
		return;
	}
	const ShapeID     id   = *obj;
	const Tile_coord  t    = obj->get_tile();
	const std::string name = get_item_name(id.get_shapenum());
	if (toggle) {
		cheat.toggle_selected(obj);
	} else if (!cheat.is_selected(obj)) {
		cheat.append_selected(obj);
		gwin->add_dirty(obj);
	}
	// only send the data if we are in combo mode
	if (cheat.get_edit_mode() == Cheat::combo_pick
		&& Object_out(
				   client_socket, toggle ? Exult_server::combo_toggle : Exult_server::combo_pick, nullptr, t.tx, t.ty, t.tz,
				   id.get_shapenum(), id.get_framenum(), 0, name)
				   == -1) {
		cout << "Error sending shape to ExultStudio" << endl;
	}
}

#endif

/*
 *  Handle events until a flag is set.
 */

static void Handle_events() {
	uint32 last_repaint = 0;    // For insuring animation repaints.
	uint32 last_rest    = 0;
#ifdef DEBUG
	uint32 last_fps = 0;
#endif
	/*
	 *  Main event loop.
	 */
	int last_x = -1;
	int last_y = -1;
	// Interpolation duration (ms) for the current smooth-scroll
	// segment. Captured when the scroll actually changes so the glide always
	// runs at the speed of the step that produced it.
	int lerp_mswait = 0;
	// True once the final catch-up glide has been shortened after the camera
	// actor stopped (see the stop-skate handling below). Reset on each new
	// step so it only ever fires once per stop.
	bool lerp_stop_anchored = false;
	bool friend_roster_sent = false;
	bool friend_selection_shown = false;
	int  host_assigned_actor = -1;
	Tile_coord last_host_state(-1, -1, -1);
	int last_host_frame = -1;
	Tile_coord last_avatar_state(-1, -1, -1);
	int last_avatar_frame = -1;
	uint32 last_npc_snapshot = 0;
	int  guest_assigned_actor = -1;
	Tile_coord last_guest_request(-1, -1, -1);
	std::unordered_set<int> guest_frozen_npcs;
	std::unordered_set<uint32> guest_frozen_creatures;
	uint32                     guest_close_at = 0;    // Ticks when a disconnected guest exits.
	uint32                     guest_leash_notice_at = 0;    // Last "must return" notice.
	while (!quitting_time) {
		friend_session.poll();
		std::string incoming_chat;
		uint32      incoming_speaker;
		while (friend_session.take_chat(incoming_speaker, incoming_chat)) {
			Friend_mode_show_chat(
					Friend_mode_remote_speaker(incoming_speaker), friend_session.remote_player_name_value(),
					incoming_chat);
		}
		if (arg_friend_host) {
			if (friend_session.take_world_request()) {
				Friend_mode_send_world();
			}
			if (friend_session.take_peer_joined()) {
				friend_host_peer_name = friend_session.remote_player_name_value();
				Friend_mode_notice(friend_host_peer_name + " has joined");
			}
			if (!friend_session.connected()) {
				friend_sent_party.clear();
				friend_leash_return_at = 0;
				if (!friend_host_peer_name.empty()) {
					Friend_mode_notice(
							host_assigned_actor >= 0
									? friend_host_peer_name + " has left; " + Friend_mode_actor_name(host_assigned_actor)
											  + " rejoins the party"
									: friend_host_peer_name + " has left");
					friend_host_peer_name.clear();
				}
				friend_host_dialogue_active = false;
				friend_roster_sent = false;
				last_avatar_state = Tile_coord(-1, -1, -1);
				last_avatar_frame = -1;
				last_npc_snapshot = 0;
				if (host_assigned_actor >= 0) {
					gwin->release_remote_friend_mode_actor();
					host_assigned_actor = -1;
					last_host_state = Tile_coord(-1, -1, -1);
					last_host_frame = -1;
				}
			} else if (!friend_roster_sent) {
				friend_sent_party = Friend_party_members();
				friend_session.send_party_members(friend_sent_party);
				std::vector<uint32> roster;
				Party_manager* party = gwin->get_party_man();
				for (int i = 0; i < party->get_count(); ++i) {
					const uint32 npc_num = party->get_member(i);
					if (Friend_actor_is_eligible(npc_num)) {
						roster.push_back(npc_num);
					}
				}
				friend_session.send_party_roster(roster);
				cout << "Friend Mode: advertised " << roster.size() << " available companion(s)" << endl;
				friend_roster_sent = true;
			} else {
				std::vector<uint32> members = Friend_party_members();
				if (members != friend_sent_party) {
					friend_sent_party = std::move(members);
					friend_session.send_party_members(friend_sent_party);
				}
			}
			uint32 requested_actor = 0;
			if (friend_session.take_actor_request(requested_actor)) {
				if (host_assigned_actor < 0 && Friend_actor_is_eligible(requested_actor)
					&& gwin->reserve_friend_mode_actor(static_cast<int>(requested_actor))) {
					host_assigned_actor = static_cast<int>(requested_actor);
					friend_session.accept_actor(requested_actor);
					Friend_mode_notice(
							friend_session.remote_player_name_value() + " enters the world as "
							+ Friend_mode_actor_name(host_assigned_actor));
					Actor* actor = gwin->get_npc(host_assigned_actor);
					last_host_state = actor->get_tile();
					last_host_frame = actor->get_framenum();
					friend_session.send_actor_state(
							last_host_state.tx, last_host_state.ty, last_host_state.tz, last_host_frame);
				} else {
					friend_session.reject_actor(requested_actor);
				}
			}
			int tx = 0;
			int ty = 0;
			int tz = 0;
			int requested_frame = 0;
			if (host_assigned_actor >= 0 && friend_session.take_move_request(tx, ty, tz, requested_frame)) {
				Actor* actor = gwin->get_npc(host_assigned_actor);
				const Tile_coord requested(tx, ty, tz);
				if (actor && actor->get_tile().distance(requested) <= 2
					&& gwin->friend_leash_allows(actor->get_tile(), requested)) {
					cout << "Friend Mode: host accepted move to " << tx << ',' << ty << ',' << tz << endl;
					// The guest already collision-tested this adjacent step. Applying it
					// directly avoids a second pathfinder disagreeing about door state.
					actor->move(requested.tx, requested.ty, requested.tz);
					actor->change_frame(requested_frame);
				} else {
					cout << "Friend Mode: host rejected out-of-range move to " << tx << ',' << ty << ',' << tz << endl;
					// Resend the true position so the guest snaps back.
					last_host_state = Tile_coord(-1, -1, -1);
				}
			}
			Friend_mode_session::Object_frame_event door_request;
			while (friend_session.take_door_use(door_request)) {
				Friend_mode_use_object_for_guest(
						door_request, host_assigned_actor >= 0 ? gwin->get_npc(host_assigned_actor) : nullptr);
			}
			// The Avatar left the guest's companion beyond the leash: after a
			// grace period, bring it back beside the Avatar like party catch-up.
			if (host_assigned_actor >= 0) {
				Actor*      actor  = gwin->get_npc(host_assigned_actor);
				Main_actor* avatar = gwin->get_main_actor();
				const bool  other_map = actor && actor->get_map() != avatar->get_map();
				if (actor
					&& (other_map
						|| gwin->friend_leash_distance(actor->get_tile()) > Game_window::friend_leash_max_tiles)) {
					const uint32 now = SDL_GetTicks();
					if (friend_leash_return_at == 0) {
						friend_leash_return_at = now + (other_map ? 0 : friend_leash_grace_ms);
					}
					if (now >= friend_leash_return_at) {
						const Tile_coord spot = Map_chunk::find_spot(avatar->get_tile(), 3, actor);
						if (spot.tx >= 0) {
							actor->move(spot, avatar->get_map_num());
							cout << "Friend Mode: " << actor->get_name() << " returned to the Avatar" << endl;
						}
						friend_leash_return_at = 0;
					}
				} else {
					friend_leash_return_at = 0;
				}
			}
			if (host_assigned_actor >= 0) {
				Actor* actor = gwin->get_npc(host_assigned_actor);
				if (actor && (actor->get_tile() != last_host_state || actor->get_framenum() != last_host_frame)) {
					last_host_state = actor->get_tile();
					last_host_frame = actor->get_framenum();
					friend_session.send_actor_state(
							last_host_state.tx, last_host_state.ty, last_host_state.tz, last_host_frame);
				}
			}
			if (friend_session.connected()) {
				Main_actor* avatar = gwin->get_main_actor();
				const Tile_coord avatar_state = avatar->get_tile();
				const int avatar_frame = avatar->get_framenum();
				if (avatar_state != last_avatar_state || avatar_frame != last_avatar_frame) {
					last_avatar_state = avatar_state;
					last_avatar_frame = avatar_frame;
					friend_session.send_avatar_state(
							avatar_state.tx, avatar_state.ty, avatar_state.tz, avatar_frame);
				}
				const uint32 now = SDL_GetTicks();
				if (last_npc_snapshot == 0 || now - last_npc_snapshot >= 100) {
					last_npc_snapshot = now;
					Actor_vector nearby;
					gwin->get_nearby_npcs(nearby);
					for (Actor* npc : nearby) {
						if (!npc || npc == gwin->get_main_actor() || npc->get_npc_num() == host_assigned_actor) {
							continue;
						}
						const Tile_coord state = npc->get_tile();
						if (npc->get_npc_num() > 0) {
							friend_session.send_npc_state(
									static_cast<uint32>(npc->get_npc_num()), state.tx, state.ty, state.tz,
									npc->get_framenum());
						} else {
							const int creature = Friend_creature_index(npc);
							if (creature >= 0) {
								friend_session.send_creature_state(
										static_cast<uint32>(creature), state.tx, state.ty, state.tz,
										npc->get_framenum());
							}
						}
					}
				}
			}
		} else if (!arg_friend_join.empty()) {
			if (friend_session.take_peer_joined()) {
				Friend_mode_notice("Joined " + friend_session.remote_player_name_value() + "'s world");
			}
			std::vector<uint32> host_party;
			if (friend_session.take_party_members(host_party)) {
				Friend_apply_party_members(host_party);
			}
			Friend_mode_session::Object_frame_event frame_event;
			while (friend_session.take_object_frame(frame_event)) {
				const Tile_coord tile(frame_event.tx, frame_event.ty, frame_event.tz);
				Game_object*     obj = Friend_find_door(tile, frame_event.shape, frame_event.old_frame);
				if (obj) {
					Friend_apply_object_change(obj, frame_event.new_shape, frame_event.new_frame);
				} else {
					cerr << "Friend Mode: no object for frame change at " << tile.tx << ',' << tile.ty << endl;
				}
			}
			// Without a companion, a guest only watches; it must not move
			// its local Avatar, which belongs to the host.
			const bool spectating = guest_assigned_actor < 0 || !gwin->has_network_friend_control();
			if (spectating != gwin->is_network_spectating()) {
				gwin->set_network_spectating(spectating);
			}
			if (friend_session.take_connection_lost()) {
				gwin->set_network_dialogue_paused(false);
				gwin->release_friend_mode_control();
				guest_assigned_actor = -1;
				// Exult cannot return to the title menu mid-game, so close.
				Friend_mode_notice("Host disconnected. Exult will close.");
				guest_close_at = SDL_GetTicks() + 4000;
			}
			if (guest_close_at != 0 && SDL_GetTicks() >= guest_close_at) {
				quitting_time = QUIT_TIME_YES;
			}
			if (friend_session.roster_ready() && !friend_selection_shown) {
				friend_selection_shown = true;
				const int selected = Select_friend_actor(friend_session.party_roster());
				if (selected >= 0) {
					friend_session.request_actor(static_cast<uint32>(selected));
				} else {
					cout << "Friend Mode: no companion selected; spectating" << endl;
				}
			}
			uint32 assigned_actor = 0;
			if (friend_session.take_actor_assignment(assigned_actor)) {
				if (!gwin->take_friend_mode_control(static_cast<int>(assigned_actor), true)) {
					cerr << "Friend Mode: assigned companion is unavailable in the local world" << endl;
					Friend_mode_notice("Could not take control; spectating");
				} else {
					guest_assigned_actor = static_cast<int>(assigned_actor);
					last_guest_request = gwin->get_controlled_actor()->get_tile();
				}
			}
			int tx = 0;
			int ty = 0;
			int tz = 0;
			int frame = 0;
			if (guest_assigned_actor >= 0 && friend_session.take_actor_state(tx, ty, tz, frame)) {
				Actor* actor = gwin->get_npc(guest_assigned_actor);
				const Tile_coord state(tx, ty, tz);
				if (actor && actor->get_tile() != state) {
					const bool beckoned = actor->get_tile().distance_2d(state) > Game_window::friend_leash_warn_tiles;
					actor->move(state.tx, state.ty, state.tz);
					gwin->set_camera_actor(actor);
					if (beckoned) {
						gwin->get_effects()->add_chat_text("You are beckoned by the Avatar", actor);
					}
				}
				if (actor && actor->get_framenum() != frame) {
					actor->change_frame(frame);
				}
				last_guest_request = state;
			}
			if (friend_session.take_avatar_state(tx, ty, tz, frame)) {
				Main_actor* avatar = gwin->get_main_actor();
				const Tile_coord state(tx, ty, tz);
				if (avatar && avatar->get_tile() != state) {
					// This is a remote visual update, not local camera movement.
					// Bypass Main_actor::move so Iolo's roof state remains local.
					avatar->Actor::move(state.tx, state.ty, state.tz);
				}
				if (avatar && avatar->get_framenum() != frame) {
					avatar->change_frame(frame);
				}
			}
			uint32 npc_num = 0;
			while (friend_session.take_npc_state(npc_num, tx, ty, tz, frame)) {
				Actor* npc = gwin->get_npc(static_cast<int>(npc_num));
				const Tile_coord state(tx, ty, tz);
				if (npc && guest_frozen_npcs.insert(static_cast<int>(npc_num)).second) {
					npc->set_schedule_type(Schedule::wait);
					npc->stop();
				}
				if (npc && npc->get_tile() != state) {
					npc->move(state.tx, state.ty, state.tz);
				}
				if (npc && npc->get_framenum() != frame) {
					npc->change_frame(frame);
				}
			}
			uint32 creature_num = 0;
			while (friend_session.take_creature_state(creature_num, tx, ty, tz, frame)) {
				Monster_actor* creature = Friend_creature_by_index(creature_num);
				const Tile_coord state(tx, ty, tz);
				if (creature && guest_frozen_creatures.insert(creature_num).second) {
					creature->set_schedule_type(Schedule::wait);
					creature->stop();
				}
				if (creature && creature->get_tile() != state) {
					creature->move(state.tx, state.ty, state.tz);
				}
				if (creature && creature->get_framenum() != frame) {
					creature->change_frame(frame);
				}
			}
			uint32 speech_source = 0;
			std::string speech_text;
			bool speech_creature = false;
			while (friend_session.take_speech(speech_source, speech_creature, speech_text)) {
				Actor* speaker = speech_creature
						? static_cast<Actor*>(Friend_creature_by_index(speech_source))
						: gwin->get_npc(static_cast<int>(speech_source));
				if (speaker) {
					speaker->say(speech_text.c_str());
				}
			}
			Friend_mode_session::Dialogue_event dialogue_event;
			Conversation* conversation = gwin->get_usecode()->get_conversation();
			while (friend_session.take_dialogue_event(dialogue_event)) {
				switch (dialogue_event.type) {
				case Friend_mode_session::Dialogue_event_type::begin:
					gwin->set_network_dialogue_paused(true);
					conversation->init_faces();
					break;
				case Friend_mode_session::Dialogue_event_type::end:
					conversation->init_faces();
					gwin->set_network_dialogue_paused(false);
					break;
				case Friend_mode_session::Dialogue_event_type::face:
					conversation->show_face(dialogue_event.a, dialogue_event.b, dialogue_event.c);
					break;
				case Friend_mode_session::Dialogue_event_type::face_frame:
					conversation->change_face_frame(dialogue_event.a, dialogue_event.b);
					break;
				case Friend_mode_session::Dialogue_event_type::remove_face:
					conversation->remove_face(dialogue_event.a);
					break;
				case Friend_mode_session::Dialogue_event_type::message:
					conversation->show_npc_message(dialogue_event.text.c_str());
					break;
				case Friend_mode_session::Dialogue_event_type::choices:
					conversation->show_remote_choices(dialogue_event.choices);
					break;
				case Friend_mode_session::Dialogue_event_type::clear_choices:
					conversation->clear_avatar_choices();
					break;
				}
			}
			if (guest_assigned_actor >= 0 && gwin->has_network_friend_control()) {
				Actor* actor = gwin->get_npc(guest_assigned_actor);
				Friend_mode_update_leash_zone(actor);
				const uint32 now = SDL_GetTicks();
				if (gwin->take_friend_leash_blocked() && actor && now - guest_leash_notice_at > 3000) {
					guest_leash_notice_at = now;
					gwin->get_effects()->add_chat_text("You must return to the Avatar", actor);
				}
			}
			if (guest_assigned_actor >= 0 && !gwin->is_network_dialogue_paused()) {
				Actor* actor = gwin->get_npc(guest_assigned_actor);
				if (actor && actor->get_tile() != last_guest_request) {
					last_guest_request = actor->get_tile();
					cout << "Friend Mode: guest requested move to " << last_guest_request.tx << ','
						 << last_guest_request.ty << ',' << last_guest_request.tz << endl;
					friend_session.send_move_request(
							last_guest_request.tx, last_guest_request.ty, last_guest_request.tz,
							actor->get_framenum());
				}
			}
		}
#ifdef USE_EXULTSTUDIO
		Server_delay();    // Handle requests.
#else
		Delay();    // Wait a fraction of a second.
#endif
		// Mouse scale factor
		// int scale = gwin->get_fastmouse() ? 1 :
		//              gwin->get_win()->get_scale();

		// NOTE: the cursor layer is deliberately NOT hidden here. Hiding it for
		// the duration of the frame was a relic of the pre-layer renderer, where
		// the cursor was blitted into the frame buffer and had to be lifted off
		// before painting.
		Mouse::mouse_update = false;

		// Get current time.
		const uint32 ticks = SDL_GetTicks();
#if defined(_WIN32) && defined(USE_EXULTSTUDIO)
		if (ticks - Game::get_ticks() < 10) {
			// Reducing processor usage with a slight delay.
			SDL_Delay(10 - (ticks - Game::get_ticks()));
			continue;
		}
#endif
		Game::set_ticks(ticks);
#ifdef DEBUG
		if (last_fps == 0 || ticks >= last_fps + 10000) {
			const double fps = (gwin->blits * 1000.0) / (ticks - last_fps);
			cerr << "***#ticks = " << ticks - last_fps << ", blits = " << gwin->blits << ", ";
			cerr << "FPS:  " << fps << endl;
			last_fps    = ticks;
			gwin->blits = 0;
		}
#endif

		SDL_Event event;
		while (!quitting_time && SDL_PollEvent(&event)) {
			Handle_event(event);
		}

		// Animate unless dormant.
		if (gwin->have_focus() && !dragging) {
			gwin->get_tqueue()->activate(ticks);
		}
		gwin->enforce_friend_mode_control_safety();

		// Moved this out of the animation loop, since we want movement to be
		// more responsive. Also, if the step delta is only 1 tile,
		// always check every loop
		if ((!gwin->is_moving() || gwin->get_step_tile_delta() == 1) && gwin->main_actor_can_act_charmed()) {
			int       x  = Mouse::mouse()->get_mousex();
			int       y  = Mouse::mouse()->get_mousey();
			const int ms = SDL_GetMouseState(nullptr, nullptr);
			if ((SDL_BUTTON_RMASK & ms) && !right_on_gump) {
				gwin->start_actor(x, y, Mouse::mouse()->avatar_speed);
			} else if (ticks > last_rest) {
				const int resttime = ticks - last_rest;
				gwin->get_main_actor()->resting(resttime);

				Party_manager* party_man = gwin->get_party_man();
				const int      cnt       = party_man->get_count();
				for (int i = 0; i < cnt; i++) {
					const int party_member = party_man->get_member(i);
					Actor*    person       = gwin->get_npc(party_member);
					if (!person) {
						continue;
					}
					person->resting(resttime);
				}
				last_rest = ticks;
			}
		}

		// handle delayed showing of clicked items (wait for possible dblclick)
		if (show_items_clicked && ticks > show_items_time) {
			gwin->show_items(show_items_x, show_items_y, false);
			show_items_clicked = false;
		}

		if (joy_aim_x != 0 || joy_aim_y != 0) {
			// Calculate the player speed
			const int speed = 200 * gwin->get_std_delay() / static_cast<int>(joy_speed_factor);

			// [re]start moving
			gwin->start_actor(joy_aim_x, joy_aim_y, speed);
		}

		// Lerping stuff...
		const int lerp    = gwin->is_lerping_enabled();
		bool      didlerp = false;

		// Update gumps every frame (drives auto-repeat for held
		// slider/spinner arrows).  Must run even when nothing is dirty.
		if (!gwin->main_actor_dont_move()) {
			gwin->get_gump_man()->update_gumps();
		}

		if (lerp) {
			// Always repaint,
			Actor*        act   = gwin->get_camera_actor();
			Barge_object* barge = gwin->get_moving_barge();

			const bool slow_walk = !barge && act && act->get_frame_time() * 2 >= 3 * gwin->get_std_delay();

			// Force a reset if position changed
			if (last_x != gwin->get_scrolltx() || last_y != gwin->get_scrollty()) {
				// printf ("%i: %i -> %i, %i -> %i\n", ticks, last_x,
				// gwin->get_scrolltx(), last_y, gwin->get_scrollty());
				gwin->lerp_reset();
				last_repaint = ticks;
				// A real step happened, so any pending stop-glide shortening no
				// longer applies; allow it to fire again once we next stop.
				lerp_stop_anchored = false;
				const int ft       = (barge && barge->contains(gwin->get_main_actor())) ? barge->get_frame_time()
																						: (act ? act->get_frame_time() : 0);
				if (ft > 0) {
					lerp_mswait = (ft * lerp) / 100;
				}
			}
			last_x = gwin->get_scrolltx();
			last_y = gwin->get_scrollty();

			int mswait = lerp_mswait;
			if (mswait <= 0) {
				mswait = (gwin->get_std_delay() * lerp) / 100;
			}

			// Shorten the final catch-up glide once the camera actor has
			// stopped.
			if (!lerp_stop_anchored && lerp_mswait > 0 && !gwin->is_moving()) {
				const int stopwait = (gwin->get_std_delay() * lerp) / 100;
				if (stopwait > 0 && stopwait < mswait) {
					int factor = ((ticks - last_repaint) * 0x10000) / mswait;
					if (factor < 0) {
						factor = 0;
					}
					if (factor > 0x10000) {
						factor = 0x10000;
					}
					// Pick last_repaint so the factor is unchanged right now but
					// advances at the faster (stopwait) rate from here on.
					last_repaint = ticks - (factor * stopwait) / 0x10000;
					lerp_mswait  = stopwait;
					mswait       = stopwait;
				}
				lerp_stop_anchored = true;
			}

			// Is lerping (smooth scrolling) enabled
			if (mswait && ticks < (last_repaint + mswait * 2)) {
				// Slow walk doesn't use smooth scrolling.
				const int factor = slow_walk ? 0x10000 : ((ticks - last_repaint) * 0x10000) / mswait;
				gwin->paint_lerped(factor);
				didlerp = true;
			}
		}
		if (!lerp || !didlerp) {       // No lerping
			if (gwin->is_dirty()) {    // Note the ending else in the above #if!
				gwin->paint_dirty();
			}
			// Reset it for lerping.
			last_x = last_y = -1;
		}
		// update mousecursor appearance if needed
		if (last_speed_cursor + 100 < SDL_GetTicks() && !dragging) {
			last_speed_cursor = SDL_GetTicks();
			Mouse::mouse()->set_speed_cursor();
			Mouse::mouse_update = true;
		}
		Mouse::mouse()->show();    // Re-display mouse.
		gwin->rotatecolours();
		Friend_mode_paint_leash_arrow();
		Friend_mode_paint_chat_input();

		if (!gwin->show() &&          // Blit to screen if necessary.
			Mouse::mouse_update) {    // If not, did mouse change?
			Mouse::mouse()->blit_dirty();
		}
	}
}

bool Translate_keyboard(const SDL_Event& event, SDL_Keycode& chr, SDL_Keycode& unicode, bool numpad) {
	unicode = 0;

	if (event.type == SDL_EVENT_TEXT_INPUT) {
		// Convert UTF-8 to UCS-4
		chr = SDLK_UNKNOWN;
		if ((event.text.text[0] & 0x80) == 0x00) {
			unicode = event.text.text[0];
		} else if ((event.text.text[0] & 0xE0) == 0xC0 && (event.text.text[1] & 0xC0) == 0x80) {
			unicode = ((event.text.text[0] & 0x1F) << 6) | ((event.text.text[1] & 0x3F));
		} else if (
				(event.text.text[0] & 0xF0) == 0xE0 && (event.text.text[1] & 0xC0) == 0x80 && (event.text.text[2] & 0xC0) == 0x80) {
			unicode = ((event.text.text[0] & 0x0F) << 12) | ((event.text.text[1] & 0x3F) << 6) | ((event.text.text[2] & 0x3F));
		} else if (
				(event.text.text[0] & 0xF8) == 0xF0 && (event.text.text[1] & 0xC0) == 0x80 && (event.text.text[1] & 0xC0) == 0x80
				&& (event.text.text[1] & 0xC0) == 0x80) {
			unicode = ((event.text.text[0] & 0x07) << 18) | ((event.text.text[1] & 0x3F) << 12) | ((event.text.text[2] & 0x3F) << 6)
					  | ((event.text.text[3] & 0x3F));
		} else {    // Invalid UTF-8
			unicode = 0;
		}
	} else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
		// Handle Shift all variants, Simulate NumLock set
		chr     = event.key.key;
		unicode = SDL_GetKeyFromScancode(event.key.scancode, (event.key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS)), false);
		unicode = (unicode & SDLK_SCANCODE_MASK ? 0 : unicode);
	} else {
		return false;
	}
	if (numpad) {
		// Extract Numeric Keypad UCS-4
		const bool numlock_active = (event.key.mod & SDL_KMOD_NUM) != 0;
		switch (event.key.scancode) {
		case SDL_SCANCODE_KP_ENTER:
			chr     = SDLK_RETURN;
			unicode = SDLK_RETURN;
			break;
		case SDL_SCANCODE_KP_PLUS:
			chr     = SDLK_PLUS;
			unicode = '+';
			break;
		case SDL_SCANCODE_KP_MINUS:
			chr     = SDLK_MINUS;
			unicode = '-';
			break;
		case SDL_SCANCODE_KP_MULTIPLY:
			chr     = SDLK_ASTERISK;
			unicode = '*';
			break;
		case SDL_SCANCODE_KP_DIVIDE:
			chr     = SDLK_SLASH;
			unicode = '/';
			break;
		case SDL_SCANCODE_KP_PERIOD:
			if (numlock_active) {
				chr     = SDLK_PERIOD;
				unicode = '.';
			} else {
				chr     = SDLK_DELETE;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_0:
			if (numlock_active) {
				chr     = SDLK_0;
				unicode = '0';
			} else {
				chr     = SDLK_INSERT;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_1:
			if (numlock_active) {
				chr     = SDLK_1;
				unicode = '1';
			} else {
				chr     = SDLK_END;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_2:
			if (numlock_active) {
				chr     = SDLK_2;
				unicode = '2';
			} else {
				chr     = SDLK_DOWN;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_3:
			if (numlock_active) {
				chr     = SDLK_3;
				unicode = '3';
			} else {
				chr     = SDLK_PAGEDOWN;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_4:
			if (numlock_active) {
				chr     = SDLK_4;
				unicode = '4';
			} else {
				chr     = SDLK_LEFT;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_5:
			if (numlock_active) {
				chr     = SDLK_5;
				unicode = '5';
			} else {
				chr     = SDLK_CLEAR;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_6:
			if (numlock_active) {
				chr     = SDLK_6;
				unicode = '6';
			} else {
				chr     = SDLK_RIGHT;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_7:
			if (numlock_active) {
				chr     = SDLK_7;
				unicode = '7';
			} else {
				chr     = SDLK_HOME;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_8:
			if (numlock_active) {
				chr     = SDLK_8;
				unicode = '8';
			} else {
				chr     = SDLK_UP;
				unicode = 0;
			}
			break;
		case SDL_SCANCODE_KP_9:
			if (numlock_active) {
				chr     = SDLK_9;
				unicode = '9';
			} else {
				chr     = SDLK_PAGEUP;
				unicode = 0;
			}
			break;
		default:
			break;
		}
	}
	return (event.key.key != SDLK_LSHIFT && event.key.key != SDLK_RSHIFT && event.key.key != SDLK_CAPSLOCK
			&& event.key.key != SDLK_RCTRL && event.key.key != SDLK_LCTRL && event.key.key != SDLK_RALT
			&& event.key.key != SDLK_LALT && event.key.key != SDLK_LGUI && event.key.key != SDLK_RGUI && event.key.key != SDLK_MODE
			&& event.key.key != SDLK_NUMLOCKCLEAR);
}

/*
 *  Handle an event.  This should work for all platforms, and should only
 *  be called in 'normal' and 'gump' modes.
 */

static void Handle_event(SDL_Event& event) {
	if (arg_friend_host && !friend_chat_active && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F9) {
		Friend_mode_spawn_test_kit();
		return;
	}
	if (friend_session.connected()) {
		if (friend_chat_active) {
			if (event.type == SDL_EVENT_TEXT_INPUT) {
				for (const char* ch = event.text.text; *ch && friend_chat_text.size() < 80; ++ch) {
					if (static_cast<unsigned char>(*ch) >= 32 && static_cast<unsigned char>(*ch) < 127) {
						friend_chat_text.push_back(*ch);
					}
				}
				gwin->set_all_dirty();
				return;
			}
			if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_ESCAPE) {
					friend_chat_active = false;
					friend_chat_text.clear();
					SDL_StopTextInput(gwin->get_win()->get_screen_window());
					gwin->set_all_dirty();
				} else if (event.key.key == SDLK_BACKSPACE) {
					if (!friend_chat_text.empty()) {
						friend_chat_text.pop_back();
						gwin->set_all_dirty();
					}
				} else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
					if (!friend_chat_text.empty()) {
						Actor* speaker = Friend_mode_local_speaker();
						Friend_mode_show_chat(speaker, friend_player_name, friend_chat_text);
						friend_session.send_chat(
								speaker ? static_cast<uint32>(speaker->get_npc_num())
										: Friend_mode_session::chat_spectator,
								friend_chat_text);
						friend_session.poll();
					}
					friend_chat_active = false;
					friend_chat_text.clear();
					SDL_StopTextInput(gwin->get_win()->get_screen_window());
					gwin->set_all_dirty();
				}
				return;
			}
			if (event.type == SDL_EVENT_KEY_UP) {
				return;
			}
		} else if (event.type == SDL_EVENT_KEY_DOWN
				   && (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)) {
			friend_chat_active = true;
			friend_chat_text.clear();
			gwin->stop_actor();
			SDL_StartTextInput(gwin->get_win()->get_screen_window());
			gwin->set_all_dirty();
			return;
		}
	}
	// Mouse scale factor
	const bool dont_move_mode = gwin->main_actor_dont_move();
	const bool avatar_can_act = gwin->main_actor_can_act();

	// We want this
	Gump_manager* gump_man = gwin->get_gump_man();
	Gump*         gump     = nullptr;

	SDL_Renderer* renderer = SDL_GetRenderer(gwin->get_win()->get_screen_window());
	// For detecting double-clicks.
	static uint32 last_b1_click     = 0;
	static uint32 last_b3_click     = 0;
	static uint32 last_b1down_click = 0;
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		// Ignore axis changes on anything but a specific thumb-stick
		// on the game-controller.
		if (event.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTX && event.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTY) {
			break;
		}

		SDL_Gamepad* input_device = SDL_GetGamepadFromID(event.gaxis.which);
		if (input_device && !dont_move_mode && avatar_can_act && gwin->main_actor_can_act_charmed()) {
			auto get_normalized_axis = [input_device](SDL_GamepadAxis axis) {
				return SDL_GetGamepadAxis(input_device, axis) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
			};
			// Collect both of the controller thumb-stick's axis values.
			// The input-event only carries one axis, and each thumb-stick
			// has two axes.  Both axes' values are needed in order to
			// call start_actor.
			float axis_x = get_normalized_axis(SDL_GAMEPAD_AXIS_LEFTX);
			float axis_y = get_normalized_axis(SDL_GAMEPAD_AXIS_LEFTY);

			// Dead-zone is applied to each axis, X and Y, on the game's 2d
			// plane. All axis readings below this are ignored
			constexpr const float axis_dead_zone = 0.25f;
			// Medium-speed threashold; below this, move slow
			constexpr const float axis_length_move_medium = 0.60f;
			// Fast-speed threashold; above this, move fast
			constexpr const float axis_length_move_fast = 0.90f;

			// Many analog game-controllers report non-zero axis values,
			// even when the controller isn't moving.  These non-zero
			// values can change over time, as the thumb-stick is moved
			// by the player.
			//
			// In order to prevent idle controller sticks from leading
			// to unwanted movements, axis-values that are small will
			// be ignored.  This is sometimes referred to as a
			// "dead zone".
			if (axis_dead_zone >= std::fabs(axis_x)) {
				axis_x = 0;
			}
			if (axis_dead_zone >= std::fabs(axis_y)) {
				axis_y = 0;
			}

			// Pick a player's speed-factor, depending on how much the
			// input-stick is being pushed in a direction.
			const float joy_axis_length = std::sqrt((axis_x * axis_x) + (axis_y * axis_y));
			joy_speed_factor            = Mouse::fast_speed_factor;
			if (joy_axis_length < axis_length_move_medium) {
				joy_speed_factor = Mouse::slow_speed_factor;
			} else if (joy_axis_length < axis_length_move_fast) {
				joy_speed_factor = Mouse::medium_speed_factor;
			}

			// Depending on axis values, we're either going to start moving,
			// restarting moving (which looks the same as starting, to us),
			// or stopping.
			if (axis_x == 0 && axis_y == 0) {
				// Both axes are zero, so we'll stop.
				gwin->stop_actor();
				joy_aim_x = 0;
				joy_aim_y = 0;
			} else {
				// At least one axis is non-zero, so we'll [re]start moving.

				// Declare a position to aim for, in window coordinates,
				// relative to the center of the window (where the player is).
				const float aim_distance = 50.f;
				const int   aim_dx       = static_cast<int>(std::lround(aim_distance * axis_x));
				const int   aim_dy       = static_cast<int>(std::lround(aim_distance * axis_y));
				joy_aim_x                = (gwin->get_width() / 2) + aim_dx;
				joy_aim_y                = (gwin->get_height() / 2) + aim_dy;
			}
		}
		break;
	}
	case SDL_EVENT_FINGER_DOWN: {
		if ((!Mouse::use_touch_input) && (event.tfinger.touchID != EXSDL_MOUSE_TOUCHID)) {
			Mouse::use_touch_input = true;
			gwin->set_painted();
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN: {
		SDL_SetWindowMouseGrab(gwin->get_win()->get_screen_window(), true);
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		if (dont_move_mode) {
			break;
		}
		last_b1down_click = SDL_GetTicks();
		if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
			break;
		}
		int x;
		int y;
		int gx;
		int gy;
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
		if (event.button.button == 1) {
			Gump_button* button;
			bool         closing_parent_gump = false;
			if ((gump = gump_man->find_gump(x, y, false)) != nullptr) {
				gump_man->map_game_to_gump(gump, x, y, gx, gy);
				button              = gump->on_button(gx, gy);
				closing_parent_gump = button != nullptr && button->is_checkmark();
			}
			// Allow dragging only either if the avatar can act or if map edit
			// or hackmove is on.
			if (avatar_can_act || cheat.in_hack_mover() || closing_parent_gump) {    // also allow closing
																					 // parent gump when
																					 // clicking on checkmark
#ifdef USE_EXULTSTUDIO
				if (cheat.in_map_editor()) {
					// Paint if shift-click.
					if (cheat.get_edit_shape() >= 0 &&
						// But always if painting.
						(cheat.get_edit_mode() == Cheat::paint || (SDL_GetModState() & SDL_KMOD_SHIFT))) {
						Paint_with_shape(event, false);
						break;
					} else if (cheat.get_edit_chunknum() >= 0 && cheat.get_edit_mode() == Cheat::paint_chunks) {
						Paint_with_chunk(event, false);
						break;
					} else if (cheat.get_edit_mode() == Cheat::select_chunks) {
						Select_chunks(event, false, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
						break;
					} else if (cheat.get_edit_mode() == Cheat::combo_pick || cheat.get_edit_mode() == Cheat::edit_pick) {
						Select_for_combo(event, false, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
						break;
					}
					// Don't drag if not in 'move' mode.
					else if (cheat.get_edit_mode() != Cheat::move) {
						break;
					}
				}
#endif
				dragging = gwin->start_dragging(x, y);
				// Mouse::mouse()->set_shape(Mouse::hand);
				dragged = false;
			}
			left_down_x = x;
			left_down_y = y;
		}

		// Middle click, use targetting crosshairs.
		if (gwin->get_mouse3rd()) {
			if (event.button.button == 2) {
				ActionTarget(nullptr);
			}
		}

		// Right click. Only walk if avatar can act.
		if (event.button.button == 3) {
			if (!dragging &&    // Causes crash if dragging.
				gump_man->can_right_click_close() && gump_man->gump_mode() && gump_man->find_gump(x, y, false)) {
				gump          = nullptr;
				right_on_gump = true;
			} else if (avatar_can_act && gwin->main_actor_can_act_charmed()) {
				// Try removing old queue entry.
				gwin->get_tqueue()->remove(gwin->get_main_actor());
				gwin->start_actor(x, y, Mouse::mouse()->avatar_speed);
			}
		}
		break;
	}
	// two-finger scrolling of view port with SDL2.
	case SDL_EVENT_FINGER_MOTION: {
		if (!cheat() || !gwin->can_scroll_with_mouse()) {
			break;
		}
		static int   numFingers = 0;
		SDL_Finger** fingers    = SDL_GetTouchFingers(event.tfinger.touchID, &numFingers);
		if (fingers) {
			SDL_free(fingers);
		}
		if (numFingers > 1) {
			if (event.tfinger.dy < 0) {
				ActionScrollUp(nullptr);
			} else if (event.tfinger.dy > 0) {
				ActionScrollDown(nullptr);
			}
			if (event.tfinger.dx > 0) {
				ActionScrollRight(nullptr);
			} else if (event.tfinger.dx < 0) {
				ActionScrollLeft(nullptr);
			}
		}
		break;
	}
	// Mousewheel scrolling of view port with SDL2.
	case SDL_EVENT_MOUSE_WHEEL: {
		// Try forwarding to open gumps first (dynamic sliders)
		if (gump_man->handle_mouse_wheel(
					event.wheel.y, event.wheel.x, Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey())) {
			break;    // consumed by gump
		}
		if (!cheat() || !gwin->can_scroll_with_mouse()) {
			break;
		}
		const SDL_Keymod mod = SDL_GetModState();
		if (event.wheel.y > 0) {
			if (mod & SDL_KMOD_ALT) {
				ActionScrollLeft(nullptr);
			} else {
				ActionScrollUp(nullptr);
			}
		} else if (event.wheel.y < 0) {
			if (mod & SDL_KMOD_ALT) {
				ActionScrollRight(nullptr);
			} else {
				ActionScrollDown(nullptr);
			}
		}
		if (event.wheel.x > 0) {
			ActionScrollRight(nullptr);
		} else if (event.wheel.x < 0) {
			ActionScrollLeft(nullptr);
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		SDL_SetWindowMouseGrab(gwin->get_win()->get_screen_window(), false);
		if (dont_move_mode) {
			break;
		}
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		int x;
		int y;
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);

		if (event.button.button == 3) {
			const uint32 curtime = SDL_GetTicks();
			// If showing gumps, ignore all double-right-click results
			if (gump_man->gump_mode()) {
				if (right_on_gump && (gump = gump_man->find_gump(x, y, false))) {
					const TileRect dirty = gump->get_dirty();
					gwin->add_dirty(dirty);
					gump_man->close_gump(gump);
					gump          = nullptr;
					right_on_gump = false;
				}
			} else if (avatar_can_act) {
				// Last right click not within .5 secs (not a doubleclick or
				// rapid right clicking)?
				if (gwin->get_allow_right_pathfind() == 1 && curtime - last_b3_click > 500 && gwin->main_actor_can_act_charmed()) {
					gwin->start_actor_along_path(x, y, Mouse::mouse()->avatar_speed);
				}

				// Last right click within .5 secs (doubleclick)?
				else if (
						gwin->get_allow_right_pathfind() == 2 && curtime - last_b3_click < 500
						&& gwin->main_actor_can_act_charmed()) {
					gwin->start_actor_along_path(x, y, Mouse::mouse()->avatar_speed);
				}

				else {
					gwin->stop_actor();
					if (Combat::is_paused() && gwin->in_combat()) {
						gwin->paused_combat_select(x, y);
					}
				}
			}
			last_b3_click = curtime;
		} else if (event.button.button == 1) {
			const uint32 curtime       = SDL_GetTicks();
			bool         click_handled = false;
			if (dragging) {
				click_handled = gwin->drop_dragged(x, y, dragged);
				Mouse::mouse()->set_speed_cursor();
			}
			if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
				break;
			}
			// Last click within .5 secs?
			if (curtime - last_b1_click < 500 && left_down_x - 1 <= x && x <= left_down_x + 1 && left_down_y - 1 <= y
				&& y <= left_down_y + 1) {
				dragging = dragged = false;
				// This function handles the trouble of deciding what to
				// do when the avatar cannot act.
				gwin->double_clicked(x, y);
				Mouse::mouse()->set_speed_cursor();
				show_items_clicked = false;
				break;
			}
			if (!dragging || !dragged) {
				last_b1_click = curtime;
			}

			if (!click_handled && (curtime - last_b1down_click > 500)) {
				if (touchui != nullptr && Combat::is_paused() && gwin->in_combat()) {
					gwin->paused_combat_select(x, y);
					break;
				}
				if (gwin->get_touch_pathfind() && avatar_can_act && gwin->main_actor_can_act_charmed() && !dragging
					&& !gump_man->find_gump(x, y, false)) {
					gwin->start_actor_along_path(x, y, Mouse::mouse()->avatar_speed);
					dragging = dragged = false;
					break;
				}
			}

			if (!click_handled && avatar_can_act && left_down_x - 1 <= x && x <= left_down_x + 1 && left_down_y - 1 <= y
				&& y <= left_down_y + 1) {
				show_items_x = x;
				show_items_y = y;
				// Identify item(s) clicked on.
				if (cheat.in_map_editor()) {
					gwin->show_items(x, y, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
				} else {
					show_items_time    = curtime + 500;
					show_items_clicked = true;
				}
			}
			dragging = dragged = false;
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION: {
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		int mx;
		int my;
		if (Mouse::use_touch_input && event.motion.which != EXULT_TOUCH_MOUSEID) {
			Mouse::use_touch_input = false;
		}
		gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

		Mouse::mouse()->move(mx, my);
		if (!dragging) {
			last_speed_cursor = SDL_GetTicks();
			Mouse::mouse()->set_speed_cursor();
		}
		Mouse::mouse_update = true;    // Need to blit mouse.
		if (right_on_gump && !(gump_man->can_right_click_close() && gump_man->gump_mode() && gump_man->find_gump(mx, my, false))) {
			right_on_gump = false;
		}

		// Dragging with left button?
		if (event.motion.state & SDL_BUTTON_LMASK) {
#ifdef USE_EXULTSTUDIO    // Painting?
			if (cheat.in_map_editor()) {
				if (cheat.get_edit_shape() >= 0
					&& (cheat.get_edit_mode() == Cheat::paint || (SDL_GetModState() & SDL_KMOD_SHIFT))) {
					Paint_with_shape(event, true);
					break;
				} else if (cheat.get_edit_chunknum() >= 0 && cheat.get_edit_mode() == Cheat::paint_chunks) {
					Paint_with_chunk(event, true);
					break;
				} else if (cheat.get_edit_mode() == Cheat::select_chunks) {
					Select_chunks(event, true, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
					break;
				} else if (cheat.get_edit_mode() == Cheat::combo_pick || cheat.get_edit_mode() == Cheat::edit_pick) {
					Select_for_combo(event, true, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
					break;
				}
			}
#endif
			dragged = gwin->drag(mx, my);
		}
		// Dragging with right?
		else if ((event.motion.state & SDL_BUTTON_RMASK) && !right_on_gump) {
			if (avatar_can_act && gwin->main_actor_can_act_charmed()) {
				gwin->start_actor(mx, my, Mouse::mouse()->avatar_speed);
			}
		}
#ifdef USE_EXULTSTUDIO    // Painting?
		else if (
				cheat.in_map_editor() && cheat.get_edit_shape() >= 0
				&& (cheat.get_edit_mode() == Cheat::paint || (SDL_GetModState() & SDL_KMOD_SHIFT))) {
			static int prevx = -1;
			static int prevy = -1;
			Move_dragged_shape(cheat.get_edit_shape(), cheat.get_edit_frame(), event.motion.x, event.motion.y, prevx, prevy, false);
			prevx = event.motion.x;
			prevy = event.motion.y;
		}
#endif
		break;
	}
	case SDL_EVENT_WINDOW_MOUSE_ENTER: {
		int   x;
		int   y;
		float fx, fy;
		SDL_GetMouseState(&fx, &fy);
		x = int(fx);
		y = int(fy);
		gwin->get_win()->screen_to_game(x, y, gwin->get_fastmouse(), x, y);
		Mouse::mouse()->move(x, y);
		gwin->set_painted();
		break;
	}
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		if (friend_session.mode() == Friend_mode_session::Mode::off) {
			gwin->get_focus();
		}
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		// A listen server and local guest must continue simulating while the
		// other window has focus. This is also required for ordinary remote
		// play when the host briefly switches to another application.
		if (friend_session.mode() == Friend_mode_session::Mode::off) {
			gwin->lose_focus();
		}
		break;
	case SDL_EVENT_QUIT:
		gwin->get_gump_man()->okay_to_quit();
		break;
	case SDL_EVENT_KEY_DOWN:    // Keystroke.
	case SDL_EVENT_KEY_UP:
		if (!dragging &&    // ESC while dragging causes crashes.
			!gwin->get_gump_man()->handle_kbd_event(&event)) {
			keybinder->HandleEvent(event);
		}
		break;
	case SDL_EVENT_DROP_TEXT:
	case SDL_EVENT_DROP_FILE: {
#ifdef USE_EXULTSTUDIO
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		int   x;
		int   y;
		float fx = event.drop.x, fy = event.drop.y;
		x = int(fx);
		y = int(fy);
#	ifdef DEBUG
		cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_TEXT ? "TEXT" : "FILE")
			 << " Event, type = " << event.drop.type << ", file (" << strlen(event.drop.data) << ") = '" << event.drop.data
			 << "', at x = " << x << ", y = " << y << endl;
#	endif
		const unsigned char* data = reinterpret_cast<const unsigned char*>(event.drop.data);
		if (Is_u7_shapeid(data) == true) {
			// Get shape info.
			int file, shape, frame;
			Get_u7_shapeid(data, file, shape, frame);
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_TEXT ? "TEXT" : "FILE")
				 << " Event, Shape: file = " << data << ", shape = " << shape << ", frame = " << frame << ", at x = " << x
				 << ", y = " << y << endl;
			if (shape >= 0) {    // Dropping a shape?
				if (file == U7_SHAPE_SHAPES) {
					// For now, just allow "shapes.vga".
					Drop_dragged_shape(shape, frame, x, y);
				}
			}
			drag_shpnum = -1;
		} else if (Is_u7_chunkid(data) == true) {
			// A whole chunk.
			int chunknum;
			Get_u7_chunkid(data, chunknum);
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_TEXT ? "TEXT" : "FILE")
				 << " Event, Chunk: num = " << chunknum << ", at x = " << x << ", y = " << y << endl;
			if (chunknum >= 0) {    // A whole chunk.
				Drop_dragged_chunk(chunknum, x, y);
			}
			drag_cnknum = -1;
		} else if (Is_u7_npcid(data) == true) {
			int npcnum;
			Get_u7_npcid(data, npcnum);
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_TEXT ? "TEXT" : "FILE")
				 << " Event, Npc: num = " << npcnum << ", at x = " << x << ", y = " << y << endl;
			if (npcnum >= 0) {    // An NPC.
				Drop_dragged_npc(npcnum, x, y);
			}
			drag_npcnum = -1;
		} else if (Is_u7_comboid(data) == true) {
			int            combo_xtiles, combo_ytiles, combo_tiles_right, combo_tiles_below, combo_cnt;
			U7_combo_data* combo;
			Get_u7_comboid(data, combo_xtiles, combo_ytiles, combo_tiles_right, combo_tiles_below, combo_cnt, combo);
			std::unique_ptr<U7_combo_data[]> combo_owner(combo);
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_TEXT ? "TEXT" : "FILE")
				 << " Event, Combo: xtiles = " << combo_xtiles << ", ytiles = " << combo_ytiles
				 << ", tiles_right = " << combo_tiles_right << ", tiles_below = " << combo_tiles_below << ", count = " << combo_cnt
				 << ", at x = " << x << ", y = " << y << endl;
			if (combo_cnt >= 0 && combo) {
				Drop_dragged_combo(combo_cnt, combo, x, y);
			}
			drag_cbcnt = -1;
		}
#	ifdef DEBUG
		cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_TEXT ? "TEXT" : "FILE") << " Event complete" << endl;
#	endif
#endif
		break;
	}
	case SDL_EVENT_DROP_BEGIN: {
#ifdef USE_EXULTSTUDIO
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		int   x;
		int   y;
		float fx = event.drop.x, fy = event.drop.y;
		x = int(fx);
		y = int(fy);
#	ifdef DEBUG
		cout << "(EXULT) SDL_EVENT_DROP_BEGIN Event, type = " << event.drop.type << ", at x = " << x << ", y = " << y << endl;
#	endif
		drag_prevx = -1;
		drag_prevy = -1;
		if (drag_shpnum != -1) {
			int file = drag_shfile, shape = drag_shpnum, frame = drag_shfnum;
#	ifdef DEBUG
			cout << "(EXULT) SDL_EVENT_DROP_BEGIN Event, Shape: file = " << file << ", shape = " << shape << ", frame = " << frame
				 << ", at x = " << x << ", y = " << y << endl;
#	endif
			if (shape >= 0) {    // Moving a shape?
				if (file == U7_SHAPE_SHAPES) {
					// For now, just allow "shapes.vga".
					Move_dragged_shape(shape, frame, x, y, drag_prevx, drag_prevy, true);
				}
			}
		} else if (drag_cbcnt != -1) {
			int combo_xtiles = drag_cbxtiles, combo_ytiles = drag_cbytiles;
			int combo_tiles_right = drag_cbrtiles;
			int combo_tiles_below = drag_cbbtiles;
			int combo_cnt         = drag_cbcnt;
#	ifdef DEBUG
			cout << "(EXULT) SDL_EVENT_DROP_BEGIN Event, Combo: xtiles = " << combo_xtiles << ", ytiles = " << combo_ytiles
				 << ", tiles_right = " << combo_tiles_right << ", tiles_below = " << combo_tiles_below << ", count = " << combo_cnt
				 << ", at x = " << x << ", y = " << y << endl;
#	endif
			if (combo_cnt >= 0) {
				Move_dragged_combo(
						combo_xtiles, combo_ytiles, combo_tiles_right, combo_tiles_below, x, y, drag_prevx, drag_prevy, true);
			}
		} else if (drag_cnknum != -1) {
#	ifdef DEBUG
			cout << "(EXULT) SDL_EVENT_DROP_BEGIN Event, Chunk: num = " << drag_cnknum << ", at x = " << x << ", y = " << y << endl;
#	endif
			int cx, cy;
			gwin->get_win()->screen_to_game(x, y, false, cx, cy);
			cx = ((gwin->get_scrolltx() + (cx / c_tilesize)) / c_tiles_per_chunk) * c_tiles_per_chunk;
			cy = ((gwin->get_scrollty() + (cy / c_tilesize)) / c_tiles_per_chunk) * c_tiles_per_chunk;
			cx = ((cx - gwin->get_scrolltx()) * c_tilesize) + c_chunksize;
			cy = ((cy - gwin->get_scrollty()) * c_tilesize) + c_chunksize;
			gwin->get_win()->game_to_screen(cx, cy, false, x, y);
			Move_grid(x, y, drag_prevx, drag_prevy, false, c_tiles_per_chunk, c_tiles_per_chunk, 0, 0);
			gwin->show();
		}
		drag_prevx = x;
		drag_prevy = y;
#	ifdef DEBUG
		cout << "(EXULT) SDL_EVENT_DROP_BEGIN Event complete" << endl;
#	endif
#endif
		break;
	}
	case SDL_EVENT_DROP_COMPLETE:    // Drag pointer left the window, handle as
									 // -1, -1 DROP_POSITION
	case SDL_EVENT_DROP_POSITION: {
#ifdef USE_EXULTSTUDIO
		int x;
		int y;
		SDL_RaiseWindow(gwin->get_win()->get_screen_window());
		if (event.type == SDL_EVENT_DROP_POSITION) {
			// activate Exult for drag'n'drop
			SDL_ConvertEventToRenderCoordinates(renderer, &event);
			float fx = event.drop.x, fy = event.drop.y;
			x = int(fx);
			y = int(fy);
		} else {
			x = -1;
			y = -1;
		}
#	ifdef DEBUG
		cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_POSITION ? "POSITION" : "COMPLETE")
			 << " Event, type = " << event.drop.type << ", at x = " << x << ", y = " << y << endl;
#	endif
		if (drag_shpnum != -1) {
			int file = drag_shfile, shape = drag_shpnum, frame = drag_shfnum;
#	ifdef DEBUG
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_POSITION ? "POSITION" : "COMPLETE")
				 << " Event, Shape: file = " << file << ", shape = " << shape << ", frame = " << frame << ", at x = " << x
				 << ", y = " << y << endl;
#	endif
			if (shape >= 0) {    // Moving a shape?
				if (file == U7_SHAPE_SHAPES) {
					// For now, just allow "shapes.vga".
					Move_dragged_shape(shape, frame, x, y, drag_prevx, drag_prevy, true);
				}
			}
		} else if (drag_cbcnt != -1) {
			int combo_xtiles = drag_cbxtiles, combo_ytiles = drag_cbytiles;
			int combo_tiles_right = drag_cbrtiles, combo_tiles_below = drag_cbbtiles;
			int combo_cnt = drag_cbcnt;
#	ifdef DEBUG
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_POSITION ? "POSITION" : "COMPLETE")
				 << " Event, Combo: xtiles = " << combo_xtiles << ", ytiles = " << combo_ytiles
				 << ", tiles_right = " << combo_tiles_right << ", tiles_below = " << combo_tiles_below << ", count = " << combo_cnt
				 << ", at x = " << x << ", y = " << y << endl;
#	endif
			if (combo_cnt >= 0) {
				Move_dragged_combo(
						combo_xtiles, combo_ytiles, combo_tiles_right, combo_tiles_below, x, y, drag_prevx, drag_prevy, true);
			}
		} else if (drag_cnknum != -1) {
#	ifdef DEBUG
			cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_POSITION ? "POSITION" : "COMPLETE")
				 << " Event, Chunk: num = " << drag_cnknum << ", at x = " << x << ", y = " << y << endl;
#	endif
			int cx, cy;
			gwin->get_win()->screen_to_game(x, y, false, cx, cy);
			cx = ((gwin->get_scrolltx() + (cx / c_tilesize)) / c_tiles_per_chunk) * c_tiles_per_chunk;
			cy = ((gwin->get_scrollty() + (cy / c_tilesize)) / c_tiles_per_chunk) * c_tiles_per_chunk;
			cx = ((cx - gwin->get_scrolltx()) * c_tilesize) + c_chunksize;
			cy = ((cy - gwin->get_scrollty()) * c_tilesize) + c_chunksize;
			gwin->get_win()->game_to_screen(cx, cy, false, x, y);
			Move_grid(x, y, drag_prevx, drag_prevy, false, c_tiles_per_chunk, c_tiles_per_chunk, 0, 0);
			gwin->show();
		}
		drag_prevx = x;
		drag_prevy = y;
#	ifdef DEBUG
		cout << "(EXULT) SDL_EVENT_DROP_" << (event.type == SDL_EVENT_DROP_POSITION ? "POSITION" : "COMPLETE") << " Event complete"
			 << endl;
#	endif
#endif
		break;
	}
	default:
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		if (event.type == ShortcutBar_gump::eventType) {
			if (!dragged) {
				if (g_shortcutBar) {    // just in case
					g_shortcutBar->handleMouseUp(event);
				}
			}
			dragging = dragged = false;
		}
		break;
	}
}

/*
 *  Wait for a click, or optionally, a kbd. chr.
 *
 *  Output: false if user hit ESC.
 */
static bool Get_click(
		int& x, int& y,
		char* chr,             // Char. returned if not null.
		bool  drag_ok,         // Okay to drag/close while here.
		bool  rotate_colors    // If the palette colors should rotate.
) {
	dragging               = false;    // Init.
	uint32 last_rotate     = 0;
	g_waiting_for_click    = true;
	SDL_Renderer* renderer = SDL_GetRenderer(gwin->get_win()->get_screen_window());
	while (true) {
		SDL_Event event;
		Delay();    // Wait a fraction of a second.

		const uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		Mouse::mouse()->hide();    // Turn off mouse.
		Mouse::mouse_update = false;

		if (rotate_colors) {
			const int rot_speed = 100 << (gwin->get_win()->fast_palette_rotate() ? 0 : 1);
			if (ticks > last_rotate + rot_speed) {
				// (Blits in simulated 8-bit mode.)
				gwin->get_win()->rotate_colors(0xfc, 3, 0);
				gwin->get_win()->rotate_colors(0xf8, 4, 0);
				gwin->get_win()->rotate_colors(0xf4, 4, 0);
				gwin->get_win()->rotate_colors(0xf0, 4, 0);
				gwin->get_win()->rotate_colors(0xe8, 8, 0);
				gwin->get_win()->rotate_colors(0xe0, 8, 1);
				while (ticks > last_rotate + rot_speed) {
					last_rotate += rot_speed;
				}
				// Non palettized needs explicit blit.
				if (!gwin->get_win()->is_palettized()) {
					gwin->set_painted();
				}
			}
		}

		// Mouse scale factor
		static bool rightclick;
		while (SDL_PollEvent(&event)) {
			SDL_ConvertEventToRenderCoordinates(renderer, &event);
			switch (event.type) {
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				SDL_SetWindowMouseGrab(gwin->get_win()->get_screen_window(), true);
				if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
					break;
				}
				if (event.button.button == 3) {
					rightclick = true;
				} else if (drag_ok && event.button.button == 1) {
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
					dragging = gwin->start_dragging(x, y);
					dragged  = false;
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				SDL_SetWindowMouseGrab(gwin->get_win()->get_screen_window(), false);
				if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
					break;
				}
				if (event.button.button == 1) {
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
					const bool drg   = dragging;
					const bool drged = dragged;
					dragging = dragged = false;
					if (!drg || !gwin->drop_dragged(x, y, drged)) {
						if (chr) {
							*chr = 0;
						}
						g_waiting_for_click = false;
						return true;
					}
				} else if (event.button.button == 3) {
					// Just stop.  Don't get followers!
					gwin->get_main_actor()->stop();
					if (gwin->get_mouse3rd() && rightclick) {
						rightclick          = false;
						g_waiting_for_click = false;
						return false;
					}
				}
				break;
			case SDL_EVENT_MOUSE_MOTION: {
				int mx;
				int my;
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

				Mouse::mouse()->move(mx, my);
				Mouse::mouse_update = true;
				if (drag_ok && (event.motion.state & SDL_BUTTON_LMASK)) {
					dragged = gwin->drag(mx, my);
				}
				break;
			}
			case SDL_EVENT_KEY_DOWN: {
				//+++++ convert to unicode first?
				const int c = event.key.key;
				switch (c) {
				case SDLK_ESCAPE:
					g_waiting_for_click = false;
					return false;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
				case SDLK_RCTRL:
				case SDLK_LCTRL:
				case SDLK_RALT:
				case SDLK_LALT:
				case SDLK_RGUI:
				case SDLK_LGUI:
				case SDLK_NUMLOCKCLEAR:
				case SDLK_CAPSLOCK:
				case SDLK_SCROLLLOCK:
					break;
				default:
					if (keybinder->IsMotionEvent(event)) {
						break;
					}
					if ((c == SDLK_S) && (event.key.mod & SDL_KMOD_ALT) && (event.key.mod & SDL_KMOD_CTRL)) {
						make_screenshot(true);
						break;
					}
					if (chr) {    // Looking for a character?
						*chr                = (event.key.mod & SDL_KMOD_SHIFT) ? toupper(c) : c;
						g_waiting_for_click = false;
						return true;
					}
					break;
				}
				break;
			}
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				gwin->get_focus();
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				gwin->lose_focus();
				break;
			}
		}
		if (dragging) {
			gwin->paint_dirty();
		}
		Mouse::mouse()->show();    // Turn on mouse.
		if (!gwin->show() &&       // Blit to screen if necessary.
			Mouse::mouse_update) {
			Mouse::mouse()->blit_dirty();
		}
	}
	g_waiting_for_click = false;
	return false;    // Shouldn't get here.
}

/*
 *  Get a click, or, optionally, a keyboard char.
 *
 *  Output: false if user hit ESC.
 *      Chr gets keyboard char., or 0 if it's was a mouse click.
 */

bool Get_click(
		int& x, int& y,                      // Location returned (if not ESC).
		Mouse::Mouse_shapes shape,           // Mouse shape to use.
		char*               chr,             // Char. returned if not null.
		bool                drag_ok,         // Okay to drag/close while here.
		Paintable*          paint,           // Paint this over everything else.
		bool                rotate_colors    // If the palette colors should rotate.
) {
	if (chr) {
		*chr = 0;    // Init.
	}
	const Mouse::Mouse_shapes saveshape = Mouse::mouse()->get_shape();
	if (shape != Mouse::dontchange) {
		Mouse::mouse()->set_shape(shape);
	}
	if (paint) {
		paint->paint();
	}
	Mouse::mouse()->show();
	gwin->show(true);    // Want to see new mouse.
	gwin->get_tqueue()->pause(Game::get_ticks());
	const bool ret = Get_click(x, y, chr, drag_ok, rotate_colors);
	gwin->get_tqueue()->resume(Game::get_ticks());
	Mouse::mouse()->set_shape(saveshape);
	return ret;
}

/*
 *  Wait for someone to stop walking.  If a timeout is given, at least
 *  one animation cycle will still always occur.
 */

void Wait_for_arrival(
		Actor*            actor,      // Whom to wait for.
		const Tile_coord& dest,       // Where he's going.
		long              maxticks    // Max. # msecs. to wait, or 0.
) {
	// Mouse scale factor
	int mx;
	int my;

	const bool    os           = Mouse::mouse()->is_onscreen();
	uint32        last_repaint = 0;    // For insuring animation repaints.
	Actor_action* orig_action  = actor->get_action();
	const uint32  stop_time    = SDL_GetTicks() + maxticks;
	bool          timeout      = false;
	while (actor->is_moving() && actor->get_action() == orig_action && actor->get_tile() != dest && !timeout) {
		Delay();    // Wait a fraction of a second.

		Mouse::mouse()->hide();    // Turn off mouse.
		Mouse::mouse_update = false;

		SDL_Renderer* renderer = SDL_GetRenderer(gwin->get_win()->get_screen_window());
		SDL_Event     event;
		while (SDL_PollEvent(&event)) {
			SDL_ConvertEventToRenderCoordinates(renderer, &event);
			switch (event.type) {
			case SDL_EVENT_MOUSE_MOTION:
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

				Mouse::mouse()->move(mx, my);
				Mouse::mouse_update = true;
				break;
			}
		}
		// Get current time, & animate.
		const uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		if (maxticks && ticks > stop_time) {
			timeout = true;
		}
		if (gwin->have_focus()) {
			gwin->get_tqueue()->activate(ticks);
		}
		// Show animation every 1/20 sec.
		if (ticks > last_repaint + 50 || gwin->was_painted()) {
			gwin->paint_dirty();
			while (ticks > last_repaint + 50) {
				last_repaint += 50;
			}
		}

		Mouse::mouse()->show();       // Re-display mouse.
		if (!gwin->show() &&          // Blit to screen if necessary.
			Mouse::mouse_update) {    // If not, did mouse change?
			Mouse::mouse()->blit_dirty();
		}
	}

	if (!os) {
		Mouse::mouse()->hide();
	}
}

/*
 *  Shift 'wizard's view' according to mouse position.
 */

static void Shift_wizards_eye(int mx, int my) {
	// Figure dir. from center.
	const int        cx         = gwin->get_width() / 2;
	const int        cy         = gwin->get_height() / 2;
	const int        dy         = cy - my;
	const int        dx         = mx - cx;
	const Direction  dir        = Get_direction_NoWrap(dy, dx);
	static const int deltas[16] = {0, -1, 1, -1, 1, 0, 1, 1, 0, 1, -1, 1, -1, 0, -1, -1};
	const int        dirx       = deltas[2 * dir];
	const int        diry       = deltas[2 * dir + 1];
	if (dirx == 1) {
		gwin->view_right();
	} else if (dirx == -1) {
		gwin->view_left();
	}
	if (diry == 1) {
		gwin->view_down();
	} else if (diry == -1) {
		gwin->view_up();
	}
}

/*
 *  Do the 'wizard's eye' spell by letting the user browse around.
 */

void Wizard_eye(long msecs    // Length of time in milliseconds.
) {
	// Center of screen.
	const int cx = gwin->get_width() / 2;
	const int cy = gwin->get_height() / 2;

	const bool   os           = Mouse::mouse()->is_onscreen();
	const uint32 stop_time    = SDL_GetTicks() + msecs;
	uint32       last_repaint = 0;    // For insuring animation repaints.
	bool         timeout      = false;
	while (!timeout) {
		if (touchui != nullptr) {
			touchui->showButtonControls();
			touchui->hideGameControls();
		}
		if (Face_stats::Visible()) {
			Face_stats::HideGump();
		}
		if (ShortcutBar_gump::Visible()) {
			ShortcutBar_gump::HideGump();
		}

		Delay();    // Wait a fraction of a second.

		Mouse::mouse()->hide();    // Turn off mouse.
		Mouse::mouse_update    = false;
		SDL_Renderer* renderer = SDL_GetRenderer(gwin->get_win()->get_screen_window());
		SDL_Event     event;
		while (SDL_PollEvent(&event)) {
			SDL_ConvertEventToRenderCoordinates(renderer, &event);
			switch (event.type) {
			case SDL_EVENT_FINGER_MOTION: {
				if (event.tfinger.dy > 0) {
					gwin->view_down();
				} else if (event.tfinger.dy < 0) {
					gwin->view_up();
				}
				if (event.tfinger.dx > 0) {
					gwin->view_right();
				} else if (event.tfinger.dx < 0) {
					gwin->view_left();
				}
				break;
			}
			case SDL_EVENT_MOUSE_MOTION: {
				int mx;
				int my;
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

				Mouse::mouse()->move(mx, my);
				Mouse::mouse()->set_shape(Mouse::mouse()->get_short_arrow(Get_direction_NoWrap(cy - my, mx - cx)));
				Mouse::mouse_update = true;
				break;
			}
			case SDL_EVENT_KEY_DOWN:
				if (event.key.key == SDLK_ESCAPE) {
					timeout = true;
				}
			}
		}
		// Get current time, & animate.
		const uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		if (ticks > stop_time) {
			timeout = true;
		}
		if (gwin->have_focus()) {
			gwin->get_tqueue()->activate(ticks);
		}
		// Show animation every 1/20 sec.
		if (ticks > last_repaint + 50 || gwin->was_painted()) {
			// Right mouse button down?
			const int ms = SDL_GetMouseState(nullptr, nullptr);
			int       mx = Mouse::mouse()->get_mousex();
			int       my = Mouse::mouse()->get_mousey();
			if (SDL_BUTTON_RMASK & ms) {
				Shift_wizards_eye(mx, my);
			}
			gwin->set_all_dirty();
			gwin->paint_dirty();
			// Paint sprite over view.
			ShapeID      eye(10, 0, SF_SPRITES_VGA);
			Shape_frame* spr = eye.get_shape();
			// Center it.
			const int w    = gwin->get_width();
			const int h    = gwin->get_height();
			const int sw   = spr->get_width();
			const int sh   = spr->get_height();
			const int topx = (w - sw) / 2;
			const int topy = (h - sh) / 2;
			eye.paint_shape(topx + spr->get_xleft(), topy + spr->get_yabove());
			const int sizex = (w - 320) / 2;
			const int sizey = (h - 200) / 2;
			if (sizey) {    // Black-fill area outside original resolution.
				gwin->get_win()->fill8(0, w, sizey, 0, 0);
				gwin->get_win()->fill8(0, w, sizey, 0, h - sizey);
			}
			if (sizex) {
				gwin->get_win()->fill8(0, sizex, 200, 0, sizey);
				gwin->get_win()->fill8(0, sizex, 200, w - sizex, sizey);
			}
			while (ticks > last_repaint + 50) {
				last_repaint += 50;
			}
		}

		Mouse::mouse()->show();       // Re-display mouse.
		if (!gwin->show() &&          // Blit to screen if necessary.
			Mouse::mouse_update) {    // If not, did mouse change?
			Mouse::mouse()->blit_dirty();
		}
		if (touchui != nullptr) {
			touchui->showGameControls();
		}
		if (!Face_stats::Visible()) {
			Face_stats::ShowGump();
		}
		if (!ShortcutBar_gump::Visible()) {
			ShortcutBar_gump::ShowGump();
		}
	}

	if (!os) {
		Mouse::mouse()->hide();
	}
	gwin->center_view(gwin->get_main_actor()->get_tile());
}

void increase_scaleval() {
	if (!cheat()) {
		return;
	}

	current_scaleval++;
	if (current_scaleval >= 9) {
		current_scaleval = 1;
	}
	set_scaleval(current_scaleval);
}

void decrease_scaleval() {
	if (!cheat()) {
		return;
	}

	current_scaleval--;
	if (current_scaleval < 1) {
		current_scaleval = 8;
	}
	set_scaleval(current_scaleval);
}

void set_scaleval(int new_scaleval) {
	const int  scaler     = gwin->get_win()->get_scaler();
	const bool fullscreen = gwin->get_win()->is_fullscreen();
	if (new_scaleval >= 1 && !fullscreen && scaler == Image_window::point && cheat.in_map_editor()) {
		current_scaleval = new_scaleval;
		bool share_settings;
		config->value("config/video/share_video_settings", share_settings, true);
		const string& vidStr = (fullscreen || share_settings) ? "config/video" : "config/video/window";
		int           resx;
		int           resy;
		config->value(vidStr + "/display/width", resx);
		config->value(vidStr + "/display/height", resy);

		// for Studio zooming we set game area to auto, fill quality to point,
		// fill mode to fill and increase/decrease the scale value
		gwin->resized(resx, resy, fullscreen, 0, 0, current_scaleval, scaler, Image_window::Fill, Image_window::point);
	}
}

void make_screenshot(bool silent) {
	// TODO: Maybe some form of "My Pictures" on Windows.
	const string     savegamepath = get_system_path("<SAVEGAME>");
	const size_t     strsize      = savegamepath.size() + 20;
	char*            fn           = new char[strsize];
	bool             namefound    = false;
	Effects_manager* eman         = gwin->get_effects();

	// look for the next available exult???.pcx or .png file
	for (int i = 0; i < 1000 && !namefound; i++) {
		snprintf(fn, strsize, "%s/exult%03i." EXULT_IMAGE_SUFFIX, savegamepath.c_str(), i);
		FILE* f = fopen(fn, "rb");
		if (f) {
			fclose(f);
		} else {
			namefound = true;
		}
	}

	if (!namefound) {
		if (!silent) {
			eman->center_text(get_text_msg(0x740 - msg_file_start));    // "Too many screenshots"
		}
	} else {
		SDL_IOStream* dst = SDL_IOFromFile(fn, "wb");

		if (gwin->get_win()->screenshot(dst, false)) {
			cout << "Screenshot saved in " << fn << endl;
			if (!silent) {
				eman->center_text(get_text_msg(0x741 - msg_file_start));    // "Screenshot"
			}
		} else {
			if (!silent) {
				eman->center_text(get_text_msg(0x742 - msg_file_start));    // "Screenshot failed"
			}
		}
	}
	delete[] fn;
}

void change_gamma(bool down) {
	double       r;
	double       g;
	double       b;
	char         text[256];
	const double delta = down ? 0.05 : -0.05;
	Image_window8::get_gamma(r, g, b);
	Image_window8::set_gamma(r + delta, g + delta, b + delta);
	gwin->get_pal()->apply(true);    // So new brightness applies.

	// Message
	Image_window8::get_gamma(r, g, b);
	std::ostringstream s;
	s << get_text_msg(0x743 - msg_file_start) << std::fixed    // "Gamma set to"
	  << std::setprecision(2) << " R: " << r << " G: " << g << " B: " << b;
	gwin->get_effects()->center_text(s.str().c_str());

	int igam = std::lround(r * 10000);
	snprintf(text, sizeof(text), "%d.%04d", igam / 10000, igam % 10000);
	config->set("config/video/gamma/red", text, true);

	igam = std::lround(g * 10000);
	snprintf(text, sizeof(text), "%d.%04d", igam / 10000, igam % 10000);
	config->set("config/video/gamma/green", text, true);

	igam = std::lround(b * 10000);
	snprintf(text, sizeof(text), "%d.%04d", igam / 10000, igam % 10000);
	config->set("config/video/gamma/blue", text, true);
}

void BuildGameMap(BaseGameInfo* game, int mapnum) {
	// create 2048x2048 screenshots of the full Ultima 7 map.
	// WARNING!! Takes up lots of memory and diskspace!
	if (arg_buildmap >= 0) {
		int maplift = 16;
		switch (arg_buildmap) {
		case 0:
			maplift = 16;
			break;
		case 1:
			maplift = 10;
			break;
		case 2:
			maplift = 5;
			break;
		}
		int w;
		int h;
		int sc;
		int sclr;
		h = w = c_tilesize * c_tiles_per_schunk;
		sc    = 1;
		sclr  = Image_window::point;
		Image_window8::set_gamma(1, 1, 1);
		const Image_window::FillMode fillmode = Image_window::Fit;

		// string    fullscreenstr;      // Check config. for fullscreen mode.
		// config->value("config/video/fullscreen",fullscreenstr,"no");
		//  set windowed mode
		// config->set("config/video/fullscreen","no",false);
		gwin = new Game_window(w, h, false, w, h, sc, sclr, fillmode, sclr);
		// restore original fullscreen setting
		// config->set("config/video/fullscreen",fullscreenstr,true);
		Audio::Init();
		Game::create_game(game);
		gwin->init_files(false);    // init, but don't show plasma
		gwin->get_map()->init();    // +++++Got to clean this up.
		gwin->set_map(mapnum);
		gwin->get_pal()->set(0);
		const string savegamepath = get_system_path("<SAVEGAME>");
		for (int x = 0; x < c_num_chunks / c_chunks_per_schunk; x++) {
			for (int y = 0; y < c_num_chunks / c_chunks_per_schunk; y++) {
				gwin->paint_map_at_tile(0, 0, w, h, x * c_tiles_per_schunk, y * c_tiles_per_schunk, maplift);
				const size_t strsize = savegamepath.size() + 20;
				char*        fn      = new char[strsize];
				snprintf(fn, strsize, "%s/u7map%02x." EXULT_IMAGE_SUFFIX, savegamepath.c_str(), (12 * y) + x);
				SDL_IOStream* dst = SDL_IOFromFile(fn, "wb");
				cerr << x << "," << y << ": ";
				gwin->get_win()->screenshot(dst, true);
			}
		}
		cout << "--buildmap saved the map screenshots in " << savegamepath << endl;
		Audio::Destroy();
		exit(0);
	}
}

// Applies the UI layer configuration from the config file to the game window.
static void apply_ui_layer_config() {
	if (gwin == nullptr) {
		return;
	}
	bool ui_universal;
	config->value("config/video/ui/universal", ui_universal, true);

	auto normalize_dims = [](int& width, int& height) {
		if (width == 0 && height == 0) {
			return;
		}
		if (width <= 0 || height <= 0) {
			width  = 420;
			height = 263;
		}
	};

	struct LayerUiCfg {
		int                    width;
		int                    height;
		int                    scaler;
		Image_window::FillMode fillmode;
		int                    fill_scaler;
		int                    palette;    // Image_window::UiPaletteMode.
	};

	auto palette_from_string = [](const string& s, int fallback) {
		if (s == "day") {
			return static_cast<int>(Image_window::UiPaletteDay);
		}
		if (s == "dusk") {
			return static_cast<int>(Image_window::UiPaletteDusk);
		}
		if (s == "invisible") {
			return static_cast<int>(Image_window::UiPaletteInvisible);
		}
		if (s == "overcast") {
			return static_cast<int>(Image_window::UiPaletteOvercast);
		}
		if (s == "fog") {
			return static_cast<int>(Image_window::UiPaletteFog);
		}
		if (s == "spell") {
			return static_cast<int>(Image_window::UiPaletteSpell);
		}
		if (s == "candle") {
			return static_cast<int>(Image_window::UiPaletteCandle);
		}
		if (s == "red") {
			return static_cast<int>(Image_window::UiPaletteRed);
		}
		if (s == "lightning") {
			return static_cast<int>(Image_window::UiPaletteLightning);
		}
		if (s == "single_light") {
			return static_cast<int>(Image_window::UiPaletteSingleLight);
		}
		if (s == "many_lights") {
			return static_cast<int>(Image_window::UiPaletteManyLights);
		}
		if (s == "disabled") {
			return static_cast<int>(Image_window::UiPaletteDisabled);
		}
		return fallback;
	};
	auto palette_to_string = [](int mode) -> const char* {
		switch (mode) {
		case Image_window::UiPaletteDay:
			return "day";
		case Image_window::UiPaletteDusk:
			return "dusk";
		case Image_window::UiPaletteInvisible:
			return "invisible";
		case Image_window::UiPaletteOvercast:
			return "overcast";
		case Image_window::UiPaletteFog:
			return "fog";
		case Image_window::UiPaletteSpell:
			return "spell";
		case Image_window::UiPaletteCandle:
			return "candle";
		case Image_window::UiPaletteRed:
			return "red";
		case Image_window::UiPaletteLightning:
			return "lightning";
		case Image_window::UiPaletteSingleLight:
			return "single_light";
		case Image_window::UiPaletteManyLights:
			return "many_lights";
		default:
			return "disabled";
		}
	};

	auto read_layer_cfg = [&](const string& base, const LayerUiCfg& fallback) {
		LayerUiCfg cfg = fallback;
		config->value(base + "/width", cfg.width, fallback.width);
		config->value(base + "/height", cfg.height, fallback.height);
		normalize_dims(cfg.width, cfg.height);

		string s;
		config->value(base + "/scale_method", s, Image_window::get_name_for_scaler(fallback.scaler));
		cfg.scaler = Image_window::get_scaler_for_name(s.c_str());
		if (cfg.scaler < 0) {
			cfg.scaler = fallback.scaler;
		}

		string fb_fmode;
		Image_window::fillmode_to_string(fallback.fillmode, fb_fmode);
		string fm;
		config->value(base + "/fill_mode", fm, fb_fmode.c_str());
		cfg.fillmode = Image_window::string_to_fillmode(fm.c_str());
		if (cfg.fillmode == 0) {
			cfg.fillmode = fallback.fillmode;
		}

		string fs;
		config->value(base + "/fill_scaler", fs, Image_window::get_name_for_scaler(fallback.fill_scaler));
		cfg.fill_scaler = Image_window::get_scaler_for_name(fs.c_str());
		if (cfg.fill_scaler < 0) {
			cfg.fill_scaler = fallback.fill_scaler;
		}

		string pl;
		config->value(base + "/palette", pl, palette_to_string(fallback.palette));
		cfg.palette = palette_from_string(pl, fallback.palette);
		return cfg;
	};

	auto write_layer_cfg = [&](const string& base, const LayerUiCfg& cfg, bool overwrite) {
		if (overwrite || !config->key_exists(base + "/width")) {
			config->set(base + "/width", cfg.width, false);
		}
		if (overwrite || !config->key_exists(base + "/height")) {
			config->set(base + "/height", cfg.height, false);
		}
		if (overwrite || !config->key_exists(base + "/scale_method")) {
			config->set(base + "/scale_method", Image_window::get_name_for_scaler(cfg.scaler), false);
		}
		if (overwrite || !config->key_exists(base + "/fill_mode")) {
			string fmode_s;
			Image_window::fillmode_to_string(cfg.fillmode, fmode_s);
			config->set(base + "/fill_mode", fmode_s, false);
		}
		if (overwrite || !config->key_exists(base + "/fill_scaler")) {
			config->set(base + "/fill_scaler", Image_window::get_name_for_scaler(cfg.fill_scaler), false);
		}
		if (overwrite || !config->key_exists(base + "/palette")) {
			config->set(base + "/palette", palette_to_string(cfg.palette), false);
		}
	};

	LayerUiCfg global_cfg;
	global_cfg.width  = 420;
	global_cfg.height = 263;
	config->value("config/video/ui/width", global_cfg.width, 420);
	config->value("config/video/ui/height", global_cfg.height, 263);
	normalize_dims(global_cfg.width, global_cfg.height);
	{
		string s;
		config->value("config/video/ui/scale_method", s, Image_window::get_name_for_scaler(Image_window::point));
		global_cfg.scaler = Image_window::get_scaler_for_name(s.c_str());
		if (global_cfg.scaler < 0) {
			global_cfg.scaler = Image_window::point;
		}
		string fit_default;
		Image_window::fillmode_to_string(Image_window::Fit, fit_default);
		string fm;
		config->value("config/video/ui/fill_mode", fm, fit_default.c_str());
		global_cfg.fillmode = Image_window::string_to_fillmode(fm.c_str());
		if (global_cfg.fillmode == 0) {
			global_cfg.fillmode = Image_window::Fit;
		}
		string fs;
		config->value("config/video/ui/fill_scaler", fs, Image_window::get_name_for_scaler(Image_window::point));
		global_cfg.fill_scaler = Image_window::get_scaler_for_name(fs.c_str());
		if (global_cfg.fill_scaler < 0) {
			global_cfg.fill_scaler = Image_window::point;
		}
		string pl;
		config->value("config/video/ui/palette", pl, "disabled");
		global_cfg.palette = palette_from_string(pl, Image_window::UiPaletteDisabled);
	}

	config->set("config/video/ui/universal", ui_universal ? "yes" : "no", false);
	write_layer_cfg("config/video/ui", global_cfg, true);

	gwin->set_ui_config(global_cfg.width, global_cfg.height, global_cfg.scaler, global_cfg.fillmode, global_cfg.fill_scaler);

	auto apply_named_layer = [&](Image_window::UiLayerKind kind, const string& key) {
		const string base = "config/video/ui/" + key;
		LayerUiCfg   cfg  = ui_universal ? global_cfg : read_layer_cfg(base, global_cfg);
		gwin->set_ui_layer_config(kind, cfg.width, cfg.height, cfg.scaler, cfg.fillmode, cfg.fill_scaler);
		gwin->set_ui_layer_palette(kind, cfg.palette);
		write_layer_cfg(base, cfg, !ui_universal);
	};

	apply_named_layer(Image_window::UiLayerConversations, "conversations");
	apply_named_layer(Image_window::UiLayerMousePointer, "mouse_pointer");
	apply_named_layer(Image_window::UiLayerGumps, "gumps");
	apply_named_layer(Image_window::UiLayerHudGumps, "hud_gumps");
	apply_named_layer(Image_window::UiLayerTextGumps, "text_gumps");
	apply_named_layer(Image_window::UiLayerModalGumps, "modal_gumps");
	apply_named_layer(Image_window::UiLayerDisplayMap, "display_map");
	apply_named_layer(Image_window::UiLayerTextEffects, "text_effect");
}

/*
 *  Most of the game setable video configuration stuff is stored here so
 *  it isn't duplicated all over the place. fullscreen is determined
 *  before coming here. config->write_back() is done after (if needed).
 *  video_init does save before trying to open the Game_window just in case
 */
void setup_video(
		bool fullscreen, int setup_video_type, int resx, int resy, int gw, int gh, int scaleval, int scaler,
		Image_window::FillMode fillmode, int fill_scaler) {
	string fmode_string;
	string sclr;
	string scalerName;
	string fillScalerName;
	bool   video_init  = false;
	bool   set_config  = false;
	bool   change_gwin = false;
	bool   menu_init   = false;
	bool   read_config = false;
	if (setup_video_type == VIDEO_INIT) {
		read_config = video_init = set_config = true;
	} else if (setup_video_type == TOGGLE_FULLSCREEN) {
		read_config = change_gwin = true;
	} else if (setup_video_type == MENU_INIT) {
		read_config = menu_init = true;
	} else if (setup_video_type == SET_CONFIG) {
		set_config = true;
	}
	bool share_settings;
	config->value("config/video/share_video_settings", share_settings, true);
	const string vidStr((fullscreen || share_settings) ? "config/video" : "config/video/window");
	if (read_config) {
#ifdef DEBUG
		cout << "Reading video menu adjustable configuration options" << endl;
#endif
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
		// Default resolution is 320x240 with 1x scaling
		const int    w                   = 320;
		const int    h                   = 240;
		const int    sc                  = 1;
		const string default_scaler      = "point";
		const string default_fill_scaler = "point";
		const string default_fmode       = "Fill";
		fullscreen                       = true;
		// [SDL 3] force_bpp is set to 32 in imagewin/imagewin.cc
		// discard the config/video/force_bpp
#else
		// Default resolution is 320x240 with 2x scaling
		const int    w                   = 320;
		const int    h                   = 240;
		const int    sc                  = 2;
		const string default_scaler      = "point";
		const string default_fill_scaler = "point";
		const string default_fmode       = "Fit";
#endif
		string fill_scaler_str;
		if (video_init) {
			// Convert from old video dims to new
			if (config->key_exists("config/video/width")) {
				config->value("config/video/width", resx, w);
				config->remove("config/video/width", false);
			} else {
				resx = w;
			}
			if (config->key_exists("config/video/height")) {
				config->value("config/video/height", resy, h);
				config->remove("config/video/height", false);
			} else {
				resy = h;
			}
		} else {
			resx = w;
			resy = h;
		}
		config->value(vidStr + "/scale_method", sclr, default_scaler.c_str());
		config->value(vidStr + "/scale", scaleval, sc);
		scaler = Image_window::get_scaler_for_name(sclr.c_str());
		// Ensure a default scaler if a wrong scaler name is set
		if (scaler == Image_window::NoScaler || scaler == Image_window::SDLScaler) {
			scaler = Image_window::get_scaler_for_name(default_scaler.c_str());
		}
		// Ensure proper values for scaleval based on scaler.
		if (scaler == Image_window::Hq3x || scaler == Image_window::_3xBR) {
			scaleval = 3;
		} else if (scaler == Image_window::Hq4x || scaler == Image_window::_4xBR) {
			scaleval = 4;
		} else if (
				scaler != Image_window::point && scaler != Image_window::SDLScaler && scaler != Image_window::interlaced
				&& scaler != Image_window::bilinear) {
			scaleval = 2;
		}
		int dw = resx * scaleval;
		int dh = resy * scaleval;
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
		// Default display is desktop
		const SDL_DisplayMode* dispmode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
		if (dispmode) {
			dw = dispmode->w;
			dh = dispmode->h;
		}
#else
		// Default display is 1024x768
		dw = (dw > 1024 ? dw : 1024);
		dh = (dh > 768 ? dh : 768);
#endif
		config->value(vidStr + "/display/width", resx, dw);
		config->value(vidStr + "/display/height", resy, dh);
		config->value(vidStr + "/game/width", gw, 320);
		config->value(vidStr + "/game/height", gh, 200);
		config->value(vidStr + "/fill_mode", fmode_string, default_fmode);
		fillmode = Image_window::string_to_fillmode(fmode_string.c_str());
		if (fillmode == 0) {
			fillmode = Image_window::AspectCorrectFit;
		}
		config->value(vidStr + "/fill_scaler", fill_scaler_str, default_fill_scaler);
		fill_scaler = Image_window::get_scaler_for_name(fill_scaler_str.c_str());
		if (fill_scaler == Image_window::NoScaler) {
			fill_scaler = Image_window::bilinear;
		}
	}
	if (set_config) {
		scalerName     = Image_window::get_name_for_scaler(scaler);
		fillScalerName = Image_window::get_name_for_scaler(fill_scaler);
		Image_window::fillmode_to_string(fillmode, fmode_string);
#ifdef DEBUG
		cout << "Setting video menu adjustable configuration options " << resx << " resX, " << resy << " resY, " << gw << " gameW, "
			 << gh << " gameH, " << scaleval << " scale, " << scalerName << " scaler, " << fmode_string << " fill mode, "
			 << fillScalerName << " fill scaler, " << (fullscreen ? "full screen" : "window") << endl;
#endif
		config->set((vidStr + "/display/width").c_str(), resx, false);
		config->set((vidStr + "/display/height").c_str(), resy, false);
		config->set((vidStr + "/game/width").c_str(), gw, false);
		config->set((vidStr + "/game/height").c_str(), gh, false);
		config->set((vidStr + "/scale").c_str(), scaleval, false);
		config->set((vidStr + "/scale_method").c_str(), scalerName, false);
		config->set((vidStr + "/fill_mode").c_str(), fmode_string, false);
		config->set((vidStr + "/fill_scaler").c_str(), fillScalerName, false);
	}
	if (video_init) {
#ifdef DEBUG
		cout << "Initializing Game_window to " << resx << " resX, " << resy << " resY, " << gw << " gameW, " << gh << " gameH, "
			 << scaleval << " scale, " << scalerName << " scaler, " << fmode_string << " fill mode, " << fillScalerName
			 << " fill scaler, " << (fullscreen ? "full screen" : "window") << endl;
#endif
		config->set("config/video/share_video_settings", share_settings ? "yes" : "no", false);
		config->write_back();
		gwin = new Game_window(resx, resy, fullscreen, gw, gh, scaleval, scaler, fillmode, fill_scaler);
		// Ensure proper clipping:
		gwin->get_win()->clear_clip();
	} else if (change_gwin) {
#ifdef DEBUG
		if (!set_config) {
			Image_window::fillmode_to_string(fillmode, fmode_string);
			scalerName     = Image_window::get_name_for_scaler(scaler);
			fillScalerName = Image_window::get_name_for_scaler(fill_scaler);
		}
		cout << "Changing Game_window to " << resx << " resX, " << resy << " resY, " << gw << " gameW, " << gh << " gameH, "
			 << scaleval << " scale, " << scalerName << " scaler, " << fmode_string << " fill mode, " << fillScalerName
			 << " fill scaler, " << (fullscreen ? "full screen" : "window") << endl;
#endif
		gwin->resized(resx, resy, fullscreen, gw, gh, scaleval, scaler, fillmode, fill_scaler);
	}
	if ((video_init || change_gwin) && gwin != nullptr) {
		// Apply the overlay-layer ("UI") scaling settings.
		apply_ui_layer_config();
	}
	if (menu_init) {
#ifdef DEBUG
		Image_window::fillmode_to_string(fillmode, fmode_string);
		scalerName     = Image_window::get_name_for_scaler(scaler);
		fillScalerName = Image_window::get_name_for_scaler(fill_scaler);
		cout << "Initializing video options menu settings " << resx << " resX, " << resy << " resY, " << gw << " gameW, " << gh
			 << " gameH, " << scaleval << " scale, " << scalerName << " scaler, " << fmode_string << " fill mode, "
			 << fillScalerName << " fill scaler, " << (fullscreen ? "full screen" : "window") << endl;
#endif
		VideoOptions_gump* videoGump = VideoOptions_gump::get_instance();
		videoGump->set_scaling(scaleval - 1);
		videoGump->set_scaler(scaler);
		videoGump->set_resolution(resx << 16 | resy);
		videoGump->set_game_resolution(gw << 16 | gh);
		videoGump->set_fill_scaler(fill_scaler);
		videoGump->set_fill_mode(fillmode);
	}
}

#ifdef USE_EXULTSTUDIO

/*
 *  Show a grid being dragged.
 */

static void Move_grid(
		int x, int y,                       // Mouse coords. within window.
		int prevx, int prevy,               // Prev. coords, or -1.
		bool ireg,                          // A single IREG object?
		int xtiles, int ytiles,             // Dimension of grid to show.
		int tiles_right, int tiles_below    // # tiles to show to right of and
											//   below (x, y).
) {
	// int scale = gwin->get_win()->get_scale();
	// x /= scale;           // Watch for scaled window.
	// y /= scale;
	gwin->get_win()->screen_to_game(x, y, false, x, y);

	const int lift = cheat.get_edit_lift();
	x += lift * 4 - 1;    // Take lift into account, round.
	y += lift * 4 - 1;
	int tx = x / c_tilesize;    // Figure tile on ground.
	int ty = y / c_tilesize;
	tx += tiles_right;
	ty += tiles_below;
	if (prevx != -1) {    // See if moved to a new tile.
		gwin->get_win()->screen_to_game(prevx, prevy, false, prevx, prevy);
		prevx += lift * 4 - 1;    // Take lift into account, round.
		prevy += lift * 4 - 1;
		int ptx = prevx / c_tilesize;
		int pty = prevy / c_tilesize;
		ptx += tiles_right;
		pty += tiles_below;
		if (tx == ptx && ty == pty) {
			return;    // Will be in same tile.
		}
		// Repaint over old area.
		const int pad = 8;
		TileRect  r(
                (ptx - xtiles + 1) * c_tilesize - pad, (pty - ytiles + 1) * c_tilesize - pad, xtiles * c_tilesize + 2 * pad,
                ytiles * c_tilesize + 2 * pad);
		r = gwin->clip_to_win(r);
		gwin->add_dirty(r);
		gwin->paint_dirty();
	}
	// First see if it's a gump.
	if (ireg && gwin->get_gump_man()->find_gump(x, y)) {
		return;    // Skip if so.
	}
	tx -= xtiles - 1;    // Get top-left of footprint.
	ty -= ytiles - 1;
	// Let's try a green outline.
	const int      pix = Shape_manager::get_instance()->get_special_pixel(POISON_PIXEL);
	Image_window8* win = gwin->get_win();
	win->set_clip(0, 0, win->get_game_width(), win->get_game_height());
	for (int Y = 0; Y <= ytiles; Y++) {
		win->fill8(pix, xtiles * c_tilesize, 1, tx * c_tilesize, (ty + Y) * c_tilesize);
	}
	for (int X = 0; X <= xtiles; X++) {
		win->fill8(pix, 1, ytiles * c_tilesize, (tx + X) * c_tilesize, ty * c_tilesize);
	}
	win->clear_clip();
	gwin->set_painted();
}

/*
 *  Show where a shape dragged from a shape-chooser will go.
 *  ALSO, this is called with shape==-1 to just force a repaint.
 */

static void Move_dragged_shape(
		int shape, int frame,    // What to create, OR -1 to just
		//   repaint window.
		int x, int y,            // Mouse coords. within window.
		int prevx, int prevy,    // Prev. coords, or -1.
		bool show                // Blit window.
) {
	if (shape == -1) {
		gwin->set_all_dirty();
		return;
	}
	const Shape_info& info = ShapeID::get_info(shape);
	// Get footprint in tiles.
	const int xtiles = info.get_3d_xtiles(frame);
	const int ytiles = info.get_3d_ytiles(frame);
	const int sclass = info.get_shape_class();
	// Is it an ireg (changeable) obj?
	const bool ireg = (sclass != Shape_info::unusable && sclass != Shape_info::building);
	Move_grid(x, y, prevx, prevy, ireg, xtiles, ytiles, 0, 0);
	if (show) {
		gwin->show();
	}
}

/*
 *  Show where a shape dragged from a shape-chooser will go.
 */

static void Move_dragged_combo(
		int xtiles, int ytiles,           // Dimensions in tiles.
		int tiles_right,                  // Tiles right of & below hot-spot.
		int tiles_below, int x, int y,    // Mouse coords. within window.
		int prevx, int prevy,             // Prev. coords, or -1.
		bool show                         // Blit window.
) {
	Move_grid(x, y, prevx, prevy, false, xtiles, ytiles, tiles_right, tiles_below);
	if (show) {
		gwin->show();
	}
}

/*
 *  Create an object as moveable (IREG) or fixed.
 */

static Game_object_shared Create_object(
		int shape, int frame,    // What to create.
		bool& ireg               // Rets. TRUE if ireg (moveable).
) {
	const Shape_info& info   = ShapeID::get_info(shape);
	const int         sclass = info.get_shape_class();
	// Is it an ireg (changeable) obj?
	ireg = (sclass != Shape_info::unusable && sclass != Shape_info::building);
	Game_object_shared newobj;
	if (ireg) {
		newobj = gwin->get_map()->create_ireg_object(info, shape, frame, 0, 0, 0);
	} else {
		newobj = gwin->get_map()->create_ifix_object(shape, frame);
	}
	return newobj;
}

/*
 *  Drop a shape dragged from a shape-chooser via drag-and-drop.
 */

static void Drop_dragged_shape(
		int shape, int frame,    // What to create.
		int x, int y             // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) {    // Get into editing mode.
		cheat.toggle_map_editor();
	}
	cheat.clear_selected();    // Remove old selected.
	gwin->get_map()->set_map_modified();
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	const ShapeID sid(shape, frame);
	if (gwin->skip_lift == 0) {    // Editing terrain?
		int            tx    = (gwin->get_scrolltx() + x / c_tilesize) % c_num_tiles;
		int            ty    = (gwin->get_scrollty() + y / c_tilesize) % c_num_tiles;
		const int      cx    = tx / c_tiles_per_chunk;
		const int      cy    = ty / c_tiles_per_chunk;
		Map_chunk*     chunk = gwin->get_map()->get_chunk(cx, cy);
		Chunk_terrain* ter   = chunk->get_terrain();
		tx %= c_tiles_per_chunk;
		ty %= c_tiles_per_chunk;
		const ShapeID curid = ter->get_flat(tx, ty);
		if (sid.get_shapenum() != curid.get_shapenum() || sid.get_framenum() != curid.get_framenum()) {
			ter->set_flat(tx, ty, sid);
			Game_map::set_chunk_terrains_modified();
			gwin->set_all_dirty();    // ++++++++For now.++++++++++
		}
		return;
	}
	Shape_frame* sh = sid.get_shape();
	if (!sh || !sh->is_rle()) {    // Flats are only for terrain.
		return;                    // Shouldn't happen.
	}
	cout << "Last drag pos: (" << x << ", " << y << ')' << endl;
	cout << "Create shape (" << shape << '/' << frame << ')' << endl;
	bool                     ireg;    // Create object.
	const Game_object_shared newobj = Create_object(shape, frame, ireg);
	Dragging_info            drag(newobj);
	drag.drop(x, y, true);    // (Dels if it fails.)
}

/*
 *  Drop a chunk dragged from a chunk-chooser via drag-and-drop.
 */

static void Drop_dragged_chunk(
		int chunknum,    // Index in 'u7chunks'.
		int x, int y     // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) {    // Get into editing mode.
		cheat.toggle_map_editor();
	}
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	cout << "Last drag pos: (" << x << ", " << y << ')' << endl;
	cout << "Set chunk (" << chunknum << ')' << endl;
	// Need chunk-coordinates.
	const int tx = (gwin->get_scrolltx() + x / c_tilesize) % c_num_tiles;
	const int ty = (gwin->get_scrollty() + y / c_tilesize) % c_num_tiles;
	const int cx = tx / c_tiles_per_chunk;
	const int cy = ty / c_tiles_per_chunk;
	gwin->get_map()->set_chunk_terrain(cx, cy, chunknum);
	gwin->paint();
}

/*
 *  Drop a npc dragged from a npc-chooser via drag-and-drop.
 */

static void Drop_dragged_npc(
		int npcnum, int x, int y    // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) {    // Get into editing mode.
		cheat.toggle_map_editor();
	}
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	cout << "Last drag pos: (" << x << ", " << y << ')' << endl;
	cout << "Set npc (" << npcnum << ')' << endl;
	Actor* npc = gwin->get_npc(npcnum);
	if (!npc) {
		return;
	}
	const Game_object_shared safe_npc = npc->shared_from_this();
	Game_object_shared       npckeep;
	if (npc->is_pos_invalid()) {    // Brand new?
		npc->clear_flag(Obj_flags::dead);
	} else {
		npc->remove_this(&npckeep);    // Remove if already on map.
	}
	Dragging_info drag(safe_npc);
	if (drag.drop(x, y)) {
		npc->set_unused(false);
	}
	gwin->paint();
}

/*
 *  Drop a combination object dragged from ExultStudio.
 */

void Drop_dragged_combo(
		int            cnt,      // # shapes.
		U7_combo_data* combo,    // The shapes.
		int x, int y             // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) {    // Get into editing mode.
		cheat.toggle_map_editor();
	}
	cheat.clear_selected();    // Remove old selected.
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	const int at_lift = cheat.get_edit_lift();
	x += at_lift * 4 - 1;    // Take lift into account, round.
	y += at_lift * 4 - 1;
	// Figure tile at mouse pos.
	const int tx = (gwin->get_scrolltx() + x / c_tilesize) % c_num_tiles;
	const int ty = (gwin->get_scrollty() + y / c_tilesize) % c_num_tiles;
	for (int i = 0; i < cnt; i++) {
		// Drop each shape.
		const U7_combo_data& elem = combo[i];
		// Figure new tile coord.
		int ntx = (tx + elem.tx) % c_num_tiles;
		int nty = (ty + elem.ty) % c_num_tiles;
		int ntz = at_lift + elem.tz;
		if (ntz < 0) {
			ntz = 0;
		}
		const ShapeID sid(elem.shape, elem.frame);
		if (gwin->skip_lift == 0) {    // Editing terrain?
			const int      cx    = ntx / c_tiles_per_chunk;
			const int      cy    = nty / c_tiles_per_chunk;
			Map_chunk*     chunk = gwin->get_map()->get_chunk(cx, cy);
			Chunk_terrain* ter   = chunk->get_terrain();
			ntx %= c_tiles_per_chunk;
			nty %= c_tiles_per_chunk;
			ter->set_flat(ntx, nty, sid);
			Game_map::set_chunk_terrains_modified();
			continue;
		}
		bool                     ireg;    // Create object.
		const Game_object_shared newobj = Create_object(elem.shape, elem.frame, ireg);
		newobj->set_invalid();    // Not in world.
		newobj->move(ntx, nty, ntz);
		// Add to selection.
		cheat.append_selected(newobj.get());
	}
	gwin->set_all_dirty();    // For now, until we clear out grid.
}

void Set_dragged_shape(int file, int shnum, int frnum) {
	drag_shfile = file;
	drag_shpnum = shnum;
	drag_shfnum = frnum;
	drag_cbcnt  = -1;
	drag_npcnum = -1;
	drag_cnknum = -1;
}

void Set_dragged_combo(int cnt, int xtl, int ytl, int rtl, int btl) {
	drag_shpnum   = -1;
	drag_cbcnt    = cnt;
	drag_cbxtiles = xtl;
	drag_cbytiles = ytl;
	drag_cbrtiles = rtl;
	drag_cbbtiles = btl;
	drag_npcnum   = -1;
	drag_cnknum   = -1;
}

void Set_dragged_npc(int npcnum) {
	drag_shpnum = -1;
	drag_cbcnt  = -1;
	drag_npcnum = npcnum;
	drag_cnknum = -1;
}

void Set_dragged_chunk(int chunknum) {
	drag_shpnum = -1;
	drag_cbcnt  = -1;
	drag_npcnum = -1;
	drag_cnknum = chunknum;
}
#endif
