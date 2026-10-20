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
#include "Text_gump.h"
#include "Notebook_gump.h"
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
#include "egg.h"
#include "gameclk.h"
#include "exult_bg_flx.h"
#include "exult_flx.h"
#include "exult_si_flx.h"
#include "exultmenu.h"
#include "fnames.h"
#include "font.h"
#include "font_map.h"
#include "friendclass.h"
#include "friendmap.h"
#include "spellbook.h"
#include "friendnet.h"
#include "game.h"
#include "gamemap.h"
#include "iregobjs.h"
#include "objiter.h"
#include "gamemgr/modmgr.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "istring.h"
#include "items.h"
#include "shapeinf/aniinf.h"
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
#include <map>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iomanip>
#if defined(_WIN32)
#	include <process.h>
#else
#	include <unistd.h>
#endif
#include <sstream>
#include <unordered_map>
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
static bool Friend_actor_is_eligible(uint32 npc_num, bool keeping = false);
// The picker's answer for "Spectate" (give the companion up).
constexpr int friend_pick_spectate = -4;
static int  Select_friend_actor(const std::vector<uint32>& roster, bool offer_watch = false);
static void Friend_mode_send_roster(int player);

// Creatures (monsters without an NPC number) are known to guests by a
// number the host gives them; see the creature sync section.
static uint32 Friend_creature_id(const Actor* actor) {
	const auto* creature = dynamic_cast<const Monster_actor*>(actor);
	return creature ? creature->get_friend_net_id() : 0;
}

static Monster_actor* Friend_creature_by_id(uint32 wanted) {
	if (wanted == 0) {
		return nullptr;
	}
	for (Monster_actor* creature = Monster_actor::get_first_in_world(); creature;
			 creature = creature->get_next_in_world()) {
		if (creature->get_friend_net_id() == wanted) {
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
static int    arg_friend_port = 1992;    // The year Ultima VII came out.
static bool friend_world_sent     = false;    // Host: the guest has our world, so keep it in step.
static string friend_player_name;    // Typed on Join (starts empty), or the Avatar's.
static Friend_mode_session friend_session;
static bool friend_host_dialogue_active = false;
// Guest: when the last conversation message came, and how long after it a
// move of the Avatar means the conversation's end was missed.
static uint32    friend_dialogue_event_at = 0;
constexpr uint32 friend_dialogue_stale_ms = 2000;
static int  friend_saved_attack_mode    = -1;    // The Avatar's, while targeting made it manual.
static std::vector<uint32> friend_sent_party;    // Party last sent to guests.
// Grace period before a companion left beyond the leash returns to the Avatar.
static constexpr uint32 friend_leash_grace_ms  = 5000;
// Guests beyond one per companion who may join to watch.
static constexpr int friend_max_spectators = 4;

// Host: the item a guest has picked up.  It is out of the world (for the
// host and everyone else) until the guest drops it or puts it back.
struct Friend_held_item {
	Game_object_shared obj;
	Game_object_weak   owner;    // Container it came from, if any.
	Tile_coord         tile;     // Or where it lay on the map.
	int                spot = -1;    // Equipment slot it came from.
};

// Host: one connected guest, by player number.
struct Friend_guest {
	std::string      name;
	int              actor = -1;    // Its companion's NPC number, or -1 (watching).
	Tile_coord       last_state{-1, -1, -1};    // Companion position last sent.
	int              last_frame      = -1;
	uint32           leash_return_at = 0;    // Ticks to bring it back, or 0.
	bool             roster_sent     = false;
	bool             journal_open    = false;    // Its journal is open.
	bool             journal_open_sent = false;  // Last "someone writing" sent.
	Friend_held_item held;
	int              attacker = -1;    // Companion fighting for it.
	Game_object_weak attack_foe;
	bool             following = false;    // The party's AI drives its companion.
	bool             menu_following = false;    // ...only while the guest is in a menu.
	uint32           move_seq  = 0;        // Number of its last step handled.
};
static std::map<int, Friend_guest> friend_guests;

// Host: a guest that drops keeps its companion for a while.  The party's AI
// drives it meanwhile, nobody else may pick it, and the same player name
// reconnecting gets it straight back.
static constexpr uint32 friend_rejoin_hold_ms = 3 * 60 * 1000;
struct Friend_rejoin_hold {
	int    actor;
	uint32 until;    // Ticks.
};
static std::map<std::string, Friend_rejoin_hold> friend_rejoin_holds;    // By player name.

static Friend_guest* Friend_guest_of(int player) {
	const auto found = friend_guests.find(player);
	return found == friend_guests.end() ? nullptr : &found->second;
}

// Host: the guest controlling this companion, if any.
static const Friend_guest* Friend_guest_with_actor(const Actor* actor) {
	for (const auto& [player, guest] : friend_guests) {
		if (actor && guest.actor >= 0 && guest.actor == actor->get_npc_num()) {
			return &guest;
		}
	}
	return nullptr;
}

// Guest: every player in the session (from the host's player list).
struct Friend_player {
	std::string name;
	int         actor     = -1;    // NPC number, or -1 (spectating).
	bool        following = false;    // The party's AI drives its companion.
};
static std::map<int, Friend_player> friend_players;
static std::vector<unsigned char>   friend_player_list_sent;    // Host.

// Guest: the player controlling this actor, by the host's list.
static std::string Friend_player_with_actor(const Actor* actor) {
	for (const auto& [player, entry] : friend_players) {
		if (actor && entry.actor >= 0 && entry.actor == actor->get_npc_num()) {
			return entry.name;
		}
	}
	return std::string();
}
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
// assignment for the sending guest; guests trust the host's NPC number.
static Actor* Friend_mode_remote_speaker(uint32 claimed, int player) {
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		const Friend_guest* guest = Friend_guest_of(player);
		return guest && guest->actor >= 0 ? gwin->get_npc(guest->actor) : nullptr;
	}
	return claimed == Friend_mode_session::chat_spectator ? nullptr : gwin->get_npc(static_cast<int>(claimed));
}

// A player's name for chat: the host knows its guests; guests use the list.
static std::string Friend_mode_player_name(int player) {
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		return friend_session.player_name(player);
	}
	const auto found = friend_players.find(player);
	return found != friend_players.end() ? found->second.name : friend_session.player_name(player);
}

// Chat log: the last few lines of player chat, whispers and session
// notices, bottom left, each fading after a while.  L hides or shows it.
// Kept to these, so it never fills up.
struct Friend_log_line {
	std::string text;
	uint32      time;
	bool        whisper;    // Shown in purple.
};
static std::deque<Friend_log_line> friend_chat_log;
static bool                        friend_chat_log_hidden  = false;
static bool                        friend_chat_log_changed = false;    // Its layer needs repainting.
static constexpr std::size_t       friend_chat_log_lines   = 6;
static constexpr uint32            friend_chat_log_ms      = 20000;

// Notices (and spectator chat or whispers while the log is hidden) near the
// top of the view, centred, for a few seconds each.
static std::deque<Friend_log_line> friend_notices;
static bool                        friend_notices_changed = false;
static constexpr std::size_t       friend_notice_lines    = 3;
static constexpr uint32            friend_notice_ms       = 4000;

static void Friend_mode_top_line(const std::string& text, bool whisper = false) {
	friend_notices.push_back({text, static_cast<uint32>(SDL_GetTicks()), whisper});
	while (friend_notices.size() > friend_notice_lines) {
		friend_notices.pop_front();
	}
	friend_notices_changed = true;
}

static void Friend_mode_log(const std::string& text, bool whisper = false) {
	friend_chat_log.push_back({text, static_cast<uint32>(SDL_GetTicks()), whisper});
	while (friend_chat_log.size() > friend_chat_log_lines) {
		friend_chat_log.pop_front();
	}
	friend_chat_log_changed = true;
}

// Emotes, Ultima Online style: "/wave" in the chat line.  The host poses
// the speaker's actor (guests' requests come as chat) and everyone sees
// "*waves*" over it, and "Marc waves." in the log.  Quick gestures end by
// themselves; kneeling and sitting last until the actor moves.
struct Friend_emote {
	const char* command;
	const char* text;    // Over the actor.
	const char* log;     // In the log, after the name.
	int         frame;
	uint32      ms;    // How long the pose lasts, or 0 until it moves.
};
static const Friend_emote friend_emotes[] = {
		{"/wave", "*waves*", "waves", Actor::raise1_frame, 1200},
		{"/bow", "*bows*", "bows", Actor::bow_frame, 1500},
		{"/point", "*points*", "points", Actor::reach1_frame, 1500},
		{"/cheer", "*cheers*", "cheers", Actor::up_frame, 1500},
		{"/kneel", "*kneels*", "kneels", Actor::kneel_frame, 0},
		{"/sit", "*sits down*", "sits down", Actor::sit_frame, 0},
};

static const Friend_emote* Friend_find_emote(const std::string& command) {
	for (const Friend_emote& emote : friend_emotes) {
		if (command == emote.command) {
			return &emote;
		}
	}
	return nullptr;
}

static void Friend_mode_show_chat(Actor* speaker, const std::string& sender, const std::string& text) {
	if (text.empty()) {
		return;
	}
	const std::string who  = sender.empty() ? std::string("Spectator") : sender;
	std::string       line = who + ": " + text;
	for (const Friend_emote& emote : friend_emotes) {
		if (text == emote.text) {    // An emote comes as its "*waves*".
			line = who + " " + emote.log + ".";
		}
	}
	Friend_mode_log(line);
	// Not over the speaker while the Avatar is in a conversation: it would
	// sit on top of the conversation.  The log has it.
	const bool in_dialogue = friend_host_dialogue_active || gwin->is_network_dialogue_paused();
	if (speaker && !in_dialogue) {
		gwin->get_effects()->add_chat_text(text.c_str(), speaker);
	} else if (friend_chat_log_hidden) {
		// Spectators have no body: with the log hidden, near the top.
		Friend_mode_top_line(line);
	}
}

// A whisper only the two players see: in the log (or near the top while
// the log is hidden).
static void Friend_mode_show_whisper(const std::string& line) {
	Friend_mode_log(line, true);
	if (friend_chat_log_hidden) {
		Friend_mode_top_line(line, true);
	}
}

// Session status shown near the top of the view, like spectator chat.
static void Friend_mode_notice(const std::string& text) {
	cout << "Friend Mode: " << text << endl;
	Friend_mode_top_line(text);
}

// A session notice (joins, leaves, companion changes): in the chat log, or
// near the top while the log is hidden; not both.
static void Friend_mode_session_notice(const std::string& text) {
	cout << "Friend Mode: " << text << endl;
	if (friend_chat_log_hidden) {
		Friend_mode_top_line(text);
	} else {
		Friend_mode_log(text);
	}
}

// Host: a notice every player sees, except 'except' (a newcomer has its
// own welcome).
static void Friend_mode_announce(const std::string& text, int except = -1) {
	Friend_mode_session_notice(text);
	if (friend_session.connected()) {
		const size_t length = std::min<size_t>(text.size(), 120);
		friend_session.send_blob(Friend_mode_session::Blob_kind::notice,
				std::vector<unsigned char>(text.begin(), text.begin() + length), Friend_mode_session::everyone, except);
	}
}

static std::string Friend_mode_actor_name(int npc_num) {
	Actor* actor = gwin->get_npc(npc_num);
	return actor ? actor->get_name() : std::string("a companion");
}

struct Friend_emote_pose {
	int    npc;
	int    frame;    // The frame set, with direction.
	uint32 until;
};
static std::vector<Friend_emote_pose> friend_emote_poses;    // Host: quick gestures to end.

static void Friend_mode_show_chat(Actor* speaker, const std::string& sender, const std::string& text);

// Host: pose 'actor' for 'player' and tell everyone.
static void Friend_mode_emote(Actor* actor, int player, const Friend_emote& emote) {
	const int frame = actor->get_dir_framenum(emote.frame);
	actor->change_frame(frame);
	if (emote.ms > 0) {
		friend_emote_poses.push_back({actor->get_npc_num(), frame, static_cast<uint32>(SDL_GetTicks()) + emote.ms});
	}
	Friend_mode_show_chat(actor, friend_session.player_name(player), emote.text);
	if (friend_session.connected()) {
		friend_session.send_chat(static_cast<uint32>(actor->get_npc_num()), player, emote.text);
	}
}

// The player a whisper is for: the longest known name that 'rest' starts
// with (names may have spaces), not counting case.  'message' gets the rest.
static int Friend_mode_find_player(const std::string& rest, std::string& message) {
	std::vector<std::pair<int, std::string>> known;
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		for (const auto& [player, guest] : friend_guests) {
			known.emplace_back(player, guest.name);
		}
	} else {
		for (const auto& [player, entry] : friend_players) {
			if (player != friend_session.local_player()) {
				known.emplace_back(player, entry.name);
			}
		}
	}
	auto lower = [](std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return text;
	};
	const std::string wanted = lower(rest);
	int               best   = -1;
	std::size_t       length = 0;
	for (const auto& [player, name] : known) {
		const std::string key = lower(name);
		if (!key.empty() && key.size() > length && wanted.compare(0, key.size(), key) == 0
			&& (wanted.size() == key.size() || wanted[key.size()] == ' ')) {
			best   = player;
			length = key.size();
		}
	}
	message = best < 0 ? std::string() : rest.substr(std::min(rest.size(), length + 1));
	return best;
}

static std::string Friend_mode_player_name(int player);
static Actor*      Friend_mode_local_speaker();
static void        Friend_mode_show_whisper(const std::string& line);

// Enter in the chat line: chat, or a command (/w name text, emotes).
static void Friend_mode_send_chat_line(const std::string& line) {
	Actor*     speaker = Friend_mode_local_speaker();
	const bool host    = friend_session.mode() == Friend_mode_session::Mode::host;
	const auto npc = speaker ? static_cast<uint32>(speaker->get_npc_num()) : Friend_mode_session::chat_spectator;
	if (line.empty() || line[0] != '/') {
		Friend_mode_show_chat(speaker, friend_player_name, line);
		friend_session.send_chat(npc, friend_session.local_player(), line);
		return;
	}
	const std::size_t space = line.find(' ');
	std::string       command = line.substr(0, space);
	std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	const std::string rest = space == std::string::npos ? std::string() : line.substr(space + 1);
	if (command == "/w" || command == "/whisper") {
		std::string message;
		const int   target = Friend_mode_find_player(rest, message);
		if (target < 0 || message.empty()) {
			Friend_mode_show_whisper("Whisper to whom? /w name text");
			return;
		}
		Friend_mode_show_whisper("[to " + Friend_mode_player_name(target) + "] " + message);
		if (host) {
			friend_session.send_whisper(target, npc, Friend_mode_session::host_player, message, target);
		} else {
			friend_session.send_whisper(target, npc, friend_session.local_player(), message);
		}
		return;
	}
	const Friend_emote* emote = Friend_find_emote(command);
	if (!emote) {
		Friend_mode_show_whisper("Try /w name text, /wave, /bow, /point, /cheer, /kneel or /sit");
	} else if (!speaker) {
		Friend_mode_show_whisper("Spectators have no one to move");
	} else if (host) {
		Friend_mode_emote(speaker, Friend_mode_session::host_player, *emote);
	} else {
		friend_session.send_chat(npc, friend_session.local_player(), command);    // The host poses us.
	}
}

// Tab in the chat line: complete the word being typed to a player's name
// (or, after "/", a command).  Tab again goes to the next match.
static std::string friend_complete_base;      // Text before the word.
static std::string friend_complete_prefix;    // What was typed of it.
static std::string friend_complete_result;    // The line after the last Tab.
static std::size_t friend_complete_index = 0;

static void Friend_mode_complete_chat(std::string& text) {
	if (text != friend_complete_result || friend_complete_result.empty()) {
		// A new completion: the word after the last space.
		const std::size_t start = text.rfind(' ') == std::string::npos ? 0 : text.rfind(' ') + 1;
		friend_complete_base   = text.substr(0, start);
		friend_complete_prefix = text.substr(start);
		friend_complete_index  = 0;
	} else {
		++friend_complete_index;    // The next match.
	}
	std::vector<std::string> names;
	if (!friend_complete_prefix.empty() && friend_complete_prefix[0] == '/') {
		names = {"/w"};
		for (const Friend_emote& emote : friend_emotes) {
			names.emplace_back(emote.command);
		}
	} else if (friend_session.mode() == Friend_mode_session::Mode::host) {
		for (const auto& [player, guest] : friend_guests) {
			names.push_back(guest.name);
		}
	} else {
		for (const auto& [player, entry] : friend_players) {
			if (player != friend_session.local_player()) {
				names.push_back(entry.name);
			}
		}
	}
	auto lower = [](std::string word) {
		std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return word;
	};
	const std::string        typed = lower(friend_complete_prefix);
	std::vector<std::string> matches;
	for (const std::string& name : names) {
		if (!name.empty() && lower(name).compare(0, typed.size(), typed) == 0) {
			matches.push_back(name);
		}
	}
	if (matches.empty()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		friend_complete_result.clear();
		return;
	}
	text = friend_complete_base + matches[friend_complete_index % matches.size()] + ' ';
	if (text.size() > 80) {
		text.resize(80);
	}
	friend_complete_result = text;
}

// Host, every frame: quick gestures go back to standing.
static void Friend_mode_end_emotes() {
	const uint32 now = static_cast<uint32>(SDL_GetTicks());
	for (auto it = friend_emote_poses.begin(); it != friend_emote_poses.end();) {
		if (static_cast<sint32>(now - it->until) < 0) {
			++it;
			continue;
		}
		Actor* actor = gwin->get_npc(it->npc);
		if (actor && actor->get_framenum() == it->frame) {    // Still posing.
			actor->change_frame(actor->get_dir_framenum(Actor::standing));
		}
		it = friend_emote_poses.erase(it);
	}
}

// Guest leash state, shown locally from the synced Avatar position.
enum class Friend_leash_zone { near, straying, beyond };
static Friend_leash_zone friend_leash_zone = Friend_leash_zone::near;

// While straying, an arrow at the screen edge points back to the Avatar.
// An arrow at the screen edge from 'from' toward 'to'.
static void Friend_mode_paint_arrow(const Tile_coord& from, const Tile_coord& to);

static void Friend_mode_paint_leash_arrow() {
	if (friend_session.mode() != Friend_mode_session::Mode::guest || friend_leash_zone == Friend_leash_zone::near) {
		return;
	}
	Actor*      actor  = gwin->get_controlled_actor();
	Main_actor* avatar = gwin->get_main_actor();
	if (!actor || actor == avatar) {
		return;
	}
	Friend_mode_paint_arrow(actor->get_tile(), avatar->get_tile());
}

static void Friend_mode_paint_arrow(const Tile_coord& from, const Tile_coord& to) {
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
	Image_buffer8* buffer = gwin->get_win()->get_ib8();
	arrow->paint_rle(buffer, x, y);
	gwin->set_painted();
}

// Each player's minimap color: a few clear ones from the palette, by player
// number.
static int Friend_player_color(int player) {
	static const int rgb[][3] = {{63, 12, 12}, {15, 30, 63}, {63, 58, 10}, {15, 58, 15},
			{58, 15, 58}, {10, 58, 58}, {63, 35, 5}, {63, 63, 63}};
	const auto&      c        = rgb[(player + 7) % 8];
	return gwin->get_pal()->find_color(c[0], c[1], c[2]);
}

// Where every player is, for the corner minimap: the Avatar, and each
// player's companion (just the Avatar in single player).
static void Friend_mode_paint_maps() {
	std::vector<Friend_map_dot> dots;
	auto add = [&](Actor* actor, int player) {
		if (!actor || actor->get_map() != gwin->get_main_actor()->get_map()) {
			return;
		}
		dots.push_back({actor->get_tile(), Friend_player_color(player)});
	};
	if (friend_session.mode() == Friend_mode_session::Mode::guest) {
		for (const auto& [player, entry] : friend_players) {
			add(entry.actor >= 0 ? gwin->get_npc(entry.actor) : nullptr, player);
		}
	} else {
		add(gwin->get_main_actor(), Friend_mode_session::host_player);
		for (const auto& [player, guest] : friend_guests) {
			add(guest.actor >= 0 ? gwin->get_npc(guest.actor) : nullptr, player);
		}
	}
	Friend_map_paint(dots);
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
	case 828:    // Secret walls: double-click slides them open (quality 0).
	case 845:
	case 787:    // Lever.
	case 788:    // Switch.
	case 252:    // Grandfather clocks: say the time.
	case 695:
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
	// Skip shapes or frames this game's data doesn't have; using them crashes.
	Shapes_vga_file& shapes = Shape_manager::get_instance()->get_shapes();
	if (shape < 0 || shape >= shapes.get_num_shapes() || frame >= shapes.get_num_frames(shape)) {
		cerr << "Friend Mode: no test shape " << shape << " frame " << frame << " in this game" << endl;
		return false;
	}
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

static void Friend_test_kit_fill(
		const Game_object_shared& container_obj, int shape, int frame, int quantity, int quality = -1) {
	Container_game_object* container = container_obj ? container_obj->as_container() : nullptr;
	Shapes_vga_file&       shapes    = Shape_manager::get_instance()->get_shapes();
	if (!container || shape >= shapes.get_num_shapes() || frame >= shapes.get_num_frames(shape)) {
		return;
	}
	const Game_object_shared item = Friend_test_kit_create(shape, frame);
	if (quality >= 0) {
		item->set_quality(quality);
	}
	if (quantity > 1) {
		item->modify_quantity(quantity - 1);
	}
	container->add(item.get(), true);
}

// A pair of linked teleporter pads: a 4x4 carpet with a party-footpad
// teleport egg (shape 275 frame 5, auto-reset) under every tile, each
// sending you to just south of the other pad.
static void Friend_test_kit_teleporters(int dx1, int dy1, int dx2, int dy2) {
	constexpr int      pad_shape = 190;    // Carpet, 4x4 and flat.
	Game_object_shared pads[2];
	if (!Friend_test_kit_place(pad_shape, 0, dx1, dy1, &pads[0])
		|| !Friend_test_kit_place(pad_shape, 0, dx2, dy2, &pads[1])) {
		return;
	}
	const Shape_info& info   = ShapeID::get_info(pad_shape);
	const int         xtiles = info.get_3d_xtiles(0);
	const int         ytiles = info.get_3d_ytiles(0);
	// Type teleport, criteria party_footpad, auto-reset.
	const unsigned short itype = Egg_object::teleport | (Egg_object::party_footpad << 4) | (1 << 15);
	for (int i = 0; i < 2; ++i) {
		pads[i]->clear_flag(Obj_flags::okay_to_take);
		const Tile_coord pad   = pads[i]->get_tile();    // Lower-right corner.
		const Tile_coord other = pads[1 - i]->get_tile();
		const Tile_coord dest(other.tx - xtiles / 2, other.ty + 2, other.tz);
		const int        schunk = (dest.ty / c_tiles_per_schunk) * 12 + dest.tx / c_tiles_per_schunk;
		const int        data1  = 255 | (schunk << 8);    // 255: jump to coordinates.
		const int        data2  = (dest.tx % c_tiles_per_schunk) | ((dest.ty % c_tiles_per_schunk) << 8);
		for (int x = 0; x < xtiles; ++x) {
			for (int y = 0; y < ytiles; ++y) {
				const Egg_object_shared egg = Egg_object::create_egg(
						false, 275, 5, 0, 0, 0, itype, 100, static_cast<short>(data1), static_cast<short>(data2),
						static_cast<short>(dest.tz));
				egg->move(pad.tx - x, pad.ty - y, pad.tz);
			}
		}
	}
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
	// Other door styles and portcullises, in a row to the south.
	Friend_test_kit_place(270, 8, -10, 10);     // Vertical door, style 2.
	Friend_test_kit_place(270, 16, -7, 10);     // Vertical door, style 3.
	Friend_test_kit_place(376, 8, -4, 10);      // Horizontal door, style 2.
	Friend_test_kit_place(376, 16, -1, 10);     // Horizontal door, style 3.
	Friend_test_kit_place(271, 0, 5, 10);       // Portcullis.
	Friend_test_kit_place(935, 0, 9, 10);       // Portcullis door.
	// Secret walls: these two wall shapes swap into each other when
	// double-clicked (Usecode 0x33C/0x34D -> 0x81F/0x820), quality 0.
	Friend_test_kit_place(828, 0, -6, 13);
	Friend_test_kit_place(845, 0, 0, 13);
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
	// Keys open the lock with the same quality; lockpicks may open either.
	constexpr int chest_lock = 77;
	constexpr int door_lock  = 78;
	Game_object_shared locked_chest;
	if (Friend_test_kit_place(522, 0, -9, 5, &locked_chest)) {    // Locked chest.
		locked_chest->set_quality(chest_lock);
		Friend_test_kit_fill(locked_chest, 644, 0, 50);    // Gold coins.
	}
	Game_object_shared picks_chest;
	if (Friend_test_kit_place(522, 0, -9, 8, &picks_chest)) {    // For lockpicks: no key.
		picks_chest->set_quality(79);
		Friend_test_kit_fill(picks_chest, 377, 0, 1);    // Food.
	}
	Game_object_shared locked_door;
	if (Friend_test_kit_place(270, 2, -10, -4, &locked_door)) {    // Locked vertical door.
		locked_door->set_quality(door_lock);
	}
	// Supplies: keys, lockpicks, potions (one of each kind) and food.
	Game_object_shared supplies;
	if (Friend_test_kit_place(802, 0, 5, 5, &supplies)) {
		Friend_test_kit_fill(supplies, 641, 0, 1, chest_lock);    // Key: locked chest.
		Friend_test_kit_fill(supplies, 641, 0, 1, door_lock);     // Key: locked door.
		Friend_test_kit_fill(supplies, 627, 0, 5);                // Lockpicks.
		for (int frame = 0; frame < 8; ++frame) {
			Friend_test_kit_fill(supplies, 340, frame, 1);    // Potions.
		}
		Friend_test_kit_fill(supplies, 377, 0, 3);    // Food.
		Friend_test_kit_fill(supplies, 823, 0, 1);    // Pigments.
		Friend_test_kit_fill(supplies, 823, 2, 1);    // Paint brush.
	}
	Friend_test_kit_place(837, 0, 10, 5);    // Easel, to paint on.
	// Magic, for the Class System's spell bar: a bag with 20 of each
	// reagent, every spell in the Avatar's spellbook (a new book in the bag
	// if it has none), and full mana.  The book still asks for the level of
	// each spell's circle.
	Game_object_shared reagents;
	if (Friend_test_kit_place(802, 0, 8, 7, &reagents)) {
		for (int frame = 0; frame < 8; ++frame) {
			Friend_test_kit_fill(reagents, 842, frame, 20);    // Reagents.
		}
		Main_actor*        avatar = gwin->get_main_actor();
		Spellbook_object*  book   = nullptr;
		Game_object_vector owned;
		avatar->get_objects(owned, c_any_shapenum, c_any_qual, c_any_framenum);
		for (Game_object* obj : owned) {
			if (!book) {
				book = obj->as_spellbook();
			}
		}
		constexpr int book_shape = 761;
		if (!book && ShapeID::get_info(book_shape).get_shape_class() == Shape_info::spellbook) {
			const Game_object_shared made = Friend_test_kit_create(book_shape, 0);
			if (Container_game_object* bag = reagents->as_container()) {
				bag->add(made.get(), true);
				book = made->as_spellbook();
			}
		}
		if (book) {
			for (int spell = 0; spell < 9 * 8; ++spell) {    // A full book: nine circles of eight.
				book->add_spell(spell);
			}
		}
		avatar->set_property(Actor::mana, avatar->get_property(Actor::magic));
	}
	// Teleporter pads west and east of the Avatar.
	Friend_test_kit_teleporters(-11, 1, 14, 1);
	gwin->set_all_dirty();
	Friend_mode_notice("Friend Mode test kit placed");
}

// Host debug aid (Shift+F9): creatures that appear after a guest joined,
// for creature sync.  Found by name, so they work in either game.
static void Friend_mode_spawn_test_creatures() {
	const char* wanted[] = {"rat", "wolf", "rabbit"};
	Shapes_vga_file& shapes = Shape_manager::get_instance()->get_shapes();
	int              made   = 0;
	for (std::size_t i = 0; i < std::size(wanted); ++i) {
		for (int shape = 150; shape < shapes.get_num_shapes(); ++shape) {
			const char* name = get_item_name(shape);
			if (!name || std::strcmp(name, wanted[i]) != 0 || !ShapeID::get_info(shape).is_npc()) {
				continue;
			}
			const Tile_coord avatar = gwin->get_main_actor()->get_tile();
			const Tile_coord want(avatar.tx + 4 + 3 * static_cast<int>(i), avatar.ty - 4, avatar.tz);
			const Tile_coord pos = Map_chunk::find_spot(want, 4, shape, 0, 1);
			if (pos.tx >= 0) {
				Monster_actor::create(shape, pos);
				++made;
			}
			break;
		}
	}
	gwin->set_all_dirty();
	Friend_mode_notice(made ? "Test creatures placed" : "No test creatures in this game");
}

// ---- Item sync (world sync W3) ------------------------------------------
// The host fingerprints the movable objects (and container contents) in the
// map chunks around the players.  When a chunk changes, it sends that chunk's
// whole object list; the guest keeps exact matches, deletes the rest and
// creates what is missing.  This catches every change whatever caused it.

namespace {
constexpr uint32 friend_item_sync_ms      = 250;
constexpr int    friend_item_avatar_chunks = 3;    // Radius around the Avatar.
constexpr int    friend_item_guest_chunks  = 3;    // Radius around the guest (past the 40-tile target range).
constexpr int    friend_item_max_depth     = 8;    // Container nesting limit.

void Friend_put8(std::vector<unsigned char>& out, int value) {
	out.push_back(static_cast<unsigned char>(value & 0xff));
}

void Friend_put16(std::vector<unsigned char>& out, int value) {
	Friend_put8(out, value);
	Friend_put8(out, value >> 8);
}

// One object as sent: top-level objects carry their tile within the chunk.
struct Friend_item {
	int                        shape = 0, frame = 0, tx = 0, ty = 0, tz = 0, quality = 0, flags = 0;
	std::vector<Friend_item>   children;
	std::vector<unsigned char> bytes;    // Serialized form, for matching.
};

enum Friend_item_flags { friend_item_okay_to_take = 1, friend_item_invisible = 2 };
}    // namespace

// A world object that item sync covers: movable (ireg), not an actor, egg or
// barge.
static bool Friend_item_synced(Game_object* obj) {
	return obj && !obj->as_actor() && !obj->is_egg() && !obj->as_barge() && dynamic_cast<Ireg_game_object*>(obj);
}

// The frame item sync sends: an animated object (a lit lamp post, a
// fountain) cycles its frames on each machine by itself, so it counts as
// the first frame of its cycle.  Otherwise its chunk would be resent at
// every pass.
static int Friend_item_frame(Game_object* obj) {
	const int frame  = obj->get_framenum();
	const int frames = obj->get_num_frames();
	if (!obj->get_info().is_animated() || frames <= 1) {
		return frame;
	}
	const int rotated = frame & (1 << 5);
	const int plain   = frame & ~(1 << 5);
	const int count   = obj->get_info().get_animation_info_safe(obj->get_shapenum(), frames)->get_frame_count();
	const int cycle   = count < 0 || count >= frames ? frames : count;
	return (plain - plain % cycle) | rotated;
}

static void Friend_put_item(std::vector<unsigned char>& out, Game_object* obj, bool top, int depth) {
	Friend_put16(out, obj->get_shapenum());
	Friend_put8(out, Friend_item_frame(obj));
	if (top) {
		Friend_put8(out, obj->get_tx());
		Friend_put8(out, obj->get_ty());
		Friend_put8(out, obj->get_lift());
	}
	Friend_put8(out, obj->get_quality());    // Also the count for stackables.
	Friend_put8(out, (obj->get_flag(Obj_flags::okay_to_take) ? friend_item_okay_to_take : 0)
							 | (obj->get_flag(Obj_flags::invisible) ? friend_item_invisible : 0));
	std::vector<std::vector<unsigned char>> children;
	Container_game_object* container = obj->as_container();
	if (container && depth < friend_item_max_depth) {
		Object_iterator next(container->get_objects());
		while (Game_object* child = next.get_next()) {
			children.emplace_back();
			Friend_put_item(children.back(), child, false, depth + 1);
		}
		// Order inside a container doesn't matter for matching.
		std::sort(children.begin(), children.end());
	}
	Friend_put16(out, static_cast<int>(children.size()));
	for (const auto& child : children) {
		out.insert(out.end(), child.begin(), child.end());
	}
}

// Serialized top-level objects of a chunk, sorted.
static std::vector<std::vector<unsigned char>> Friend_chunk_items(Map_chunk* chunk) {
	std::vector<std::vector<unsigned char>> items;
	Object_iterator                         next(chunk->get_objects());
	while (Game_object* obj = next.get_next()) {
		if (Friend_item_synced(obj)) {
			items.emplace_back();
			Friend_put_item(items.back(), obj, true, 0);
		}
	}
	std::sort(items.begin(), items.end());
	return items;
}

static uint64 Friend_hash_items(const std::vector<std::vector<unsigned char>>& items) {
	uint64 hash = 1469598103934665603ULL;    // FNV-1a.
	for (const auto& item : items) {
		for (unsigned char byte : item) {
			hash = (hash ^ byte) * 1099511628211ULL;
		}
		hash = (hash ^ 0xffU) * 1099511628211ULL;
	}
	return hash;
}

static std::unordered_map<int, uint64> friend_chunk_hashes;
static uint32                          friend_next_item_sync = 0;

// The map chunks (keys cy * c_num_chunks + cx) near the players that the
// host keeps guests in step with: items and creatures.
static std::unordered_set<int> Friend_sync_chunks() {
	Main_actor* avatar = gwin->get_main_actor();
	std::unordered_set<int> chunks;
	auto                    add_around = [&](Actor* actor, int radius) {
        if (!actor || actor->get_map() != avatar->get_map()) {
            return;
        }
        const int acx = actor->get_cx();
        const int acy = actor->get_cy();
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const int cx = (acx + dx + c_num_chunks) % c_num_chunks;
                const int cy = (acy + dy + c_num_chunks) % c_num_chunks;
                chunks.insert(cy * c_num_chunks + cx);
            }
        }
	};
	if (!avatar) {
		return chunks;
	}
	add_around(avatar, friend_item_avatar_chunks);
	for (const auto& each : gwin->get_remote_friends()) {
		add_around(each.actor, friend_item_guest_chunks);
	}
	return chunks;
}

