/**
 ** Party.h - Manage the party.
 **
 ** Written: 4/8/02 - JSF
 **/
/*  Copyright (C) 2002-2022  The Exult Team
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

#ifndef INCL_PARTY_H
#define INCL_PARTY_H 1

#include "singles.h"
#include "tiles.h"

#include <array>
#include <cstddef>
#include <vector>

class Actor;

#define EXULT_PARTY_MAX 8

/*
 *  Manage the party.
 */
class Party_manager : public Game_singletons {
	// NPC #'s of party members.
	std::array<int, EXULT_PARTY_MAX> party;
	// # of NPC's in party.
	size_t party_count;
	// NPC #'s of dead party members.
	std::array<int, 16> dead_party;
	// # of dead NPC's in party.
	size_t dead_party_count;
	// NPC's able to walk with Avatar.
	std::array<Actor*, EXULT_PARTY_MAX> valid;
	// # of valid NPC's in party.
	size_t validcnt;
	// Formation-walking:
	void move_followers(Actor* npc, int vindex, int dir) const;
	// Friend Mode: near a door or swamp, or in a narrow passage, one
	// behind another (false otherwise).
	bool single_file(Actor* leader, int dir) const;
	// Steps left in single file after the last reason for it.
	mutable int single_file_left = 0;
	// Place in the formation: the party number, with -1 for the leader
	// (the Avatar, or a companion led in Friend Mode, whose place the
	// Avatar takes).
	int formation_slot(const Actor* npc) const;
	// 'aside': out of the way of one it follows walking toward it.
	int  step(Actor* npc, Actor* leader, int dir, const Tile_coord& dest, bool aside = false) const;
	// Friend Mode: the leader's last direction, and steps left after it
	// turned (90 degrees or more), when followers don't trade places.
	mutable int        last_dir        = -1;
	mutable int        turn_steps_left = 0;
	mutable Tile_coord last_lead_pos{-1, -1, -1};
	// While followers step: the leader, and which way it really went (0-7;
	// the direction it is given is only one of four).
	mutable Actor* lead     = nullptr;
	mutable int    lead_dir = 0;
	mutable int    lead_face = 0;    // And which way it faces (one of four).
	// The formation mirrored (left and right exchanged), and choosing that
	// on a turn.
	mutable bool mirrored = false;
	void         choose_mirror(Actor* leader, int face) const;
	// Which way the formation faces (0-3: N, E, S, W; -1 not yet).
	int formation_face = -1;
	// A turn waiting (see get_followers): which way, and steps that way.
	int pending_face  = -1;
	int pending_steps = 0;
	// The followers walking in formation, in its order.
	std::vector<Actor*> formation_members(const Actor* leader) const;

public:
	Party_manager();

	void set_count(int n) {    // For initializing from file.
		party_count = n;
	}

	void set_member(int i, int npcnum) {
		party[i] = npcnum;
	}

	int get_count() const {    // Get # party members.
		return party_count;
	}

	int get_member(int i) const {    // Get npc# of i'th party member.
		return party[i];
	}

	// Friend Mode: move the member at place 'from' to place 'to' (from 0),
	// the ones between shifting over one.  The places are the formation's
	// and the faces bar's order.  False if nothing moved.
	bool move_member(int from, int to);

	int get_dead_count() const {    // Same for dead party members.
		return dead_party_count;
	}

	int get_dead_member(int i) const {
		return dead_party[i];
	}

	// A Bidirectional Iterator that iterates the Party as Actors
	class party_actor_iterator {
		using value_type = Actor*;
		int num;

	public:
		party_actor_iterator(int num) : num(num) {}

		Actor* operator*() {
			return partyman->get_actor(num);
		}

		Actor* operator->() {
			return partyman->get_actor(num);
		}

		party_actor_iterator& operator++(int) {
			++num;
			return *this;
		}

		party_actor_iterator operator++() {
			return party_actor_iterator(num++);
		}

		party_actor_iterator& operator--(int) {
			--num;
			return *this;
		}

		party_actor_iterator operator--() {
			return party_actor_iterator(num--);
		}

		bool operator==(const party_actor_iterator& other) {
			return num == other.num;
		}

		bool operator!=(const party_actor_iterator& other) {
			return num != other.num;
		}

		bool operator<(const party_actor_iterator& other) {
			return num < other.num;
		}

		bool operator<=(const party_actor_iterator& other) {
			return num < other.num;
		}
	};

	party_actor_iterator begin() const {
		return party_actor_iterator(0);
	}

	party_actor_iterator end() const {
		return party_actor_iterator(get_count());
	}

	// Iterate the party including the MainActor
	struct {
		party_actor_iterator begin() const {
			return party_actor_iterator(-1);
		}

		party_actor_iterator end() const {
			return party_actor_iterator(partyman->get_count());
		}

	} IterateWithMainActor;

	// Get Party Member as Actor*, -1 will return Main Actor. Out of range
	// returns nullptr
	Actor* get_actor(int i) const;

	// Add/remove party member.
	bool add_to_party(Actor* npc);
	bool remove_from_party(Actor* npc);
	int  in_dead_party(Actor* npc) const;
	bool add_to_dead_party(Actor* npc);
	bool remove_from_dead_party(Actor* npc);
	// Update status of NPC that died or
	//   was resurrected.
	void update_party_status(Actor* npc);
	void link_party();    // Set party's id's.
	// Formation-walking:
	void get_followers(int dir);
	// Friend Mode: where a follower's spot in the formation is now.
	bool formation_spot(const Actor* npc, Tile_coord& spot) const;
};

#endif
