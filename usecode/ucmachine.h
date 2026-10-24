/*
 *  ucmachine.cc - Interpreter for usecode.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef UCMACHINE_H
#define UCMACHINE_H

#include "exceptions.h"
#include "singles.h"

#include <algorithm>
#include <array>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

class Game_window;
class Usecode_machine;
class Conversation;
class Keyring;
class Game_object;
class Actor;
class Usecode_value;
class Usecode_class_symbol;
class Tile_coord;

/*
 *  Here's our virtual machine for running usecode.  The actual internals
 *  are in Usecode_internal.
 */
class Usecode_machine : nonreplicatable, public Game_singletons {
protected:
	// Global flags.
	std::vector<unsigned char> gflags;
	// Handles the SI keyring
	Keyring* keyring = nullptr;
	// Handles conversations
	Conversation* conv = nullptr;
	// Friend Mode guest: the companion talking to a merchant on this copy.
	Actor* friend_shopper = nullptr;
	// And the only answers the merchant's own script may offer it.
	int                      friend_answers_function = -1;
	std::vector<std::string> friend_answers;

	// "Page size" for global flags.
	constexpr static const size_t gflags_page_size = 2048;

public:
	constexpr static const size_t max_num_gflags = 32768;
	constexpr static const size_t last_gflag     = max_num_gflags - 1;

	friend class Usecode_script;
	// Create Usecode_internal.
	static Usecode_machine* create();
	Usecode_machine();
	virtual ~Usecode_machine();
	// Read in usecode from static and patch. Called by Usecode_internal constructor but can be called again later to reload usecode
	virtual void read_usecode()=0;
	// Read in usecode functions.
	virtual void read_usecode(std::istream& file, bool patch = false) = 0;

	// Possible events:
	enum Usecode_events {
		npc_proximity = 0,
		double_click  = 1,
		internal_exec = 2,    // Internal call via intr. 1 or 2.
		egg_proximity = 3,
		weapon        = 4,    // From weapons.dat.
		readied       = 5,    // Wear an item.
		unreadied     = 6,    // Removed an item.
		died          = 7,    // In SI only, I think.
		chat          = 9,    // When a NPC wants to talk to you in SI
		si_path_fail  = 14,
		num_events    = 15,
	};

	enum Global_flag_names {
		si_did_first_scene    = 0x03,    // After Iolo's talk at the start of SI.
		failed_copy_protect   = 0x38,
		did_first_scene       = 0x3b,    // Went through 1st scene with Iolo.
		have_trinsic_password = 0x3d,
		found_stable_key      = 0x3c,
		left_trinsic          = 0x57,
		doing_xenka_return    = 0x272,
		avatar_is_thief       = 0x2eb,
	};

	// Get ith flag, without bounds checks.
	bool get_global_flag_unsafe(int i) {
		return gflags[i] != 0;
	}

	// Set ith flag, without bounds checks.
	void set_global_flag_unsafe(int i, bool val) {
		gflags[i] = static_cast<unsigned char>(val);
	}

	// Get ith flag, with bounds checks. Returns std::nullopt if out of range.
	std::optional<bool> get_global_flag(int i) {
		if (i >= 0 && static_cast<size_t>(i) < gflags.size()) {
			return get_global_flag_unsafe(i);
		}
		return std::nullopt;
	}

	bool get_global_flag_bool(int i) {
		return get_global_flag(i).value_or(false);
	}

	// Set ith flag, with bounds checks. Resizes gflags vector if needed, to a
	// multiple of the page size. Returns false if i < 0 || i >= max_num_gflags.
	bool set_global_flag(int i, bool val) {
		if (i < 0 || static_cast<size_t>(i) >= max_num_gflags) {
			return false;
		}
		if (i >= static_cast<int>(gflags.size())) {
			set_num_global_flags(i + 1);
		}
		set_global_flag_unsafe(i, val);
		return true;
	}

	size_t get_num_global_flags() const {
		return gflags.size();
	}

	void set_num_global_flags(size_t n) {
		size_t newsize = ((n + gflags_page_size - 1) / gflags_page_size) * gflags_page_size;
		newsize        = std::clamp(newsize, gflags_page_size, max_num_gflags);
		gflags.resize(newsize, 0);
	}