// Host: fingerprint chunks near the players; send the ones that changed.
// With send false, only record the current state (right after the guest
// downloads the world, both sides match).
static void Friend_mode_item_pass(bool send) {
	Game_map*   map    = gwin->get_map();
	Main_actor* avatar = gwin->get_main_actor();
	if (!map || !avatar) {
		return;
	}
	const std::unordered_set<int> chunks = Friend_sync_chunks();
	for (int key : chunks) {
		const int cx = key % c_num_chunks;
		const int cy = key / c_num_chunks;
		if (!map->is_chunk_read(cx, cy)) {
			continue;
		}
		Map_chunk* chunk = map->get_chunk_unsafe(cx, cy);
		if (!chunk) {
			continue;
		}
		const auto   items = Friend_chunk_items(chunk);
		const uint64 hash  = Friend_hash_items(items);
		auto         found = friend_chunk_hashes.find(key);
		if (found == friend_chunk_hashes.end()) {
			// First sight: unchanged since the guest's copy was made.
			friend_chunk_hashes[key] = hash;
			continue;
		}
		if (found->second == hash) {
			continue;
		}
		found->second = hash;
		if (!send) {
			continue;
		}
		std::vector<unsigned char> payload;
		Friend_put16(payload, cx);
		Friend_put16(payload, cy);
		Friend_put16(payload, static_cast<int>(items.size()));
		for (const auto& item : items) {
			payload.insert(payload.end(), item.begin(), item.end());
		}
		friend_session.send_blob(Friend_mode_session::Blob_kind::chunk_objects, payload);
		cout << "Friend Mode: sent chunk " << cx << ',' << cy << " (" << items.size() << " objects)" << endl;
	}
}

// Parse one object; returns false on malformed data.
static bool Friend_read_item(
		const std::vector<unsigned char>& data, std::size_t& pos, bool top, int depth, Friend_item& item) {
	const std::size_t start  = pos;
	// shape(2) frame(1) [tx ty tz](3) quality(1) flags(1), then count(2).
	const std::size_t needed = top ? 8 : 5;
	if (depth > friend_item_max_depth || pos + needed + 2 > data.size()) {
		return false;
	}
	auto get8  = [&]() { return static_cast<int>(data[pos++]); };
	auto get16 = [&]() {
		const int lo = get8();
		return lo | (get8() << 8);
	};
	item.shape = get16();
	item.frame = get8();
	if (top) {
		item.tx = get8();
		item.ty = get8();
		item.tz = get8();
	}
	item.quality   = get8();
	item.flags     = get8();
	const int count = get16();
	for (int i = 0; i < count; ++i) {
		item.children.emplace_back();
		if (!Friend_read_item(data, pos, false, depth + 1, item.children.back())) {
			return false;
		}
	}
	item.bytes.assign(data.begin() + start, data.begin() + pos);
	return true;
}

static Game_object_shared Friend_create_item(const Friend_item& item) {
	Shapes_vga_file& shapes = Shape_manager::get_instance()->get_shapes();
	if (item.shape >= shapes.get_num_shapes()) {
		return nullptr;
	}
	const Game_object_shared obj = gwin->get_map()->create_ireg_object(
			ShapeID::get_info(item.shape), item.shape, item.frame, 0, 0, 0);
	obj->set_quality(item.quality);
	if (item.flags & friend_item_okay_to_take) {
		obj->set_flag(Obj_flags::okay_to_take);
	} else {
		obj->clear_flag(Obj_flags::okay_to_take);
	}
	if (item.flags & friend_item_invisible) {
		obj->set_flag(Obj_flags::invisible);
	}
	if (Container_game_object* container = obj->as_container()) {
		for (const Friend_item& child : item.children) {
			if (Game_object_shared made = Friend_create_item(child)) {
				container->add(made.get(), true);
			}
		}
	}
	return obj;
}

// Delete an object and its contents without spilling them, closing any
// window open on it first.
static void Friend_delete_item(Game_object* obj) {
	Gump_manager* gumps = gwin->get_gump_man();
	while (Gump* gump = gumps->find_gump(obj, c_any_shapenum)) {
		gumps->close_gump(gump);
	}
	if (Container_game_object* container = obj->as_container()) {
		std::vector<Game_object*> contents;
		Object_iterator           next(container->get_objects());
		while (Game_object* child = next.get_next()) {
			contents.push_back(child);
		}
		for (Game_object* child : contents) {
			Friend_delete_item(child);
		}
	}
	obj->remove_this();
}

// The same object as 'item', ignoring contents (and, at top level, on the
// same tile)?
static bool Friend_same_object(Game_object* obj, const Friend_item& item, bool top) {
	const int flags = (obj->get_flag(Obj_flags::okay_to_take) ? friend_item_okay_to_take : 0)
					  | (obj->get_flag(Obj_flags::invisible) ? friend_item_invisible : 0);
	if (obj->get_shapenum() != item.shape || obj->get_framenum() != item.frame || obj->get_quality() != item.quality
		|| flags != item.flags) {
		return false;
	}
	return !top || (obj->get_tx() == item.tx && obj->get_ty() == item.ty && obj->get_lift() == item.tz);
}

// Make a container's contents match 'wanted' in place: keep exact matches,
// reconcile nested containers that are the same object, delete the rest and
// create what is missing.  Open windows on kept containers stay open.
static void Friend_reconcile_contents(Container_game_object* container, const std::vector<Friend_item>& wanted, int depth) {
	std::vector<std::pair<Game_object*, std::vector<unsigned char>>> current;
	Object_iterator                                                  next(container->get_objects());
	while (Game_object* child = next.get_next()) {
		current.emplace_back(child, std::vector<unsigned char>());
		Friend_put_item(current.back().second, child, false, depth);
	}
	std::vector<bool> kept(current.size(), false);
	std::vector<bool> done(wanted.size(), false);
	for (std::size_t w = 0; w < wanted.size(); ++w) {    // Exact matches.
		for (std::size_t i = 0; i < current.size(); ++i) {
			if (!kept[i] && current[i].second == wanted[w].bytes) {
				kept[i] = done[w] = true;
				break;
			}
		}
	}
	for (std::size_t w = 0; w < wanted.size(); ++w) {    // Same container, new contents.
		for (std::size_t i = 0; !done[w] && i < current.size(); ++i) {
			Container_game_object* inner = current[i].first->as_container();
			if (!kept[i] && inner && Friend_same_object(current[i].first, wanted[w], false)) {
				Friend_reconcile_contents(inner, wanted[w].children, depth + 1);
				kept[i] = done[w] = true;
			}
		}
	}
	for (std::size_t i = 0; i < current.size(); ++i) {
		if (!kept[i]) {
			Friend_delete_item(current[i].first);
		}
	}
	for (std::size_t w = 0; w < wanted.size(); ++w) {
		if (!done[w]) {
			if (Game_object_shared made = Friend_create_item(wanted[w])) {
				container->add(made.get(), true);
			}
		}
	}
}

// Guest: make a chunk's movable objects match the host's list.
static void Friend_mode_apply_chunk(const std::vector<unsigned char>& data) {
	if (data.size() < 6) {
		return;
	}
	std::size_t pos   = 0;
	const int   cx    = data[0] | (data[1] << 8);
	const int   cy    = data[2] | (data[3] << 8);
	const int   count = data[4] | (data[5] << 8);
	pos               = 6;
	if (cx >= c_num_chunks || cy >= c_num_chunks) {
		return;
	}
	std::vector<Friend_item> wanted(static_cast<std::size_t>(count));
	for (Friend_item& item : wanted) {
		if (!Friend_read_item(data, pos, true, 0, item)) {
			cerr << "Friend Mode: malformed chunk " << cx << ',' << cy << endl;
			return;
		}
	}
	Game_map* map = gwin->get_map();
	map->ensure_chunk_read(cx, cy);
	Map_chunk* chunk = map->get_chunk(cx, cy);
	// Current objects with their serialized form.
	std::vector<std::pair<Game_object*, std::vector<unsigned char>>> current;
	Object_iterator                                                  next(chunk->get_objects());
	while (Game_object* obj = next.get_next()) {
		if (Friend_item_synced(obj)) {
			current.emplace_back(obj, std::vector<unsigned char>());
			Friend_put_item(current.back().second, obj, true, 0);
		}
	}
	std::vector<bool> kept(current.size(), false);
	std::vector<const Friend_item*> missing;
	for (const Friend_item& item : wanted) {
		bool matched = false;
		for (std::size_t i = 0; i < current.size(); ++i) {
			if (!kept[i] && current[i].second == item.bytes) {
				kept[i] = matched = true;
				break;
			}
		}
		// Same container whose contents changed: update it in place so an
		// open window on it stays open.
		for (std::size_t i = 0; !matched && i < current.size(); ++i) {
			Container_game_object* container = current[i].first->as_container();
			if (!kept[i] && container && Friend_same_object(current[i].first, item, true)) {
				Friend_reconcile_contents(container, item.children, 1);
				kept[i] = matched = true;
			}
		}
		if (!matched) {
			missing.push_back(&item);
		}
	}
	int removed = 0;
	for (std::size_t i = 0; i < current.size(); ++i) {
		if (!kept[i]) {
			Friend_delete_item(current[i].first);
			++removed;
		}
	}
	for (const Friend_item* item : missing) {
		if (Game_object_shared obj = Friend_create_item(*item)) {
			obj->move(cx * c_tiles_per_chunk + item->tx, cy * c_tiles_per_chunk + item->ty, item->tz);
		}
	}
	gwin->set_all_dirty();    // Contents may have changed in place too.
	if (removed || !missing.empty()) {
		cout << "Friend Mode: chunk " << cx << ',' << cy << ": removed " << removed << ", added " << missing.size()
			 << endl;
	}
}

// ---- Party inventories and guest item moves (world sync W4) -------------

static std::unordered_map<int, uint64> friend_inventory_hashes;

// Serialized inventory entries: equipment slot (255 = none), then the item.
static std::vector<std::vector<unsigned char>> Friend_inventory_items(Actor* actor) {
	std::vector<std::vector<unsigned char>> items;
	Object_iterator                         next(actor->get_objects());
	while (Game_object* obj = next.get_next()) {
		items.emplace_back();
		const int spot = actor->find_readied(obj);
		Friend_put8(items.back(), spot < 0 ? 255 : spot);
		Friend_put_item(items.back(), obj, false, 0);
	}
	std::sort(items.begin(), items.end());
	return items;
}

// Host: send party inventories that changed (first sight is the baseline).
static void Friend_mode_inventory_pass(bool send) {
	std::vector<Actor*> party_actors{gwin->get_main_actor()};
	Party_manager*      party = gwin->get_party_man();
	for (int i = 0; i < party->get_count(); ++i) {
		party_actors.push_back(gwin->get_npc(party->get_member(i)));
	}
	for (Actor* actor : party_actors) {
		if (!actor) {
			continue;
		}
		const auto   items = Friend_inventory_items(actor);
		const uint64 hash  = Friend_hash_items(items);
		const int    npc   = actor->get_npc_num();
		auto         found = friend_inventory_hashes.find(npc);
		if (found == friend_inventory_hashes.end()) {
			friend_inventory_hashes[npc] = hash;
			continue;
		}
		if (found->second == hash) {
			continue;
		}
		found->second = hash;
		if (!send) {
			continue;
		}
		std::vector<unsigned char> payload;
		Friend_put16(payload, npc);
		Friend_put16(payload, static_cast<int>(items.size()));
		for (const auto& item : items) {
			payload.insert(payload.end(), item.begin(), item.end());
		}
		friend_session.send_blob(Friend_mode_session::Blob_kind::actor_inventory, payload);
		cout << "Friend Mode: sent inventory of " << actor->get_name() << " (" << items.size() << " items)" << endl;
	}
}

// Guest: make a party member's inventory match the host's.
static void Friend_mode_apply_inventory(const std::vector<unsigned char>& data) {
	if (data.size() < 4) {
		return;
	}
	const int npc   = data[0] | (data[1] << 8);
	const int count = data[2] | (data[3] << 8);
	Actor*    actor = gwin->get_npc(npc);
	if (!actor) {
		return;
	}
	std::size_t                                    pos = 4;
	std::vector<std::pair<int, Friend_item>>       wanted;
	std::vector<std::vector<unsigned char>>        wanted_bytes;
	for (int i = 0; i < count; ++i) {
		if (pos >= data.size()) {
			cerr << "Friend Mode: malformed inventory for NPC " << npc << endl;
			return;
		}
		const std::size_t start = pos;
		const int         spot  = data[pos++];
		Friend_item       item;
		if (!Friend_read_item(data, pos, false, 0, item)) {
			cerr << "Friend Mode: malformed inventory for NPC " << npc << endl;
			return;
		}
		wanted.emplace_back(spot, std::move(item));
		wanted_bytes.emplace_back(data.begin() + start, data.begin() + pos);
	}
	std::vector<std::pair<Game_object*, std::vector<unsigned char>>> current;
	Object_iterator                                                  next(actor->get_objects());
	while (Game_object* obj = next.get_next()) {
		current.emplace_back(obj, std::vector<unsigned char>());
		const int spot = actor->find_readied(obj);
		Friend_put8(current.back().second, spot < 0 ? 255 : spot);
		Friend_put_item(current.back().second, obj, false, 0);
	}
	std::vector<bool> kept(current.size(), false);
	std::vector<std::size_t> missing;
	for (std::size_t w = 0; w < wanted.size(); ++w) {
		bool matched = false;
		for (std::size_t i = 0; i < current.size(); ++i) {
			if (!kept[i] && current[i].second == wanted_bytes[w]) {
				kept[i] = matched = true;
				break;
			}
		}
		// The same pack or bag in the same slot with new contents: update in
		// place so an open window on it stays open.
		for (std::size_t i = 0; !matched && i < current.size(); ++i) {
			Game_object*           obj       = current[i].first;
			Container_game_object* container = obj->as_container();
			const int              spot      = actor->find_readied(obj);
			if (!kept[i] && container && (spot < 0 ? 255 : spot) == wanted[w].first
				&& Friend_same_object(obj, wanted[w].second, false)) {
				Friend_reconcile_contents(container, wanted[w].second.children, 1);
				kept[i] = matched = true;
			}
		}
		if (!matched) {
			missing.push_back(w);
		}
	}
	int removed = 0;
	for (std::size_t i = 0; i < current.size(); ++i) {
		if (!kept[i]) {
			Friend_delete_item(current[i].first);
			++removed;
		}
	}
	for (std::size_t w : missing) {
		Game_object_shared obj = Friend_create_item(wanted[w].second);
		if (!obj) {
			continue;
		}
		const int spot = wanted[w].first;
		if (spot == 255 || !actor->add_readied(obj.get(), spot, true, true)) {
			actor->add(obj.get(), true);
		}
	}
	gwin->set_all_dirty();    // Contents may have changed in place too.
	if (removed || !missing.empty()) {
		cout << "Friend Mode: inventory of " << actor->get_name() << ": removed " << removed << ", added "
			 << missing.size() << endl;
	}
}

// Where an object is, as a path from a root: kind 0 = an object on the map
// (tile, then the object), 1 = an actor's inventory (NPC number), 2 = a bare
// map tile (drop destination).  Each step is shape, frame and quality.
enum Friend_path_kind { friend_path_world = 0, friend_path_actor = 1, friend_path_tile = 2, friend_path_held = 3 };

static bool Friend_put_path(std::vector<unsigned char>& out, Game_object* obj) {
	std::vector<Game_object*> chain;
	for (Game_object* cur = obj; cur; cur = cur->get_owner()) {
		chain.push_back(cur);
		if (chain.size() > static_cast<std::size_t>(friend_item_max_depth)) {
			return false;
		}
	}
	std::reverse(chain.begin(), chain.end());    // Outermost first.
	Actor* root_actor = chain.front()->as_actor();
	if (root_actor) {
		if (root_actor->get_npc_num() <= 0 && root_actor != gwin->get_main_actor()) {
			return false;    // Only numbered NPCs (and the Avatar) can be named.
		}
		Friend_put8(out, friend_path_actor);
		Friend_put16(out, root_actor->get_npc_num());
		chain.erase(chain.begin());
	} else {
		const Tile_coord t = chain.front()->get_tile();
		Friend_put8(out, friend_path_world);
		Friend_put16(out, t.tx);
		Friend_put16(out, t.ty);
		Friend_put8(out, t.tz);
	}
	Friend_put8(out, static_cast<int>(chain.size()));
	for (Game_object* step : chain) {
		Friend_put16(out, step->get_shapenum());
		Friend_put8(out, step->get_framenum());
		Friend_put8(out, step->get_quality());
	}
	return true;
}

static void Friend_put_tile_path(std::vector<unsigned char>& out, const Tile_coord& t) {
	Friend_put8(out, friend_path_tile);
	Friend_put16(out, t.tx);
	Friend_put16(out, t.ty);
	Friend_put8(out, t.tz);
	Friend_put8(out, 0);
}

// Resolve a path.  For a bare tile, returns nullptr and sets 'tile'.
static bool Friend_read_path(
		const std::vector<unsigned char>& data, std::size_t& pos, Game_object*& found, Tile_coord& tile) {
	found = nullptr;
	if (pos >= data.size()) {
		return false;
	}
	auto get8  = [&]() { return pos < data.size() ? static_cast<int>(data[pos++]) : -1; };
	auto get16 = [&]() {
		const int lo = get8();
		return lo | (get8() << 8);
	};
	const int kind = get8();
	Game_object* cur = nullptr;
	if (kind == friend_path_actor) {
		const int npc = get16();
		cur           = npc == 0 ? static_cast<Actor*>(gwin->get_main_actor()) : gwin->get_npc(npc);
		if (!cur) {
			return false;
		}
	} else if (kind == friend_path_world || kind == friend_path_tile) {
		tile.tx = get16();
		tile.ty = get16();
		tile.tz = get8();
	} else {
		return false;
	}
	const int steps = get8();
	if (steps < 0 || pos + static_cast<std::size_t>(steps) * 4 > data.size()) {
		return false;
	}
	for (int i = 0; i < steps; ++i) {
		const int shape   = get16();
		const int frame   = get8();
		const int quality = get8();
		Game_object* next_obj = nullptr;
		if (!cur) {    // First step of a map path: the object on that tile.
			Game_object_vector candidates;
			Game_object::find_nearby(candidates, tile, shape, 0, 0xb0, quality, frame);
			for (Game_object* obj : candidates) {
				if (obj->get_tile() == tile && !obj->get_owner()) {
					next_obj = obj;
					break;
				}
			}
		} else if (Container_game_object* container = cur->as_container()) {
			Object_iterator next(container->get_objects());
			while (Game_object* child = next.get_next()) {
				if (child->get_shapenum() == shape && child->get_framenum() == frame && child->get_quality() == quality) {
					next_obj = child;
					break;
				}
			}
		}
		if (!next_obj) {
			return false;
		}
		cur = next_obj;
	}
	found = cur;
	return kind == friend_path_tile || found != nullptr;
}

static bool friend_guest_lifted = false;    // Guest: the host holds our dragged item.

void Friend_mode_note_drag_start(Game_object* obj) {
	friend_guest_lifted = false;
	if (!obj || friend_session.mode() != Friend_mode_session::Mode::guest || !gwin->has_network_friend_control()
		|| !friend_session.connected()) {
		return;
	}
	std::vector<unsigned char> source;
	if (Friend_put_path(source, obj)) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::guest_lift, source);
		friend_guest_lifted = true;
	}
}

void Friend_mode_note_drag_put_back() {
	if (friend_guest_lifted && friend_session.connected()) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::guest_put_back, {});
	}
	friend_guest_lifted = false;
}

void Friend_mode_note_drag_drop(Game_object* dropped, int quantity, Game_object* gump_container, bool whole) {
	if (!dropped || !friend_guest_lifted || !friend_session.connected()) {
		return;
	}
	// The source is the item the host is holding for us.
	std::vector<unsigned char> payload{friend_path_held, 0};
	Container_game_object*     owner = dropped->get_owner();
	if (!owner && !dropped->get_chunk()) {
		// Merged into a matching stack: it went into the window's container.
		owner = gump_container ? gump_container->as_container() : nullptr;
		if (!owner) {
			Friend_mode_note_drag_put_back();    // Unknown destination.
			return;
		}
	}
	if (owner) {
		if (!Friend_put_path(payload, owner)) {
			Friend_mode_note_drag_put_back();
			return;
		}
		Actor*    actor_owner = owner->as_actor();
		const int spot        = actor_owner ? actor_owner->find_readied(dropped) : -1;
		Friend_put8(payload, spot < 0 ? 255 : spot);
	} else {
		Friend_put_tile_path(payload, dropped->get_tile());
		Friend_put8(payload, 255);
	}
	Friend_put16(payload, std::max(1, quantity));
	friend_session.send_blob(Friend_mode_session::Blob_kind::guest_move, payload);
	if (whole) {
		friend_guest_lifted = false;    // Otherwise the rest is put back.
	}
}

// Host: may the guest's companion handle things inside or at 'obj'?  Its own
// inventory, or something on the map within reach.  Not other people's packs.
static bool Friend_guest_may_touch(Actor* actor, Game_object* obj) {
	Game_object* outer = obj->get_outermost();
	if (Actor* holder = outer->as_actor()) {
		return holder == actor;
	}
	return Fast_pathfinder_client::is_grabable(actor, outer);
}

// Make the next passes resend everything near the guest and its inventory,
// so a guest whose own move was refused or changed snaps back to the truth.
static void Friend_force_item_resync(Actor* actor) {
	for (auto& entry : friend_chunk_hashes) {
		entry.second = 0;
	}
	if (actor) {
		friend_inventory_hashes[actor->get_npc_num()] = 0;
	}
	friend_next_item_sync = 0;
}

// Return the held item (or what is left of it) to where it came from.
// Put a held item where it came from: its container (its slot, for a
// party member's equipment) or its tile.
static void Friend_place_held_item(const Friend_held_item& friend_guest_held, const Game_object_shared& obj) {
	const Game_object_shared owner    = friend_guest_held.owner.lock();
	Container_game_object*   container = owner ? owner->as_container() : nullptr;
	bool                     placed    = false;
	if (container) {
		Actor* holder = container->as_actor();
		placed        = holder && friend_guest_held.spot >= 0
						 && holder->add_readied(obj.get(), friend_guest_held.spot, true, true);
		if (!placed) {
			placed = container->add(obj.get(), true);
		}
	}
	if (!placed) {
		obj->move(friend_guest_held.tile);
	}
}

static void Friend_return_held_item(Friend_held_item& friend_guest_held) {
	if (!friend_guest_held.obj) {
		return;
	}
	const Game_object_shared obj = std::move(friend_guest_held.obj);
	Friend_place_held_item(friend_guest_held, obj);
	friend_guest_held = Friend_held_item();
	gwin->set_all_dirty();
}

// Host: a save is being written.  Items guests hold are out of the world,
// so each goes back where it came from for the save ('before'), and into
// the guest's hand again afterwards.  Otherwise a save made mid-drag (an
// autosave, or a joining guest's world) loses the item.
void Friend_mode_saving(bool before) {
	Friend_class_saving(before);
	// Targeting's manual attack mode is for this fight only: saves keep the
	// Avatar's own.
	static bool swapped = false;
	Main_actor* avatar  = gwin->get_main_actor();
	if (before && avatar && friend_saved_attack_mode >= 0 && avatar->get_attack_mode() == Actor::manual) {
		avatar->set_attack_mode(static_cast<Actor::Attack_mode>(friend_saved_attack_mode), avatar->did_user_set_attack());
		swapped = true;
	} else if (!before && swapped && avatar) {
		avatar->set_attack_mode(Actor::manual, avatar->did_user_set_attack());
		swapped = false;
	}
	for (auto& [player, guest] : friend_guests) {
		Friend_held_item& held = guest.held;
		if (!held.obj) {
			continue;
		}
		if (before) {
			Friend_place_held_item(held, held.obj);
		} else if (Game_object* obj = held.obj.get(); obj->get_owner() || !obj->is_pos_invalid()) {
			obj->remove_this(&held.obj);
		}
	}
}

// Host: a guest picked an item up.
static void Friend_mode_guest_lift(
		const std::vector<unsigned char>& data, Actor* actor, Friend_held_item& friend_guest_held) {
	Friend_return_held_item(friend_guest_held);    // Only one item in hand at a time.
	std::size_t  pos = 0;
	Game_object* src = nullptr;
	Tile_coord   tile;
	if (!actor || !Friend_read_path(data, pos, src, tile) || !src || src->as_actor()
		|| !Friend_guest_may_touch(actor, src)) {
		cout << "Friend Mode: refused guest pick-up (not found, out of reach or not theirs)" << endl;
		Friend_force_item_resync(actor);
		return;
	}
	Container_game_object* owner = src->get_owner();
	if (owner) {
		friend_guest_held.owner = weak_from_obj(owner);
		Actor* holder           = owner->as_actor();
		friend_guest_held.spot  = holder ? holder->find_readied(src) : -1;
	}
	friend_guest_held.tile = src->get_tile();
	src->remove_this(&friend_guest_held.obj);
	gwin->set_all_dirty();
	cout << "Friend Mode: guest picked up shape " << friend_guest_held.obj->get_shapenum() << endl;
}

// Host: carry out a guest's item move after checking it.
static void Friend_mode_perform_guest_move(
		const std::vector<unsigned char>& data, Actor* actor, Friend_held_item& friend_guest_held) {
	auto refuse = [&](const char* why) {
		cout << "Friend Mode: refused guest item move (" << why << ')' << endl;
		Friend_force_item_resync(actor);
	};
	if (!actor) {
		return refuse("no companion");
	}
	std::size_t  pos = 0;
	Game_object* src = nullptr;
	Game_object* dst = nullptr;
	Tile_coord   src_tile;
	Tile_coord   dst_tile;
	const bool   held = data.size() >= 2 && data[0] == friend_path_held;
	if (held) {
		src = friend_guest_held.obj.get();
		pos = 2;
		if (!src) {
			return refuse("not holding that item (someone else took it)");
		}
	} else if (!Friend_read_path(data, pos, src, src_tile) || !src) {
		return refuse("source not found");
	}
	const bool dst_ok = Friend_read_path(data, pos, dst, dst_tile);
	if (!dst_ok || pos + 3 > data.size()) {
		Friend_return_held_item(friend_guest_held);
		return refuse("destination not found");
	}
	const int spot     = data[pos];
	const int quantity = data[pos + 1] | (data[pos + 2] << 8);
	// A refused drop of a held item goes back where it came from.
	auto refuse_drop = [&](const char* why) {
		if (held) {
			Friend_return_held_item(friend_guest_held);
		}
		refuse(why);
	};
	if (!held && (src->as_actor() || !Friend_guest_may_touch(actor, src))) {
		return refuse("source out of reach or not yours");
	}
	Container_game_object* container = dst ? dst->as_container() : nullptr;
	if (dst && !container) {
		return refuse_drop("destination is not a container");
	}
	if (container) {
		if (!Friend_guest_may_touch(actor, container)) {
			return refuse_drop("destination out of reach or not yours");
		}
		for (Game_object* up = container; up; up = up->get_owner()) {
			if (up == src) {
				return refuse_drop("container into itself");
			}
		}
		// Party members can carry only so much.
		Game_object* holder = container->get_outermost();
		if (holder->get_flag(Obj_flags::in_party)
			&& (holder->get_weight() + src->get_weight()) / 10 > holder->get_max_weight()) {
			return refuse_drop("too heavy");
		}
	} else if (actor->get_tile().distance(dst_tile) > 8 || dst_tile.tz > actor->get_lift() + 5) {
		return refuse_drop("drop spot out of reach");
	}
	// Lift the item, splitting a stack if only part moves.
	Container_game_object* src_owner = src->get_owner();
	const Tile_coord       src_at    = src->get_tile();
	// Where it really came from (a held item is out of the world already).
	const Game_object_shared held_from = held ? friend_guest_held.owner.lock() : nullptr;
	Game_object* const     origin_owner = held ? held_from.get() : src_owner;
	const Tile_coord       origin_at    = held ? friend_guest_held.tile : src_at;
	const bool             okay_to_move = src->get_flag(Obj_flags::okay_to_take);
	Game_object_shared     moving;
	Friend_held_item       held_origin;
	const bool             split     = quantity < src->get_quantity();
	if (split) {
		moving = gwin->get_map()->create_ireg_object(src->get_shapenum(), src->get_framenum());
		moving->modify_quantity(quantity - 1);
		if (src->get_flag(Obj_flags::okay_to_take)) {
			moving->set_flag(Obj_flags::okay_to_take);
		}
		src->modify_quantity(-quantity);
	} else if (held) {    // The whole item; keep its origin until it lands.
		held_origin       = std::move(friend_guest_held);
		friend_guest_held = Friend_held_item();
		moving            = held_origin.obj;
	} else {
		src->remove_this(&moving);
	}
	bool placed = false;
	if (container) {
		Actor* holder_actor = container->as_actor();
		placed = (holder_actor && spot != 255) ? holder_actor->add_readied(moving.get(), spot)
											   : container->add(moving.get(), false, true);
	} else {
		moving->move(dst_tile);
		placed = true;
	}
	if (!placed) {    // Put it back where it was.
		if (split) {
			src->modify_quantity(quantity);
			if (held) {
				Friend_return_held_item(friend_guest_held);
			}
		} else if (held) {
			friend_guest_held = std::move(held_origin);
			Friend_return_held_item(friend_guest_held);
		} else if (src_owner) {
			src_owner->add(moving.get(), true);
		} else {
			moving->move(src_at);
		}
		return refuse("no room");
	}
	gwin->set_all_dirty();
	cout << "Friend Mode: moved shape " << moving->get_shapenum() << " for guest" << endl;
	// Theft, as for the Avatar's drags: an item not okay to take, from
	// outside the party, that actually moved (into another container, or
	// more than 2 tiles), outside dungeons.  Witnesses look at the guest.
	Game_object* const from_holder = origin_owner ? origin_owner->get_outermost() : nullptr;
	const bool         from_party  = from_holder && from_holder->get_flag(Obj_flags::in_party);
	const bool         moved       = container ? container != origin_owner : dst_tile.distance(origin_at) > 2;
	if (!okay_to_move && !from_party && moved && !gwin->is_in_dungeon()) {
		gwin->theft(actor);
	}
	// The guest already moved it locally; resend in case the result differs
	// (stacking, placement inside the container).
	Friend_force_item_resync(actor);
}

// ---- Guest item use -------------------------------------------------------
// Food, potions, keys and lockpicks.  Their Usecode asks for a target click
// (food 0x179 -> 0x813, potion 0x154, lockpick 0x273, key 0x281), so the
// host runs it with that click already answered: the guest's own companion
// for food and potions, or the door or chest the guest picked.

bool Friend_mode_guest_local_shape(int shape) {
	if (!GAME_BG) {
		return false;    // Serpent Isle shapes not reviewed yet.
	}
	// Each script (and everything it calls) was checked: it only shows
	// something, so it runs on the guest's own copy and the host never sees
	// it.  Books and scrolls are here too; the few that set a story flag
	// when read still need the Avatar to read them for the story to count.
	switch (shape) {
	case 178:    // Cloth map.
	case 379:    // Sign.
	case 715:    // Tombstone.
	case 642:    // Book.
	case 797:    // Scroll.
	case 744:    // Telescope.
	case 650:    // Sextant: says where you are.
	case 284:    // Sundial: says the time.
	case 770:    // Orrery viewer.
	case 729:    // Crystal ball.
	case 944:    // Pot: a remark.
	case 742:    // Stuffed toy: a remark.
	case 837:    // Easel: a remark (paint on it with brush and pigments).
	case 644:    // Gold coin: "Call it." heads or tails (closes windows).
		return true;
	default:
		return false;
	}
}

Friend_item_use Friend_mode_guest_item_use(int shape) {
	if (!GAME_BG) {
		return Friend_item_use::none;    // Serpent Isle shapes not reviewed yet.
	}
	// Each of these scripts was checked (Black Gate usecode): it only
	// changes the item or its user, with no story flags, conversation or
	// windows (which would open on the host's screen, where scripts run).
	switch (shape) {
	case 377:    // Food.
	case 340:    // Potion.
	case 616:    // Bottle (drinks): the food script.
	case 595:    // Torch: lights it.
	case 701:    // Lit torch: puts it out.
	case 159:    // Pocketwatch: says the time.
		return Friend_item_use::on_self;
	case 641:    // Key.
	case 627:    // Lockpick.
		return Friend_item_use::on_target;
	case 823:    // Artist's equipment: the brush paints (two clicks, see
				 // Friend_mode_guest_use_item); the pigments only get a remark.
		return Friend_item_use::on_self;
	default:
		return Friend_item_use::none;
	}
}

// Black Gate artist's equipment (shape 823): frames 0-1 are pigments, the
// rest the brush.  Its script: brush, then click the pigments, then an easel.
static bool Friend_is_paint_brush(const Game_object* obj) {
	return GAME_BG && obj->get_shapenum() == 823 && obj->get_framenum() >= 2;
}

static bool Friend_is_pigments(const Game_object* obj) {
	return GAME_BG && obj->get_shapenum() == 823 && obj->get_framenum() < 2;
}

bool Friend_mode_guest_use_item(Game_object* item) {
	Actor* own = gwin->get_controlled_actor();
	if (!own) {
		return false;
	}
	if (Friend_is_paint_brush(item)) {
		Game_object* pigments = Friend_mode_pick_item_target();
		Game_object* easel    = pigments ? Friend_mode_pick_item_target() : nullptr;
		if (easel) {
			Friend_mode_request_item_use(item, pigments, easel);
		}
		return true;
	}
	switch (Friend_mode_guest_item_use(item->get_shapenum())) {
	case Friend_item_use::on_self:
		Friend_mode_request_item_use(item, own);
		return true;
	case Friend_item_use::on_target:
		if (Game_object* target = Friend_mode_pick_item_target()) {
			Friend_mode_request_item_use(item, target);
		}
		return true;
	case Friend_item_use::none:
		break;
	}
	return false;
}

bool Friend_mode_guest_activate(Game_object* item) {
	Actor* own = gwin->get_controlled_actor();
	if (!own) {
		return false;
	}
	if (Friend_mode_guest_use_item(item)) {
		return true;
	}
	if (Friend_mode_guest_local_shape(item->get_shapenum())) {    // Shows something, here only.
		gwin->get_usecode()->init_conversation();
		item->activate();
		return true;
	}
	gwin->get_effects()->add_text("The Avatar must do this", own);
	return true;
}

Game_object* Friend_mode_pick_item_target() {
	int x = 0;
	int y = 0;
	if (!Get_click(x, y, Mouse::greenselect)) {
		return nullptr;
	}
	// Same lookup as Usecode's click_on_item: open windows first.
	Gump_manager* gumps = gwin->get_gump_man();
	if (Gump* gump = gumps->find_gump(x, y)) {
		int gx = 0;
		int gy = 0;
		gumps->map_game_to_gump(gump, x, y, gx, gy);
		return gump->find_object(gx, gy);
	}
	return gwin->find_object(x, y);
}

void Friend_mode_request_item_use(Game_object* item, Game_object* target, Game_object* second) {
	if (!item || !target || friend_session.mode() != Friend_mode_session::Mode::guest || !friend_session.connected()) {
		return;
	}
	std::vector<unsigned char> payload;
	if (!Friend_put_path(payload, item) || !Friend_put_path(payload, target)
		|| (second && !Friend_put_path(payload, second))) {
		return;
	}
	friend_session.send_blob(Friend_mode_session::Blob_kind::guest_use_item, payload);
}

// Host: use an item for a guest after checking it.
static void Friend_mode_use_item_for_guest(const std::vector<unsigned char>& data, Actor* actor) {
	auto refuse = [&](const char* why) {
		cout << "Friend Mode: refused guest item use (" << why << ')' << endl;
		Friend_force_item_resync(actor);
	};
	if (!actor) {
		return refuse("no companion");
	}
	std::size_t  pos    = 0;
	Game_object* item   = nullptr;
	Game_object* target = nullptr;
	Tile_coord   tile;
	if (!Friend_read_path(data, pos, item, tile) || !item || item->as_actor()
		|| !Friend_read_path(data, pos, target, tile) || !target) {
		return refuse("not found");
	}
	Game_object* second = nullptr;    // A second click (painting).
	if (pos < data.size() && (!Friend_read_path(data, pos, second, tile) || !second)) {
		return refuse("second target not found");
	}
	const Friend_item_use use = Friend_mode_guest_item_use(item->get_shapenum());
	if (use == Friend_item_use::none) {
		return refuse("item not allowed");
	}
	if (!Friend_guest_may_touch(actor, item)) {
		return refuse("item out of reach or not theirs");
	}
	if (Friend_is_paint_brush(item)) {
		// The brush script asks for the pigments, then the easel.
		if (!Friend_is_pigments(target) || !second || second->get_shapenum() != 837) {
			return refuse("the brush needs pigments, then an easel");
		}
		if (!Friend_guest_may_touch(actor, target) || !Friend_guest_may_touch(actor, second)) {
			return refuse("pigments or easel out of reach");
		}
	} else if (second) {
		return refuse("only painting takes two clicks");
	} else if (use == Friend_item_use::on_self) {
		if (target != actor) {
			return refuse("food and potions are for the guest's own companion");
		}
	} else if (target->as_actor() || !(target->get_info().is_door() || target->as_container())) {
		return refuse("keys and lockpicks work on doors and chests");
	} else if (!Friend_guest_may_touch(actor, target)) {
		return refuse("target out of reach");
	}
	Usecode_machine* usecode = gwin->get_usecode();
	friend_usecode_walker    = actor;
	usecode->init_conversation();
	usecode->intercept_click_on_item(target);
	if (second) {
		usecode->queue_click_on_item(second);
	}
	item->activate();
	usecode->intercept_click_on_item(nullptr);    // In case it went unused.
	friend_usecode_walker = nullptr;
	gwin->set_all_dirty();
	cout << "Friend Mode: used shape " << item->get_shapenum() << " on " << target->get_name() << " for guest" << endl;
}

// Host: the NPCs whose moves and health guests need: Exult's 'nearby' list
// (around the host's view) plus everyone within a screenful of the guest's
// companion, which the host may not be drawing.
static void Friend_npcs_near_players(Actor_vector& out) {
	gwin->get_nearby_npcs(out);
	for (const auto& each : gwin->get_remote_friends()) {
		Actor_vector near_guest;
		each.actor->find_nearby_actors(near_guest, c_any_shapenum, 1 + c_screen_tile_size);
		near_guest.push_back(each.actor);    // Other guests see it move.
		for (Actor* actor : near_guest) {
			if (std::find(out.begin(), out.end(), actor) == out.end()) {
				out.push_back(actor);
			}
		}
	}
}

// Host: creatures and NPCs within a screenful of the guest's companion wake
// as if the host could see them (Exult wakes them only when painted), so
// they move and fight near the guest too.
static void Friend_mode_wake_near_guest() {
	for (const auto& each : gwin->get_remote_friends()) {
		Actor_vector nearby;
		each.actor->find_nearby_actors(nearby, c_any_shapenum, 1 + c_screen_tile_size);
		for (Actor* actor : nearby) {
			Npc_actor* npc = actor->as_npc();
			// Also keeps them in the 'nearby' list, whose moves go to guests.
			if (npc && !npc->is_in_party() && !npc->is_dead()) {
				npc->wake();
			}
		}
	}
}

// Combat.  A guest in combat mode double-clicks a foe; the host runs Exult's
// combat schedule for the guest's companion against that foe alone.  When
// the foe dies or is lost, or the guest walks, the companion waits again.

enum Friend_attack_target { friend_attack_npc = 0, friend_attack_creature = 1, friend_attack_stop = 2 };
// Targeting range: anything on screen, at most 40 tiles off each way (a
// square, as Exult measures distance).  That is how far around a guest's
// companion the host keeps creatures awake.  Screens bigger than that show
// the limit as a faint frame.
constexpr int friend_attack_max_tiles = Game_window::friend_target_range;

// Creatures, and NPCs who are hostile.  Peaceful people stay the Avatar's
// business, as with talking to them.
static bool Friend_guest_may_attack(const Actor* foe) {
	if (foe->get_npc_num() > 0) {
		const int align = foe->get_effective_alignment();
		return align == Actor::evil || align == Actor::chaotic;
	}
	return Friend_creature_id(foe) != 0;
}

// Someone 'own' may pick and fight: not oneself, the Avatar, the party, the
// dead or a body.
static bool Friend_is_attack_target(const Actor* own, const Actor* foe) {
	return own && foe && foe != own && foe != gwin->get_main_actor() && !foe->is_in_party() && !foe->is_dead()
		   && !foe->get_info().is_body_shape();
}

bool Friend_mode_guest_attack(Game_object* obj) {
	Actor* own = gwin->get_controlled_actor();
	Actor* foe = obj ? obj->as_actor() : nullptr;
	if (!Friend_is_attack_target(own, foe) || friend_session.mode() != Friend_mode_session::Mode::guest) {
		return false;    // Not a foe: doors and the like work as usual.
	}
	if (!Friend_guest_may_attack(foe)) {
		gwin->get_effects()->add_text("The Avatar must do this", foe);
		return true;
	}
	if (!friend_session.connected()) {
		return true;
	}
	std::vector<unsigned char> payload;
	if (foe->get_npc_num() > 0) {
		Friend_put8(payload, friend_attack_npc);
		Friend_put16(payload, foe->get_npc_num());
		Friend_put16(payload, 0);
	} else {
		const uint32 id = Friend_creature_id(foe);
		Friend_put8(payload, friend_attack_creature);
		Friend_put16(payload, static_cast<int>(id & 0xffff));
		Friend_put16(payload, static_cast<int>(id >> 16));
	}
	friend_session.send_blob(Friend_mode_session::Blob_kind::guest_attack, payload);
	cout << "Friend Mode: guest attacks " << foe->get_name() << endl;
	return true;
}

// ---- The Target System (Q, E, Space) -------------------------------------
//
// Keyboard targeting, for single player as well as Co-Op Mode (the
// "Target System" options page, config/gameplay/keyboard_targeting and
// config/gameplay/targeting/...; docs/friend-mode/targeting-groups.md).
// It works for whoever is controlled: the Avatar, or a guest's companion,
// whose actions go through the host as its mouse actions do.
//
// - Q picks people and creatures on screen within range
//   (friend_attack_max_tiles), nearest first, Shift+Q the previous; in
//   combat mode hostiles first; out of combat the party last.  The pick is
//   Game_window's action target, with an arrow over it
//   (Friend_mode_paint_target_arrow).
// - E picks things (friend_object_target): with a container window open,
//   what is in the open windows; otherwise containers, bodies and signs
//   within reach and, near one's feet, the groups the options page turns
//   on (Friend_thing_group_of).  Green arrow.
// - Both start again at the nearest after one moves, and pass "No target"
//   on the way round.
// - Space acts on the pick (Friend_mode_space): talk or attack a person;
//   open, use, read, take, ready or drop a thing.  Shift is "more".  Taking
//   and dropping go through the drag code (Game_window::move_object).
// - Fighting through targeting keeps to the pick: the Avatar's attack mode
//   is "manual" until combat mode ends; a knocked-out pick stays attacked
//   until dead, then the nearest hostile is next.
// - On screens bigger than the range, a faint circle shows it
//   (Friend_mode_paint_range_frame).
//
// Nothing here needs the network; the Friend_mode_is_guest() branches send
// a guest's requests to the host.  See docs/friend-mode/upstreaming.md.

// Hostile: evil or chaotic, and someone one may fight.
static bool Friend_is_hostile_target(const Actor* own, const Actor* foe) {
	if (!Friend_is_attack_target(own, foe)) {
		return false;
	}
	const int align = foe->get_effective_alignment();
	return align == Actor::evil || align == Actor::chaotic;
}

static void Friend_mode_take_manual();

// The pick is being attacked (through targeting), so a knockout doesn't end
// the fight and a kill moves on to the next hostile.
static bool friend_attacking_pick = false;

// "Targeting Deer", "Attacking Deer" or "No target" near the top, replacing
// the last one.
// With 'hint', a second line under it says what Space (and Shift+Space)
// would do with the pick: "Talk (Space)", "Take (Space)  Split
// (Shift+Space)" (Friend_space_hint).  Any later notice of these replaces
// both.
static std::string Friend_space_hint();
static std::string friend_hint_shown;            // The hint line showing, if any.
static uint32      friend_hint_at      = 0;      // When it was set (it fades with the notice).
static bool        friend_hint_changed = false;

static void Friend_mode_target_notice(const std::string& text, bool hint = false) {
	friend_notices.erase(std::remove_if(friend_notices.begin(), friend_notices.end(),
								 [](const Friend_log_line& line) {
									 return line.text.rfind("Targeting ", 0) == 0
											|| line.text.rfind("Attacking ", 0) == 0 || line.text == "No target";
								 }),
						 friend_notices.end());
	Friend_mode_top_line(text);
	friend_hint_shown   = hint ? Friend_space_hint() : std::string();
	friend_hint_at      = static_cast<uint32>(SDL_GetTicks());
	friend_hint_changed = true;
}

// A pick's name for the notices.
static std::string Friend_target_name(const Actor* actor) {
	const std::string name = actor->get_name();
	return name.empty() ? std::string("someone") : name;
}

// Combat mode on, the pick not (or no longer) attacked: the Avatar stands,
// or the host stops fighting for a guest's companion.
static void Friend_mode_stop_attack() {
	friend_attacking_pick = false;
	if (!gwin->in_combat()) {
		return;
	}
	if (Friend_mode_is_guest()) {
		if (friend_session.connected()) {
			friend_session.send_blob(Friend_mode_session::Blob_kind::guest_attack, {friend_attack_stop, 0, 0, 0, 0});
		}
	} else {
		Friend_mode_take_manual();
		gwin->get_main_actor()->set_target(nullptr);
	}
}

void Friend_mode_combat_ended();

// Clearing the target in combat stops the attack: the Avatar (now
// manual) stands, or the host stops fighting for a guest's companion.
static void Friend_mode_clear_target() {
	if (!gwin->get_action_target()) {
		return;
	}
	gwin->set_action_target(nullptr);
	Friend_mode_target_notice("No target");
	Friend_mode_stop_attack();
}

// Out of combat the Avatar may also pick a party member (to talk to); a
// guest's companions are the Avatar's business.
static bool Friend_is_party_pick(const Actor* own, const Actor* actor) {
	return own && actor && Friend_targeting_party && !Friend_mode_is_guest() && !gwin->in_combat() && actor != own
		   && actor->is_in_party()
		   && !actor->is_dead();
}

// Anyone Q may pick just now.
static bool Friend_is_pickable(const Actor* own, const Actor* actor) {
	return Friend_is_attack_target(own, actor) || Friend_is_party_pick(own, actor);
}

// E: things.  The picked container or body (Q picks people instead).
static Game_object_weak friend_object_target;
static bool             friend_object_in_window = false;    // It was picked in a window.
static uint32           friend_range_shown_until = 0;    // Ticks; after Q or E.

// Q and E start again at the nearest once one has moved; standing still,
// they go on to the next (and past the last to "No target", then round).
static Tile_coord friend_cycle_at(-1, -1, -1);

// True (once) if 'own' has moved since the last Q or E.
static bool Friend_cycle_moved(const Actor* own) {
	const bool moved = own->get_tile() != friend_cycle_at;
	friend_cycle_at  = own->get_tile();
	return moved;
}

// Who Q can pick: on screen, in range; party members out of combat (last).
static std::vector<Actor*> Friend_target_candidates(Actor* own) {
	Actor_vector nearby;
	own->find_nearby_actors(nearby, c_any_shapenum, friend_attack_max_tiles);
	const TileRect      view = gwin->get_win_tile_rect();
	std::vector<Actor*> foes;
	for (Actor* actor : nearby) {
		const Tile_coord at = actor->get_tile();
		if (Friend_is_pickable(own, actor) && view.has_point(at.tx, at.ty)) {
			foes.push_back(actor);
		}
	}
	return foes;
}

// Q (step 1) and Shift+Q (step -1): the next or previous person.  In combat
// mode a hostile pick is attacked at once.
static void Friend_mode_cycle_target(int step) {
	Actor* own = gwin->get_controlled_actor();
	if (!own || (Friend_mode_is_guest() && !gwin->has_network_friend_control()) || gwin->network_input_blocked()
		|| !gwin->main_actor_can_act()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		return;
	}
	friend_range_shown_until     = SDL_GetTicks() + 2000;
	friend_object_target.reset();    // Q picks people, E things.
	const bool          fighting = gwin->in_combat();
	std::vector<Actor*> foes     = Friend_target_candidates(own);
	if (foes.empty()) {
		Friend_mode_clear_target();
		Mouse::mouse()->flash_shape(Mouse::redx);
		return;
	}
	// Nearest first; in combat mode hostiles come before everyone else, and
	// out of it the party comes last.
	const Tile_coord from = own->get_tile();
	std::stable_sort(foes.begin(), foes.end(), [&](const Actor* a, const Actor* b) {
		const bool pa = a->is_in_party();
		const bool pb = b->is_in_party();
		if (pa != pb) {
			return pb;
		}
		if (fighting) {
			const bool ha = Friend_is_hostile_target(own, a);
			const bool hb = Friend_is_hostile_target(own, b);
			if (ha != hb) {
				return ha;
			}
		}
		return from.distance(a->get_tile()) < from.distance(b->get_tile());
	});
	// Past the last (or, going back, the first) the pick clears; the next Q
	// starts again, as does one after walking.
	const bool moved   = Friend_cycle_moved(own);
	const auto current = moved ? foes.end() : std::find(foes.begin(), foes.end(), gwin->get_action_target());
	const int  count   = static_cast<int>(foes.size());
	int        index   = step < 0 ? count - 1 : 0;
	if (current != foes.end()) {
		index = static_cast<int>(current - foes.begin()) + step;
		if (index < 0 || index >= count) {
			Friend_mode_clear_target();
			return;
		}
	}
	// In combat mode a hostile pick is attacked at once; a peaceful one waits
	// for Space (and whatever was being attacked is let go).
	Actor* foe = foes[index];
	gwin->set_action_target(foe);
	if (fighting && Friend_is_hostile_target(own, foe) && Friend_mode_attack_target(foe)) {
		return;
	}
	Friend_mode_target_notice("Targeting " + Friend_target_name(foe), true);
	Friend_mode_stop_attack();
}

// Once the Avatar fights through targeting, it attacks only what is picked:
// its attack mode is "manual" (the combat AI picks no foes of its own) until
// combat mode ends, when its own mode comes back (friend_saved_attack_mode;
// saves keep the own one).  Someone who never uses Q fights as U7 always did.

static void Friend_mode_take_manual() {
	Main_actor* avatar = gwin->get_main_actor();
	if (Friend_mode_is_guest() || !avatar || avatar->get_attack_mode() == Actor::manual) {
		return;    // Already (a fresh load brings the Avatar's own mode back).
	}
	friend_saved_attack_mode = avatar->get_attack_mode();
	avatar->set_attack_mode(Actor::manual, avatar->did_user_set_attack());
}

// Combat mode went off (toggle_combat): the Avatar gets its own attack mode
// back, and nothing is being attacked through targeting.
void Friend_mode_combat_ended() {
	friend_attacking_pick = false;
	Main_actor* avatar = gwin->get_main_actor();
	if (avatar && friend_saved_attack_mode >= 0 && avatar->get_attack_mode() == Actor::manual) {
		avatar->set_attack_mode(static_cast<Actor::Attack_mode>(friend_saved_attack_mode), avatar->did_user_set_attack());
	}
	friend_saved_attack_mode = -1;
}

// Attack the picked target (combat mode is on): "Attacking Deer".  False
// if it can't be (a guest and a peaceful person: "The Avatar must do this").
bool Friend_mode_attack_target(Actor* foe) {
	Actor* own = gwin->get_controlled_actor();
	if (!foe || !Friend_is_attack_target(own, foe)) {
		return false;
	}
	if (Friend_mode_is_guest()) {
		const bool allowed = Friend_guest_may_attack(foe);
		Friend_mode_guest_attack(foe);    // The host fights for our companion.
		if (!allowed) {
			return false;
		}
	} else {
		Main_actor* avatar = gwin->get_main_actor();
		if (!avatar->can_act_charmed()) {
			return false;
		}
		Friend_mode_take_manual();
		avatar->set_target(foe);    // As a double-click in combat mode.
	}
	Friend_mode_target_notice("Attacking " + Friend_target_name(foe), true);
	friend_attacking_pick = true;
	return true;
}

// The pick died while being attacked: the nearest hostile on screen is
// next (never a peaceful one), or nothing.
static void Friend_mode_target_next_hostile() {
	Actor* own = gwin->get_controlled_actor();
	if (!own || !gwin->in_combat()) {
		return;
	}
	const Tile_coord from = own->get_tile();
	Actor*           next = nullptr;
	for (Actor* actor : Friend_target_candidates(own)) {
		if (Friend_is_hostile_target(own, actor)
			&& (!next || from.distance(actor->get_tile()) < from.distance(next->get_tile()))) {
			next = actor;
		}
	}
	if (next) {
		gwin->set_action_target(next);
		Friend_mode_attack_target(next);
	}
}

// Combat mode came on: a hostile picked with Q is attacked.  A peaceful one
// isn't, so that turning on combat never starts a fight with townsfolk.
void Friend_mode_combat_started() {
	Actor* target = gwin->get_action_target();
	if (target && Friend_is_hostile_target(gwin->get_controlled_actor(), target)) {
		Friend_mode_attack_target(target);
	}
}

// The arrow over the target, red in combat mode and white otherwise: its own
// layer, placed over the target's head every frame at the world's scale, so
// it leaves nothing behind as the target moves and draws over roofs and
// trees.
static void Friend_mode_paint_target_arrow() {
	static int    layer   = -1;
	static int    painted = -1;    // The colors it has: combat mode, and 2 for a thing.
	Image_window* iwin    = gwin->get_win();
	const Game_object_shared thing  = friend_object_target.lock();
	Game_object*             target = gwin->get_action_target();
	if (!target) {
		target = thing.get();
	}
	Shape_frame* frame = target ? target->get_shape() : nullptr;
	// A thing in a window is drawn in the window's own (scaled) layer.
	Gump* window = thing && target == thing.get() && friend_object_in_window
						   ? gwin->get_gump_man()->find_gump(thing.get())
						   : nullptr;
	const bool visible = target && frame && (target->get_chunk() || window) && !gwin->is_in_exult_menu();
	if (!visible) {
		if (layer >= 0 && iwin->layer_is_visible(layer)) {
			gwin->layer_set_visible(layer, false);
			gwin->set_painted();
		}
		return;
	}
	// A 9x6 arrow pointing down: red (or white) inside a black edge.
	constexpr int         aw         = 9;
	constexpr int         ah         = 6;
	constexpr const char* arrow[ah] = {"BBBBBBBBB", "BRRRRRRRB", ".BRRRRRB.", "..BRRRB..", "...BRB...", "....B...."};
	if (layer < 0) {
		constexpr int text_layer_z = (1 << 18) + (1 << 16);
		layer                      = gwin->create_layer("Target arrow", aw, ah, 255, 0, text_layer_z - 1);
		painted                    = -1;
		if (layer < 0) {
			return;
		}
	}
	// Red in combat, white out of it; green on a thing (E).
	const int fighting = thing && target == thing.get() ? 2 : gwin->in_combat() ? 1 : 0;
	if (painted != fighting) {
		Image_buffer8* lbuf = gwin->get_layer_ibuf(layer);
		if (!lbuf) {
			return;
		}
		const unsigned char fill
				= fighting == 2 ? static_cast<unsigned char>(gwin->get_pal()->find_color(15, 58, 15))
								: Shape_manager::get_instance()->get_special_pixel(fighting ? HIT_PIXEL : PROTECT_PIXEL);
		lbuf->fill8(255);
		for (int y = 0; y < ah; ++y) {
			for (int x = 0; x < aw; ++x) {
				if (arrow[y][x] != '.') {
					lbuf->put_pixel8(arrow[y][x] == 'R' ? fill : 0, x, y);
				}
			}
		}
		gwin->layer_set_dirty(layer);
		painted = fighting;
	}
	int x;
	int y;
	if (window) {
		window->get_shape_location(target, x, y);
	} else {
		gwin->get_shape_location(target, x, y);
	}
	const int left = x - frame->get_xleft();
	const int top  = y - frame->get_yabove();
	const int ax   = left + frame->get_width() / 2 - aw / 2;
	const int ay   = top - ah - 1;
	int       sx0;
	int       sy0;
	int       sx1;
	int       sy1;
	const int wl = window ? window->get_render_layer() : -1;
	if (wl >= 0 && iwin->layer_is_visible(wl)) {
		// Through the window's layer, as Gump_manager::map_game_to_gump
		// does the other way.
		iwin->layer_to_screen(wl, ax - window->layer_bounds.x, ay - window->layer_bounds.y, sx0, sy0);
		iwin->layer_to_screen(wl, ax + aw - window->layer_bounds.x, ay + ah - window->layer_bounds.y, sx1, sy1);
	} else {
		iwin->game_to_screen(ax, ay, false, sx0, sy0);
		iwin->game_to_screen(ax + aw, ay + ah, false, sx1, sy1);
	}
	gwin->layer_set_dest(layer, sx0, sy0, sx1 - sx0, sy1 - sy0);
	gwin->layer_set_visible(layer, true);
	gwin->set_painted();
}

// The targeting range, on screens bigger than it: a faint circle with a
// border around one's character, in combat mode and for a moment after Q
// or E.  On smaller screens the screen's edge is the range, so nothing.
// The circle is a picture of the range, not the range itself (a square
// of 40 tiles each way): it shrinks to stay wholly in view, so it shows
// why something at the far edge of a big screen can't be picked.

static void Friend_mode_paint_range_frame() {
	static int    fill_layer   = -1;
	static int    border_layer = -1;
	static int    drawn_radius = -1;
	Image_window* iwin         = gwin->get_win();
	Actor*        own          = gwin->get_controlled_actor();
	const int     range        = friend_attack_max_tiles;
	const TileRect view        = gwin->get_win_tile_rect();
	const bool    bigger       = view.w > 2 * range + 1 || view.h > 2 * range + 1;
	const bool    wanted       = gwin->in_combat() || SDL_GetTicks() < friend_range_shown_until;
	const bool    visible      = Friend_keyboard_targeting && Friend_targeting_circle && bigger && wanted && own && own->get_chunk()
						 && !gwin->is_in_exult_menu() && !gwin->network_input_blocked();
	if (!visible) {
		for (const int layer : {fill_layer, border_layer}) {
			if (layer >= 0 && iwin->layer_is_visible(layer)) {
				gwin->layer_set_visible(layer, false);
				gwin->set_painted();
			}
		}
		return;
	}
	const int size = (2 * range + 1) * c_tilesize;    // In game pixels.
	if (fill_layer < 0) {
		// Under the target arrow and text.  The fill is one color, made faint
		// by the layer's alpha; the border is its own, stronger layer.
		constexpr int text_layer_z = (1 << 18) + (1 << 16);
		fill_layer                 = gwin->create_layer("Target range fill", size, size, 255, 0, text_layer_z - 4);
		border_layer               = gwin->create_layer("Target range border", size, size, 255, 0, text_layer_z - 3);
		if (fill_layer < 0 || border_layer < 0) {
			return;
		}
		gwin->layer_set_alpha(fill_layer, 20);
		gwin->layer_set_alpha(border_layer, 140);
		drawn_radius = -1;
	}
	// Centred on the tile the character stands on (its shape's anchor is
	// that tile's bottom right corner); as big as the range, or as fits
	// in view with a few pixels to spare, in steps of 4 pixels.
	int x;
	int y;
	gwin->get_shape_location(own, x, y);
	const int cx     = x - c_tilesize / 2;
	const int cy     = y - c_tilesize / 2;
	constexpr int margin = 4;
	int       radius = std::min({size / 2 - 1, cx - margin, gwin->get_width() - cx - margin, cy - margin,
				  gwin->get_height() - cy - margin});
	radius           = std::max(radius, 4 * c_tilesize) / 4 * 4;
	if (radius != drawn_radius) {
		Image_buffer8* fill = gwin->get_layer_ibuf(fill_layer);
		Image_buffer8* line = gwin->get_layer_ibuf(border_layer);
		if (!fill || !line) {
			return;
		}
		const unsigned char color = Shape_manager::get_instance()->get_special_pixel(PROTECT_PIXEL);
		fill->fill8(255);
		line->fill8(255);
		const int inner = radius - 2;    // The border is 2 pixels wide.
		for (int py = 0; py < size; ++py) {
			const int dy = 2 * py + 1 - size;    // Doubled, from the centre.
			for (int px = 0; px < size; ++px) {
				const int dx = 2 * px + 1 - size;
				const int d2 = dx * dx + dy * dy;
				if (d2 <= 4 * inner * inner) {
					fill->put_pixel8(color, px, py);
				} else if (d2 <= 4 * radius * radius) {
					line->put_pixel8(color, px, py);
				}
			}
		}
		gwin->layer_set_dirty(fill_layer);
		gwin->layer_set_dirty(border_layer);
		drawn_radius = radius;
	}
	const int left = cx - size / 2;
	const int top  = cy - size / 2;
	int       sx0;
	int       sy0;
	int       sx1;
	int       sy1;
	iwin->game_to_screen(left, top, false, sx0, sy0);
	iwin->game_to_screen(left + size, top + size, false, sx1, sy1);
	for (const int layer : {fill_layer, border_layer}) {
		gwin->layer_set_dest(layer, sx0, sy0, sx1 - sx0, sy1 - sy0);
		gwin->layer_set_visible(layer, true);
	}
	gwin->set_painted();
}

// Walking in combat with a target picked stops the attack (the target stays
// picked; Space takes it up again).  A guest's walking already ends the
// host's attack for it.
static void Friend_mode_walking_stops_attack() {
	if (Friend_mode_is_guest() || !gwin->in_combat() || !gwin->get_action_target()) {
		return;
	}
	Main_actor* avatar = gwin->get_main_actor();
	friend_attacking_pick = false;
	if (avatar->get_target()) {
		Friend_mode_take_manual();
		avatar->set_target(nullptr);
		Friend_mode_target_notice("Targeting " + Friend_target_name(gwin->get_action_target()), true);
	}
}

// Every frame: drop the target once it is dead, gone or far off screen.
static void Friend_mode_check_target() {
	Actor* target = gwin->get_action_target();
	Actor* own    = gwin->get_controlled_actor();
	// A thing (E) goes when it is gone: on the map, taken or far off; in a
	// window, out of it or the window closed.
	if (const Game_object_shared thing = friend_object_target.lock()) {
		const bool gone = friend_object_in_window
								  ? !thing->get_owner() || !gwin->get_gump_man()->find_gump(thing.get())
								  : thing->get_owner() || !thing->get_chunk() || !own
											|| own->get_tile().distance(thing->get_tile()) > friend_attack_max_tiles + 4;
		if (!Friend_keyboard_targeting || !own || gone) {
			friend_object_target.reset();
		}
	}
	if (!target) {
		// Gone while attacked: on a guest, a creature the host killed is
		// simply removed.  The next hostile, as for a death.
		if (friend_attacking_pick) {
			friend_attacking_pick = false;
			if (gwin->in_combat()) {
				Friend_mode_target_next_hostile();
				if (!gwin->get_action_target()) {
					Friend_mode_target_notice("No target");
				}
			}
		}
		return;
	}
	if (!Friend_keyboard_targeting || !own || !target->get_chunk()
		|| own->get_tile().distance(target->get_tile()) > friend_attack_max_tiles + 4) {
		Friend_mode_clear_target();
	} else if (!Friend_is_pickable(own, target)) {
		// Dead, or a party member once combat starts.  A kill moves on to
		// the next hostile.
		const bool next = friend_attacking_pick && gwin->in_combat() && target->is_dead();
		Friend_mode_clear_target();
		if (next) {
			Friend_mode_target_next_hostile();
		}
	} else if (friend_attacking_pick && gwin->in_combat() && !Friend_mode_is_guest()) {
		// Knocked out: Exult stops everyone attacking it, but the Avatar
		// keeps at its pick until it is dead.
		Main_actor* avatar = gwin->get_main_actor();
		if (!avatar->get_target() && target->get_flag(Obj_flags::asleep)) {
			avatar->set_target(target);
		}
	}
}

// What E may offer, by group (the Targeting options page turns groups on
// and off).  Black Gate's shape classes lump a chair, a plate and a gem
// together, so the groups are our own lists, from the shapes that can be
// picked up (tools/friend-mode/pickup_items.py lists them); see
// docs/friend-mode/targeting-groups.md.  Weapons, armour and ammunition
// are loot by their game data; anything not listed is decor.
enum class Friend_thing_group { box, loot, decor, furniture, interactive, lever, sign };

// The group of a thing in the world (containers and bodies are 'box').
static Friend_thing_group Friend_thing_group_of(Game_object* obj) {
	static const std::unordered_set<int> loot{
			159, 178, 194, 285, 295, 296, 298, 330, 340, 377, 444, 581, 583, 584, 585, 616, 627, 640, 641, 642, 644,
			645, 646, 648, 649, 650, 662, 668, 691, 692, 693, 726, 728, 738, 746, 747, 754, 756, 760, 761, 769, 771,
			772, 785, 786, 790, 797, 827, 838, 839, 842, 877, 914, 915, 916, 921, 937, 955, 981};
	static const std::unordered_set<int> furniture{
			336, 338, 997, 873, 697, 702, 704, 812, 813, 837, 987, 991, 992, 995, 1013};
	// Shutters, sconces, lit lamp, curtains, lamp post, and the secret
	// walls (828, 845) that slide open.
	static const std::unordered_set<int> interactive{290, 291, 322, 372, 435, 481, 526, 657, 678, 828, 845, 889};
	const Shape_info& info  = obj->get_info();
	const int         shape = obj->get_shapenum();
	if (info.get_shape_class() == Shape_info::container || info.is_body_shape()) {
		return Friend_thing_group::box;
	}
	if (shape == 787 || shape == 788) {    // Levers and switches.
		return Friend_thing_group::lever;
	}
	if (shape == 379 || shape == 820 || shape == 715) {    // Sign, plaque, tombstone.
		return Friend_thing_group::sign;
	}
	if (info.is_door() || interactive.count(shape)) {
		return Friend_thing_group::interactive;
	}
	if (furniture.count(shape)) {
		return Friend_thing_group::furniture;
	}
	if (loot.count(shape) || info.get_weapon_info() || info.get_armor_info() || info.get_ammo_info()) {
		return Friend_thing_group::loot;
	}
	return Friend_thing_group::decor;
}

// Whether the options page lets E offer 'group'.
static bool Friend_thing_group_on(Friend_thing_group group) {
	switch (group) {
	case Friend_thing_group::box:
		return true;
	case Friend_thing_group::loot:
		return Friend_targeting_loot;
	case Friend_thing_group::decor:
		return Friend_targeting_decor;
	case Friend_thing_group::furniture:
		return Friend_targeting_furniture;
	case Friend_thing_group::interactive:
		return Friend_targeting_interactive;
	case Friend_thing_group::lever:
		return Friend_targeting_levers;
	case Friend_thing_group::sign:
		return Friend_targeting_signs;
	}
	return false;
}

// "10 gold coins", "sword": a thing's name for the notices (Exult's names
// already carry the quantity).
static std::string Friend_thing_name(Game_object* obj) {
	const std::string name = obj->get_name();
	return name.empty() ? std::string("something") : name;
}

// Space asks twice for a theft or a drop: the second Space must be on the
// same thing, for the same action, within 3 seconds.
enum class Friend_confirm { none, take, drop };
static Game_object_weak friend_confirm_object;
static Friend_confirm   friend_confirm_action = Friend_confirm::none;
static uint32           friend_confirm_until  = 0;

// True on the second Space; on the first, shows 'ask' and remembers it.
static bool Friend_confirmed(Game_object* obj, Friend_confirm action, const std::string& ask) {
	const uint32 now = SDL_GetTicks();
	if (friend_confirm_action == action && friend_confirm_object.lock().get() == obj && now < friend_confirm_until) {
		friend_confirm_action = Friend_confirm::none;
		return true;
	}
	friend_confirm_object = weak_from_obj(obj);
	friend_confirm_action = action;
	friend_confirm_until  = now + 3000;
	Friend_mode_target_notice(ask);
	return false;
}

// Where taken things go: the backpack, else whatever the actor can hold.
static Container_game_object* Friend_pack_of(Actor* own) {
	Game_object*           pack      = own->get_readied(backpack);
	Container_game_object* container = pack ? pack->as_container() : nullptr;
	return container ? container : own;
}