	void reset_global_flags(size_t n = gflags_page_size) {
		gflags.clear();
		set_num_global_flags(n);
	}

	void compact_global_flags() {
		auto rit = std::find(gflags.rbegin(), gflags.rend(), 1);
		if (rit == gflags.rend()) {
			gflags.resize(gflags_page_size);
		} else {
			// rit dereferences to the last nonzero flag.
			// it dereferences to one after the last nonzero flag.
			auto   it     = rit.base();
			size_t offset = std::distance(gflags.begin(), it);
			set_num_global_flags(offset);
		}
	}

	// Start speech, or show text.
	virtual void do_speech(int num)                                      = 0;
	virtual bool in_usecode()                                            = 0;    // Currently in a usecode function?
	virtual bool in_usecode_for(Game_object* item, Usecode_events event) = 0;

	Keyring* getKeyring() const {
		return keyring;
	}

	// Call desired function.
	virtual int                   call_usecode(int id, Game_object* item, Usecode_events event) = 0;
	virtual bool                  call_method(Usecode_value* inst, int id, Game_object* item)   = 0;
	virtual int                   find_function(const char* nm, bool noerr = false)             = 0;
	virtual const char*           find_function_name(int funcid)                                = 0;
	virtual Usecode_class_symbol* get_class(int n)                                              = 0;
	virtual Usecode_class_symbol* get_class(const char* nm)                                     = 0;
	virtual int                   get_shape_fun(int n)                                          = 0;
	virtual void                  write() = 0;    // Write out 'gamedat/usecode.dat'.
	virtual void                  read()  = 0;    // Read in 'gamedat/usecode.dat'.

	void init_conversation();
	int  get_num_faces_on_screen() const;

	// Friend Mode.  While a guest's companion talks to a merchant on the
	// guest's own copy, purchases go to the host and the companion's
	// portrait stands in for the Avatar's.
	void set_friend_shopper(Actor* actor) {
		friend_shopper = actor;
	}

	Actor* get_friend_shopper() const {
		return friend_shopper;
	}

	// While a guest talks to a merchant, answers that the merchant's own
	// script ('function') adds are kept to 'answers' (buying and training,
	// not the story).  Answers added by the routines it calls (a shop's
	// list, yes and no) all show.  -1 ends it.
	void set_friend_answers(int function, std::vector<std::string> answers) {
		friend_answers_function = function;
		friend_answers          = std::move(answers);
	}

	// Host: carry out a guest's purchase.  Each entry is quantity, shape,
	// quality, frame.  Takes 'removes' from the party (all or nothing),
	// then gives 'adds' to 'buyer' (the rest of the party if its pack is
	// full).  False if the party can't pay.
	using Party_trade_item = std::array<int, 4>;
	virtual bool friend_party_trade(
			const std::vector<Party_trade_item>& removes, const std::vector<Party_trade_item>& adds, Actor* buyer)
			= 0;
	// Friend Mode's spell bar: whether function 'funcid' calls intrinsic
	// 'intrinsic' itself (a spell that asks for a target calls
	// click_on_item).
	virtual bool friend_calls_intrinsic(int funcid, int intrinsic) = 0;
	Conversation* get_conversation() const {
		return conv;
	}

	// intercept the next click_on_item intrinsic
	virtual void         intercept_click_on_item(Game_object* obj)          = 0;
	virtual Game_object* get_intercept_click_on_item() const                = 0;
	// Answer a later click_on_item too, after the intercepted one (Friend
	// Mode: a script that asks for two clicks, like painting).
	virtual void         queue_click_on_item(Game_object* obj)              = 0;
	virtual void         intercept_click_on_tile(Tile_coord* t)             = 0;
	virtual Tile_coord*  get_intercept_click_on_tile() const                = 0;
	virtual void         save_intercept(Game_object*& obj, Tile_coord*& t)  = 0;
	virtual void         restore_intercept(Game_object* obj, Tile_coord* t) = 0;

	virtual bool function_exists(int funcid) = 0;

	virtual int get_num_funcs () = 0;
};

#endif /* INCL_USECODE */