// "How many?" for a stack, with Shift; 0 if cancelled.  Without Shift, all.
static int Friend_how_many(Game_object* obj, bool shift) {
	const int count = obj->get_quantity();
	if (!shift || count <= 1) {
		return count;
	}
	bool      escaped = false;
	const int chosen  = gwin->get_gump_man()->prompt_for_number(0, count, 1, count, nullptr, &escaped);
	return escaped ? 0 : chosen;
}

// After a take from a window, pick the next thing in it, so that Space
// again and again empties a chest.
static std::vector<Game_object*> Friend_window_candidates();
static void Friend_mode_pick_thing(Game_object* obj, bool in_window);

// After a take: the thing now at the taken one's place in the open
// windows' list ('index'), or the one before it if that was the last, so
// taking the 9th of 12 doesn't start the cycle over; none when empty.
static void Friend_mode_pick_next_in_window(int index = 0) {
	const std::vector<Game_object*> things = Friend_window_candidates();
	if (things.empty()) {
		friend_object_target.reset();
		Friend_mode_target_notice("No target");
	} else {
		const int last = static_cast<int>(things.size()) - 1;
		Friend_mode_pick_thing(things[std::clamp(index, 0, last)], true);
	}
}

// Where 'obj' is in the open windows' list (E's order), or 0.
static int Friend_window_index(Game_object* obj) {
	const std::vector<Game_object*> things = Friend_window_candidates();
	const auto                      at     = std::find(things.begin(), things.end(), obj);
	return at == things.end() ? 0 : static_cast<int>(at - things.begin());
}

// Taking 'obj' would be theft: not okay to take, outside dungeons, and not
// already the party's (Exult's own rule for a drag).
static bool Friend_is_theft(Game_object* obj) {
	return !obj->get_flag(Obj_flags::okay_to_take) && !gwin->is_in_dungeon()
		   && !obj->get_outermost()->get_flag(Obj_flags::in_party);
}

// Something to read: books, scrolls (letters and notes are scroll frames)
// and the cloth map.  Space reads it wherever it is; Shift+Space picks it
// up, or drops one's own.
static bool Friend_is_readable(const Game_object* obj) {
	const int shape = obj->get_shapenum();
	return GAME_BG && (shape == 642 || shape == 797 || shape == 178);
}

// Readying: an item meant to be worn or wielded.  Anything can be held in
// a hand, so a gem doesn't count: weapons, armour, and items whose own
// spot isn't a hand.
static bool Friend_is_wearable(Actor* own, Game_object* obj) {
	const Shape_info& info = obj->get_info();
	int               prefered;
	int               alt1;
	int               alt2;
	own->get_prefered_slots(obj, prefered, alt1, alt2);
	return info.get_weapon_info() || info.get_armor_info()
		   || (prefered != lhand && prefered != rhand && prefered != backpack
			   && prefered != belt && prefered >= 0);
}

// The spot an item would go in, if free; -1 if none is.
static int Friend_free_spot(Actor* own, Game_object* obj) {
	int prefered;
	int alt1;
	int alt2;
	own->get_prefered_slots(obj, prefered, alt1, alt2);
	for (const int spot : {prefered, alt1, alt2}) {
		if (spot >= 0 && own->fits_in_spot(obj, spot)) {
			return spot;
		}
	}
	return -1;
}

// Space on a worn or wielded thing in one's own inventory: off to the pack.
// On one in the pack: readied, swapping out what is in its spot.
static void Friend_mode_equip(Actor* own, Game_object* obj, Gump* window) {
	Container_game_object* pack = Friend_pack_of(own);
	if (own->find_readied(obj) >= 0) {
		if (pack == own) {
			Friend_mode_target_notice("No backpack");
		} else if (gwin->move_object(obj, window, pack)) {
			Friend_mode_target_notice("Put away " + Friend_thing_name(obj));
		}
		return;
	}
	int spot = Friend_free_spot(own, obj);
	std::vector<Game_object_shared> swapped;    // Taken off to make room.
	if (spot < 0) {
		int prefered;
		int alt1;
		int alt2;
		own->get_prefered_slots(obj, prefered, alt1, alt2);
		// The spot itself (a two-handed weapon's is the left hand, and it
		// needs the right free too).
		int main = prefered;
		if (main == both_hands) {
			main = lhand;
		} else if (main == lrgloves) {
			main = lfinger;
		} else if (main == neck) {
			main = amulet;
		} else if (main == scabbard) {
			main = belt;
		}
		std::vector<int> spots{main};
		if (prefered == both_hands) {
			spots.push_back(rhand);
		}
		for (const int each : spots) {
			Game_object* worn = own->get_readied(each);
			if (!worn) {
				continue;
			}
			if (pack == own || !gwin->move_object(worn, gwin->get_gump_man()->find_gump(worn), pack)) {
				Friend_mode_target_notice("No room");
				break;
			}
			swapped.push_back(worn->shared_from_this());
		}
		spot = Friend_free_spot(own, obj);
	}
	Gump* from = gwin->get_gump_man()->find_gump(obj);
	if (spot < 0 || !gwin->move_object(obj, from ? from : window, own, spot)) {
		// Couldn't: what was taken off goes back on.
		for (const Game_object_shared& worn : swapped) {
			const int back = Friend_free_spot(own, worn.get());
			if (back >= 0) {
				gwin->move_object(worn.get(), gwin->get_gump_man()->find_gump(worn.get()), own, back);
			}
		}
		if (spot < 0) {
			Friend_mode_target_notice("Can't ready that");
		}
		return;
	}
	Friend_mode_target_notice("Readied " + Friend_thing_name(obj));
}

// Space with a pick.
//
// A person out of combat is talked to (a guest gets its usual answer, a
// merchant or "The Avatar must do this").  In combat mode the pick is
// attacked, peaceful or not (after walking stopped an attack, again);
// while the Avatar is already attacking it, Space pauses combat as before
// (in single player only: there is no pausing in co-op).
//
// A thing (E): on the map, a container or body opens, and a door, lamp,
// lever or piece of furniture is used (Shift: picked up, if it can be).  A loose thing, or one in someone else's open container, goes
// into the pack (Shift on a stack: how many; Shift on a single thing in a
// window: everything in it).  Books, scrolls and the cloth map are read
// wherever they are; Shift+Space picks them up, or drops one's own.  Not
// okay to take: asks first.  One's own:
// used if it has a use, readied or put away if it is worn, else dropped at
// one's feet, after asking (Shift on a stack: how many to drop).
//
// False if Space should do its usual thing.
static bool Friend_mode_space(bool shift) {
	Actor*                   own    = gwin->get_controlled_actor();
	Actor*                   target = gwin->get_action_target();
	const Game_object_shared thing  = friend_object_target.lock();
	if (!own || (!target && !thing)) {
		return false;
	}
	const bool fighting = gwin->in_combat();
	if (!thing && fighting
		&& (!Friend_is_attack_target(own, target)
			|| (!Friend_mode_is_guest() && gwin->get_main_actor()->get_target() == target))) {
		return false;
	}
	if (gwin->network_input_blocked() || !gwin->main_actor_can_act()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		return true;
	}
	if (!thing) {
		if (!fighting) {
			gwin->double_clicked_object(target);    // Talk.
		} else {
			Friend_mode_attack_target(target);
		}
		return true;
	}
	Game_object*      obj    = thing.get();
	Gump*             window = friend_object_in_window ? gwin->get_gump_man()->find_gump(obj) : nullptr;
	const bool        mine   = obj->get_outermost() == own;
	if (!cheat.in_hack_mover() && !mine && !Fast_pathfinder_client::is_grabable(own, obj->get_outermost())) {
		Friend_mode_target_notice("Too far away");
		Mouse::mouse()->flash_shape(Mouse::outofrange);
		return true;
	}
	// Something to read: Space reads it, wherever it is.
	const bool readable = Friend_is_readable(obj);
	if (readable && !shift) {
		gwin->double_clicked_object(obj, window != nullptr || mine);
		return true;
	}
	// One's own.  Space: use, ready or put away; what has none of those is
	// dropped, after asking.  Shift+Space always drops, at once (a stack:
	// how many).
	if (mine) {
		if (!shift && (obj->as_container() || obj->usecode_exists())) {
			gwin->double_clicked_object(obj, true);
		} else if (!shift && Friend_is_wearable(own, obj)) {
			Friend_mode_equip(own, obj, window);
		} else {
			const int         count = Friend_how_many(obj, shift);
			const std::string name  = count < obj->get_quantity()
											 ? std::to_string(count) + " of " + Friend_thing_name(obj)
											 : Friend_thing_name(obj);
			const int index = window ? Friend_window_index(obj) : 0;
			if (count > 0 && (shift || Friend_confirmed(obj, Friend_confirm::drop, "Drop " + name + "? Space again"))
				&& gwin->move_object(obj, window, nullptr, -1, count)) {
				Friend_mode_target_notice("Dropped " + name);
				if (window) {
					Friend_mode_pick_next_in_window(index);    // Stay in place.
				}
			}
		}
		return true;
	}
	// On the map, a container or body opens, and a door, lamp, lever or
	// piece of furniture is used; Shift picks it up instead, if it can be.
	const Friend_thing_group group = Friend_thing_group_of(obj);
	const bool use = !window && group != Friend_thing_group::loot && group != Friend_thing_group::decor;
	if (use && !(shift && obj->is_dragable())) {
		gwin->double_clicked_object(obj);
		return true;
	}
	// Take it.  Shift on a single thing in a window: everything there.
	Container_game_object* pack = Friend_pack_of(own);
	if (shift && window && obj->get_quantity() <= 1 && !readable) {
		Container_game_object* from = obj->get_owner();
		std::vector<Game_object_shared> all;
		Object_iterator                 next(from->get_objects());
		bool                            theirs = false;    // Some of it isn't okay to take.
		while (Game_object* each = next.get_next()) {
			all.push_back(each->shared_from_this());
			theirs = theirs || !each->get_flag(Obj_flags::okay_to_take);
		}
		if (theirs && !gwin->is_in_dungeon() && !from->get_outermost()->get_flag(Obj_flags::in_party)
			&& !Friend_confirmed(obj, Friend_confirm::take, "Not all yours: Shift+Space again to take all")) {
			return true;
		}
		int taken = 0;
		for (const Game_object_shared& each : all) {
			if (!gwin->move_object(each.get(), gwin->get_gump_man()->find_gump(each.get()), pack)) {
				break;
			}
			++taken;
		}
		Friend_mode_target_notice(taken ? "Took " + std::to_string(taken) + (taken == 1 ? " thing" : " things")
										: std::string("Nothing taken"));
		Friend_mode_pick_next_in_window();
		return true;
	}
	if (Friend_is_theft(obj) && !Friend_confirmed(obj, Friend_confirm::take, "Steal? Space again")) {
		return true;
	}
	const int         count = Friend_how_many(obj, shift);
	const std::string name  = count < obj->get_quantity()
									 ? std::to_string(count) + " of " + Friend_thing_name(obj)
									 : Friend_thing_name(obj);
	const int index = window ? Friend_window_index(obj) : 0;
	if (count > 0 && gwin->move_object(obj, window, pack, -1, count)) {
		Friend_mode_target_notice("Took " + name);
		if (window) {
			Friend_mode_pick_next_in_window(index);
		} else {
			friend_object_target.reset();
		}
	}
	return true;
}

// What Space (and Shift+Space) would do with the pick, by the same rules
// as Friend_mode_space, for the line under "Targeting ...".
static bool Friend_is_merchant(int npc_num);

static std::string Friend_space_hint() {
	Actor*                   own    = gwin->get_controlled_actor();
	Actor*                   target = gwin->get_action_target();
	const Game_object_shared thing  = friend_object_target.lock();
	if (!own || (!target && !thing)) {
		return {};
	}
	auto two = [](const std::string& space, const std::string& shift) {
		return shift.empty() ? space + " (Space)" : space + " (Space)  " + shift + " (Shift+Space)";
	};
	if (!thing) {
		if (!gwin->in_combat()) {
			// People with a conversation have an NPC number and a script
			// of their own; a deer or a rat has neither.
			const bool talks = target->get_npc_num() > 0
							   && gwin->get_usecode()->function_exists(target->get_usecode());
			if (!talks) {
				return "Attack (C, then Space)";
			}
			// A guest talks only to merchants and trainers.
			return Friend_mode_is_guest() && !Friend_is_merchant(target->get_npc_num()) ? std::string("Avatar only")
																						 : two("Talk", "");
		}
		if (!Friend_is_attack_target(own, target)) {
			return {};
		}
		const bool attacking = !Friend_mode_is_guest() && gwin->get_main_actor()->get_target() == target;
		return two(attacking ? "Pause" : "Attack", "");
	}
	Game_object* obj    = thing.get();
	const bool   window = friend_object_in_window;
	const bool   mine   = obj->get_outermost() == own;
	const bool   stack  = obj->get_quantity() > 1;
	// Taking it would be a crime: "Steal" (Space asks again first).
	const std::string take = Friend_is_theft(obj) ? "Steal" : "Take";
	if (Friend_is_readable(obj)) {
		return two("Read", mine ? "Drop" : take);
	}
	if (mine) {
		const std::string drop = stack ? "Drop some" : "Drop";
		if (obj->as_container() || obj->usecode_exists()) {
			return two(obj->as_container() ? "Open" : "Use", drop);
		}
		if (Friend_is_wearable(own, obj)) {
			return two(own->find_readied(obj) >= 0 ? "Put away" : "Ready", drop);
		}
		return two("Drop", stack ? "Drop some" : "");
	}
	if (window) {
		return two(take, stack ? "Split" : "Take all");
	}
	switch (Friend_thing_group_of(obj)) {
	case Friend_thing_group::box:
		return two("Open", obj->is_dragable() ? "Pick up" : "");
	case Friend_thing_group::sign:
		return two("Read", "");
	case Friend_thing_group::interactive:
	case Friend_thing_group::lever:
	case Friend_thing_group::furniture:
		return two("Use", obj->is_dragable() ? "Pick up" : "");
	default:
		return two(take, stack ? "Split" : "");
	}
}

// E: things.  With a container window open (a chest, a body, a pack, a
// paperdoll), what is in the open windows, the newest window first, top to
// bottom and left to right.  Otherwise things in the world within reach,
// nearest first: containers and bodies, and loose things within 6 tiles
// (not one in the next building).  Shift+E the previous; past the last the
// pick clears.  See Friend_mode_space for what Space does with them.
static std::vector<Game_object*> Friend_window_candidates() {
	std::vector<Gump*> windows;
	for (Gump* gump : *gwin->get_gump_man()) {
		if (gump && gump->get_container() && !gump->is_persistent()) {
			windows.insert(windows.begin(), gump);    // Newest first.
		}
	}
	std::vector<Game_object*> things;
	for (Gump* gump : windows) {
		std::vector<Game_object*> inside;
		Object_iterator           next(gump->get_container()->get_objects());
		while (Game_object* obj = next.get_next()) {
			if (!obj->get_flag(Obj_flags::invisible)) {
				inside.push_back(obj);
			}
		}
		std::stable_sort(inside.begin(), inside.end(), [](const Game_object* a, const Game_object* b) {
			return a->get_ty() != b->get_ty() ? a->get_ty() < b->get_ty() : a->get_tx() < b->get_tx();
		});
		things.insert(things.end(), inside.begin(), inside.end());
	}
	return things;
}

// E with no container window open: things in the world, nearest first.
static std::vector<Game_object*> Friend_object_candidates(Actor* own) {
	constexpr int      loose_tiles = 6;
	Game_object_vector nearby;
	own->find_nearby(nearby, c_any_shapenum, friend_attack_max_tiles, 0);
	const TileRect            view = gwin->get_win_tile_rect();
	const Tile_coord          from = own->get_tile();
	std::vector<Game_object*> things;
	for (Game_object* obj : nearby) {
		if (obj->as_actor() || obj->get_owner() || !obj->get_chunk()) {
			continue;
		}
		const Tile_coord         at    = obj->get_tile();
		const Friend_thing_group group = Friend_thing_group_of(obj);
		// Containers, bodies and signs within reach; the rest near one's
		// feet: things one can pick up, and doors, lamps and levers to use.
		const bool fixture = group == Friend_thing_group::interactive || group == Friend_thing_group::lever;
		const bool near    = (obj->is_dragable() || fixture) && from.distance(at) <= loose_tiles;
		const bool far     = group == Friend_thing_group::box || group == Friend_thing_group::sign;
		if (Friend_thing_group_on(group) && (far || near)
			&& view.has_point(at.tx, at.ty) && std::abs(at.tz - from.tz) <= 5
			&& (cheat.in_hack_mover() || Fast_pathfinder_client::is_grabable(own, obj))) {
			things.push_back(obj);
		}
	}
	std::stable_sort(things.begin(), things.end(), [&](const Game_object* a, const Game_object* b) {
		return from.distance(a->get_tile()) < from.distance(b->get_tile());
	});
	return things;
}

// Make 'obj' the E pick, saying so near the top.
static void Friend_mode_pick_thing(Game_object* obj, bool in_window) {
	friend_object_target    = weak_from_obj(obj);
	friend_object_in_window = in_window;
	friend_confirm_action   = Friend_confirm::none;
	Friend_mode_target_notice("Targeting " + Friend_thing_name(obj), true);
}

// Backpacks.  Shift+1 to Shift+8 open one party member's backpack (Shift+1
// the Avatar's), or shut it if open, as 1 to 8 do the paperdolls.  Shift+I
// opens the next one not open yet, the Avatar's first; with all open, it
// shuts them all.  A guest has only its companion's, which either opens
// and shuts.  (I alone still opens the paperdolls.)
static Gump* Friend_backpack_window(Actor* who) {
	Game_object* pack = who ? who->get_readied(backpack) : nullptr;
	if (!pack) {
		return nullptr;
	}
	for (Gump* gump : *gwin->get_gump_man()) {
		if (gump && gump->get_container() == pack) {
			return gump;
		}
	}
	return nullptr;
}

// Open or shut 'who's backpack.  True if it opened.
static bool Friend_mode_toggle_backpack_of(Actor* who) {
	if (Gump* open = Friend_backpack_window(who)) {
		gwin->get_gump_man()->close_gump(open);
		gwin->set_all_dirty();
		return false;
	}
	Game_object*           pack      = who ? who->get_readied(backpack) : nullptr;
	Container_game_object* container = pack ? pack->as_container() : nullptr;
	if (!container) {
		Friend_mode_target_notice("No backpack");
		return false;
	}
	gwin->get_effects()->remove_text_effects();
	container->show_gump();
	return true;
}

// Party member 'index': 0 the Avatar, 1 the first companion, and so on.
static Actor* Friend_party_member(int index) {
	if (index == 0) {
		return gwin->get_main_actor();
	}
	Party_manager* party = gwin->get_party_man();
	return index <= party->get_count() ? gwin->get_npc(party->get_member(index - 1)) : nullptr;
}

// Shift+C: sort the newest open window of a container in someone's
// inventory (a backpack, a bag in it; chests keep their own layout).
static void Friend_mode_sort_window() {
	Gump* newest = nullptr;
	for (Gump* gump : *gwin->get_gump_man()) {
		if (gump && gump->is_inventory_container() && !gump->is_persistent()) {
			newest = gump;    // The list runs oldest to newest.
		}
	}
	if (!newest) {
		Friend_mode_target_notice("No backpack open");
		return;
	}
	newest->sort_objects();
}

// 'member': 0 the Avatar, 1 the first companion..., or -1 for Shift+I.
static void Friend_mode_backpack_key(int member) {
	Actor* own = gwin->get_controlled_actor();
	if (!own || gwin->network_input_blocked()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		return;
	}
	if (own != gwin->get_main_actor()) {    // A guest: its companion's only.
		Friend_mode_toggle_backpack_of(own);
		return;
	}
	if (member >= 0) {
		if (Actor* who = Friend_party_member(member)) {
			Friend_mode_toggle_backpack_of(who);
		}
		return;
	}
	const int count = gwin->get_party_man()->get_count();
	for (int i = 0; i <= count; ++i) {
		Actor* who = Friend_party_member(i);
		if (who && who->get_readied(backpack) && !Friend_backpack_window(who)) {
			Friend_mode_toggle_backpack_of(who);
			return;
		}
	}
	for (int i = 0; i <= count; ++i) {    // All open: shut them.
		if (Gump* open = Friend_backpack_window(Friend_party_member(i))) {
			gwin->get_gump_man()->close_gump(open);
		}
	}
	gwin->set_all_dirty();
}

// E (step 1) and Shift+E (step -1): the next or previous thing.
static void Friend_mode_cycle_object(int step) {
	Actor* own = gwin->get_controlled_actor();
	if (!own || (Friend_mode_is_guest() && !gwin->has_network_friend_control()) || gwin->network_input_blocked()
		|| !gwin->main_actor_can_act()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		return;
	}
	if (gwin->get_action_target()) {
		Friend_mode_clear_target();    // E picks things, Q people.
	}
	std::vector<Game_object*> things    = Friend_window_candidates();
	const bool                in_window = !things.empty();
	if (!in_window) {
		friend_range_shown_until = SDL_GetTicks() + 2000;
		things                   = Friend_object_candidates(own);
	}
	const Game_object_shared picked  = friend_object_target.lock();
	const bool               moved   = Friend_cycle_moved(own) && !in_window;
	const auto current = moved ? things.end() : std::find(things.begin(), things.end(), picked.get());
	const int                count   = static_cast<int>(things.size());
	int                      index   = step < 0 ? count - 1 : 0;
	if (current != things.end()) {
		index = static_cast<int>(current - things.begin()) + step;
	}
	if (things.empty() || index < 0 || index >= count) {
		friend_object_target.reset();
		Friend_mode_target_notice("No target");
		if (things.empty()) {
			Mouse::mouse()->flash_shape(Mouse::redx);
		}
		return;
	}
	Friend_mode_pick_thing(things[index], in_window);
}

// Host: a guest's companion stops fighting and waits for the guest again.
static void Friend_mode_end_guest_attack(Friend_guest& guest, const char* why) {
	if (guest.attacker < 0) {
		return;
	}
	Actor* actor = gwin->get_npc(guest.attacker);
	if (actor && gwin->is_remote_friend(actor)) {
		actor->set_target(nullptr);
		if (actor->get_schedule_type() != Schedule::wait) {
			actor->set_schedule_type(Schedule::wait);
		}
	}
	cout << "Friend Mode: guest's attack ended (" << why << ')' << endl;
	guest.attacker = -1;
	guest.attack_foe.reset();
}

static void Friend_mode_attack_for_guest(const std::vector<unsigned char>& data, Friend_guest& guest) {
	Actor* actor = guest.actor >= 0 ? gwin->get_npc(guest.actor) : nullptr;
	if (!actor || data.size() < 5) {
		return;
	}
	const int    kind = data[0];
	if (kind == friend_attack_stop) {
		Friend_mode_end_guest_attack(guest, "guest cleared target");
		return;
	}
	const uint32 id   = data[1] | (data[2] << 8) | (data[3] << 16) | (static_cast<uint32>(data[4]) << 24);
	Actor*       foe  = kind == friend_attack_npc ? gwin->get_npc(static_cast<int>(id))
						: kind == friend_attack_creature ? static_cast<Actor*>(Friend_creature_by_id(id))
														 : nullptr;
	if (!Friend_is_attack_target(actor, foe) || !Friend_guest_may_attack(foe)) {
		cout << "Friend Mode: refused guest attack (not a foe)" << endl;
		return;
	}
	if (actor->is_dead() || actor->get_flag(Obj_flags::asleep) || actor->get_flag(Obj_flags::paralyzed)
		|| actor->get_map() != foe->get_map() || actor->distance(foe) > friend_attack_max_tiles) {
		cout << "Friend Mode: refused guest attack (can't reach)" << endl;
		return;
	}
	if (actor->get_attack_mode() == Actor::flee) {
		actor->set_attack_mode(Actor::nearest);
	}
	guest.attacker   = actor->get_npc_num();
	guest.attack_foe = weak_from_obj(foe);
	if (actor->get_schedule_type() != Schedule::combat) {
		actor->set_schedule_type(Schedule::combat);
	}
	actor->set_target(foe);
	cout << "Friend Mode: " << actor->get_name() << " attacks " << foe->get_name() << " for guest" << endl;
}

// Host, every frame: end the attack once its foe is gone or the combat
// schedule moved on (to another foe, or back to following the Avatar).
static void Friend_mode_check_guest_attack(Friend_guest& guest) {
	if (guest.attacker < 0) {
		return;
	}
	Actor* actor = gwin->get_npc(guest.attacker);
	if (guest.attacker != guest.actor || !actor || !gwin->is_remote_friend(actor)) {
		guest.attacker = -1;    // Released; its old schedule was restored.
		guest.attack_foe.reset();
		return;
	}
	const Game_object_shared foe   = guest.attack_foe.lock();
	const Actor*             enemy = foe ? foe->as_actor() : nullptr;
	if (!enemy || enemy->is_dead()) {
		Friend_mode_end_guest_attack(guest, "foe defeated");
	} else if (actor->get_schedule_type() == Schedule::combat && !actor->get_target()
			   && enemy->get_flag(Obj_flags::asleep)) {
		// Knocked out: Exult stops everyone attacking it, but the guest
		// picked it, so keep at it until it is dead.
		actor->set_target(foe.get());
	} else if (actor->get_schedule_type() != Schedule::combat || actor->get_target() != enemy) {
		Friend_mode_end_guest_attack(guest, "foe lost");
	}
}

// Follow mode.  A guest presses Y: the party's AI takes its companion (it
// follows the Avatar, and fights when the party does) but the companion
// stays the guest's, until the guest walks, attacks, switches or presses Y
// again.  For stepping away without losing one's companion.

static bool friend_following = false;    // Guest: the host's AI drives ours.

// What the Class System (friendclass.cc) needs from here.
Friend_mode_session& Friend_mode_get_session() {
	return friend_session;
}

void Friend_mode_class_notice(const std::string& text) {
	if (!text.empty()) {
		Friend_mode_top_line(text);
	}
}

bool Friend_mode_is_hostile(const Actor* own, const Actor* foe) {
	return Friend_is_hostile_target(own, foe);
}

int Friend_mode_player_playing(const Actor* actor) {
	for (const auto& [player, guest] : friend_guests) {
		if (actor && guest.actor >= 0 && guest.actor == actor->get_npc_num() && !guest.following) {
			return player;
		}
	}
	return -1;
}

bool Friend_mode_guest_following() {
	return friend_following;
}

bool Friend_mode_in_session() {
	return friend_session.mode() != Friend_mode_session::Mode::off;
}

// Someone's network identity: 0 and an NPC number, or 1 and a creature's
// number from the host.  False if guests don't know it.
bool Friend_mode_actor_net_id(const Actor* actor, int& kind, uint32& id) {
	if (!actor) {
		return false;
	}
	if (actor->get_npc_num() >= 0) {
		kind = 0;
		id   = static_cast<uint32>(actor->get_npc_num());
		return true;
	}
	kind = 1;
	id   = Friend_creature_id(actor);
	return id != 0;
}

Actor* Friend_mode_actor_by_net_id(int kind, uint32 id) {
	if (kind == 0) {
		return gwin->get_npc(static_cast<int>(id));
	}
	return kind == 1 ? Friend_creature_by_id(id) : nullptr;
}

// Host: start or stop following for a guest, and tell it the result.
// 'restore_wait' leaves the companion waiting for the guest again; an
// attack or a switch sets its schedule itself.
static void Friend_mode_set_follow(
		int player, Friend_guest& guest, bool on, bool restore_wait = true, bool reply = true) {
	Actor* actor = guest.actor >= 0 ? gwin->get_npc(guest.actor) : nullptr;
	on           = on && actor && !actor->is_dead();
	if (on != guest.following) {
		guest.following = on;
		if (actor && on) {
			Friend_mode_end_guest_attack(guest, "following");
			actor->set_schedule_type(gwin->in_combat() ? Schedule::combat : Schedule::follow_avatar);
		} else if (actor && restore_wait) {
			actor->set_target(nullptr);
			actor->stop();
			actor->set_schedule_type(Schedule::wait);
		}
		cout << "Friend Mode: " << guest.name << (on ? " follows the Avatar" : " takes over again") << endl;
	}
	if (reply) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::guest_follow, {static_cast<unsigned char>(on)}, player);
	}
}

// Guest: while one of Exult's menus is open (they stop the main loop), the
// host's AI drives our companion, so it isn't left standing in a fight; it
// is ours again when the menu closes.  Quiet: no bar, no notice.
constexpr unsigned char friend_follow_menu_open  = 2;
constexpr unsigned char friend_follow_menu_close = 3;
static bool             friend_menu_following    = false;

void Friend_mode_modal_menu(bool open) {
	if (friend_session.mode() != Friend_mode_session::Mode::guest || !friend_session.connected()) {
		friend_menu_following = false;
		return;
	}
	if (open && !friend_menu_following && !friend_following && gwin->has_network_friend_control()
		&& !gwin->is_network_spectating()) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::guest_follow, {friend_follow_menu_open});
		friend_menu_following = true;
		friend_session.poll();
	} else if (!open && friend_menu_following) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::guest_follow, {friend_follow_menu_close});
		friend_menu_following = false;
		friend_session.poll();
	}
}

// Guest: the host started or stopped following for us.
static void Friend_mode_apply_follow(const std::vector<unsigned char>& data) {
	const bool on = !data.empty() && data[0] != 0;
	if (on != friend_following) {
		friend_following = on;
		gwin->set_all_dirty();    // The bar appears or goes.
	}
}

// Shared journal.  The host's journal is the real one.  Someone writing in a
// note holds it (the host says who) until the cursor leaves it or the
// journal closes; then a guest sends the finished note, and the host sends
// the whole journal to everyone when it changes.

// Who writes in a note: the host is 1, a guest its player number plus one.
constexpr int friend_writer_host = 1;
static int Friend_journal_writer(int player) {
	return player + 1;
}
constexpr int    friend_journal_new       = 0xffff;    // Edit index of a new note.
constexpr size_t friend_journal_max_text  = 4000;
constexpr uint32 friend_journal_sync_ms   = 500;
static std::map<int, int>         friend_journal_writers;    // Host: note -> writer.
static std::vector<unsigned char> friend_journal_sent;       // Host: last journal sent.
static uint32                     friend_next_journal_sync = 0;

static void Friend_put_note(std::vector<unsigned char>& out, const Notebook_gump::Shared_note& note) {
	Friend_put16(out, note.day);
	Friend_put8(out, note.hour);
	Friend_put8(out, note.minute);
	Friend_put16(out, note.tx);
	Friend_put16(out, note.ty);
	Friend_put16(out, note.gflag + 1);
	const size_t length = std::min(note.text.size(), friend_journal_max_text);
	Friend_put16(out, static_cast<int>(length));
	out.insert(out.end(), note.text.begin(), note.text.begin() + length);
}

static int Friend_get16(const std::vector<unsigned char>& data, size_t& pos) {
	if (pos + 2 > data.size()) {
		pos = data.size() + 1;    // Past the end: marks a bad message.
		return 0;
	}
	const int value = data[pos] | (data[pos + 1] << 8);
	pos += 2;
	return value;
}

static bool Friend_read_note(const std::vector<unsigned char>& data, size_t& pos, Notebook_gump::Shared_note& note) {
	note.day = Friend_get16(data, pos);
	if (pos + 2 > data.size()) {
		return false;
	}
	note.hour   = data[pos++] % 24;
	note.minute = data[pos++] % 60;
	note.tx     = Friend_get16(data, pos);
	note.ty     = Friend_get16(data, pos);
	note.gflag  = Friend_get16(data, pos) - 1;
	const size_t length = Friend_get16(data, pos);
	if (pos > data.size() || pos + length > data.size() || length > friend_journal_max_text) {
		return false;
	}
	note.text.clear();
	for (size_t i = 0; i < length; ++i) {    // Printable text and line breaks only.
		const unsigned char chr = data[pos + i];
		if (chr == '\n' || (chr >= ' ' && chr < 127 && chr != '^')) {
			note.text += static_cast<char>(chr);
		}
	}
	pos += length;
	return true;
}

bool Friend_mode_journal_shared() {
	return friend_session.mode() != Friend_mode_session::Mode::off && friend_session.connected();
}

// Host: tell the guest who writes where, and show the guest's note as taken.
static void Friend_mode_send_journal_writers() {
	std::map<int, std::string> others;
	std::vector<unsigned char> payload;
	Friend_put16(payload, static_cast<int>(friend_journal_writers.size()));
	for (const auto& [note, writer] : friend_journal_writers) {
		Friend_put16(payload, note);
		Friend_put8(payload, writer);
		if (writer != friend_writer_host) {
			others[note] = friend_session.player_name(writer - 1);
		}
	}
	Notebook_gump::set_remote_writers(others);
	if (friend_session.connected()) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::journal_locks, payload);
	}
}

bool Friend_mode_journal_begin_edit(int index) {
	if (!Friend_mode_journal_shared()) {
		return true;
	}
	if (friend_session.mode() == Friend_mode_session::Mode::guest) {
		std::vector<unsigned char> payload;    // The host may still say no.
		Friend_put16(payload, index);
		Friend_put8(payload, 1);
		friend_session.send_blob(Friend_mode_session::Blob_kind::journal_lock, payload);
		return true;
	}
	const auto found = friend_journal_writers.find(index);
	if (found != friend_journal_writers.end() && found->second != friend_writer_host) {
		return false;
	}
	friend_journal_writers[index] = friend_writer_host;
	Friend_mode_send_journal_writers();
	return true;
}

bool Friend_mode_journal_end_edit(
		int index, bool changed, int day, int hour, int minute, int tx, int ty, const std::string& text) {
	if (!Friend_mode_journal_shared()) {
		return false;
	}
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		// The journal goes out on the next sync.
		if (index >= 0 && friend_journal_writers.erase(index)) {
			Friend_mode_send_journal_writers();
		}
		return false;
	}
	std::vector<unsigned char> payload;
	if (!changed) {
		if (index >= 0) {    // Only looked: let go of it.
			Friend_put16(payload, index);
			Friend_put8(payload, 0);
			friend_session.send_blob(Friend_mode_session::Blob_kind::journal_lock, payload);
		}
		return false;
	}
	Friend_put16(payload, index >= 0 ? index : friend_journal_new);
	Friend_put_note(payload, {day, hour, minute, tx, ty, -1, text});
	friend_session.send_blob(Friend_mode_session::Blob_kind::journal_edit, payload);
	cout << "Friend Mode: sent journal note " << index << endl;
	return true;
}

// Host: a guest starts or stops writing in a note.
static void Friend_mode_journal_lock_from_guest(const std::vector<unsigned char>& data, int player) {
	const int writer = Friend_journal_writer(player);
	size_t    pos   = 0;
	const int index = Friend_get16(data, pos);
	if (pos >= data.size()) {
		return;
	}
	const bool lock  = data[pos] != 0;
	const auto found = friend_journal_writers.find(index);
	if (lock) {
		if (found == friend_journal_writers.end()) {
			// One note at a time per guest.
			for (auto it = friend_journal_writers.begin(); it != friend_journal_writers.end();) {
				it = it->second == writer ? friend_journal_writers.erase(it) : std::next(it);
			}
			friend_journal_writers[index] = writer;
		}    // Else someone else has it: the reply tells the guest so.
	} else if (found != friend_journal_writers.end() && found->second == writer) {
		friend_journal_writers.erase(found);
	}
	Friend_mode_send_journal_writers();
}

// Host: a guest finished a note.
static void Friend_mode_journal_edit_from_guest(const std::vector<unsigned char>& data, int player) {
	const int writer = Friend_journal_writer(player);
	size_t                     pos   = 0;
	const int                  index = Friend_get16(data, pos);
	Notebook_gump::Shared_note note;
	if (!Friend_read_note(data, pos, note)) {
		return;
	}
	const auto found = friend_journal_writers.find(index);
	const bool taken = found != friend_journal_writers.end() && found->second != writer;
	if (index == friend_journal_new) {
		if (!note.text.empty() && Notebook_gump::apply_shared_edit(-1, note)) {
			cout << "Friend Mode: guest added a journal note" << endl;
		}
	} else if (!taken && Notebook_gump::apply_shared_edit(index, note)) {
		cout << "Friend Mode: guest changed journal note " << index << endl;
	} else {
		cout << "Friend Mode: refused guest journal note " << index << endl;
	}
	if (found != friend_journal_writers.end() && found->second == writer) {
		friend_journal_writers.erase(found);
	}
	Friend_mode_send_journal_writers();
	// Send it back now, even unchanged: a refused note must go back to ours.
	friend_journal_sent.clear();
	friend_next_journal_sync = 0;
}

// Host: send the journal when it changed (notes finished here or by the
// guest, and automatic notes from the story).
static void Friend_mode_journal_pass(bool force) {
	std::vector<unsigned char>                    payload;
	const std::vector<Notebook_gump::Shared_note> notes = Notebook_gump::get_shared_notes();
	Friend_put16(payload, static_cast<int>(notes.size()));
	for (const auto& note : notes) {
		Friend_put_note(payload, note);
	}
	if (force || payload != friend_journal_sent) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::journal, payload);
		friend_journal_sent = std::move(payload);
	}
}

// Guest: the host's journal and who is writing where.
static void Friend_mode_apply_journal(const std::vector<unsigned char>& data) {
	size_t    pos   = 0;
	const int count = Friend_get16(data, pos);
	std::vector<Notebook_gump::Shared_note> notes;
	for (int i = 0; i < count; ++i) {
		Notebook_gump::Shared_note note;
		if (!Friend_read_note(data, pos, note)) {
			cerr << "Friend Mode: bad journal" << endl;
			return;
		}
		notes.push_back(std::move(note));
	}
	Notebook_gump::set_shared_notes(notes);
}

// Music.  The host's tracks (region music, combat, victory, scripts) play
// on the guest too; the guest's own copy of the world picks none while
// connected, since its eggs and fights would choose differently.
struct Friend_music_state {
	bool        playing    = false;
	int         num        = -1;
	bool        continuous = false;
	int         force      = 0;
	std::string flex;
};
static Friend_music_state friend_host_music;           // Host: what is playing.
static bool               friend_music_from_host = false;    // Guest: playing the host's.

static void Friend_mode_send_music() {
	std::vector<unsigned char> payload;
	Friend_put8(payload, friend_host_music.playing);
	Friend_put16(payload, friend_host_music.num & 0xffff);
	Friend_put8(payload, friend_host_music.continuous);
	Friend_put8(payload, friend_host_music.force);
	const size_t length = std::min<size_t>(friend_host_music.flex.size(), 255);
	Friend_put8(payload, static_cast<int>(length));
	payload.insert(payload.end(), friend_host_music.flex.begin(), friend_host_music.flex.begin() + length);
	friend_session.send_blob(Friend_mode_session::Blob_kind::music, payload);
}

bool Friend_mode_note_music(bool start, int num, bool continuous, int force, const std::string& flex) {
	if (friend_session.mode() == Friend_mode_session::Mode::guest) {
		return friend_music_from_host || !friend_session.connected();
	}
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		const bool stop        = !start || num == -1 || num == 255;
		const bool was_stopped = !friend_host_music.playing || friend_host_music.num == -1 || friend_host_music.num == 255;
		friend_host_music      = {start, num, continuous, force, flex};
		// Exult asks for silence about once a second while nothing plays.
		if (friend_session.connected() && !(stop && was_stopped)) {
			cout << "Friend Mode: sent music " << (start ? "track " + std::to_string(num) : std::string("stop")) << endl;
			Friend_mode_send_music();
		}
	}
	return true;
}

static void Friend_mode_apply_music(const std::vector<unsigned char>& data) {
	if (data.size() < 6) {
		return;
	}
	const bool   playing    = data[0] != 0;
	const int    num        = static_cast<sint16>(data[1] | (data[2] << 8));
	const bool   continuous = data[3] != 0;
	const int    force      = std::min<int>(data[4], MyMidiPlayer::Force_Ogg);
	const size_t length     = data[5];
	if (data.size() < 6 + length) {
		return;
	}
	const std::string flex(data.begin() + 6, data.begin() + 6 + length);
	cout << "Friend Mode: host music " << (playing ? "track " + std::to_string(num) : std::string("stopped")) << endl;
	friend_music_from_host = true;
	if (playing) {
		Audio::get_ptr()->start_music(num, continuous, static_cast<MyMidiPlayer::ForceType>(force), flex);
	} else {
		Audio::get_ptr()->stop_music();
	}
	friend_music_from_host = false;
}

// Missiles, sprite effects and health.  Fights run on the host; guests
// see the arrows, bolts, spells and blood as pictures, and the health shown
// when clicking a foe in combat mode.

enum Friend_actor_ref { friend_ref_none = 0, friend_ref_npc = 1, friend_ref_creature = 2 };

static void Friend_put_actor_ref(std::vector<unsigned char>& out, const Game_object* obj) {
	const Actor* actor = obj ? obj->as_actor() : nullptr;
	uint32       id    = 0;
	int          kind  = friend_ref_none;
	if (actor && (actor->get_npc_num() > 0 || actor == gwin->get_main_actor())) {
		kind = friend_ref_npc;
		id   = static_cast<uint32>(actor->get_npc_num());
	} else if (actor && (id = Friend_creature_id(actor)) != 0) {
		kind = friend_ref_creature;
	}
	Friend_put8(out, kind);
	Friend_put16(out, static_cast<int>(id & 0xffff));
	Friend_put16(out, static_cast<int>(id >> 16));
}

static Actor* Friend_get_actor_ref(const std::vector<unsigned char>& data, size_t& pos) {
	if (pos + 5 > data.size()) {
		pos = data.size() + 1;
		return nullptr;
	}
	const int    kind = data[pos];
	const uint32 id   = data[pos + 1] | (data[pos + 2] << 8) | (data[pos + 3] << 16) | (static_cast<uint32>(data[pos + 4]) << 24);
	pos += 5;
	if (kind == friend_ref_npc) {
		return id == 0 ? gwin->get_main_actor() : gwin->get_npc(static_cast<int>(id));
	}
	return kind == friend_ref_creature ? Friend_creature_by_id(id) : nullptr;
}

static void Friend_put_tile(std::vector<unsigned char>& out, const Tile_coord& t) {
	Friend_put16(out, t.tx);
	Friend_put16(out, t.ty);
	Friend_put8(out, t.tz);
}

static Tile_coord Friend_get_tile(const std::vector<unsigned char>& data, size_t& pos) {
	const int tx = Friend_get16(data, pos);
	const int ty = Friend_get16(data, pos);
	const int tz = pos < data.size() ? data[pos++] : 0;
	return Tile_coord(tx, ty, tz);
}

static bool Friend_mode_sharing_effects() {
	return friend_session.mode() == Friend_mode_session::Mode::host && friend_session.connected() && friend_world_sent;
}

void Friend_mode_note_projectile(const Tile_coord& from, const Tile_coord& to, int weapon, int sprite, int speed) {
	if (!Friend_mode_sharing_effects()) {
		return;
	}
	std::vector<unsigned char> payload;
	Friend_put_tile(payload, from);
	Friend_put_tile(payload, to);
	Friend_put16(payload, weapon & 0xffff);
	Friend_put16(payload, sprite);
	Friend_put8(payload, speed);
	friend_session.send_blob(Friend_mode_session::Blob_kind::projectile, payload);
}

void Friend_mode_note_sprite(
		int num, Game_object* item, const Tile_coord& pos, int xoff, int yoff, int dx, int dy, int delay, int frame,
		int reps) {
	if (!Friend_mode_sharing_effects()) {
		return;
	}
	std::vector<unsigned char> payload;
	Friend_put16(payload, num);
	Friend_put_actor_ref(payload, item);    // Follows an actor, if any.
	Friend_put_tile(payload, pos);
	for (const int value : {xoff, yoff, dx, dy, delay, frame, reps}) {
		Friend_put16(payload, value & 0xffff);    // Signed, 16 bits.
	}
	friend_session.send_blob(Friend_mode_session::Blob_kind::sprite_effect, payload);
}

static void Friend_mode_apply_projectile(const std::vector<unsigned char>& data) {
	size_t           pos    = 0;
	const Tile_coord from   = Friend_get_tile(data, pos);
	const Tile_coord to     = Friend_get_tile(data, pos);
	const int        weapon = static_cast<sint16>(Friend_get16(data, pos));
	const int        sprite = static_cast<sint16>(Friend_get16(data, pos));
	if (pos >= data.size()) {
		return;
	}
	const int speed = data[pos];
	gwin->get_effects()->add_effect(std::make_unique<Projectile_effect>(from, to, weapon, sprite, speed));
}

static void Friend_mode_apply_sprite(const std::vector<unsigned char>& data) {
	size_t           pos  = 0;
	const int        num  = Friend_get16(data, pos);
	Actor*           item = Friend_get_actor_ref(data, pos);
	const Tile_coord at   = Friend_get_tile(data, pos);
	int              values[7];
	for (int& value : values) {
		value = static_cast<sint16>(Friend_get16(data, pos));
	}
	if (pos > data.size()) {
		return;
	}
	const auto [xoff, yoff, dx, dy, delay, frame, reps] = values;
	if (item) {
		gwin->get_effects()->add_effect(std::make_unique<Sprites_effect>(num, item, xoff, yoff, dx, dy, frame, reps));
	} else {
		gwin->get_effects()->add_effect(std::make_unique<Sprites_effect>(num, at, dx, dy, delay, frame, reps));
	}
}

// Host: health of creatures and NPCs near the players, when it changes,
// with strength (the most health): a guest's own copy of a creature rolls
// its own random strength, which made full health look hurt.
static std::map<uint32, int> friend_health_sent;

static void Friend_mode_health_pass() {
	Actor_vector nearby;
	Friend_npcs_near_players(nearby);
	std::vector<unsigned char> payload;
	int                        count = 0;
	Friend_put16(payload, 0);    // Count, filled in below.
	for (Actor* actor : nearby) {
		if (!actor || actor->is_in_party() || actor->is_dead()) {
			continue;
		}
		const uint32 creature = Friend_creature_id(actor);
		if (actor->get_npc_num() <= 0 && creature == 0) {
			continue;    // Not known to guests by number.
		}
		const uint32 key    = actor->get_npc_num() > 0 ? static_cast<uint32>(actor->get_npc_num()) : (creature | 0x80000000U);
		const int    health   = actor->get_property(static_cast<int>(Actor::health));
		const int    strength = actor->get_property(static_cast<int>(Actor::strength));
		const int    state    = (health & 0xffff) | (strength << 16);
		const auto   found    = friend_health_sent.find(key);
		if (found != friend_health_sent.end() && found->second == state) {
			continue;
		}
		friend_health_sent[key] = state;
		Friend_put_actor_ref(payload, actor);
		Friend_put16(payload, health & 0xffff);
		Friend_put16(payload, strength & 0xffff);
		++count;
	}
	if (count > 0) {
		payload[0] = static_cast<unsigned char>(count & 0xff);
		payload[1] = static_cast<unsigned char>(count >> 8);
		friend_session.send_blob(Friend_mode_session::Blob_kind::actor_health, payload);
	}
}

static void Friend_mode_apply_health(const std::vector<unsigned char>& data) {
	size_t    pos   = 0;
	const int count = Friend_get16(data, pos);
	for (int i = 0; i < count && pos < data.size(); ++i) {
		Actor*    actor    = Friend_get_actor_ref(data, pos);
		const int health   = static_cast<sint16>(Friend_get16(data, pos));
		const int strength = static_cast<sint16>(Friend_get16(data, pos));
		if (actor && !actor->is_in_party() && pos <= data.size()) {
			actor->set_property(static_cast<int>(Actor::strength), strength);
			actor->set_property(static_cast<int>(Actor::health), health);
		}
	}
}

// Host: a hit on someone ("combat shows hits"): sent whatever the host's
// own setting, and shown on a guest if the guest's is on.
void Friend_mode_note_hit(Game_object* target, int damage, int health_left) {
	if (!Friend_mode_sharing_effects()) {
		return;
	}
	std::vector<unsigned char> payload;
	Friend_put_actor_ref(payload, target);
	Friend_put16(payload, damage & 0xffff);
	Friend_put16(payload, health_left & 0xffff);
	friend_session.send_blob(Friend_mode_session::Blob_kind::actor_hit, payload);
}

static void Friend_mode_apply_hit(const std::vector<unsigned char>& data) {
	size_t    pos    = 0;
	Actor*    target = Friend_get_actor_ref(data, pos);
	const int damage = static_cast<sint16>(Friend_get16(data, pos));
	const int left   = static_cast<sint16>(Friend_get16(data, pos));
	if (!target || pos > data.size() || !Combat::show_hits) {
		return;
	}
	char text[50];
	snprintf(text, sizeof(text), "-%d(%d)", damage, left);
	gwin->get_effects()->remove_text_effect(target);
	gwin->get_effects()->add_text(text, target);
}

// Host: world sounds (doors, levers and other scripts, hits, explosions),
// so guests hear what happens near them.  Ambient sounds and menu clicks
// are each machine's own and are not sent.
void Friend_mode_note_sound(int sfx, const Game_object* obj, const Tile_coord* tile) {
	if (sfx < 0 || !Friend_mode_sharing_effects()) {
		return;
	}
	std::vector<unsigned char> payload;
	Friend_put16(payload, sfx);
	if (obj || tile) {
		Friend_put8(payload, 1);
		Friend_put_tile(payload, tile ? *tile : obj->get_outermost()->get_center_tile());
	} else {
		Friend_put8(payload, 0);
	}
	friend_session.send_blob(Friend_mode_session::Blob_kind::world_sound, payload);
}

static void Friend_mode_apply_sound(const std::vector<unsigned char>& data) {
	size_t    pos    = 0;
	const int sfx    = Friend_get16(data, pos);
	if (pos >= data.size()) {
		return;
	}
	if (data[pos++] != 0) {
		const Tile_coord tile = Friend_get_tile(data, pos);
		if (pos <= data.size()) {
			Audio::get_ptr()->play_sound_effect(sfx, tile);
		}
	} else {
		Audio::get_ptr()->play_sound_effect(sfx);
	}
}

// Shops.  A guest's companion talks to a merchant on the guest's own copy
// (nobody else sees it, and the story flags it sets stay there); each
// purchase (the item, then the gold) goes to the host, which takes the
// party's gold and gives the item.  See docs/friend-mode/shops-and-trainers.md.

// Black Gate merchants and trainers, by NPC number
// (tools/friend-mode/shops_and_trainers.py, "shop" and "train" rows).
static const std::unordered_set<int> friend_merchants{
		18,  19,  24,  37,  38,  47,  50,  51,  52,  53,  54,  55,  59,  67,  74,  79,  87,  92,  102, 118, 121, 123,
		130, 153, 163, 179, 181, 188, 189, 190, 194, 196, 198, 199, 206, 207, 215, 216, 219, 222, 229, 231, 237,
		// Trainers (Inforlem, 181, does both).
		7, 20, 36, 49, 76, 94, 95, 104, 115, 119, 159, 161, 192, 205, 228, 238};
constexpr int friend_gold_shape = 644;

// One a guest may talk to (a merchant or trainer).
static bool Friend_is_merchant(int npc_num) {
	return GAME_BG && friend_merchants.count(npc_num) != 0;
}

// The answers a guest sees with each of them: name, job, bye, and those
// that lead to buying, selling or training; not the story ("murder").
// Made by tools/friend-mode/merchant_answers.py.
static const std::unordered_map<int, std::vector<std::string>> friend_merchant_answers{
		{7, {"bye", "job", "name", "train"}},
		{18, {"armour", "buy", "bye", "job", "name", "nothing", "provisions", "weapons"}},
		{19, {"buy", "bye", "job", "name"}},
		{20, {"bye", "job", "name", "train"}},
		{24, {"bye", "job", "magic", "name", "reagents"}},
		{36, {"bye", "job", "name", "train"}},
		{37, {"buy", "bye", "job", "name"}},
		{38, {"buy", "bye", "job", "name"}},
		{47, {"buy", "bye", "job", "name"}},
		{49, {"bye", "job", "name", "train"}},
		{50, {"buy", "bye", "job", "meats", "name"}},
		{51, {"buy", "bye", "job", "name"}},
		{52, {"bread", "buy", "bye", "job", "name", "sell flour"}},
		{53, {"buy", "bye", "job", "name"}},
		{54, {"buy", "bye", "job", "name"}},
		{55, {"armour", "buy", "bye", "job", "name", "weapons"}},
		{59, {"buy", "bye", "job", "name"}},
		{67, {"bye", "job", "mutton", "name"}},
		{74, {"bye", "job", "name", "reagents"}},
		{76, {"bye", "job", "name", "train"}},
		{79, {"buy", "bye", "job", "name"}},
		{87, {"blacksmith", "buy armour", "buy weapons", "bye", "job", "name", "weapons and armour"}},
		{92, {"buy", "bye", "job", "name"}},
		{94, {"bye", "job", "name", "teacher"}},
		{95, {"bye", "job", "name", "trainer"}},
		{102, {"bye", "job", "name", "reagents"}},
		{104, {"bye", "job", "name", "train"}},
		{115, {"bye", "job", "name", "train"}},
		{118, {"buy refreshment", "bye", "drink", "food", "job", "name", "tailor"}},
		{119, {"Library of Scars", "bye", "dagger", "demonstration", "duellists", "job", "name", "potential", "weaponry"}},
		{121, {"Armoury", "armour", "buy", "bye", "job", "name", "weapons"}},
		{123, {"buy", "bye", "job", "name"}},
		{130, {"Modest Damsel", "bye", "eat or drink", "job", "name"}},
		{153, {"bye", "ink wells", "job", "name", "potions", "quills", "reagents", "sell", "shelves", "they"}},
		{159, {"bye", "job", "name", "tutor"}},
		{161, {"bye", "job", "name", "train"}},
		{163, {"buy", "bye", "drink", "food", "job", "name"}},
		{179, {"buy", "bye", "job", "name"}},
		{181, {"buy", "bye", "job", "mage", "name", "train", "warrior"}},
		{188, {"buy", "bye", "jewelry and potions", "job", "name", "reagents"}},
		{189, {"buy", "bye", "job", "name"}},
		{190, {"buy", "bye", "job", "name"}},
		{192, {"bye", "job", "name", "train"}},
		{194, {"bye", "job", "name", "provisions"}},
		{196, {"armour", "bye", "job", "name", "weapons"}},
		{198, {"bye", "job", "missiles", "name", "sell", "weapons"}},
		{199, {"bye", "job", "name", "sell"}},
		{205, {"bye", "job", "name", "train"}},
		{206, {"buy", "bye", "job", "name"}},
		{207, {"buy", "bye", "drink", "food", "job", "name"}},
		{215, {"buy", "bye", "job", "name"}},
		{216, {"bye", "job", "magic", "name", "potions", "reagents", "sell"}},
		{219, {"buy provisions", "bye", "job", "name"}},
		{222, {"bye", "drink", "job", "name"}},
		{228, {"bye", "job", "name", "train"}},
		{229, {"armour", "bye", "job", "name", "provisions", "weapons"}},
		{231, {"buy", "bye", "job", "name"}},
		{237, {"apothecary", "buy potions", "bye", "job", "name", "potions"}},
		{238, {"bye", "job", "name"}},
};


static std::unordered_map<int, int>                    friend_host_schedules;    // Guest: NPC -> host's schedule.
static std::vector<Usecode_machine::Party_trade_item> friend_pending_adds;      // Guest: bought, not paid yet.
// Guest: gold paid for nothing yet (training pays first), and the
// companion's stat changes that follow it.
static std::optional<Usecode_machine::Party_trade_item> friend_pending_payment;
static std::vector<std::array<int, 2>>                  friend_pending_training;    // Property, change.
static std::vector<unsigned char>                      friend_flags_sent;        // Host: last flags sent.
static std::vector<unsigned char>                      friend_flags_waiting;     // Guest: arrived mid-conversation.
static std::vector<unsigned char>                      friend_host_flags;        // Guest: the host's, last applied.

static void Friend_mode_apply_story_flags(const std::vector<unsigned char>& data);

int Friend_mode_host_schedule(const Actor* npc) {
	if (!npc || friend_session.mode() != Friend_mode_session::Mode::guest) {
		return -1;
	}
	const auto found = friend_host_schedules.find(npc->get_npc_num());
	return found == friend_host_schedules.end() ? -1 : found->second;
}

// Guest: send one purchase or training session to the host: what was paid,
// what the party got, and the companion's stat changes.
static void Friend_mode_send_trade(
		const Usecode_machine::Party_trade_item& payment, const std::vector<Usecode_machine::Party_trade_item>& adds,
		const std::vector<std::array<int, 2>>& training) {
	if (!friend_session.connected()) {
		return;
	}
	std::vector<unsigned char> payload;
	auto put_item = [&](const Usecode_machine::Party_trade_item& item) {
		for (const int value : item) {
			Friend_put16(payload, value & 0xffff);
		}
	};
	Friend_put8(payload, 1);
	put_item(payment);
	Friend_put8(payload, static_cast<int>(adds.size()));
	for (const auto& item : adds) {
		put_item(item);
	}
	Friend_put8(payload, static_cast<int>(training.size()));
	for (const auto& [prop, delta] : training) {
		Friend_put8(payload, prop);
		Friend_put16(payload, delta & 0xffff);
	}
	friend_session.send_blob(Friend_mode_session::Blob_kind::guest_trade, payload);
	cout << "Friend Mode: sent a " << (training.empty() ? "purchase" : "training session") << " (" << payment[0]
		 << " gold)" << endl;
}

// Guest: a payment held for training goes out with the stat changes made
// since (none: a payment for nothing, which stays here for now).
static void Friend_mode_flush_training() {
	if (friend_pending_payment && !friend_pending_training.empty()) {
		Friend_mode_send_trade(*friend_pending_payment, {}, friend_pending_training);
	}
	friend_pending_payment.reset();
	friend_pending_training.clear();
}

void Friend_mode_note_party_trade(bool add, int quantity, int shape, int quality, int frame) {
	if (friend_session.mode() != Friend_mode_session::Mode::guest) {
		return;
	}
	if (add) {
		friend_pending_adds.push_back({quantity, shape, quality, frame});
		return;
	}
	Friend_mode_flush_training();    // A new payment ends the last one.
	if (friend_pending_adds.empty()) {
		// Paid first: training (its stat changes come next).
		friend_pending_payment = Usecode_machine::Party_trade_item{quantity, shape, quality, frame};
		return;
	}
	// Paying for what the party just got.
	Friend_mode_send_trade({quantity, shape, quality, frame}, friend_pending_adds, {});
	friend_pending_adds.clear();
}

void Friend_mode_note_training(int prop, int delta) {
	if (friend_session.mode() == Friend_mode_session::Mode::guest && friend_pending_payment) {
		friend_pending_training.push_back({prop, delta});
	}
}

// Host: a guest's purchase.  Only gold may be paid; the party must have it.
static void Friend_mode_trade_for_guest(const std::vector<unsigned char>& data, Actor* actor) {
	if (!actor) {
		return;
	}
	size_t pos       = 0;
	auto   read_list = [&](std::vector<Usecode_machine::Party_trade_item>& out) {
        const int count = pos < data.size() ? data[pos++] : 0;
        for (int i = 0; i < count && i < 8; ++i) {
            Usecode_machine::Party_trade_item item{};
            for (int& value : item) {
                value = static_cast<sint16>(Friend_get16(data, pos));
            }
            out.push_back(item);
        }
	};
	std::vector<Usecode_machine::Party_trade_item> removes;
	std::vector<Usecode_machine::Party_trade_item> adds;
	read_list(removes);
	read_list(adds);
	// Training: the companion's stat changes (strength, dexterity,
	// intelligence, combat, mana, magic, training points), small ones only.
	std::vector<std::array<int, 2>> training;
	const int                       stat_count = pos < data.size() ? data[pos++] : 0;
	for (int i = 0; i < stat_count && i < 8; ++i) {
		const int prop  = pos < data.size() ? data[pos++] : -1;
		const int delta = static_cast<sint16>(Friend_get16(data, pos));
		training.push_back({prop, delta});
	}
	const bool sane_training = std::all_of(training.begin(), training.end(), [](const auto& change) {
		static const std::unordered_set<int> trainable{
				Actor::strength, Actor::dexterity, Actor::intelligence, Actor::combat,
				Actor::mana,     Actor::magic,     Actor::training};
		return trainable.count(change[0]) && change[1] >= -10 && change[1] <= 10;
	});
	const bool gold_only = !removes.empty()
						   && std::all_of(removes.begin(), removes.end(), [](const auto& item) {
								  return item[1] == friend_gold_shape && item[0] > 0;
							  });
	const bool sane_adds = std::all_of(adds.begin(), adds.end(), [](const auto& item) {
		return item[0] > 0 && item[0] <= 500 && item[1] >= 0;
	});
	// Something must be bought: items, or training.
	if (pos > data.size() || !gold_only || !sane_adds || !sane_training || (adds.empty() == training.empty())) {
		cout << "Friend Mode: refused guest purchase (not a purchase)" << endl;
		Friend_force_item_resync(actor);
		return;
	}
	// The guest already showed the purchase on its copy: resend every party
	// pack either way, so a refused one goes back.
	for (auto& entry : friend_inventory_hashes) {
		entry.second = 0;
	}
	friend_next_item_sync = 0;
	if (!gwin->get_usecode()->friend_party_trade(removes, adds, actor)) {
		cout << "Friend Mode: refused guest purchase (the party can't pay)" << endl;
		return;
	}
	for (const auto& [prop, delta] : training) {    // Only ever the guest's own companion.
		actor->set_property(prop, actor->get_property(prop) + delta);
	}
	if (training.empty()) {
		cout << "Friend Mode: " << actor->get_name() << " bought shape " << adds.front()[1] << " for "
			 << removes.front()[0] << " gold" << endl;
	} else {
		cout << "Friend Mode: " << actor->get_name() << " trained (" << training.size() << " changes) for "
			 << removes.front()[0] << " gold" << endl;
	}
}

bool Friend_mode_guest_talk_to_merchant(Actor* npc) {
	Actor* own = gwin->get_controlled_actor();
	if (!GAME_BG || !npc || !own || friend_session.mode() != Friend_mode_session::Mode::guest
		|| !friend_merchants.count(npc->get_npc_num())) {
		return false;
	}
	Usecode_machine* usecode = gwin->get_usecode();
	friend_pending_adds.clear();
	friend_pending_payment.reset();
	friend_pending_training.clear();
	usecode->set_friend_shopper(own);
	const auto answers = friend_merchant_answers.find(npc->get_npc_num());
	if (answers != friend_merchant_answers.end()) {
		usecode->set_friend_answers(npc->get_usecode(), answers->second);
	}
	gwin->get_effects()->remove_text_effects();
	usecode->init_conversation();
	npc->activate();
	usecode->set_friend_shopper(nullptr);
	usecode->set_friend_answers(-1, {});
	Friend_mode_flush_training();
	friend_pending_adds.clear();
	// Throw away whatever else the conversation changed on this copy: the
	// host's story flags go back, and the host resends the party's packs
	// and the items nearby (a free gift, say, never reached the host).
	std::vector<unsigned char> flags;
	flags.swap(friend_flags_waiting);
	if (flags.empty()) {
		flags = friend_host_flags;
	}
	if (!flags.empty()) {
		Friend_mode_apply_story_flags(flags);
	}
	if (friend_session.connected()) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::resync_request, {1});
	}
	return true;
}

// Host: the story flags, as bits, when they change.
static void Friend_mode_story_flags_pass(bool force) {
	Usecode_machine*           usecode = gwin->get_usecode();
	const int                  count   = static_cast<int>(usecode->get_num_global_flags());
	std::vector<unsigned char> payload;
	Friend_put16(payload, count & 0xffff);
	payload.resize(2 + (count + 7) / 8, 0);
	for (int i = 0; i < count; ++i) {
		if (usecode->get_global_flag_unsafe(i)) {
			payload[2 + i / 8] |= static_cast<unsigned char>(1 << (i % 8));
		}
	}
	if (force || payload != friend_flags_sent) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::story_flags, payload);
		friend_flags_sent = std::move(payload);
	}
}

// Guest: take the host's story flags (after a merchant conversation, if
// one is open, so its answers don't change under it).
static void Friend_mode_apply_story_flags(const std::vector<unsigned char>& data) {
	Usecode_machine* usecode = gwin->get_usecode();
	if (usecode->get_friend_shopper()) {
		friend_flags_waiting = data;
		return;
	}
	size_t    pos   = 0;
	const int count = Friend_get16(data, pos);
	for (int i = 0; i < count && 2 + static_cast<size_t>(i / 8) < data.size(); ++i) {
		usecode->set_global_flag(i, (data[2 + i / 8] >> (i % 8)) & 1);
	}
	if (&data != &friend_host_flags) {
		friend_host_flags = data;
	}
}

// Both sides: say when our journal opens or closes, so the other side can
// show "Someone is writing in the party journal."
static bool friend_journal_open_sent = false;
static bool friend_peer_journal_open = false;

// The host tells each guest whether anyone else (the host or another
// guest) has the journal open.
static void Friend_mode_journal_open_pass(bool force) {
	if (!friend_session.connected()) {
		friend_journal_open_sent = false;
		if (friend_peer_journal_open) {
			friend_peer_journal_open = false;
			gwin->set_all_dirty();
		}
		return;
	}
	const bool open = Notebook_gump::get_instance() != nullptr;
	if (friend_session.mode() == Friend_mode_session::Mode::guest) {
		if (force || open != friend_journal_open_sent) {
			friend_journal_open_sent = open;
			friend_session.send_blob(
					Friend_mode_session::Blob_kind::journal_open, {static_cast<unsigned char>(open)});
		}
		return;
	}
	bool any_guest = false;
	for (auto& [player, guest] : friend_guests) {
		any_guest = any_guest || guest.journal_open;
		bool others = open;
		for (const auto& [other, entry] : friend_guests) {
			others = others || (other != player && entry.journal_open);
		}
		if (force || others != guest.journal_open_sent) {
			guest.journal_open_sent = others;
			friend_session.send_blob(
					Friend_mode_session::Blob_kind::journal_open, {static_cast<unsigned char>(others)}, player);
		}
	}
	if (any_guest != friend_peer_journal_open) {
		friend_peer_journal_open = any_guest;
		gwin->set_all_dirty();    // The bar appears or goes.
	}
}

// Guest: someone else's journal opened or closed.  Host: a guest's did.
static void Friend_mode_apply_journal_open(const std::vector<unsigned char>& data, int player = 0) {
	const bool open = !data.empty() && data[0] != 0;
	if (Friend_guest* guest = Friend_guest_of(player)) {
		guest->journal_open = open;
		return;    // The next pass updates the bar and tells the others.
	}
	if (open != friend_peer_journal_open) {
		friend_peer_journal_open = open;
		gwin->set_all_dirty();    // The bar appears or goes.
	}
}

static void Friend_mode_apply_journal_writers(const std::vector<unsigned char>& data) {
	size_t                     pos   = 0;
	const int                  count = Friend_get16(data, pos);
	std::map<int, std::string> others;
	for (int i = 0; i < count && pos + 3 <= data.size(); ++i) {
		const int note   = Friend_get16(data, pos);
		const int writer = data[pos++];
		if (writer == friend_writer_host) {
			others[note] = friend_session.remote_player_name_value();
		} else if (writer != Friend_journal_writer(friend_session.local_player())) {
			others[note] = Friend_mode_player_name(writer - 1);
		}
	}
	Notebook_gump::set_remote_writers(others);
}

// Guest: show text an object spoke on the host.  Its shape may already have
// changed here or not yet (a chest unlocks by swapping shape), so fall back
// to a door, then to anything else on that tile.
static void Friend_mode_apply_object_speech(const std::vector<unsigned char>& data) {
	if (data.size() < 8) {
		return;
	}
	auto            get16 = [&](std::size_t at) { return data[at] | (data[at + 1] << 8); };
	const Tile_coord tile(get16(0), get16(2), data[4]);
	const int        shape = get16(5);
	const std::string text(data.begin() + 7, data.end());
	Game_object* obj = Friend_find_door(tile, shape, c_any_framenum);
	if (!obj) {
		Game_object_vector found;
		Game_object::find_nearby(found, tile, c_any_shapenum, 0, 0xb0);
		for (Game_object* candidate : found) {
			if (candidate->get_tile() == tile && !candidate->get_owner()) {
				obj = candidate;
				break;
			}
		}
	}
	if (obj) {
		gwin->get_effects()->remove_text_effect(obj);
		gwin->get_effects()->add_text(text.c_str(), obj);
	}
}

// ---- Party status ---------------------------------------------------------
// The host owns health, food and status effects (poison runs on its timers).
// It sends each party member's status when it changes; the guest copies it
// without starting timers of its own, and flashes the hit outline when
// health drops so poison and damage show.

namespace {
constexpr int friend_status_flags[] = {Obj_flags::asleep,    Obj_flags::charmed,    Obj_flags::cursed,
									   Obj_flags::paralyzed, Obj_flags::poisoned,   Obj_flags::protection,
									   Obj_flags::might};
std::vector<unsigned char> friend_last_party_status;
}    // namespace

// Stats sent with party status (after health and food).
constexpr std::array<int, 7> friend_status_stats{
		Actor::strength, Actor::dexterity, Actor::intelligence, Actor::combat,
		Actor::mana,     Actor::magic,     Actor::training};

static std::vector<unsigned char> Friend_party_status() {
	std::vector<Actor*> party_actors{gwin->get_main_actor()};
	Party_manager*      party = gwin->get_party_man();
	for (int i = 0; i < party->get_count(); ++i) {
		party_actors.push_back(gwin->get_npc(party->get_member(i)));
	}
	std::vector<unsigned char> out;
	for (Actor* actor : party_actors) {
		if (!actor) {
			continue;
		}
		int bits = 0;
		for (std::size_t i = 0; i < std::size(friend_status_flags); ++i) {
			if (actor->get_flag(friend_status_flags[i])) {
				bits |= 1 << i;
			}
		}
		Friend_put16(out, actor->get_npc_num());
		Friend_put8(out, bits);
		Friend_put16(out, actor->get_property(static_cast<int>(Actor::health)) + 128);    // Can go negative.
		Friend_put8(out, actor->get_property(static_cast<int>(Actor::food_level)));
		// Stats, training points and experience (the level comes from it),
		// so training and level-ups show on guests.
		for (const int prop : friend_status_stats) {
			Friend_put16(out, actor->get_property(prop) & 0xffff);
		}
		const int exp = actor->get_property(static_cast<int>(Actor::exp));
		Friend_put16(out, exp & 0xffff);
		Friend_put16(out, (exp >> 16) & 0xffff);
	}
	return out;
}

// Host: send party status if it changed (or always, after a world send).
static void Friend_mode_status_pass(bool force) {
	std::vector<unsigned char> status = Friend_party_status();
	if (force || status != friend_last_party_status) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::party_status, status);
		friend_last_party_status = std::move(status);
	}
}

static void Friend_mode_apply_party_status(const std::vector<unsigned char>& data) {
	auto              get16 = [&](std::size_t at) { return data[at] | (data[at + 1] << 8); };
	const std::size_t entry = 6 + 2 * std::size(friend_status_stats) + 4;
	for (std::size_t at = 0; at + entry <= data.size(); at += entry) {
		const int npc    = get16(at);
		const int bits   = data[at + 2];
		const int health = get16(at + 3) - 128;
		const int food   = data[at + 5];
		Actor*    actor  = npc == 0 ? static_cast<Actor*>(gwin->get_main_actor()) : gwin->get_npc(npc);
		if (!actor) {
			continue;
		}
		for (std::size_t i = 0; i < std::size(friend_status_flags); ++i) {
			const bool on = (bits & (1 << i)) != 0;
			if (actor->get_flag(friend_status_flags[i]) != on) {
				actor->set_friend_status_flag(friend_status_flags[i], on);
			}
		}
		if (health < actor->get_property(static_cast<int>(Actor::health))) {
			actor->flash_hit();
		}
		actor->set_property(static_cast<int>(Actor::health), health);
		actor->set_property(static_cast<int>(Actor::food_level), food);
		// Experience first: on its own it would add training points for a
		// new level, which the host's training value then replaces.
		const std::size_t stats = at + 6;
		const std::size_t exp_at = stats + 2 * std::size(friend_status_stats);
		actor->set_property(static_cast<int>(Actor::exp), get16(exp_at) | (get16(exp_at + 2) << 16));
		for (std::size_t i = 0; i < std::size(friend_status_stats); ++i) {
			actor->set_property(friend_status_stats[i], static_cast<sint16>(get16(stats + 2 * i)));
		}
	}
	gwin->set_all_dirty();    // Portraits show poison and health; stats windows.
}

// ---- Game clock and weather ---------------------------------------------
// The host sends its day, time and weather twice a second when they change.
// The guest's clock stops advancing on its own (hunger, healing and schedule
// changes stay with the host) and jumps to the host's time.

namespace {
constexpr uint32           friend_clock_sync_ms = 500;
std::vector<unsigned char> friend_last_clock;
int                        friend_guest_weather = 0;    // Weather the guest last applied.
}    // namespace

static void Friend_mode_clock_pass(bool force) {
	const Game_clock*          clock = gwin->get_clock();
	std::vector<unsigned char> out;
	Friend_put16(out, clock->get_day() & 0xffff);
	Friend_put16(out, clock->get_day() >> 16);
	Friend_put8(out, clock->get_hour());
	Friend_put8(out, clock->get_minute());
	Friend_put8(out, clock->get_ticks());
	Friend_put8(out, gwin->get_effects()->get_weather());
	if (force || out != friend_last_clock) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::game_clock, out);
		friend_last_clock = std::move(out);
	}
}

static void Friend_mode_apply_clock(const std::vector<unsigned char>& data) {
	if (data.size() < 8) {
		return;
	}
	const int day = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	Game_clock* clock = gwin->get_clock();
	clock->set_network_follower(true);
	clock->apply_network_time(day, data[4], data[5], data[6]);
	const int weather = data[7];
	if (weather != friend_guest_weather) {
		friend_guest_weather = weather;
		Egg_object::set_weather(weather, 0, nullptr);    // Lasts until the host's changes.
	}
}

// ---- Creature sync -------------------------------------------------------
// Monsters have no NPC number, so the host gives each one near the players a
// network ID.  When the set of creatures near the players changes (one
// appears, dies or leaves), the host sends the whole list; guests create the
// ones they lack, frozen, and delete the rest, including any their own world
// made.  Moves go out every 100 ms as creature states keyed by ID.

namespace {
struct Friend_creature_entry {
	uint32     id    = 0;
	int        shape = 0, frame = 0, align = 0, flags = 0;
	Tile_coord tile;
};

enum Friend_creature_flags { friend_creature_invisible = 1 };

uint32                                      friend_next_creature_id = 1;
std::vector<unsigned char>                  friend_last_creature_set;    // Host: IDs, shapes, alignment, flags.
std::unordered_map<uint32, uint64>          friend_creature_sent;        // Host: ID -> last tile and frame sent.
std::map<uint32, Friend_creature_entry>     friend_guest_creatures;      // Guest: the host's list, kept current.
uint32                                      friend_next_guest_creature_pass = 0;
bool                                        friend_guest_have_creatures     = false;    // A list has arrived.

uint64 Friend_pack_creature_state(const Tile_coord& t, int frame) {
	return static_cast<uint64>(t.tx & 0xffff) | (static_cast<uint64>(t.ty & 0xffff) << 16)
		   | (static_cast<uint64>(t.tz & 0xff) << 32) | (static_cast<uint64>(frame & 0xff) << 40);
}
}    // namespace

// A creature the host syncs: no NPC number, alive, on the map near players.
static bool Friend_creature_in_sync_area(Monster_actor* creature, const std::unordered_set<int>& chunks) {
	if (creature->get_npc_num() > 0 || creature->is_dead() || creature->is_pos_invalid()
		|| creature->get_map() != gwin->get_main_actor()->get_map()) {
		return false;
	}
	return chunks.count(creature->get_cy() * c_num_chunks + creature->get_cx()) != 0;
}

// Host: send the creature list if creatures appeared or went (or always).
static void Friend_mode_creature_pass(bool force) {
	if (!gwin->get_main_actor()) {
		return;
	}
	const std::unordered_set<int> chunks = Friend_sync_chunks();
	std::vector<Monster_actor*>   creatures;
	for (Monster_actor* creature = Monster_actor::get_first_in_world(); creature;
		 creature = creature->get_next_in_world()) {
		if (!Friend_creature_in_sync_area(creature, chunks)) {
			continue;
		}
		if (creature->get_friend_net_id() == 0) {
			creature->set_friend_net_id(friend_next_creature_id++);
		}
		creatures.push_back(creature);
	}
	std::sort(creatures.begin(), creatures.end(), [](const Monster_actor* a, const Monster_actor* b) {
		return a->get_friend_net_id() < b->get_friend_net_id();
	});
	std::vector<unsigned char> set;
	std::vector<unsigned char> payload;
	Friend_put16(payload, static_cast<int>(creatures.size()));
	for (Monster_actor* creature : creatures) {
		const uint32     id    = creature->get_friend_net_id();
		const int        flags = creature->get_flag(Obj_flags::invisible) ? friend_creature_invisible : 0;
		const Tile_coord t     = creature->get_tile();
		for (std::vector<unsigned char>* out : {&set, &payload}) {
			Friend_put16(*out, static_cast<int>(id & 0xffff));
			Friend_put16(*out, static_cast<int>(id >> 16));
			Friend_put16(*out, creature->get_shapenum());
			Friend_put8(*out, creature->get_alignment());
			Friend_put8(*out, flags);
		}
		Friend_put8(payload, creature->get_framenum());
		Friend_put16(payload, t.tx);
		Friend_put16(payload, t.ty);
		Friend_put8(payload, t.tz);
	}
	if (!force && set == friend_last_creature_set) {
		return;
	}
	friend_last_creature_set = std::move(set);
	friend_creature_sent.clear();
	for (Monster_actor* creature : creatures) {
		friend_creature_sent[creature->get_friend_net_id()]
				= Friend_pack_creature_state(creature->get_tile(), creature->get_framenum());
	}
	friend_session.send_blob(Friend_mode_session::Blob_kind::creature_list, payload);
	cout << "Friend Mode: sent " << creatures.size() << " creature(s)" << endl;
}

// Host: send the creatures in the list that moved or changed frame.
static void Friend_mode_creature_moves() {
	for (Monster_actor* creature = Monster_actor::get_first_in_world(); creature;
		 creature = creature->get_next_in_world()) {
		auto found = friend_creature_sent.find(creature->get_friend_net_id());
		if (found == friend_creature_sent.end() || creature->is_pos_invalid()) {
			continue;
		}
		const Tile_coord t     = creature->get_tile();
		const uint64     state = Friend_pack_creature_state(t, creature->get_framenum());
		if (state != found->second) {
			found->second = state;
			friend_session.send_creature_state(found->first, t.tx, t.ty, t.tz, creature->get_framenum());
		}
	}
}

// Guest: make a creature's look and place match the host's entry.
static void Friend_creature_match(Monster_actor* creature, const Friend_creature_entry& entry) {
	if (creature->get_tile() != entry.tile) {
		creature->move(entry.tile.tx, entry.tile.ty, entry.tile.tz);
	}
	if (creature->get_framenum() != entry.frame) {
		creature->change_frame(entry.frame);
	}
	const bool invisible = (entry.flags & friend_creature_invisible) != 0;
	if (creature->get_flag(Obj_flags::invisible) != invisible) {
		if (invisible) {
			creature->set_flag(Obj_flags::invisible);
		} else {
			creature->clear_flag(Obj_flags::invisible);
		}
	}
}

// Guest: keep exactly the host's creatures.  Runs when the list arrives and
// a few times a second, since the guest's own map loading can add creatures
// (from the map files) or drop them.
static void Friend_mode_guest_creature_pass(bool now) {
	const uint32 ticks = SDL_GetTicks();
	if (!now && ticks < friend_next_guest_creature_pass) {
		return;
	}
	friend_next_guest_creature_pass = ticks + friend_item_sync_ms;
	if (!friend_guest_have_creatures || !gwin->get_main_actor()) {
		return;
	}
	std::vector<Monster_actor*> extra;
	std::unordered_set<uint32>  present;
	for (Monster_actor* creature = Monster_actor::get_first_in_world(); creature;
		 creature = creature->get_next_in_world()) {
		if (creature->get_npc_num() > 0) {
			continue;    // Numbered NPCs follow NPC states.
		}
		const uint32 id    = creature->get_friend_net_id();
		auto         found = friend_guest_creatures.find(id);
		if (id == 0 || found == friend_guest_creatures.end() || found->second.shape != creature->get_shapenum()
			|| !present.insert(id).second) {
			extra.push_back(creature);
		}
	}
	bool changed = !extra.empty();
	for (Monster_actor* creature : extra) {
		gwin->add_dirty(creature);
		creature->remove_this();
	}
	Game_map* map = gwin->get_map();
	for (const auto& [id, entry] : friend_guest_creatures) {
		if (present.count(id)) {
			continue;
		}
		const int cx = entry.tile.tx / c_tiles_per_chunk;
		const int cy = entry.tile.ty / c_tiles_per_chunk;
		if (!map || !map->is_chunk_read(cx, cy)) {
			continue;    // Created once the guest's map has that area.
		}
		const Game_object_shared made
				= Monster_actor::create(entry.shape, entry.tile, Schedule::wait, entry.align, true, false);
		auto* creature = static_cast<Monster_actor*>(made.get());
		creature->set_friend_net_id(id);
		creature->set_alignment(entry.align);
		creature->stop();
		Friend_creature_match(creature, entry);
		changed = true;
	}
	if (changed) {
		gwin->set_all_dirty();
	}
}

static void Friend_mode_apply_creature_list(const std::vector<unsigned char>& data) {
	auto get16 = [&](std::size_t at) { return data[at] | (data[at + 1] << 8); };
	if (data.size() < 2) {
		return;
	}
	const int   count = get16(0);
	std::size_t at    = 2;
	friend_guest_creatures.clear();
	friend_guest_have_creatures = true;
	for (int i = 0; i < count && at + 14 <= data.size(); ++i, at += 14) {
		Friend_creature_entry entry;
		entry.id    = static_cast<uint32>(get16(at)) | (static_cast<uint32>(get16(at + 2)) << 16);
		entry.shape = get16(at + 4);
		entry.align = data[at + 6];
		entry.flags = data[at + 7];
		entry.frame = data[at + 8];
		entry.tile  = Tile_coord(get16(at + 9), get16(at + 11), data[at + 13]);
		friend_guest_creatures[entry.id] = entry;
	}
	Friend_mode_guest_creature_pass(true);
	// Existing creatures may have moved since the last states.
	for (const auto& [id, entry] : friend_guest_creatures) {
		if (Monster_actor* creature = Friend_creature_by_id(id)) {
			Friend_creature_match(creature, entry);
		}
	}
}

static void Friend_mode_apply_creature_state(uint32 id, const Tile_coord& tile, int frame) {
	auto found = friend_guest_creatures.find(id);
	if (found == friend_guest_creatures.end()) {
		return;    // Not (yet) in the host's list.
	}
	found->second.tile  = tile;
	found->second.frame = frame;
	if (Monster_actor* creature = Friend_creature_by_id(id)) {
		Friend_creature_match(creature, found->second);
	}
}

bool Friend_mode_is_guest() {
	return friend_session.mode() == Friend_mode_session::Mode::guest;
}

// Keyboard play options (exult.h): WASD movement (Game Input) and the
// Target System (its own options page).  Each machine keeps its own.
bool                            Friend_wasd_movement  = false;
bool                            Friend_keyboard_targeting = true;
bool                            Friend_targeting_circle      = true;
bool                            Friend_targeting_party       = true;
bool                            Friend_targeting_loot        = true;
bool                            Friend_targeting_decor       = false;
bool                            Friend_targeting_furniture   = false;
bool                            Friend_targeting_interactive = false;
bool                            Friend_targeting_levers      = true;
bool                            Friend_targeting_signs       = true;
bool                            Friend_sort_key              = true;

// The Target System options, from the config (at start) and back to it
// (the options page's OK).
void Friend_targeting_load_settings() {
	config->value("config/gameplay/keyboard_targeting", Friend_keyboard_targeting, true);
	config->value("config/gameplay/targeting/range_circle", Friend_targeting_circle, true);
	config->value("config/gameplay/targeting/party", Friend_targeting_party, true);
	config->value("config/gameplay/targeting/loot", Friend_targeting_loot, true);
	config->value("config/gameplay/targeting/decor", Friend_targeting_decor, false);
	config->value("config/gameplay/targeting/furniture", Friend_targeting_furniture, false);
	config->value("config/gameplay/targeting/doors_and_lamps", Friend_targeting_interactive, false);
	config->value("config/gameplay/targeting/levers", Friend_targeting_levers, true);
	config->value("config/gameplay/targeting/signs", Friend_targeting_signs, true);
	config->value("config/gameplay/smart_placement", Gump::smart_placement, true);
	config->value("config/gameplay/sort_key", Friend_sort_key, true);
	Gump::sort_button = Friend_sort_key;
}

void Friend_targeting_save_settings() {
	auto put = [](const char* key, bool on) {
		config->set(key, on ? "yes" : "no", false);
	};
	put("config/gameplay/keyboard_targeting", Friend_keyboard_targeting);
	put("config/gameplay/targeting/range_circle", Friend_targeting_circle);
	put("config/gameplay/targeting/party", Friend_targeting_party);
	put("config/gameplay/targeting/loot", Friend_targeting_loot);
	put("config/gameplay/targeting/decor", Friend_targeting_decor);
	put("config/gameplay/targeting/furniture", Friend_targeting_furniture);
	put("config/gameplay/targeting/doors_and_lamps", Friend_targeting_interactive);
	put("config/gameplay/targeting/levers", Friend_targeting_levers);
	put("config/gameplay/targeting/signs", Friend_targeting_signs);
	put("config/gameplay/smart_placement", Gump::smart_placement);
	put("config/gameplay/sort_key", Friend_sort_key);
	Gump::sort_button = Friend_sort_key;
	config->write_back();
}

// WASD movement.  The main loop reads the four keys' state every frame and
// walks that way (two keys diagonally), as the arrow keys do; reading the
// state rather than key-up events means a key let go while a menu or
// another window had the keyboard never sticks.  The key bindings don't
// see the keys; Shift with one does what the key alone did (Shift+W the
// watch, Shift+S Save).
static int friend_wasd_dx = 0;    // The direction walked last frame.
static int friend_wasd_dy = 0;

// One of the four keys, by where it sits on the keyboard.
static bool Friend_is_wasd_key(SDL_Scancode code) {
	return code == SDL_SCANCODE_W || code == SDL_SCANCODE_A || code == SDL_SCANCODE_S || code == SDL_SCANCODE_D;
}

// Return to Main Menu: Exult cannot tear its game down and run the title
// menu again in the same process, so main() starts it afresh (the same
// arguments without the Co-Op ones, skipping the intro).
static bool                     friend_return_to_menu = false;
static bool                     arg_skip_intro        = false;
static std::vector<std::string> friend_restart_args;    // As launched; [0] is the program.

void Friend_mode_request_main_menu() {
	friend_return_to_menu = true;
}

static void Friend_mode_restart_at_menu() {
	friend_session.stop();    // Guests (or the host) see us leave now.
	std::vector<std::string> args;
	for (std::size_t i = 0; i < friend_restart_args.size(); ++i) {
		const std::string& arg = friend_restart_args[i];
		if (arg == "--friend-join" || arg == "--friend-port") {
			++i;    // And its value.
		} else if (arg != "--friend-host" && arg != "--skip-intro") {
			args.push_back(arg);
		}
	}
	args.emplace_back("--skip-intro");
	std::vector<char*> argv;
	for (std::string& arg : args) {
		argv.push_back(arg.data());
	}
	argv.push_back(nullptr);
	SDL_Quit();
#if defined(_WIN32)
	_spawnv(_P_NOWAIT, argv[0], argv.data());
#else
	execv(argv[0], argv.data());
	perror("Return to Main Menu: could not start Exult again");
#endif
}

// Co-Op Mode hosting: chosen on the title menu (or --friend-host) before a
// game starts, or switched on during play from the game menu.
bool Friend_mode_hosting() {
	return arg_friend_host;
}

void Friend_mode_set_hosting(bool on) {
	arg_friend_host = on;
	if (on) {
		arg_friend_join.clear();
	}
}

static bool Friend_mode_start_hosting() {
	friend_player_name = gwin->get_main_actor()->get_name();
	if (friend_player_name.empty()) {
		friend_player_name = "Host";
	}
	friend_session.set_local_player_name(friend_player_name);
	if (!friend_session.start_host(arg_friend_port, Game::Get_unique_gamecode())) {
		arg_friend_host = false;
		Friend_mode_notice("Could not open Co-Op Mode on port " + std::to_string(arg_friend_port));
		return false;
	}
	Friend_mode_notice("Co-Op Mode is open on port " + std::to_string(arg_friend_port));
	return true;
}

bool Friend_mode_open_to_coop() {
	if (friend_session.mode() != Friend_mode_session::Mode::off) {
		return false;    // Already hosting, or a guest.
	}
	arg_friend_host = true;
	return Friend_mode_start_hosting();
}

static bool friend_world_reloaded = false;    // Host loaded a save.
static bool friend_picker_requested = false;    // Guest pressed Tab; roster on its way.
static bool friend_spectate_requested = false;    // Guest chose Spectate itself.
// Guest spectating: the player companion the view follows (X cycles), or
// -1 for the Avatar.
static int        friend_watched_npc = -1;
static Tile_coord friend_watched_at(-1, -1, -1);

// Guest: X while spectating watches the next player (the Avatar first).
static void Friend_mode_watch_next() {
	std::vector<int> actors = {-1};
	for (const auto& [player, entry] : friend_players) {
		if (entry.actor > 0 && gwin->get_npc(entry.actor)) {
			actors.push_back(entry.actor);
		}
	}
	auto      current = std::find(actors.begin(), actors.end(), friend_watched_npc);
	const int next    = current == actors.end() || current + 1 == actors.end() ? actors.front() : *(current + 1);
	friend_watched_npc = next;
	friend_watched_at  = Tile_coord(-1, -1, -1);
	Actor* actor       = next >= 0 ? gwin->get_npc(next) : gwin->get_main_actor();
	gwin->set_camera_actor(actor);
	gwin->center_view(actor->get_tile());
	const std::string who = next >= 0 ? Friend_player_with_actor(actor) : friend_session.remote_player_name_value();
	Friend_mode_notice("Watching " + (next >= 0 ? actor->get_name() : std::string("the Avatar"))
					   + (who.empty() ? std::string() : " (" + who + ")"));
}


void Friend_mode_note_world_loaded() {
	Friend_class_world_loaded();
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		friend_world_reloaded = true;
	}
}

// Temporary save used to hand the host's live world to a joining guest.
static constexpr const char* friend_world_file = "<SAVEGAME>/friend_join.sav";

// Guest: the host loaded a save mid-game; load the same world.
static bool Friend_mode_load_world(const std::vector<unsigned char>& data) {
	{
		auto out = U7open_out(friend_world_file);
		if (!out) {
			cerr << "Friend Mode: could not store the host's world" << endl;
			return false;
		}
		out->write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	}
	gwin->restore_gamedat(friend_world_file);
	U7remove(friend_world_file);
	gwin->read();
	friend_guest_creatures.clear();
	friend_guest_have_creatures = false;
	friend_guest_weather        = 0;
	cout << "Friend Mode: loaded the host's new world (" << data.size() << " bytes)" << endl;
	return true;
}

static bool Friend_mode_read_file(const char* fname, std::vector<unsigned char>& data) {
	auto in = U7open_in(fname);
	if (!in) {
		return false;
	}
	data.assign(std::istreambuf_iterator<char>(*in), std::istreambuf_iterator<char>());
	return in->good() || in->eof();
}

// Host: write the live game to a temporary save and send it to a guest
// (or every guest, after the host loads a save).
static void Friend_mode_send_world(int to = Friend_mode_session::everyone) {
	if (friend_world_sent) {
		// Guests already playing get what changed up to now: the baseline
		// below starts from here.
		Friend_mode_item_pass(true);
		Friend_mode_inventory_pass(true);
	}
	gwin->write(true);
	gwin->save_gamedat(friend_world_file, "Friend Mode join");
	std::vector<unsigned char> data;
	if (Friend_mode_read_file(friend_world_file, data)) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::world_save, data, to);
		cout << "Friend Mode: sent world (" << data.size() << " bytes)" << endl;
		friend_world_sent = true;
		// Both sides now match: record every nearby chunk as the baseline.
		friend_chunk_hashes.clear();
		friend_inventory_hashes.clear();
		Friend_mode_item_pass(false);
		Friend_mode_inventory_pass(false);
		Friend_mode_status_pass(true);
		Friend_mode_clock_pass(true);
		Friend_mode_creature_pass(true);
		Friend_mode_journal_pass(true);
		Friend_mode_send_journal_writers();
		Friend_mode_journal_open_pass(true);
		Friend_mode_send_music();    // Join mid-tune.
		Friend_mode_story_flags_pass(true);
		friend_health_sent.clear();
		friend_player_list_sent.clear();    // The newcomer needs it too.
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

static bool friend_host_paused_sent = false;    // Host: guests told we paused.

// Host autosave while guests play: every 5 minutes, into three slots of
// its own at the far end of the numbering (997-999, shown in the load
// menu as "Co-Op Autosave"), each time over the oldest.  A player's own
// saves take the first free numbers, so they are never touched.
static constexpr uint32 friend_autosave_ms    = 5 * 60 * 1000;
static constexpr int    friend_autosave_first = 997;
static constexpr int    friend_autosave_slots = 3;
static uint32           friend_next_autosave  = 0;

static void Friend_mode_autosave() {
	int                             slot = friend_autosave_first;
	std::filesystem::file_time_type oldest;
	for (int i = 0; i < friend_autosave_slots; ++i) {
		char fname[50];
		snprintf(fname, sizeof(fname), SAVENAME, friend_autosave_first + i,
				 GAME_BG ? "bg" : (GAME_SI ? "si" : "dev"));
		if (!U7exists(fname)) {
			slot = friend_autosave_first + i;    // An unused one first.
			break;
		}
		std::error_code                       error;
		const std::filesystem::file_time_type time
				= std::filesystem::last_write_time(get_system_path(fname), error);
		if (!error && (i == 0 || time < oldest)) {
			oldest = time;
			slot   = friend_autosave_first + i;
		}
	}
	try {
		gwin->write(true);
		gwin->save_gamedat(slot, "Co-Op Autosave");
		cout << "Friend Mode: autosaved to slot " << slot << endl;
		if (friend_chat_log_hidden) {
			Friend_mode_top_line("Autosaved");
		} else {
			Friend_mode_log("Autosaved");
		}
	} catch (exult_exception& /*e*/) {
		Friend_mode_notice("Autosave failed");
	}
}

void Friend_mode_background_poll() {
	if (friend_session.mode() == Friend_mode_session::Mode::off) {
		return;
	}
	friend_session.poll();
	// The host's world is paused while a menu or dialog is up: tell guests.
	if (friend_session.mode() == Friend_mode_session::Mode::host && friend_session.connected()
		&& !friend_host_paused_sent) {
		friend_session.send_host_paused(true);
		friend_host_paused_sent = true;
	}
	// A guest joining while the host is in a menu still gets the world:
	// saving from a menu is what Exult's own Save does.  Not while a script
	// is running, though; that request waits for the main loop.
	int requester = 0;
	if (friend_session.mode() == Friend_mode_session::Mode::host && !gwin->get_usecode()->in_usecode()
		&& friend_session.take_world_request(&requester)) {
		Friend_mode_send_world(requester);
	}
}

bool Friend_mode_download_world() {
	friend_session.set_local_player_name(friend_player_name.empty() ? "Player" : friend_player_name);
	if (!friend_session.start_guest(arg_friend_join, arg_friend_port, Game::Get_unique_gamecode())) {
		Friend_mode_show_status("Could not connect to host", arg_friend_join);
		SDL_Delay(2500);
		return false;
	}
	bool         requested = false;
	const uint32 deadline  = SDL_GetTicks() + 120000;
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
			cheat.disable_for_friend_guest();    // The host's world: no cheats.
			gwin->restore_gamedat(friend_world_file);
			U7remove(friend_world_file);
			return true;
		}
		if (!friend_session.peer_open() || SDL_GetTicks() > deadline) {
			Friend_mode_show_status(
					friend_session.rejected_full() ? "The host's game is full"
					: friend_session.peer_open()   ? "The host did not respond"
												   : "The host refused the connection",
					friend_session.rejected_full() ? "Every companion already has a player"
												   : "Check that both use the same Exult build and game");
			SDL_Delay(3000);
			friend_session.stop();
			return false;
		}
		const uint32 remaining = friend_session.blob_bytes_remaining();
		if (remaining > 0) {
			Friend_mode_show_status("Downloading the host's world", std::to_string(remaining / 1024) + " KB to go");
		} else {
			Friend_mode_show_status(friend_session.connected() ? "Waiting for the host's world" : "Connecting to host",
					friend_session.connected() ? "The host may be in a menu" : arg_friend_join);
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

// Friend Mode's own lines along the bottom stack up from the HUD row (the
// party's faces and the skill bar, when they are along the bottom): the chat
// line first, then the status bar, then the chat log.
static int Friend_mode_chat_line_top() {
	auto      font = fontManager.get_font("MENU_FONT");
	const int h    = (font ? font->get_text_height() : 8) + 8;
	return std::min(gwin->get_height() - h - 8, Friend_hud_bottom_y() - h - 4);
}

// The chat line, bottom of the view: its own layer at the world's scale,
// above a conversation (whose layer would cover it in the game view).
static void Friend_mode_paint_chat_input() {
	static int         layer = -1;
	static std::string painted;
	static int         painted_w = 0;
	Image_window*      iwin      = gwin->get_win();
	auto               font      = fontManager.get_font("MENU_FONT");
	if (!friend_chat_active || !font) {
		if (layer >= 0 && iwin->layer_is_visible(layer)) {
			gwin->layer_set_visible(layer, false);
			gwin->set_painted();
		}
		return;
	}
	const int   x     = 8;
	const int   h     = font->get_text_height() + 8;
	const int   y     = Friend_mode_chat_line_top();
	const int   w     = std::max(120, gwin->get_width() - 16);
	std::string shown = "Say: " + friend_chat_text + "_";
	while (shown.size() > 6 && font->get_text_width(shown.c_str()) > w - 10) {
		shown.erase(5, 1);
	}
	if (layer < 0 || shown != painted || w != painted_w) {
		if (layer >= 0) {
			gwin->destroy_layer(layer);
		}
		constexpr int text_layer_z = (1 << 18) + (1 << 16);    // As effects.cc's text.
		layer                      = gwin->create_layer("Chat line", w, h, 255, 0, text_layer_z);
		if (layer < 0) {
			return;
		}
		Image_buffer8* lbuf = gwin->get_layer_ibuf(layer);
		if (!lbuf) {
			return;
		}
		lbuf->clear_clip();
		lbuf->fill8(255);
		// Fonts paint into the current render target, not the buffer given.
		Image_buffer8* prev = gwin->push_render_target(lbuf);
		lbuf->draw_box(0, 0, w, h, 1, 0, 15);
		font->draw_text(lbuf, 5, 4, shown.c_str());
		gwin->pop_render_target(prev);
		gwin->layer_set_dirty(layer);
		painted   = shown;
		painted_w = w;
	}
	int sx0;
	int sy0;
	int sx1;
	int sy1;
	iwin->game_to_screen(x, y, false, sx0, sy0);
	iwin->game_to_screen(x + w, y + h, false, sx1, sy1);
	gwin->layer_set_dest(layer, sx0, sy0, sx1 - sx0, sy1 - sy0);
	gwin->layer_set_visible(layer, true);
	gwin->set_painted();
}

// Guest: while the host is in conversation or has paused the game, explain
// it in a bar at the bottom of the screen, clear of the conversation (and
// above the chat line when that is open).  It stays until the wait ends.
static int friend_status_bar_top = -1;    // Its top this frame, or -1: the chat log sits above it.

static void Friend_mode_paint_dialogue_wait() {
	friend_status_bar_top = -1;
	if (friend_session.mode() == Friend_mode_session::Mode::off) {
		return;
	}
	const bool guest = friend_session.mode() == Friend_mode_session::Mode::guest;
	std::vector<std::string> lines;
	if (!guest) {
		if (!friend_peer_journal_open || !friend_session.connected()) {
			return;
		}
		lines = {"Someone is writing in the party journal.", "Someone is writing in", "the party journal."};
	} else if (gwin->is_network_dialogue_paused()) {
		lines = {"The Avatar is having a conversation. Please wait.", "The Avatar is having a conversation.",
				 "Please wait."};
	} else if (gwin->is_network_host_paused()) {
		lines = {"The host has paused the game.", "The host has paused", "the game."};
	} else if (friend_following && friend_session.connected()) {
		lines = {"Following the Avatar. Move to take over.", "Following the Avatar.", "Move to take over."};
	} else if (gwin->is_network_spectating() && friend_session.connected()) {
		lines = {"Spectating. J: choose a companion. X: watch another player.", "Spectating. J: choose a companion.",
				 "X: watch another player."};
	} else if (friend_peer_journal_open && friend_session.connected()) {
		lines = {"Someone is writing in the party journal.", "Someone is writing in", "the party journal."};
	} else {
		return;
	}
	auto font = fontManager.get_font("MENU_FONT");
	if (!font) {
		return;
	}
	// One line if it fits, else the two-line form.
	const int max_w = gwin->get_width() - 16;
	if (font->get_text_width(lines[0].c_str()) + 16 > max_w) {
		lines.erase(lines.begin());
	} else {
		lines.resize(1);
	}
	int text_w = 0;
	for (const std::string& line : lines) {
		text_w = std::max(text_w, font->get_text_width(line.c_str()));
	}
	const int line_h = font->get_text_height() + 2;
	const int w      = std::min(max_w, text_w + 16);
	const int h      = static_cast<int>(lines.size()) * line_h + 6;
	const int x      = (gwin->get_width() - w) / 2;
	// Above the party's faces and the skill bar, when they are along the
	// bottom, and above the chat line.
	int y = std::min(gwin->get_height() - h - 8, Friend_hud_bottom_y() - h - 4);
	if (friend_chat_active) {    // Sit above the chat line.
		y = Friend_mode_chat_line_top() - h - 4;
	}
	Image_buffer8* buffer = gwin->get_win()->get_ib8();
	buffer->draw_box(x, y, w, h, 1, 0, 15);
	friend_status_bar_top = y;
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const int line_x = x + (w - font->get_text_width(lines[i].c_str())) / 2;
		font->draw_text(buffer, line_x, y + 4 + static_cast<int>(i) * line_h, lines[i].c_str());
	}
	gwin->set_painted();
}

// Lines pinned to the screen rather than the world (the chat log, the
// notices): their own layer, scaled like overhead chat (the text-effect UI
// size), with the same white letters and dark shadow; whispers are purple.
struct Friend_pinned_text {
	const char* name;
	int         layer   = -1;
	int         dest[4] = {0, 0, 0, 0};
};

// Paint 'lines' (when 'changed') into 'pinned' and place it: its bottom
// left corner at game point (x, y), or with 'centre_top' its top centre.
static void Friend_mode_paint_pinned(Friend_pinned_text& pinned, const std::deque<Friend_log_line>& lines,
		bool& changed, bool shown, int x, int y, bool centre_top) {
	Image_window* iwin = gwin->get_win();
	if (!shown || lines.empty()) {
		if (pinned.layer >= 0 && iwin->layer_is_visible(pinned.layer)) {
			gwin->layer_set_visible(pinned.layer, false);
			gwin->set_painted();
		}
		return;
	}
	Shape_manager* sman   = Shape_manager::get_instance();
	const int      line_h = sman->get_text_height(0) + 1;
	int            w      = 8;
	for (const Friend_log_line& line : lines) {
		w = std::max(w, sman->get_text_width(0, line.text.c_str()) + 2);
	}
	const int h = static_cast<int>(lines.size()) * line_h;
	if (changed || pinned.layer < 0) {
		if (pinned.layer >= 0) {
			gwin->destroy_layer(pinned.layer);
		}
		constexpr int text_layer_z = (1 << 18) + (1 << 16);    // As effects.cc's text.
		pinned.layer               = gwin->create_layer(pinned.name, w, h, 255, 0, text_layer_z);
		if (pinned.layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(pinned.layer, Image_window::UiLayerTextEffects);
		Image_buffer8* lbuf = gwin->get_layer_ibuf(pinned.layer);
		if (!lbuf) {
			return;
		}
		lbuf->clear_clip();
		lbuf->fill8(255);    // Fully transparent.
		// Recolor the font's yellow body, keeping its shadow and outline.
		const Palette*      pal    = gwin->get_pal();
		const unsigned char white  = sman->get_special_pixel(PROTECT_PIXEL);
		const unsigned char purple = static_cast<unsigned char>(pal->find_color(52, 30, 63));
		unsigned char       plain[256];
		unsigned char       whisper[256];
		for (int i = 0; i < 256; ++i) {
			const bool body = pal->get_red(i) > 35 && pal->get_green(i) > 35 && pal->get_blue(i) < 30;
			plain[i]        = body ? white : static_cast<unsigned char>(i);
			whisper[i]      = body ? purple : static_cast<unsigned char>(i);
		}
		// Fonts paint into the current render target, not the buffer given.
		Image_buffer8* prev = gwin->push_render_target(lbuf);
		int            ly   = 0;
		for (const Friend_log_line& line : lines) {
			const int lx = centre_top ? (w - sman->get_text_width(0, line.text.c_str())) / 2 : 0;
			sman->get_font(0)->paint_text(lbuf, line.text.c_str(), static_cast<int>(line.text.size()), lx, ly,
					line.whisper ? whisper : plain);
			ly += line_h;
		}
		gwin->pop_render_target(prev);
		gwin->layer_set_dirty(pinned.layer);
		changed        = false;
		pinned.dest[2] = -1;    // Place it again.
	}
	const float f = iwin->get_ui_scale_factor(Image_window::UiLayerTextEffects);
	int         sx;
	int         sy;
	iwin->game_to_screen(x, y, false, sx, sy);
	const int dw      = static_cast<int>(w * f);
	const int dh      = static_cast<int>(h * f);
	const int dest[4] = {centre_top ? sx - dw / 2 : sx, centre_top ? sy : sy - dh, dw, dh};
	if (!std::equal(dest, dest + 4, pinned.dest) || !iwin->layer_is_visible(pinned.layer)) {
		std::copy(dest, dest + 4, pinned.dest);
		gwin->layer_set_dest(pinned.layer, dest[0], dest[1], dest[2], dest[3]);
		gwin->layer_set_visible(pinned.layer, true);
		gwin->set_painted();
	}
}

// The Target System's hint ("Open (Space)  Pick up (Shift+Space)"): its
// top centre at screen point (sx, sy), under the notices, in the menu font (the chat
// line's; the overhead font has no '+') and a little smaller than the
// notices.  It fades with them.
static void Friend_mode_paint_hint(int sx, int sy) {
	static Friend_pinned_text pinned{"Target hint"};
	static int                w = 0;
	static int                h = 0;
	Image_window*             iwin = gwin->get_win();
	auto                      font = fontManager.get_font("MENU_FONT");
	const bool                shown
			= font && !friend_hint_shown.empty()
			  && static_cast<uint32>(SDL_GetTicks()) - friend_hint_at < friend_notice_ms;
	if (!shown) {
		if (pinned.layer >= 0 && iwin->layer_is_visible(pinned.layer)) {
			gwin->layer_set_visible(pinned.layer, false);
			gwin->set_painted();
		}
		return;
	}
	if (friend_hint_changed || pinned.layer < 0) {
		if (pinned.layer >= 0) {
			gwin->destroy_layer(pinned.layer);
		}
		w = font->get_text_width(friend_hint_shown.c_str()) + 4;
		h = font->get_text_height() + 4;
		constexpr int text_layer_z = (1 << 18) + (1 << 16);
		pinned.layer               = gwin->create_layer(pinned.name, w, h, 255, 0, text_layer_z);
		if (pinned.layer < 0) {
			return;
		}
		gwin->layer_set_ui_kind(pinned.layer, Image_window::UiLayerTextEffects);
		Image_buffer8* lbuf = gwin->get_layer_ibuf(pinned.layer);
		if (!lbuf) {
			return;
		}
		lbuf->clear_clip();
		lbuf->fill8(255);    // Fully transparent.
		// Fonts paint into the current render target, not the buffer given.
		Image_buffer8* prev = gwin->push_render_target(lbuf);
		// Word by word: this font draws a space as a solid black box.  Each
		// word first in black one pixel all round (an outline, like the
		// notices' font has), then in its own colors on top.
		unsigned char black[256];
		std::fill(std::begin(black), std::end(black), static_cast<unsigned char>(0));
		const int space = font->get_text_width(" ");
		for (int pass = 0; pass < 2; ++pass) {
			int    wx    = 2;
			size_t start = 0;
			while (start <= friend_hint_shown.size()) {
				size_t end = friend_hint_shown.find(' ', start);
				if (end == std::string::npos) {
					end = friend_hint_shown.size();
				}
				const std::string word = friend_hint_shown.substr(start, end - start);
				if (!word.empty()) {
					if (pass == 0) {
						for (int dy = -1; dy <= 1; ++dy) {
							for (int dx = -1; dx <= 1; ++dx) {
								if (dx || dy) {
									font->draw_text(lbuf, wx + dx, 2 + dy, word.c_str(), black);
								}
							}
						}
					} else {
						font->draw_text(lbuf, wx, 2, word.c_str());
					}
					wx += font->get_text_width(word.c_str());
				}
				wx += space;
				start = end + 1;
			}
		}
		gwin->pop_render_target(prev);
		gwin->layer_set_dirty(pinned.layer);
		friend_hint_changed = false;
		pinned.dest[2]      = -1;    // Place it again.
	}
	constexpr float smaller = 0.8f;
	const float     f       = iwin->get_ui_scale_factor(Image_window::UiLayerTextEffects) * smaller;
	const int       dw      = static_cast<int>(w * f);
	const int dh      = static_cast<int>(h * f);
	const int dest[4] = {sx - dw / 2, sy, dw, dh};
	if (!std::equal(dest, dest + 4, pinned.dest) || !iwin->layer_is_visible(pinned.layer)) {
		std::copy(dest, dest + 4, pinned.dest);
		gwin->layer_set_dest(pinned.layer, dest[0], dest[1], dest[2], dest[3]);
		gwin->layer_set_visible(pinned.layer, true);
		gwin->set_painted();
	}
}

// Drop lines older than 'ms'.
static void Friend_mode_expire_lines(std::deque<Friend_log_line>& lines, uint32 ms, bool& changed) {
	const uint32 now = static_cast<uint32>(SDL_GetTicks());
	while (!lines.empty() && now - lines.front().time > ms) {
		lines.pop_front();    // The line fades out.
		changed = true;
	}
}

// The chat log, bottom left: recent lines only, newest at the bottom; and
// the notices, centred near the top.
static void Friend_mode_paint_chat_log() {
	static Friend_pinned_text log{"Chat log"};
	static Friend_pinned_text notices{"Notices"};
	const bool                on = friend_session.mode() != Friend_mode_session::Mode::off;
	Friend_mode_expire_lines(friend_chat_log, friend_chat_log_ms, friend_chat_log_changed);
	Friend_mode_expire_lines(friend_notices, friend_notice_ms, friend_notices_changed);
	// The log's bottom 20 pixels above the bottom of the view, 6 from the
	// left, and clear of the chat line and the status bar (spectating,
	// following, paused) when those show.
	int bottom = gwin->get_height() - 20;
	if (friend_chat_active) {
		bottom = std::min(bottom, Friend_mode_chat_line_top() - 3);
	}
	if (friend_status_bar_top >= 0) {
		bottom = std::min(bottom, friend_status_bar_top - 3);
	}
	bottom = std::min(bottom, Friend_hud_bottom_y() - 4);    // Above the faces and the skill bar.
	Friend_mode_paint_pinned(log, friend_chat_log, friend_chat_log_changed, on && !friend_chat_log_hidden, 6, bottom, false);
	Friend_mode_paint_pinned(notices, friend_notices, friend_notices_changed, true, gwin->get_width() / 2, 4, true);
	// The hint just under the notices as they are on screen.
	int top_x;
	int top_y;
	gwin->get_win()->game_to_screen(gwin->get_width() / 2, 4, false, top_x, top_y);
	if (!friend_notices.empty() && notices.layer >= 0) {
		top_y = notices.dest[1] + notices.dest[3] + 2;
	}
	Friend_mode_paint_hint(top_x, top_y);
}

// Chat, whispers and guests' emotes that arrived: shown, and on the host
// passed on.  From the main loop, and on the host also while it waits for a
// click (a conversation), so chat isn't held up until the Avatar is done.
// Returns whether anything came.
static bool Friend_mode_take_chats() {
	bool any = false;
	std::string incoming_chat;
	uint32      incoming_speaker = 0;
	int         incoming_player  = 0;
	int         chat_from        = 0;
	int         chat_target      = -1;
	while (friend_session.take_chat(incoming_speaker, incoming_player, incoming_chat, &chat_from, &chat_target)) {
		any = true;
		Actor*     speaker = Friend_mode_remote_speaker(incoming_speaker, incoming_player);
		const bool hosting = friend_session.mode() == Friend_mode_session::Mode::host;
		if (chat_target >= 0) {    // A whisper.
			if (!hosting || chat_target == Friend_mode_session::host_player) {
				Friend_mode_show_whisper(
						"[" + Friend_mode_player_name(incoming_player) + " whispers] " + incoming_chat);
			} else if (Friend_guest_of(chat_target)) {
				friend_session.send_whisper(chat_target,
						speaker ? static_cast<uint32>(speaker->get_npc_num()) : Friend_mode_session::chat_spectator,
						chat_from, incoming_chat, chat_target);
			}
			continue;
		}
		if (hosting && incoming_chat[0] == '/') {    // A guest's emote.
			const Friend_emote* emote = Friend_find_emote(incoming_chat);
			if (emote && speaker) {
				Friend_mode_emote(speaker, chat_from, *emote);
			}
			continue;
		}
		Friend_mode_show_chat(speaker, Friend_mode_player_name(incoming_player), incoming_chat);
		if (hosting) {
			// Pass it on to the other guests, with the speaker we know.
			friend_session.send_chat(
					speaker ? static_cast<uint32>(speaker->get_npc_num()) : Friend_mode_session::chat_spectator,
					chat_from, incoming_chat, Friend_mode_session::everyone, chat_from);
		}
	}
	return any;
}

// Suggest a reply: a guest clicks one of the Avatar's choices during the
// Avatar's conversation.  The host checks it is on offer, draws it in that
// player's color and passes it on; everyone logs "Marc suggests: Name".
// The host reads these while waiting for its own click (Get_click), since
// its main loop is not running during a conversation.
static void Friend_mode_take_suggestions() {
	if (friend_session.mode() == Friend_mode_session::Mode::off) {
		return;
	}
	const bool    hosting      = friend_session.mode() == Friend_mode_session::Mode::host;
	Conversation* conversation = gwin->get_usecode()->get_conversation();
	int           player       = 0;
	std::string   choice;
	bool          any = false;
	while (friend_session.take_suggestion(player, choice)) {
		if (hosting && (!Friend_guest_of(player) || !friend_host_dialogue_active)) {
			continue;
		}
		const auto color = static_cast<unsigned char>(Friend_player_color(player));
		if (!conversation || !conversation->suggest_choice(choice, color)) {
			continue;    // Not on offer (any more).
		}
		if (hosting) {
			friend_session.send_suggestion(player, choice);
		}
		Friend_mode_session_notice(Friend_mode_player_name(player) + " suggests: " + choice);
		any = true;
	}
	if (any) {
		Friend_mode_paint_chat_log();    // Also while the host's conversation waits.
		gwin->set_painted();
	}
}

// Host, waiting for a click (a conversation's choices among others): keep
// reading guests' suggestions.
static void Friend_mode_click_wait_poll() {
	if (friend_session.mode() != Friend_mode_session::Mode::host) {
		return;
	}
	friend_session.poll();
	Friend_mode_take_chats();
	Friend_mode_take_suggestions();
	if (friend_host_dialogue_active) {
		Friend_mode_paint_chat_input();
		Friend_mode_paint_chat_log();
	}
}

// Hold Tab: a list of the players, their characters and health.
static bool friend_player_list_shown = false;

static void Friend_mode_paint_player_list() {
	if (!friend_player_list_shown || friend_session.mode() == Friend_mode_session::Mode::off) {
		return;
	}
	auto font = fontManager.get_font("MENU_FONT");
	if (!font) {
		return;
	}
	struct Row {
		std::string player, character, health;
	};
	auto row_for = [](const std::string& player, int npc, bool following) {
		Row row{player.empty() ? std::string("Player") : player, "Spectating", "-"};
		if (Actor* actor = npc >= 0 ? gwin->get_npc(npc) : nullptr) {
			row.character = actor == gwin->get_main_actor() ? std::string("Avatar") : actor->get_name();
			if (following) {
				row.character += " (following)";
			}
			row.health = std::to_string(actor->get_property(Actor::health)) + "/"
						 + std::to_string(actor->get_property(Actor::strength));
		}
		return row;
	};
	std::vector<Row> rows = {{"Player", "Character", "Health"}};
	if (friend_session.mode() == Friend_mode_session::Mode::host) {
		rows.push_back(row_for(friend_player_name, 0, false));
		for (const auto& [player, guest] : friend_guests) {
			rows.push_back(row_for(guest.name, guest.actor, guest.following));
		}
	} else {
		for (const auto& [player, entry] : friend_players) {
			rows.push_back(row_for(entry.name, entry.actor, entry.following));
		}
	}
	int widths[3] = {0, 0, 0};
	for (const Row& row : rows) {
		widths[0] = std::max(widths[0], font->get_text_width(row.player.c_str()));
		widths[1] = std::max(widths[1], font->get_text_width(row.character.c_str()));
		widths[2] = std::max(widths[2], font->get_text_width(row.health.c_str()));
	}
	constexpr int gap    = 12;
	const int     line_h = font->get_text_height() + 3;
	const int     w      = std::min(gwin->get_width() - 8, widths[0] + widths[1] + widths[2] + 2 * gap + 16);
	const int     h      = static_cast<int>(rows.size()) * line_h + 8;
	const int     x      = (gwin->get_width() - w) / 2;
	const int     y      = std::max(8, gwin->get_height() / 6);
	Image_buffer8* buffer = gwin->get_win()->get_ib8();
	buffer->draw_box(x, y, w, h, 1, 0, 15);
	for (std::size_t i = 0; i < rows.size(); ++i) {
		const int line_y = y + 5 + static_cast<int>(i) * line_h;
		font->draw_text(buffer, x + 8, line_y, rows[i].player.c_str());
		font->draw_text(buffer, x + 8 + widths[0] + gap, line_y, rows[i].character.c_str());
		font->draw_text(buffer, x + 8 + widths[0] + widths[1] + 2 * gap, line_y, rows[i].health.c_str());
	}
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
		} else if (const Friend_guest* guest = Friend_guest_with_actor(actor)) {
			player_name = guest->name;
		}
	} else {
		if (actor == gwin->get_main_actor()) {
			player_name = friend_session.remote_player_name_value();
		} else if (gwin->has_network_friend_control() && actor == gwin->get_controlled_actor()) {
			player_name = friend_player_name;
		} else {
			player_name = Friend_player_with_actor(actor);
		}
	}

	return player_name.empty() ? shown_name : shown_name + " (" + player_name + ")";
}

bool Friend_mode_collect_connection_details() {
	auto font = fontManager.get_font("MENU_FONT");
	if (!font) {
		return true;
	}
	// The last name and address typed here are kept in the config, so a
	// rejoin needs only Enter (and the same name gets the companion back).
	std::string last_name;
	std::string last_host;
	config->value("config/friend/player_name", last_name, "");
	config->value("config/friend/last_host", last_host, "127.0.0.1");
	std::array<std::string, 2> values{friend_player_name.empty() ? last_name : friend_player_name,
			arg_friend_join.empty() ? last_host : arg_friend_join};
	int active = 0;
	SDL_Window* window = gwin->get_win()->get_screen_window();
	SDL_StartTextInput(window);
	for (;;) {
		gwin->clear_screen(false);
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
					config->set("config/friend/player_name", friend_player_name, false);
					config->set("config/friend/last_host", arg_friend_join, true);
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
		cout << "Friend Mode: conversation begins"
			 << (gwin->get_usecode()->in_usecode() ? "" : " (outside a script)") << endl;
		friend_session.send_dialogue_begin();
	}
}

void Friend_mode_note_speech(Game_object* source, const char* text) {
	if (!source || !text || friend_session.mode() != Friend_mode_session::Mode::host || !friend_session.connected()) {
		return;
	}
	Actor* actor = source->as_actor();
	if (!actor) {
		// Doors and chests ("Locked", "Unlocked"): send the tile and shape of
		// the object, or of whatever it is inside on the map.
		Game_object* outer = source->get_outermost();
		if (outer->as_actor() || !outer->get_chunk()) {
			return;
		}
		const Tile_coord           t = outer->get_tile();
		std::vector<unsigned char> payload;
		Friend_put16(payload, t.tx);
		Friend_put16(payload, t.ty);
		Friend_put8(payload, t.tz);
		Friend_put16(payload, outer->get_shapenum());
		payload.insert(payload.end(), text, text + std::min<std::size_t>(std::strlen(text), 200));
		friend_session.send_blob(Friend_mode_session::Blob_kind::object_speech, payload);
		friend_session.poll();
		return;
	}
	uint32 source_id = 0;
	bool creature = false;
	if (actor->get_npc_num() >= 0) {
		source_id = static_cast<uint32>(actor->get_npc_num());
	} else {
		source_id = Friend_creature_id(actor);
		if (source_id == 0) {
			return;    // Not near the players, so guests don't have it.
		}
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
		cout << "Friend Mode: conversation ends" << endl;
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

// A companion a guest can take: in the party and alive, and awake and able
// to move unless the guest is 'keeping' one it already had (after a load or
// a rejoin), which it keeps asleep or paralyzed too.
static bool Friend_actor_is_eligible(uint32 npc_num, bool keeping) {
	Actor* actor = gwin->get_npc(npc_num);
	if (!actor || actor == gwin->get_main_actor() || actor->is_dead()
		|| (!keeping && (actor->get_flag(Obj_flags::asleep) || actor->get_flag(Obj_flags::paralyzed)))) {
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

// Is this companion free for 'player' (nobody else controls it)?
static bool Friend_actor_is_free(uint32 npc_num, int player) {
	for (const auto& [other, guest] : friend_guests) {
		if (other != player && guest.actor == static_cast<int>(npc_num)) {
			return false;
		}
	}
	const Friend_guest* asking = Friend_guest_of(player);
	for (const auto& [name, hold] : friend_rejoin_holds) {
		if (hold.actor == static_cast<int>(npc_num) && (!asking || asking->name != name)) {
			return false;    // Kept for a player who dropped.
		}
	}
	return true;
}

// Host: offer a guest the companions it may control: its own and the free ones.
static void Friend_mode_send_roster(int player) {
	std::vector<uint32> roster;
	Party_manager*      party = gwin->get_party_man();
	for (int i = 0; i < party->get_count(); ++i) {
		const uint32 npc_num = party->get_member(i);
		if (Friend_actor_is_eligible(npc_num) && Friend_actor_is_free(npc_num, player)) {
			roster.push_back(npc_num);
		}
	}
	friend_session.send_party_roster(roster, player);
	cout << "Friend Mode: advertised " << roster.size() << " available companion(s) to player " << player << endl;
}

// Host: a guest's companion is lost (dead, out of the party, or missing
// after a load): the guest spectates, and its picker opens with what is free.
static void Friend_mode_drop_companion(int player, Friend_guest& guest, const std::string& notice) {
	if (guest.actor < 0) {
		return;
	}
	guest.following = false;
	Friend_return_held_item(guest.held);
	gwin->release_remote_friend_mode_actor(guest.actor);    // If still reserved.
	guest.actor = -1;
	Friend_mode_announce(notice);
	friend_session.accept_actor(Friend_mode_session::spectate_actor, player);
	Friend_mode_send_roster(player);
}

// Host: every player, their name and companion, sent when it changes.
static void Friend_mode_player_list_pass() {
	std::vector<unsigned char> payload;
	auto put_player = [&](int player, const std::string& name, int actor, bool following) {
		Friend_put8(payload, player);
		Friend_put16(payload, actor >= 0 ? actor : 0xffff);
		Friend_put8(payload, following ? 1 : 0);
		const size_t length = std::min<size_t>(name.size(), 20);
		Friend_put8(payload, static_cast<int>(length));
		payload.insert(payload.end(), name.begin(), name.begin() + length);
	};
	Friend_put8(payload, static_cast<int>(friend_guests.size() + 1));
	put_player(Friend_mode_session::host_player, friend_player_name, 0, false);
	for (const auto& [player, guest] : friend_guests) {
		put_player(player, guest.name, guest.actor, guest.following);
	}
	if (payload != friend_player_list_sent) {
		friend_session.send_blob(Friend_mode_session::Blob_kind::player_list, payload);
		friend_player_list_sent = std::move(payload);
	}
}

// Guest: the host's player list.
static void Friend_mode_apply_player_list(const std::vector<unsigned char>& data) {
	friend_players.clear();
	size_t pos = 1;
	for (int i = 0; i < (data.empty() ? 0 : data[0]) && pos + 5 <= data.size(); ++i) {
		const int    player    = data[pos];
		const int    actor     = data[pos + 1] | (data[pos + 2] << 8);
		const bool   following = data[pos + 3] != 0;
		const size_t length    = data[pos + 4];
		pos += 5;
		if (pos + length > data.size()) {
			break;
		}
		std::string name;
		for (size_t c = 0; c < length; ++c) {
			if (data[pos + c] >= 32 && data[pos + c] < 127) {
				name += static_cast<char>(data[pos + c]);
			}
		}
		pos += length;
		friend_players[player] = {name, actor == 0xffff ? -1 : actor, following};
	}
	gwin->set_all_dirty();    // Name labels.
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
		// Each line of the description, wrapped to the text area and centred.
		const int   space_width = font->get_text_width(" ");
		int         line_y      = y1 + 34;
		auto        paint_line  = [&](const std::vector<std::string>& words, int line_width) {
			int word_x = text_left + (text_width - line_width) / 2;
			for (const std::string& current_word : words) {
				font->paint_text(buffer, current_word.c_str(), word_x, line_y);
				word_x += font->get_text_width(current_word.c_str()) + space_width;
			}
			line_y += font->get_text_height() + 2;
		};
		std::size_t line_start = 0;
		while (line_start <= description.size()) {
			const std::size_t        line_end = description.find('\n', line_start);
			std::istringstream       line_stream(description.substr(line_start, line_end - line_start));
			std::vector<std::string> words;
			std::string              word;
			int                      line_width = 0;
			while (line_stream >> word) {
				const int word_width = font->get_text_width(word.c_str());
				if (!words.empty() && line_width + space_width + word_width > text_width) {
					paint_line(words, line_width);
					words.clear();
					line_width = 0;
				}
				line_width += (words.empty() ? 0 : space_width) + word_width;
				words.push_back(word);
			}
			paint_line(words, line_width);
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

static int Select_friend_actor(const std::vector<uint32>& roster, bool offer_watch) {
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
		gwin->clear_screen(false);
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
		if (offer_watch) {
			auto* spectate = new MenuTextEntry(hot_font, font, "Spectate", centerx, title_y + 36 + card_height + 12);
			spectate->set_id(friend_pick_spectate);
			menu.add_entry(spectate);
		}
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
	parameters.declare("--friend-port", &arg_friend_port, 1992);
	parameters.declare("--skip-intro", &arg_skip_intro, true);    // Set by Return to Main Menu.
	parameters.declare("--installdata", &arg_installdata);
	parameters.declare("--installmod", &arg_installmod);
#if defined _WIN32
	bool portable = false;
	parameters.declare("-p", &portable, true);
#endif
	// Process the args
	parameters.process(argc, argv);
	for (int i = 0; i < argc; ++i) {
		friend_restart_args.emplace_back(argv[i]);
	}
	if (!friend_restart_args.empty()) {
		// The program by full path, in case the working folder changes.
		std::error_code   error;
		const auto        program = std::filesystem::absolute(friend_restart_args[0], error);
		if (!error && friend_restart_args[0].find('/') != std::string::npos) {
			friend_restart_args[0] = program.string();
		}
	}
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
			 << "--friend-port <port>\tFriend Mode TCP port (default 1992)" << endl;

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
		if (friend_return_to_menu) {
			Friend_mode_restart_at_menu();
		}
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
		config->value("config/gameplay/wasd_movement", Friend_wasd_movement);
		Friend_targeting_load_settings();
		Friend_class_load_settings();

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
		if ((!skip_splash && !force_skip_splash && !arg_skip_intro) && (Game::get_game_type() != EXULT_DEVEL_GAME || U7exists(INTRO_DAT))) {
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
		Friend_mode_start_hosting();
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
	bool friend_selection_shown = false;
	Tile_coord last_avatar_state(-1, -1, -1);
	int last_avatar_frame = -1;
	uint32 last_npc_snapshot = 0;
	uint32 friend_next_clock_sync = 0;
	int  guest_assigned_actor = -1;
	Tile_coord last_guest_request(-1, -1, -1);
	// Guest: the steps sent to the host, numbered, until it answers them.
	uint32                                    guest_move_seq = 0;
	std::deque<std::pair<uint32, Tile_coord>> guest_sent_steps;
	std::unordered_set<int> guest_frozen_npcs;
	uint32                     guest_close_at = 0;    // Ticks when a disconnected guest exits.
	uint32                     guest_leash_notice_at = 0;    // Last "must return" notice.
	uint32 friend_last_loop = SDL_GetTicks();
	while (!quitting_time) {
		{
			// A main-loop pass that took long (a hang, or a conversation or menu
			// it waited in): logged to help find freezes.
			const uint32 now = SDL_GetTicks();
			if (now - friend_last_loop > 750) {
				cout << "Friend Mode: main loop waited " << (now - friend_last_loop) << " ms"
					 << (gwin->in_combat() ? " (in combat)" : "") << endl;
			}
			friend_last_loop = now;
		}
		friend_session.poll();
		Friend_mode_journal_open_pass(false);
		Friend_mode_take_chats();
		Friend_mode_take_suggestions();
		if (arg_friend_host) {
			if (friend_host_paused_sent) {    // Back from a menu or dialog.
				if (friend_session.connected()) {
					friend_session.send_host_paused(false);
				}
				friend_host_paused_sent = false;
			}
			// The main loop runs only outside conversations (they wait for
			// clicks in their own loop), so one still open here never sent
			// its end: send it, or the guests stay paused.
			if (friend_host_dialogue_active && !gwin->get_usecode()->in_usecode()) {
				cout << "Friend Mode: conversation left open; ending it for guests" << endl;
				Friend_mode_note_dialogue_reset();
			}
			int requester = 0;
			while (friend_session.take_world_request(&requester)) {
				Friend_mode_send_world(requester);
			}
			// Guests arriving and leaving.  Each has its own companion, held
			// item, leash and attack; leaving hands its companion back.
			int joined = 0;
			while (friend_session.take_peer_joined(&joined)) {
				Friend_guest& guest = friend_guests[joined];
				guest.name          = friend_session.player_name(joined);
				const auto hold     = friend_rejoin_holds.find(guest.name);
				const int  kept     = hold != friend_rejoin_holds.end() ? hold->second.actor : -1;
				if (hold != friend_rejoin_holds.end()) {
					friend_rejoin_holds.erase(hold);
				}
				if (kept >= 0 && Friend_actor_is_eligible(static_cast<uint32>(kept), true)
					&& Friend_actor_is_free(static_cast<uint32>(kept), joined) && gwin->reserve_friend_mode_actor(kept)) {
					// Back from a drop: straight to its companion, no picker.
					guest.actor       = kept;
					guest.roster_sent = true;
					friend_session.send_party_members(Friend_party_members(), joined);
					friend_session.accept_actor(static_cast<uint32>(kept), joined);
					Actor* actor     = gwin->get_npc(kept);
					guest.last_state = actor->get_tile();
					guest.last_frame = actor->get_framenum();
					friend_session.send_actor_state(
							guest.last_state.tx, guest.last_state.ty, guest.last_state.tz, guest.last_frame, joined);
					Friend_mode_announce(guest.name + " is back as " + Friend_mode_actor_name(kept), joined);
				} else {
					Friend_mode_announce(guest.name + " has joined", joined);
				}
				if (friend_host_paused_sent) {
					friend_session.send_host_paused(true, joined);    // We're in a menu.
				}
			}
			int         left = 0;
			std::string left_name;
			while (friend_session.take_peer_left(left, left_name)) {
				Friend_guest* guest = Friend_guest_of(left);
				if (!guest) {
					continue;
				}
				Friend_return_held_item(guest->held);    // Left mid-drag.
				Friend_mode_end_guest_attack(*guest, "guest left");
				const int writer = Friend_journal_writer(left);
				bool      freed  = false;
				for (auto it = friend_journal_writers.begin(); it != friend_journal_writers.end();) {
					freed = freed || it->second == writer;
					it    = it->second == writer ? friend_journal_writers.erase(it) : std::next(it);
				}
				if (freed) {
					Friend_mode_send_journal_writers();    // Its note is free again.
				}
				if (guest->actor >= 0) {
					// Kept for a while in case they reconnect; the party's AI
					// drives it meanwhile.
					gwin->release_remote_friend_mode_actor(guest->actor);
					friend_rejoin_holds[left_name]
							= {guest->actor, static_cast<uint32>(SDL_GetTicks()) + friend_rejoin_hold_ms};
					Friend_mode_announce(left_name + " has left; " + Friend_mode_actor_name(guest->actor)
										 + " follows the party until they return");
				} else {
					Friend_mode_announce(left_name + " has left");
				}
				friend_guests.erase(left);
			}
			Friend_mode_end_emotes();
			if (friend_session.connected() && friend_world_sent) {
				const uint32 now = static_cast<uint32>(SDL_GetTicks());
				if (friend_next_autosave == 0) {
					friend_next_autosave = now + friend_autosave_ms;
				} else if (static_cast<sint32>(now - friend_next_autosave) >= 0 && !gwin->get_usecode()->in_usecode()
						   && !friend_host_dialogue_active && !dragging) {
					Friend_mode_autosave();
					friend_next_autosave = now + friend_autosave_ms;
				}
			} else {
				friend_next_autosave = 0;    // Starts again with the next guest.
			}
			for (auto it = friend_rejoin_holds.begin(); it != friend_rejoin_holds.end();) {
				if (static_cast<sint32>(static_cast<uint32>(SDL_GetTicks()) - it->second.until) >= 0) {
					Friend_mode_announce(Friend_mode_actor_name(it->second.actor) + " is free again");
					it = friend_rejoin_holds.erase(it);
				} else {
					++it;
				}
			}
			if (friend_world_reloaded) {
				friend_world_reloaded = false;
				// The load replaced the world: anything a guest held is gone
				// with it.  Send the new world, then hand each guest its
				// companion again if it is still in the party.
				friend_journal_writers.clear();    // A new journal.
				for (auto& [player, guest] : friend_guests) {
					guest.held      = Friend_held_item();
					guest.following = false;    // Its companion waits again in the new world.
				}
				if (friend_session.connected()) {
					friend_world_sent = false;    // Nothing to catch up on in the old world.
					Friend_mode_send_world();
					friend_sent_party = Friend_party_members();
					friend_session.send_party_members(friend_sent_party);
					last_avatar_state = Tile_coord(-1, -1, -1);
					last_avatar_frame = -1;
					last_npc_snapshot = 0;
					for (auto& [player, guest] : friend_guests) {
						if (guest.actor < 0) {
							continue;
						}
						const auto actor_num = static_cast<uint32>(guest.actor);
						if (Friend_actor_is_eligible(actor_num, true) && gwin->reserve_friend_mode_actor(guest.actor)) {
							friend_session.accept_actor(actor_num, player);
							Actor* actor     = gwin->get_npc(guest.actor);
							guest.last_state = actor->get_tile();
							guest.last_frame = actor->get_framenum();
							friend_session.send_actor_state(guest.last_state.tx, guest.last_state.ty,
									guest.last_state.tz, guest.last_frame, player);
						} else {
							Friend_mode_drop_companion(player, guest,
									Friend_mode_actor_name(guest.actor) + " is not in the party in this save; " + guest.name
											+ " is now spectating");
						}
					}
				}
			}
			// Spectating guests (no companion) are kept in step too.
			if (friend_session.connected() && friend_world_sent) {
				const uint32 now = SDL_GetTicks();
				// A host drag lifts the item out of the world, and guests see
				// it gone at once, the same as when a guest picks one up.
				if (now >= friend_next_item_sync) {
					friend_next_item_sync = now + friend_item_sync_ms;
					Friend_mode_item_pass(true);
					Friend_mode_inventory_pass(true);
					Friend_mode_status_pass(false);
					Friend_mode_wake_near_guest();
					Friend_mode_creature_pass(false);
					Friend_mode_health_pass();
				}
				if (now >= friend_next_clock_sync) {
					friend_next_clock_sync = now + friend_clock_sync_ms;
					Friend_mode_clock_pass(false);
					Friend_mode_story_flags_pass(false);
				}
				if (now >= friend_next_journal_sync) {
					friend_next_journal_sync = now + friend_journal_sync_ms;
					Friend_mode_journal_pass(false);
				}
			}
			if (!friend_session.connected()) {
				// Nobody left: start over for the next guest.
				friend_chunk_hashes.clear();
				friend_inventory_hashes.clear();
				friend_sent_party.clear();
				friend_creature_sent.clear();
				friend_last_creature_set.clear();
				friend_world_sent = false;
				friend_journal_sent.clear();
				friend_health_sent.clear();
				friend_flags_sent.clear();
				friend_player_list_sent.clear();
				if (!friend_journal_writers.empty()) {
					friend_journal_writers.clear();
					Notebook_gump::set_remote_writers({});
				}
				friend_host_dialogue_active = false;
				last_avatar_state = Tile_coord(-1, -1, -1);
				last_avatar_frame = -1;
				last_npc_snapshot = 0;
			} else {
				std::vector<uint32> members = Friend_party_members();
				if (members != friend_sent_party) {
					friend_sent_party = std::move(members);
					friend_session.send_party_members(friend_sent_party);
				}
				int eligible = 0;
				for (const uint32 member : friend_sent_party) {
					eligible += Friend_actor_is_eligible(member) ? 1 : 0;
				}
				// One guest per companion, plus a few watching.
				friend_session.set_guest_limit(eligible + friend_max_spectators);
				for (auto& [player, guest] : friend_guests) {
					if (!guest.roster_sent) {
						friend_session.send_party_members(friend_sent_party, player);
						Friend_mode_send_roster(player);
						guest.roster_sent = true;
					}
				}
				Friend_mode_player_list_pass();
			}
			uint32 requested_actor = 0;
			while (friend_session.take_actor_request(requested_actor, &requester)) {
				// A guest may pick again at any time (Tab): its old companion
				// goes back to the party's AI.  Others' companions are taken.
				Friend_guest* guest = Friend_guest_of(requester);
				if (!guest) {
					continue;
				}
				if (requested_actor == Friend_mode_session::spectate_actor) {
					// Give the companion back and watch.
					if (guest->actor >= 0) {
						guest->following = false;
						Friend_return_held_item(guest->held);
						gwin->release_remote_friend_mode_actor(guest->actor);
						Friend_mode_announce(guest->name + " is now spectating; " + Friend_mode_actor_name(guest->actor)
											 + " rejoins the party");
						guest->actor = -1;
					}
					friend_session.accept_actor(requested_actor, requester);
					continue;
				}
				const int  previous = guest->actor;
				// Not mid-conversation: the guests are watching it, and a
				// script may be using the companion.
				const bool can_take = !friend_host_dialogue_active && static_cast<int>(requested_actor) != previous
									  && Friend_actor_is_eligible(requested_actor)
									  && Friend_actor_is_free(requested_actor, requester);
				if (previous >= 0 && can_take) {    // Refused requests keep the old one.
					guest->following = false;
					Friend_return_held_item(guest->held);
					gwin->release_remote_friend_mode_actor(previous);
					guest->actor = -1;
				}
				if (static_cast<int>(requested_actor) == previous) {
					friend_session.accept_actor(requested_actor, requester);    // Kept the same one.
				} else if (can_take && gwin->reserve_friend_mode_actor(static_cast<int>(requested_actor))) {
					guest->actor = static_cast<int>(requested_actor);
					friend_session.accept_actor(requested_actor, requester);
					Friend_mode_announce(
							guest->name + (previous >= 0 ? " switches to " : " enters the world as ")
							+ Friend_mode_actor_name(guest->actor));
					Actor* actor      = gwin->get_npc(guest->actor);
					guest->last_state = actor->get_tile();
					guest->last_frame = actor->get_framenum();
					friend_session.send_actor_state(guest->last_state.tx, guest->last_state.ty,
							guest->last_state.tz, guest->last_frame, requester);
				} else {
					friend_session.reject_actor(requested_actor, requester);
					Friend_mode_send_roster(requester);    // What is free now.
				}
			}
			int tx = 0;
			int ty = 0;
			int tz = 0;
			int requested_frame = 0;
			uint32 move_seq = 0;
			while (friend_session.take_move_request(tx, ty, tz, requested_frame, move_seq, &requester)) {
				Friend_guest* guest = Friend_guest_of(requester);
				Actor*        actor = guest && guest->actor >= 0 ? gwin->get_npc(guest->actor) : nullptr;
				if (!actor) {
					continue;
				}
				guest->move_seq = move_seq;
				if (guest->following) {
					Friend_mode_set_follow(requester, *guest, false);    // Walking takes over.
				}
				const Tile_coord requested(tx, ty, tz);
				// Up to 3 tiles: a guest running on a slow frame can cover
				// more than one step between the positions it sends.
				if (actor->get_tile().distance(requested) <= 3
					&& gwin->friend_leash_allows(actor->get_tile(), requested)) {
					cout << "Friend Mode: host accepted move to " << tx << ',' << ty << ',' << tz << endl;
					Friend_mode_end_guest_attack(*guest, "guest moved");    // Walking takes over.
					// The guest already collision-tested this adjacent step. Applying it
					// directly avoids a second pathfinder disagreeing about door state.
					const Tile_coord from = actor->get_tile();
					actor->move(requested.tx, requested.ty, requested.tz);
					actor->change_frame(requested_frame);
					// Eggs fire as for any party member's step (teleporters and
					// the like); the guest's own copy leaves them to us.
					if (Map_chunk* chunk = actor->get_chunk()) {
						chunk->activate_eggs(actor, requested.tx, requested.ty, requested.tz, from.tx, from.ty);
					}
				} else {
					cout << "Friend Mode: host rejected out-of-range move to " << tx << ',' << ty << ',' << tz << endl;
					// Resend the true position so the guest snaps back.
					guest->last_state = Tile_coord(-1, -1, -1);
				}
			}
			{
				Friend_mode_session::Blob_kind kind;
				std::vector<unsigned char>     request;
				while (friend_session.take_blob(kind, request, &requester)) {
					Friend_guest* guest = Friend_guest_of(requester);
					if (!guest) {
						continue;
					}
					Actor* guest_actor = guest->actor >= 0 ? gwin->get_npc(guest->actor) : nullptr;
					if (Friend_class_host_blob(kind, request, requester)) {
						continue;    // The Class System's.
					}
					if (kind == Friend_mode_session::Blob_kind::guest_lift) {
						Friend_mode_guest_lift(request, guest_actor, guest->held);
					} else if (kind == Friend_mode_session::Blob_kind::guest_move) {
						Friend_mode_perform_guest_move(request, guest_actor, guest->held);
					} else if (kind == Friend_mode_session::Blob_kind::guest_put_back) {
						Friend_return_held_item(guest->held);
					} else if (kind == Friend_mode_session::Blob_kind::guest_use_item) {
						Friend_mode_use_item_for_guest(request, guest_actor);
					} else if (kind == Friend_mode_session::Blob_kind::roster_request) {
						Friend_mode_send_roster(requester);
					} else if (kind == Friend_mode_session::Blob_kind::guest_attack) {
						if (guest->following) {
							Friend_mode_set_follow(requester, *guest, false, false);
						}
						Friend_mode_attack_for_guest(request, *guest);
					} else if (kind == Friend_mode_session::Blob_kind::guest_follow) {
						const int what = request.empty() ? 0 : request[0];
						if (what == friend_follow_menu_open) {
							if (!guest->following) {    // Not if it follows already (Y).
								Friend_mode_set_follow(requester, *guest, true, true, false);
								guest->menu_following = true;
							}
						} else if (what == friend_follow_menu_close) {
							if (guest->menu_following) {
								guest->menu_following = false;
								Friend_mode_set_follow(requester, *guest, false, true, false);
							}
						} else {
							guest->menu_following = false;
							Friend_mode_set_follow(requester, *guest, what != 0);
						}
					} else if (kind == Friend_mode_session::Blob_kind::journal_edit) {
						Friend_mode_journal_edit_from_guest(request, requester);
					} else if (kind == Friend_mode_session::Blob_kind::journal_lock) {
						Friend_mode_journal_lock_from_guest(request, requester);
					} else if (kind == Friend_mode_session::Blob_kind::journal_open) {
						Friend_mode_apply_journal_open(request, requester);
					} else if (kind == Friend_mode_session::Blob_kind::guest_trade) {
						Friend_mode_trade_for_guest(request, guest_actor);
					} else if (kind == Friend_mode_session::Blob_kind::resync_request) {
						// After a guest's conversation: its copy may hold changes
						// the host never had.
						for (auto& entry : friend_inventory_hashes) {
							entry.second = 0;
						}
						Friend_force_item_resync(guest_actor);
					}
				}
			}
			Friend_mode_session::Object_frame_event door_request;
			while (friend_session.take_door_use(door_request, &requester)) {
				const Friend_guest* guest = Friend_guest_of(requester);
				Friend_mode_use_object_for_guest(
						door_request, guest && guest->actor >= 0 ? gwin->get_npc(guest->actor) : nullptr);
			}
			for (auto& [player, guest] : friend_guests) {
				Friend_mode_check_guest_attack(guest);
				Actor* actor = guest.actor >= 0 ? gwin->get_npc(guest.actor) : nullptr;
				if (guest.actor >= 0 && (!actor || actor->is_dead() || !actor->is_in_party())) {
					// Asleep or paralyzed passes; dead or gone does not.
					const std::string companion = Friend_mode_actor_name(guest.actor);
					Friend_mode_drop_companion(player, guest,
							companion + (actor && actor->is_dead() ? " has fallen; " : " has left the party; ")
									+ guest.name + " is now spectating");
					continue;
				}
				if (!actor) {
					continue;
				}
				// The Avatar left the guest's companion beyond the leash: after a
				// grace period, bring it back beside the Avatar like party catch-up.
				Main_actor* avatar    = gwin->get_main_actor();
				const bool  other_map = actor->get_map() != avatar->get_map();
				if (other_map || gwin->friend_leash_distance(actor->get_tile()) > Game_window::friend_leash_max_tiles) {
					const uint32 now = SDL_GetTicks();
					if (guest.leash_return_at == 0) {
						guest.leash_return_at = now + (other_map ? 0 : friend_leash_grace_ms);
					}
					if (now >= guest.leash_return_at) {
						const Tile_coord spot = Map_chunk::find_spot(avatar->get_tile(), 3, actor);
						if (spot.tx >= 0) {
							actor->move(spot, avatar->get_map_num());
							cout << "Friend Mode: " << actor->get_name() << " returned to the Avatar" << endl;
						}
						guest.leash_return_at = 0;
					}
				} else {
					guest.leash_return_at = 0;
				}
				if (actor->get_tile() != guest.last_state || actor->get_framenum() != guest.last_frame) {
					guest.last_state = actor->get_tile();
					guest.last_frame = actor->get_framenum();
					friend_session.send_actor_state(guest.last_state.tx, guest.last_state.ty, guest.last_state.tz,
							guest.last_frame, player, guest.move_seq);
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
					Friend_npcs_near_players(nearby);
					// Every guest gets every NPC, including the companions other
					// guests control; each skips its own.
					for (Actor* npc : nearby) {
						if (!npc || npc == gwin->get_main_actor()) {
							continue;
						}
						const Tile_coord state = npc->get_tile();
						if (npc->get_npc_num() > 0) {
							// The frame's upper bits carry the schedule, plus one
							// (so a merchant's shop hours are right on guests).
							const Schedule* schedule = npc->get_schedule();
							const int       sched    = schedule ? schedule->get_actual_type(npc) : npc->get_schedule_type();
							friend_session.send_npc_state(
									static_cast<uint32>(npc->get_npc_num()), state.tx, state.ty, state.tz,
									npc->get_framenum() | (((sched + 1) & 0xff) << 16));
						}
					}
					Friend_mode_creature_moves();
				}
			}
		} else if (!arg_friend_join.empty()) {
			if (friend_session.take_peer_joined()) {
				Friend_mode_notice("Joined " + friend_session.remote_player_name_value() + "'s world");
			}
			bool host_paused = false;
			if (friend_session.take_host_paused(host_paused)) {
				gwin->set_network_host_paused(host_paused);
				gwin->set_all_dirty();    // The wait bar appears or goes.
			}
			std::vector<uint32> host_party;
			if (friend_session.take_party_members(host_party)) {
				Friend_apply_party_members(host_party);
			}
			Friend_mode_session::Blob_kind blob_kind;
			std::vector<unsigned char>     blob;
			while (friend_session.take_blob(blob_kind, blob)) {
				if (Friend_class_guest_blob(blob_kind, blob)) {
					// The Class System's.
				} else if (blob_kind == Friend_mode_session::Blob_kind::world_save) {
					// The host loaded a save: load the same world, then wait
					// for the host to hand our companion back.
					gwin->set_network_dialogue_paused(false);
					gwin->release_friend_mode_control();
					guest_assigned_actor = -1;
					Friend_mode_apply_follow({0});
					if (Friend_mode_load_world(blob)) {
						guest_frozen_npcs.clear();
						last_guest_request = Tile_coord(-1, -1, -1);
						Friend_mode_notice("The host loaded a saved game");
					}
				} else if (blob_kind == Friend_mode_session::Blob_kind::chunk_objects) {
					Friend_mode_apply_chunk(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::actor_inventory) {
					Friend_mode_apply_inventory(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::object_speech) {
					Friend_mode_apply_object_speech(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::party_status) {
					Friend_mode_apply_party_status(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::game_clock) {
					Friend_mode_apply_clock(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::creature_list) {
					Friend_mode_apply_creature_list(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::journal) {
					Friend_mode_apply_journal(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::journal_locks) {
					Friend_mode_apply_journal_writers(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::journal_open) {
					Friend_mode_apply_journal_open(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::music) {
					Friend_mode_apply_music(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::projectile) {
					Friend_mode_apply_projectile(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::sprite_effect) {
					Friend_mode_apply_sprite(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::actor_health) {
					Friend_mode_apply_health(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::actor_hit) {
					Friend_mode_apply_hit(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::world_sound) {
					Friend_mode_apply_sound(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::story_flags) {
					Friend_mode_apply_story_flags(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::player_list) {
					Friend_mode_apply_player_list(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::guest_follow) {
					Friend_mode_apply_follow(blob);
				} else if (blob_kind == Friend_mode_session::Blob_kind::notice) {
					std::string text;
					for (const unsigned char chr : blob) {
						if (chr >= 32 && chr < 127) {
							text += static_cast<char>(chr);
						}
					}
					if (!text.empty()) {
						Friend_mode_session_notice(text);
					}
				}
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
				friend_watched_npc = -1;    // Spectating starts (or ends) on the Avatar.
			}
			if (spectating && friend_watched_npc >= 0) {
				// Follow the watched player; if they switched, spectate or left,
				// back to the Avatar.
				Actor* watched = gwin->get_npc(friend_watched_npc);
				const bool still_played = watched && !Friend_player_with_actor(watched).empty();
				if (!still_played) {
					friend_watched_npc = -1;
					gwin->set_camera_actor(gwin->get_main_actor());
					gwin->center_view(gwin->get_main_actor()->get_tile());
				} else if (watched->get_tile() != friend_watched_at) {
					const Tile_coord at = watched->get_tile();
					if (friend_watched_at.tx < 0 || friend_watched_at.distance(at) > 8) {
						gwin->center_view(at);
					} else {
						gwin->scroll_if_needed(watched, at);
					}
					friend_watched_at = at;
				}
			}
			if (friend_session.take_connection_lost()) {
				gwin->set_network_dialogue_paused(false);
				gwin->set_network_host_paused(false);
				gwin->release_friend_mode_control();
				guest_assigned_actor = -1;
				// Exult cannot return to the title menu mid-game, so close.
				Friend_mode_notice("Host disconnected. Exult will close.");
				guest_close_at = SDL_GetTicks() + 4000;
			}
			if (guest_close_at != 0 && SDL_GetTicks() >= guest_close_at) {
				quitting_time = QUIT_TIME_YES;
			}
			// The picker waits out a host conversation (a companion can leave
			// the party in one).
			if (friend_picker_requested && !gwin->is_network_dialogue_paused()
				&& friend_session.take_roster_update()) {
				// J: pick a companion again (or a first one, if spectating).
				friend_picker_requested = false;
				friend_selection_shown  = true;
				const int selected = Select_friend_actor(friend_session.party_roster(), guest_assigned_actor >= 0);
				if (selected == friend_pick_spectate) {
					friend_spectate_requested = true;
					friend_session.request_actor(Friend_mode_session::spectate_actor);
				} else if (selected >= 0 && selected != guest_assigned_actor) {
					friend_session.request_actor(static_cast<uint32>(selected));
				} else if (selected >= 0) {
					cout << "Friend Mode: kept the same companion" << endl;
				}
			} else if (friend_session.roster_ready() && !friend_selection_shown) {
				friend_selection_shown = true;
				friend_session.take_roster_update();
				const int selected = Select_friend_actor(friend_session.party_roster());
				if (selected >= 0) {
					friend_session.request_actor(static_cast<uint32>(selected));
				} else {
					cout << "Friend Mode: no companion selected; spectating" << endl;
				}
			}
			if (friend_session.take_actor_rejected()) {
				// Taken by another guest (or the host is busy): pick again from
				// what is free now; the host has sent a fresh list.
				Friend_mode_notice("That companion is taken");
				friend_picker_requested = true;
			}
			uint32 assigned_actor = 0;
			if (friend_session.take_actor_assignment(assigned_actor)) {
				if (guest_assigned_actor >= 0 && guest_assigned_actor != static_cast<int>(assigned_actor)) {
					gwin->release_friend_mode_control();    // Switching companions.
					// The one we left now moves as the host says, like any NPC.
					guest_frozen_npcs.erase(guest_assigned_actor);
					guest_assigned_actor = -1;
					Friend_mode_apply_follow({0});
				}
				if (assigned_actor == Friend_mode_session::spectate_actor) {
					cout << "Friend Mode: spectating" << endl;
					if (!friend_spectate_requested) {
						friend_picker_requested = true;    // Lost it: pick another (the list follows).
					}
					friend_spectate_requested = false;
				} else if (!gwin->take_friend_mode_control(static_cast<int>(assigned_actor), true)) {
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
			uint32 state_seq = 0;
			if (guest_assigned_actor >= 0 && friend_session.take_actor_state(tx, ty, tz, frame, state_seq)) {
				Actor* actor = gwin->get_npc(guest_assigned_actor);
				const Tile_coord state(tx, ty, tz);
				// The host echoes each step it accepts, a round trip late.  An
				// echo of a step we sent (and have since walked past) is
				// ignored, or the companion is pulled back while running.
				// Anything else (a refused step, the leash, a teleporter, the
				// host's AI walking it) is the truth.
				const auto sent = std::find_if(guest_sent_steps.begin(), guest_sent_steps.end(),
						[state_seq](const std::pair<uint32, Tile_coord>& step) {
							return step.first == state_seq;
						});
				const bool echo = sent != guest_sent_steps.end() && sent->second == state
								  && state_seq != guest_move_seq;
				if (echo) {
					guest_sent_steps.erase(guest_sent_steps.begin(), sent);
				} else if (actor && actor->get_tile() != state) {
					// A long jump to beside the Avatar is the leash; anywhere else
					// it was a teleporter.
					const bool beckoned = actor->get_tile().distance_2d(state) > Game_window::friend_leash_warn_tiles
										  && gwin->get_main_actor()->get_tile().distance_2d(state) <= 4;
					actor->move(state.tx, state.ty, state.tz);
					gwin->set_camera_actor(actor);
					if (beckoned) {
						gwin->get_effects()->add_chat_text("You are beckoned by the Avatar", actor);
					}
				}
				if (!echo && actor && actor->get_framenum() != frame) {
					actor->change_frame(frame);
				}
				if (!echo) {
					last_guest_request = state;
				}
			}
			if (friend_session.take_avatar_state(tx, ty, tz, frame)) {
				Main_actor* avatar = gwin->get_main_actor();
				const Tile_coord state(tx, ty, tz);
				if (avatar && avatar->get_tile() != state) {
					// The Avatar can't move during the host's conversation (its
					// main loop waits for the click), so a move long after the
					// last conversation message means we missed its end.
					if (gwin->is_network_dialogue_paused()
						&& SDL_GetTicks() - friend_dialogue_event_at > friend_dialogue_stale_ms) {
						cout << "Friend Mode: the Avatar moved; ending a conversation we never saw end" << endl;
						gwin->get_usecode()->get_conversation()->init_faces();
						gwin->set_network_dialogue_paused(false);
						gwin->set_all_dirty();
					}
					if (gwin->is_network_spectating() && friend_watched_npc >= 0) {
						avatar->Actor::move(state.tx, state.ty, state.tz);    // Watching another player.
					} else if (gwin->is_network_spectating()) {
						// Watching: the view (and roofs) follow the Avatar.
						const Tile_coord from = avatar->get_tile();
						avatar->move(state.tx, state.ty, state.tz);
						if (from.distance(state) > 8) {
							gwin->center_view(state);
						} else {
							gwin->scroll_if_needed(avatar, state);
						}
					} else {
						// This is a remote visual update, not local camera movement.
						// Bypass Main_actor::move so Iolo's roof state remains local.
						avatar->Actor::move(state.tx, state.ty, state.tz);
					}
				}
				if (avatar && avatar->get_framenum() != frame) {
					avatar->change_frame(frame);
				}
			}
			uint32 npc_num = 0;
			while (friend_session.take_npc_state(npc_num, tx, ty, tz, frame)) {
				if (static_cast<int>(npc_num) == guest_assigned_actor) {
					continue;    // Ours comes as our own actor state.
				}
				Actor* npc = gwin->get_npc(static_cast<int>(npc_num));
				const Tile_coord state(tx, ty, tz);
				friend_host_schedules[static_cast<int>(npc_num)] = ((frame >> 16) & 0xff) - 1;
				frame &= 0xffff;
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
				Friend_mode_apply_creature_state(creature_num, Tile_coord(tx, ty, tz), frame);
			}
			Friend_mode_guest_creature_pass(false);
			uint32 speech_source = 0;
			std::string speech_text;
			bool speech_creature = false;
			while (friend_session.take_speech(speech_source, speech_creature, speech_text)) {
				Actor* speaker = speech_creature
						? static_cast<Actor*>(Friend_creature_by_id(speech_source))
						: gwin->get_npc(static_cast<int>(speech_source));
				if (speaker) {
					speaker->say(speech_text.c_str());
				}
			}
			Friend_mode_session::Dialogue_event dialogue_event;
			Conversation* conversation = gwin->get_usecode()->get_conversation();
			while (friend_session.take_dialogue_event(dialogue_event)) {
				friend_dialogue_event_at = SDL_GetTicks();
				if (dialogue_event.type == Friend_mode_session::Dialogue_event_type::begin
					|| dialogue_event.type == Friend_mode_session::Dialogue_event_type::end) {
					cout << "Friend Mode: host conversation "
						 << (dialogue_event.type == Friend_mode_session::Dialogue_event_type::begin ? "began" : "ended")
						 << endl;
				}
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
					guest_sent_steps.emplace_back(++guest_move_seq, last_guest_request);
					if (guest_sent_steps.size() > 64) {
						guest_sent_steps.pop_front();
					}
					friend_session.send_move_request(last_guest_request.tx, last_guest_request.ty,
							last_guest_request.tz, actor->get_framenum(), guest_move_seq);
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
			if ((SDL_BUTTON_RMASK & ms) && !right_on_gump && !friend_wasd_dx && !friend_wasd_dy) {
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

		// WASD movement: walk toward the held keys every frame, as the arrow
		// keys do (two keys walk diagonally), so a new direction takes effect
		// at once.  Read from the keyboard itself, so a key let go while a
		// menu or another window had it never sticks.
		if (Friend_wasd_movement) {
			const bool*      keys   = SDL_GetKeyboardState(nullptr);
			const SDL_Keymod mod    = SDL_GetModState();
			const bool       typing = friend_chat_active || Notebook_gump::get_instance()
								|| (mod & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) || !gwin->have_focus()
								|| dragging;
			const int dx = typing ? 0 : (keys[SDL_SCANCODE_D] ? 1 : 0) - (keys[SDL_SCANCODE_A] ? 1 : 0);
			const int dy = typing ? 0 : (keys[SDL_SCANCODE_S] ? 1 : 0) - (keys[SDL_SCANCODE_W] ? 1 : 0);
			if ((dx || dy) && gwin->main_actor_can_act_charmed()) {
				if (!friend_wasd_dx && !friend_wasd_dy) {
					Friend_mode_walking_stops_attack();
				}
				gwin->start_actor(gwin->get_width() / 2 + 50 * dx, gwin->get_height() / 2 + 50 * dy, get_walking_speed(nullptr));
			} else if (friend_wasd_dx || friend_wasd_dy) {
				gwin->stop_actor();    // Let go.
			}
			friend_wasd_dx = dx;
			friend_wasd_dy = dy;
		}
		Friend_mode_check_target();

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
		Friend_mode_paint_maps();
		Friend_mode_paint_range_frame();
		if (Combat::is_paused() && Friend_mode_in_session()) {
			Combat::resume();    // Paused before co-op began: no pausing in co-op.
		}
		Friend_class_frame();
		Friend_mode_paint_target_arrow();
		Friend_mode_paint_leash_arrow();
		Friend_mode_paint_dialogue_wait();
		Friend_mode_paint_chat_input();
		Friend_mode_paint_chat_log();
		Friend_mode_paint_player_list();

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

// The chat line: Enter opens it (not while writing in the journal), typing,
// Tab completion, Enter sends, Escape closes.  From Handle_event, and on the
// host also while a conversation waits for a click.  True if the event was
// the chat line's.
static bool Friend_mode_chat_event(SDL_Event& event) {
	if (friend_session.connected()) {
		if (friend_chat_active) {
			if (event.type == SDL_EVENT_TEXT_INPUT) {
				for (const char* ch = event.text.text; *ch && friend_chat_text.size() < 80; ++ch) {
					if (static_cast<unsigned char>(*ch) >= 32 && static_cast<unsigned char>(*ch) < 127) {
						friend_chat_text.push_back(*ch);
					}
				}
				gwin->set_all_dirty();
				return true;
			}
			if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_ESCAPE) {
					friend_chat_active = false;
					friend_chat_text.clear();
					SDL_StopTextInput(gwin->get_win()->get_screen_window());
					gwin->set_all_dirty();
				} else if (event.key.key == SDLK_TAB) {
					Friend_mode_complete_chat(friend_chat_text);    // Player names, commands.
					gwin->set_all_dirty();
				} else if (event.key.key == SDLK_BACKSPACE) {
					if (!friend_chat_text.empty()) {
						friend_chat_text.pop_back();
						gwin->set_all_dirty();
					}
				} else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
					if (!friend_chat_text.empty()) {
						Friend_mode_send_chat_line(friend_chat_text);
						friend_session.poll();
					}
					friend_chat_active = false;
					friend_chat_text.clear();
					SDL_StopTextInput(gwin->get_win()->get_screen_window());
					gwin->set_all_dirty();
				}
				return true;
			}
			if (event.type == SDL_EVENT_KEY_UP) {
				return true;
			}
		} else if (event.type == SDL_EVENT_KEY_DOWN
				   && (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
				   && !Notebook_gump::get_instance()) {    // Enter in the journal is a new line.
			friend_chat_active = true;
			friend_chat_text.clear();
			gwin->stop_actor();
			SDL_StartTextInput(gwin->get_win()->get_screen_window());
			gwin->set_all_dirty();
			return true;
		}
	}
	return false;
}

/*
 *  Handle an event.  This should work for all platforms, and should only
 *  be called in 'normal' and 'gump' modes.
 */

static void Handle_event(SDL_Event& event) {
	if (Friend_keyboard_targeting && event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_Q
		&& !event.key.repeat && !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !friend_chat_active
		&& !Notebook_gump::get_instance()) {
		Friend_mode_cycle_target((event.key.mod & SDL_KMOD_SHIFT) ? -1 : 1);    // Q: the next one.
		return;
	}
	if (Friend_keyboard_targeting && event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_SPACE
		&& !event.key.repeat && !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !friend_chat_active
		&& !Notebook_gump::get_instance() && Friend_mode_space(event.key.mod & SDL_KMOD_SHIFT)) {
		return;    // Space: talk to, use or attack the pick.
	}
	if (Friend_keyboard_targeting && event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_E
		&& !event.key.repeat && !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !friend_chat_active
		&& !Notebook_gump::get_instance()) {
		Friend_mode_cycle_object((event.key.mod & SDL_KMOD_SHIFT) ? -1 : 1);    // E: the next thing.
		return;
	}
	if (event.type == SDL_EVENT_KEY_DOWN && (event.key.mod & SDL_KMOD_SHIFT) && !event.key.repeat
		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !friend_chat_active
		&& !Notebook_gump::get_instance() && gwin->get_main_actor()
		&& (event.key.scancode == SDL_SCANCODE_I
			|| (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_8))) {
		// Shift+I: the next backpack; Shift+1 to Shift+8: one member's.
		// (Shift+C, sorting a window, is just below.)
		Friend_mode_backpack_key(event.key.scancode == SDL_SCANCODE_I ? -1 : event.key.scancode - SDL_SCANCODE_1);
		return;
	}
	if (Friend_sort_key && event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_C
		&& (event.key.mod & SDL_KMOD_SHIFT) && !event.key.repeat
		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !friend_chat_active
		&& !Notebook_gump::get_instance() && gwin->get_main_actor()) {
		Friend_mode_sort_window();    // Shift+C: tidy the newest window.
		return;
	}
	if (Friend_wasd_movement && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
		&& Friend_is_wasd_key(event.key.scancode) && !friend_chat_active && !Notebook_gump::get_instance()) {
		// WASD movement: the keys alone walk (the main loop reads them as
		// they are held), so the key bindings don't get them; with Shift
		// they do what the key alone did (Shift+W the watch, Shift+S Save).
		if (event.key.mod & SDL_KMOD_SHIFT) {
			event.key.mod = static_cast<SDL_Keymod>(event.key.mod & ~SDL_KMOD_SHIFT);
			event.key.key = SDL_GetKeyFromScancode(event.key.scancode, SDL_KMOD_NONE, false);
		} else if (!(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) {
			return;
		}
	}
	if (friend_session.mode() != Friend_mode_session::Mode::off && !friend_chat_active
		&& event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_L && !event.key.repeat
		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !Notebook_gump::get_instance()) {
		friend_chat_log_hidden = !friend_chat_log_hidden;    // L: hide or show the chat log.
		Friend_mode_notice(friend_chat_log_hidden ? "Chat Log Disabled" : "Chat Log Enabled");
		return;
	}
	if (friend_session.mode() != Friend_mode_session::Mode::off && !friend_chat_active
		&& (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.key == SDLK_TAB) {
		// Hold Tab: who plays whom.
		const bool shown = event.type == SDL_EVENT_KEY_DOWN;
		if (shown != friend_player_list_shown) {
			friend_player_list_shown = shown;
			gwin->set_all_dirty();
		}
		return;
	}
	if (Friend_mode_is_guest() && friend_session.connected() && !friend_chat_active
		&& event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_X && !event.key.repeat
		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !Notebook_gump::get_instance()
		&& gwin->is_network_spectating()) {
		Friend_mode_watch_next();    // X: watch the next player.
		return;
	}
	if (Friend_mode_is_guest() && friend_session.connected() && !friend_chat_active
		&& event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_J && !event.key.repeat
		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !Notebook_gump::get_instance()) {
		// J: pick a companion (again).  Not while the host is in a conversation
		// or paused; the picker waits for the host's current list.
		if (gwin->is_network_dialogue_paused() || gwin->is_network_host_paused()) {
			Mouse::mouse()->flash_shape(Mouse::redx);
		} else if (!friend_picker_requested) {
			friend_picker_requested = true;
			friend_session.take_roster_update();    // Wait for the fresh list.
			friend_session.send_blob(Friend_mode_session::Blob_kind::roster_request, {1});
		}
		return;
	}
	if (Friend_mode_is_guest() && friend_session.connected() && !friend_chat_active
		&& event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_Y && !event.key.repeat
		&& !(event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) && !Notebook_gump::get_instance()) {
		// Y: follow the Avatar (the party's AI drives our companion) until we
		// walk, or press E again.
		Actor* own = gwin->has_network_friend_control() ? gwin->get_controlled_actor() : nullptr;
		if (!own || gwin->is_network_dialogue_paused() || gwin->is_network_host_paused()) {
			Mouse::mouse()->flash_shape(Mouse::redx);
		} else {
			own->stop();
			friend_session.send_blob(
					Friend_mode_session::Blob_kind::guest_follow, {static_cast<unsigned char>(!friend_following)});
		}
		return;
	}
	if (arg_friend_host && !friend_chat_active && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F9) {
		if (event.key.mod & SDL_KMOD_SHIFT) {
			Friend_mode_spawn_test_creatures();
		} else {
			Friend_mode_spawn_test_kit();
		}
		return;
	}
	if (Friend_mode_chat_event(event)) {
		return;
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
				Friend_mode_walking_stops_attack();
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
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		int x;
		int y;
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
		if (Friend_mode_is_guest() && event.button.button == 1 && gwin->is_network_dialogue_paused()) {
			// Suggest a reply: a click on one of the Avatar's choices.
			Conversation* conversation = gwin->get_usecode()->get_conversation();
			const int     choice = conversation && conversation->choices_shown() ? conversation->conversation_choice(x, y) : -1;
			if (choice >= 0 && choice < conversation->get_num_answers()) {
				friend_session.send_suggestion(friend_session.local_player(), conversation->get_answer(choice));
				break;
			}
		}
		if (dont_move_mode) {
			break;
		}

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
// Friend Mode: while a sign, plaque, scroll or book waits, Space closes it
// (or turns the page) as a click does.
static bool get_click_space_continues = false;

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
		Friend_mode_click_wait_poll();

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
			// The host chats during a conversation too.
			if (friend_host_dialogue_active && Friend_mode_chat_event(event)) {
				continue;
			}
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
					if (!chr && c == SDLK_SPACE && get_click_space_continues && !event.key.repeat) {
						x                   = -1;
						y                   = -1;
						g_waiting_for_click = false;
						return true;
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
	const bool save_space     = get_click_space_continues;
	get_click_space_continues = dynamic_cast<Text_gump*>(paint) != nullptr;
	const bool ret            = Get_click(x, y, chr, drag_ok, rotate_colors);
	get_click_space_continues = save_space;
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
