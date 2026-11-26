/**
 ** Party.cc - Manage the party.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "party.h"

#include "Face_stats.h"
#include "ShortcutBar_gump.h"
#include "actors.h"
#include "chunks.h"
#include "dir.h"
#include "exult.h"
#include "frameseq.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "schedule.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;

/*
 *  Create.
 */

Party_manager::Party_manager() : party{}, party_count(0), dead_party{}, dead_party_count(0), valid{}, validcnt(0) {}

/*
 *  Add NPC to party.
 *
 *  Output: false if no room or already a member.
 */

bool Party_manager::add_to_party(Actor* npc    // (Should not be the Avatar.)
) {
	const size_t maxparty = party.size();
	if (!npc || party_count == maxparty || npc->is_in_party()) {
		return false;
	}
	remove_from_dead_party(npc);    // Just to be sure.
	npc->set_alignment(Actor::good);
	npc->set_party_id(party_count);
	npc->set_flag(Obj_flags::in_party);
	// We can take items.
	npc->set_flag_recursively(Obj_flags::okay_to_take);
	party[party_count++] = npc->get_npc_num();
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
	return true;
}

/*
 *  Friend Mode: change the party's order.
 */

bool Party_manager::move_member(int from, int to) {
	const int count = static_cast<int>(party_count);
	if (from < 0 || to < 0 || from >= count || to >= count || from == to) {
		return false;
	}
	const int num = party[from];
	if (from < to) {
		for (int i = from; i < to; ++i) {
			party[i] = party[i + 1];
		}
	} else {
		for (int i = from; i > to; --i) {
			party[i] = party[i - 1];
		}
	}
	party[to] = num;
	for (int i = std::min(from, to); i <= std::max(from, to); ++i) {
		if (Actor* npc = gwin->get_npc(party[i])) {
			npc->set_party_id(i);
		}
	}
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
	return true;
}

/*
 *  Remove party member.
 *
 *  Output: false if not found.
 */

bool Party_manager::remove_from_party(Actor* npc) {
	if (!npc) {
		return false;
	}
	const int id = npc->get_party_id();
	if (id == -1) {    // Not in party?
		return false;
	}
	const int npc_num = npc->get_npc_num();
	if (party[id] != npc_num) {
		cout << "Party mismatch!!" << endl;
		return false;
	}
	// Shift the rest down.
	for (size_t i = id + 1; i < party_count; i++) {
		Actor* npc2 = gwin->get_npc(party[i]);
		if (npc2) {
			npc2->set_party_id(i - 1);
		}
		party[i - 1] = party[i];
	}
	npc->clear_flag(Obj_flags::in_party);
	party_count--;
	party[party_count] = 0;
	npc->set_party_id(-1);
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
	return true;
}

/*
 *  Find index of NPC in dead party list.
 *
 *  Output: Index, or -1 if not found.
 */

int Party_manager::in_dead_party(Actor* npc) const {
	const int num = npc->get_npc_num();
	for (size_t i = 0; i < dead_party_count; i++) {
		if (dead_party[i] == num) {
			return i;
		}
	}
	return -1;
}

/*
 *  Add NPC to dead party list.
 *
 *  Output: false if no room or already a member.
 */

bool Party_manager::add_to_dead_party(Actor* npc    // (Should not be the Avatar.)
) {
	const size_t maxparty = dead_party.size();
	if (!npc || dead_party_count == maxparty || in_dead_party(npc) >= 0) {
		return false;
	}
	dead_party[dead_party_count++] = npc->get_npc_num();
	return true;
}

/*
 *  Remove NPC from dead party list.
 *
 *  Output: false if not found.
 */

bool Party_manager::remove_from_dead_party(Actor* npc) {
	if (!npc) {
		return false;
	}
	const int id = in_dead_party(npc);    // Get index.
	if (id == -1) {                       // Not in list?
		return false;
	}
	// Shift the rest down.
	for (size_t i = id + 1; i < dead_party_count; i++) {
		dead_party[i - 1] = dead_party[i];
	}
	dead_party_count--;
	dead_party[dead_party_count] = 0;
	return true;
}

/*
 *  Update party status of an NPC that has died or been resurrected.
 */

void Party_manager::update_party_status(Actor* npc) {
	if (npc->is_dead()) {    // Dead?
		// Move party members to dead list.
		if (remove_from_party(npc)) {
			add_to_dead_party(npc);
		}
	} else {    // Alive.
		if (remove_from_dead_party(npc)) {
			add_to_party(npc);
		}
	}
}

/*
 *  In case NPC's were read after usecode, set party members' id's, and
 *  move dead members into separate list.
 */

void Party_manager::link_party() {
	// avatar is a party member too
	gwin->get_main_actor()->set_flag(Obj_flags::in_party);
	// You own your own stuff.
	gwin->get_main_actor()->set_flag_recursively(Obj_flags::okay_to_take);
	const size_t tmp_party_count = party_count;
	party_count = dead_party_count = 0;
	// Now process them.
	for (size_t i = 0; i < tmp_party_count; i++) {
		if (party[i] <= 0) {    // Fix corruption.
			continue;
		}
		Actor* npc = gwin->get_npc(party[i]);
		// Shouldn't happen!
		if (!npc) {
			continue;    // Skip bad entry.
		}
		int oldid = npc->get_party_id();
		// But this has happened:
		if (oldid >= 0 && static_cast<unsigned int>(oldid) < party_count) {
			continue;    // Skip bad entry.
		}
		const int npc_num = npc->get_npc_num();
		if (npc->is_dead()) {    // Put dead in special list.
			npc->set_party_id(-1);
			if (dead_party_count >= dead_party.size()) {
				continue;
			}
			dead_party[dead_party_count++] = npc_num;
			continue;
		}
		npc->set_party_id(party_count);
		party[party_count++] = npc_num;
		// ++++This messes up places where they should wait, and should be
		// unnecessary.
		//		npc->set_schedule_type(Schedule::follow_avatar);
		// We can use all his/her items.
		npc->set_flag_recursively(Obj_flags::okay_to_take);
		npc->set_flag(Obj_flags::in_party);
	}
}

// QOTC_PARTY_LOG=<file>: why the party walks as it does, step by step.
static std::FILE* Party_log() {
	static std::FILE* log = [] {
		const char* name = std::getenv("QOTC_PARTY_LOG");
		return name ? std::fopen(name, "w") : nullptr;
	}();
	return log;
}

/*
 *  Friend Mode (2026-11-25): the formation as the original game has it
 *  (checked in DOSBox), one shape for each number of followers, rather
 *  than Exult's tree (each member two tiles behind and to one side of
 *  another, so a big party fanned out into a wide triangle).  Spots are
 *  tiles from the leader with it facing north: x to its right, y behind
 *  it.  The first followers are nearest the leader; the order is the
 *  party's (Shift+O).  Seven followers at most (eight with the Avatar).
 */
namespace {
	struct Formation_spot {
		int x, y;
	};

	constexpr int formation_max = 7;
	// Straight steps the new way before the formation turns.
	constexpr int formation_turn_steps = 2;

	const Formation_spot formation_spots[formation_max][formation_max] = {
			{{2, 2}},
			{{-2, 2}, {2, 2}},
			{{-2, 2}, {2, 2}, {0, 4}},
			{{-2, 2}, {2, 2}, {-1, 4}, {1, 4}},
			{{-2, 2}, {2, 2}, {-2, 4}, {2, 4}, {0, 6}},
			{{-2, 2}, {2, 2}, {-1, 4}, {1, 4}, {-2, 6}, {2, 6}},
			{{-2, 2}, {2, 2}, {-3, 4}, {0, 4}, {3, 4}, {-2, 6}, {2, 6}},
	};

	/*
	 *  A spot as tiles from the leader, for the way the formation faces
	 *  (0-3: north, east, south, west), mirrored (left and right
	 *  exchanged) or not.
	 */
	Tile_coord Formation_offset(const Formation_spot& spot, int face, bool mirror) {
		const int x = mirror ? -spot.x : spot.x;
		const int y = spot.y;
		switch (face) {
		case 1:    // East: its right is south, behind it west.
			return Tile_coord(-y, x, 0);
		case 2:    // South.
			return Tile_coord(-x, -y, 0);
		case 3:    // West: its right is north, behind it east.
			return Tile_coord(y, -x, 0);
		default:    // North.
			return Tile_coord(x, y, 0);
		}
	}

	/*
	 *  Who a spot's follower keeps near, stepping: the nearest spot in a
	 *  row ahead of it, or the leader (-1).
	 */
	int Formation_guide(int count, int k) {
		const Formation_spot& me   = formation_spots[count - 1][k];
		int                   best = -1;
		int                   dist = me.x * me.x + me.y * me.y;
		for (int i = 0; i < count; ++i) {
			const Formation_spot& o = formation_spots[count - 1][i];
			if (o.y >= me.y) {
				continue;
			}
			const int d = (o.x - me.x) * (o.x - me.x) + (o.y - me.y) * (o.y - me.y);
			if (d < dist) {
				dist = d;
				best = i;
			}
		}
		return best;
	}
}    // namespace

/*
 *  This should be called each time the Avatar takes a step while under
 *  control of the user.
 */

void Party_manager::get_followers(int dir    // Direction (0-7) Avatar just stepped.
) {
	// Friend Mode: a companion one leads is the formation's root, and the
	// Avatar walks in its place (formation_slot).
	Actor* leader = gwin->get_party_leader();
	validcnt      = 0;    // Get party members to control.
	for (size_t i = 0; i < party_count; i++) {
		Actor* npc = gwin->get_npc(party[i]);
		if (npc && npc == leader) {
			npc = gwin->get_main_actor();
		}
		if (!npc || npc->get_flag(Obj_flags::asleep) || npc->get_flag(Obj_flags::paralyzed) || npc->is_dead()) {
			continue;    // Not available.
		}
		// Friend Mode: a guest's companion steps only for its guest (one
		// seated in a chair was pulled along, then walked back to it).
		if (Friend_mode_player_playing(npc) >= 0) {
			continue;
		}
		const int sched = npc->get_schedule_type();
		// Skip if in combat or set to 'wait'.  Friend Mode (2026-11-25):
		// one walking a path back to the party (catching up to its spot,
		// see move_followers) keeps its place in the formation; it just
		// isn't stepped.
		if (sched != Schedule::combat && sched != Schedule::wait &&
			// Loiter added for SI.
			sched != Schedule::loiter
			&& (!npc->in_queue() || sched == Schedule::follow_avatar)) {    // Already walking?
			valid[validcnt++] = npc;
		}
	}
	// Friend Mode: after a sharp turn (90 degrees or more), for a few steps
	// followers don't trade places with one another (see step).
	// Which way it went, from where it was (the direction given is one of
	// four: walking a diagonal, east and north by turns, looked like a turn
	// every step, so they never traded places and one in the way stuck).
	const Tile_coord lpos     = leader ? leader->get_tile() : Tile_coord(-1, -1, -1);
	int              move_dir = dir;
	if (last_lead_pos.tx >= 0 && lpos != last_lead_pos && last_lead_pos.distance_2d(lpos) <= 2) {
		move_dir = Get_direction(last_lead_pos.ty - lpos.ty, lpos.tx - last_lead_pos.tx);
	}
	last_lead_pos = lpos;
	if (last_dir >= 0) {
		const int diff = (move_dir - last_dir + 8) % 8;
		if (std::min(diff, 8 - diff) >= 2) {
			turn_steps_left = 4;
		} else if (turn_steps_left > 0) {
			--turn_steps_left;
		}
	}
	last_dir = move_dir;
	// Friend Mode (2026-11-25): the formation faces one of four ways, as in
	// the original.  A straight step turns it that way; a diagonal one
	// keeps the way it faces when that is one of the diagonal's two (going
	// northeast facing north, it slides along), else the way the leader
	// faces.
	int        face     = formation_face;
	const bool diagonal = move_dir % 2 != 0;
	const int  vert     = (move_dir == 1 || move_dir == 7) ? 0 : 2;    // A diagonal's two ways.
	const int  horiz    = (move_dir == 1 || move_dir == 3) ? 1 : 3;
	if (!diagonal) {
		face = move_dir / 2;
	} else if (face != vert && face != horiz) {
		face = (dir / 2) % 4;
	}
	// Friend Mode (2026-11-25): a little grace before it turns.  The
	// formation turns only after a few steps the new way: with WASD a key
	// tapped on the way into a diagonal (east, then north and east) was a
	// straight step, and the party swung round for it.  A diagonal step
	// leaning the new way neither counts nor cancels it; a step any other
	// way does.
	if (formation_face >= 0 && face != formation_face) {
		if (face == pending_face) {
			++pending_steps;
		} else {
			pending_face  = face;
			pending_steps = 1;
		}
		if (pending_steps < formation_turn_steps) {
			face = formation_face;
		}
	} else if (pending_face >= 0 && !(diagonal && (pending_face == vert || pending_face == horiz))) {
		pending_face  = -1;
		pending_steps = 0;
	}
	if (face != formation_face) {
		pending_face  = -1;
		pending_steps = 0;
		// Turning: the formation or its mirror image, whichever is nearer
		// where everyone is.
		if (formation_face >= 0 && leader) {
			turn_steps_left = 4;
			choose_mirror(leader, face);
		}
		formation_face = face;
	}
	lead      = leader;
	lead_dir  = move_dir;
	lead_face = dir;
	if (validcnt) {
		// Followers glide in time with the leader's step.
		uint32 start;
		int    ms;
		Actor::follow_glide_ms = leader && leader->get_glide_timing(start, ms) ? ms : 0;
		move_followers(leader, -1, dir);
		Actor::follow_glide_ms = 0;
	}
	lead = nullptr;
}

/*
 *  Friend Mode: the followers walking in formation, in its order (nearest
 *  the leader first), seven at most.
 */

std::vector<Actor*> Party_manager::formation_members(const Actor* leader) const {
	std::vector<Actor*> line;
	for (size_t i = 0; i < validcnt; ++i) {
		if (valid[i] != leader && !valid[i]->is_dead()) {
			line.push_back(valid[i]);
		}
	}
	std::stable_sort(line.begin(), line.end(), [this](const Actor* a, const Actor* b) {
		return formation_slot(a) < formation_slot(b);
	});
	if (line.size() > static_cast<size_t>(formation_max)) {
		line.resize(formation_max);
	}
	return line;
}

/*
 *  Friend Mode (2026-11-22): on a turn, the formation or its mirror image,
 *  whichever is nearer where everyone is.  Turning around, the spots to
 *  the left and right change sides, and everyone swung across to keep
 *  theirs (one at the formation's south corner going east went to its
 *  north corner going west); mirrored, each turns about and keeps to its
 *  side, in the same row.
 */

void Party_manager::choose_mirror(Actor* leader, int face) const {
	const std::vector<Actor*> line = formation_members(leader);
	if (line.empty()) {
		return;
	}
	const int        count = static_cast<int>(line.size());
	const Tile_coord lpos  = leader->get_tile();
	auto             cost  = [&](bool mirror) {
        int total = 0;
        for (int k = 0; k < count; ++k) {
            const Tile_coord at = lpos + Formation_offset(formation_spots[count - 1][k], face, mirror);
            const Tile_coord t  = line[k]->get_tile();
            const int        dx = Tile_coord::delta(t.tx, at.tx);
            const int        dy = Tile_coord::delta(t.ty, at.ty);
            total += dx * dx + dy * dy;
        }
        return total;
	};
	if (cost(!mirrored) < cost(mirrored)) {
		mirrored = !mirrored;
	}
}


int Party_manager::formation_slot(const Actor* npc) const {
	const Actor* leader = gwin->get_party_leader();
	const Actor* avatar = gwin->get_main_actor();
	if (leader != avatar) {
		if (npc == leader) {
			return -1;
		}
		if (npc == avatar) {
			return leader->get_party_id();
		}
	}
	return npc->get_party_id();
}

/*
 *  Friend Mode (2026-11-25): each follower steps toward its own spot in
 *  the formation, measured from the leader, front rows first.  (Exult's
 *  tree had each follow another member, looked up by party number in a
 *  list that leaves out the guests' companions and the busy: in co-op one
 *  could be stepped twice toward two spots, back and forth, and another
 *  never, left behind.)
 */

void Party_manager::move_followers(
		Actor* npc,       // The leader, who just stepped.
		int    vindex,    // (Unused.)
		int    dir        // Direction (0-7) it stepped in.
) const {
	ignore_unused_variable_warning(vindex);
	if (single_file(npc, dir)) {
		return;
	}
	const std::vector<Actor*> line = formation_members(npc);
	if (line.empty()) {
		return;
	}
	const int        count = static_cast<int>(line.size());
	const int        face  = formation_face >= 0 ? formation_face : (dir / 2) % 4;
	const Tile_coord pos   = npc->get_tile();
	if (std::FILE* log = Party_log()) {
		std::fprintf(log, "%d,%d facing %d%s (waiting %d/%d) going %d\n", pos.tx, pos.ty, face, mirrored ? " mirrored" : "",
					 pending_face, pending_steps, lead_dir);
		std::fflush(log);
	}
	// The leader's step, to aim ahead of a spot that moves with it.
	const Tile_coord next  = pos.get_neighbor(lead_dir);
	const int        ahead_x = Tile_coord::delta(pos.tx, next.tx);
	const int        ahead_y = Tile_coord::delta(pos.ty, next.ty);
	int              speed = npc->get_frame_time();
	if (speed <= 0) {
		speed = gwin->get_std_delay();
	}
	for (int k = 0; k < count; ++k) {
		Actor* each = line[k];
		if (each->is_dead() || each->in_queue()) {
			continue;    // (Walking a path back to its spot.)
		}
		Tile_coord spot = pos + Formation_offset(formation_spots[count - 1][k], face, mirrored);
		spot.tz         = pos.tz;
		// Friend Mode (2026-11-25): one step a step never closes the gap to
		// a spot that moves as fast (after a turn, a follower was left half
		// way, the party strung out in a line).  Three tiles or more off,
		// it walks there half again as fast as the leader, aiming where the
		// spot will be, then steps with the rest.
		const Tile_coord at  = each->get_tile();
		const int        off = at.distance(spot);
		// Is its spot ahead of it, the way the leader goes (moving away), or
		// behind it (coming toward it)?
		const int toward = ahead_x * Tile_coord::delta(at.tx, spot.tx) + ahead_y * Tile_coord::delta(at.ty, spot.ty);
		// Not to a spot coming toward it: the path goes where the spot was.
		// Turning around, the last in the formation walked back the whole
		// length of it while the party went the other way, and was left
		// far behind.  Stepping, they meet half way.
		if (off >= 3 && toward >= 0) {
			const int  lead_steps = std::min(off / 2, 3);
			Tile_coord goal       = spot + Tile_coord(ahead_x * lead_steps, ahead_y * lead_steps, 0);
			// A few tiles at a time, so it aims again as the spot moves.
			constexpr int leg = 4;
			goal.tx           = at.tx + std::clamp(Tile_coord::delta(at.tx, goal.tx), -leg, leg);
			goal.ty           = at.ty + std::clamp(Tile_coord::delta(at.ty, goal.ty), -leg, leg);
			goal.tz           = pos.tz;
			if (each->walk_path_to_tile(goal, std::max(speed * 2 / 3, 50), 0, 1, 1)) {
				if (std::FILE* log = Party_log()) {
					std::fprintf(log, "  %d %s at %d,%d spot %d,%d off %d: path to %d,%d\n", k, each->get_name().c_str(),
								 at.tx, at.ty, spot.tx, spot.ty, off, goal.tx, goal.ty);
				}
				continue;
			}
		}
		const int guide = Formation_guide(count, k);
		Actor*    near  = guide >= 0 && !line[guide]->is_dead() ? line[guide] : npc;
		step(each, near, dir, spot, true);
		if (std::FILE* log = Party_log()) {
			const Tile_coord now = each->get_tile();
			std::fprintf(log, "  %d %s at %d,%d spot %d,%d off %d: %s\n", k, each->get_name().c_str(), at.tx, at.ty,
						 spot.tx, spot.ty, off, now == at ? "no step" : "step");
		}
	}
}


/*
 *  Friend Mode (2026-11-25): where a follower's spot in the formation is
 *  now, for one walking back to the party (Actor::follow).
 *
 *  Output: false if it has none (not walking in formation).
 */

bool Party_manager::formation_spot(const Actor* npc, Tile_coord& spot) const {
	const Actor* leader = gwin->get_party_leader();
	if (!leader || npc == leader || formation_face < 0) {
		return false;
	}
	const std::vector<Actor*> line = formation_members(leader);
	const auto                it   = std::find(line.begin(), line.end(), npc);
	if (it == line.end()) {
		return false;
	}
	const int count = static_cast<int>(line.size());
	spot            = leader->get_tile()
			+ Formation_offset(formation_spots[count - 1][it - line.begin()], formation_face, mirrored);
	spot.tz = leader->get_lift();
	return true;
}

/*
 *  Is the ground at a tile poisonous (a swamp)?  Only the flat counts, and
 *  only for one standing on the ground (lift 0), as Actor::step has it.
 */

static bool Is_poison_ground(Game_map* map, int tx, int ty) {
	tx = (tx + c_num_tiles) % c_num_tiles;
	ty = (ty + c_num_tiles) % c_num_tiles;
	Map_chunk* chunk = map->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
	if (!chunk) {
		return false;
	}
	const ShapeID flat = chunk->get_flat(tx % c_tiles_per_chunk, ty % c_tiles_per_chunk);
	return flat.get_shapenum() != -1 && flat.get_info().has_contact_effect();
}

/*
 *  Would stepping onto a tile poison an NPC (swamp, without boots)?
 */

static bool Poisons(Actor* npc, const Tile_coord& to) {
	if (to.tz != 0) {
		return false;
	}
	const int  tx    = (to.tx + c_num_tiles) % c_num_tiles;
	const int  ty    = (to.ty + c_num_tiles) % c_num_tiles;
	Map_chunk* chunk = npc->get_map()->get_chunk_safely(tx / c_tiles_per_chunk, ty / c_tiles_per_chunk);
	if (!chunk) {
		return false;
	}
	bool water;
	bool poison;
	Actor::get_tile_info(
			npc, Game_window::get_instance(), chunk, tx % c_tiles_per_chunk, ty % c_tiles_per_chunk, water, poison);
	return poison;
}

// Where the leader has been, latest first (Leader_trail[0] is where it is).
static std::vector<Tile_coord> Leader_trail;
// Steps left walking one behind another for a door.
static int Door_file_left = 0;

/*
 *  Friend Mode (2026-11-01): through doorways in single file.  The
 *  formation's spots, two tiles behind and to each side, could land in the
 *  pocket between an open door's leaf and the wall, and a follower there
 *  stayed behind (followers take steps, they don't find paths).  While a
 *  door (open or closed) is within 3 tiles of the leader or a follower,
 *  each follower instead steps into the tile the one ahead of it just
 *  left, so the party threads through behind the leader.
 *
 *  Friend Mode (2026-11-20): the same along swamps and through narrow
 *  passages.  The formation's spots fan out two tiles to each side, so on
 *  a dry path through a swamp someone always waded in and was poisoned,
 *  and in a corridor the outer ones bumped along the walls.  Single file
 *  also while swamp ground is within 3 tiles of anyone in the party on
 *  the ground, or while anyone has walls (not party members) within 2
 *  tiles on both sides, and for a few steps after, so the party doesn't
 *  flip between the two at the edges.  Here they walk not one right
 *  behind another (their figures overlapped) but in two columns along
 *  the leader's trail, a row every two of its steps, one tile to each
 *  side of it.  A spot that would be wall or swamp puts that one on the
 *  trail itself, alone in its row, so on a path one tile wide they go in
 *  single file, two tiles apart.
 *
 *  Output: false (formation as usual) with no reason for single file.
 */

bool Party_manager::single_file(Actor* leader, int dir) const {
	// How far 'at' is from the nearest tile of 'door'.
	auto door_distance = [](Game_object* door, const Tile_coord& at) {
		const TileRect foot = door->get_footprint();
		const int      dx   = std::max({foot.x - at.tx, at.tx - (foot.x + foot.w - 1), 0});
		const int      dy   = std::max({foot.y - at.ty, at.ty - (foot.y + foot.h - 1), 0});
		return std::max(dx, dy);
	};
	const Tile_coord pos = leader->get_tile();
	// Remember the trail (afresh after a jump, as by a teleport).
	if (!Leader_trail.empty()) {
		const Tile_coord& last = Leader_trail.front();
		if (last.distance_2d(pos) > 2 || std::abs(last.tz - pos.tz) > 4) {
			Leader_trail.clear();
		}
	}
	if (Leader_trail.empty() || Leader_trail.front() != pos) {
		Leader_trail.insert(Leader_trail.begin(), pos);
		if (Leader_trail.size() > 24) {
			Leader_trail.pop_back();
		}
	}
	Game_object_vector found;
	Game_object::find_nearby(found, pos, c_any_shapenum, 12, 0);
	std::vector<Game_object*> doors;
	for (Game_object* obj : found) {
		if (obj->get_info().is_door() && !obj->get_owner()) {
			doors.push_back(obj);
		}
	}
	auto near_door = [&](const Actor* who) {
		const Tile_coord at = who->get_tile();
		return std::any_of(doors.begin(), doors.end(), [&](Game_object* door) {
			return std::abs(door->get_lift() - at.tz) <= 4 && door_distance(door, at) <= 3;
		});
	};
	Game_map* map        = leader->get_map();
	auto      near_swamp = [&](const Actor* who) {
		const Tile_coord at = who->get_tile();
		if (at.tz != 0 || !map) {
			return false;
		}
		for (int dy = -3; dy <= 3; ++dy) {
			for (int dx = -3; dx <= 3; ++dx) {
				if (Is_poison_ground(map, at.tx + dx, at.ty + dy)) {
					return true;
				}
			}
		}
		return false;
	};
	// Blocked, but not by one of the party?
	auto is_wall = [&](Actor* who, Tile_coord t) {
		if (!who->is_blocked(t)) {
			return false;
		}
		auto in_party_at = [&](Actor* each) {
			Tile_coord at = t;
			at.tz         = each->get_lift();
			return each != who && each->blocks(at);
		};
		if (in_party_at(leader)) {
			return false;
		}
		for (size_t i = 0; i < validcnt; ++i) {
			if (in_party_at(valid[i])) {
				return false;
			}
		}
		return true;
	};
	// How many tiles to a wall to one side (0 if none within 4, or 3 on
	// a diagonal, about the same distance).
	const int reach     = dir % 2 ? 3 : 4;
	auto      wall_side = [&](Actor* who, int sdir) {
		Tile_coord t = who->get_tile();
		for (int n = 1; n <= reach; ++n) {
			t = t.get_neighbor(sdir);
			if (is_wall(who, t)) {
				return n;
			}
		}
		return 0;
	};
	// Walls on both sides, across the way the party goes, too close for
	// the formation (it fans out six tiles to each side)?
	auto in_passage = [&](Actor* who) {
		return wall_side(who, (dir + 2) % 8) && wall_side(who, (dir + 6) % 8);
	};
	const bool lead_door    = near_door(leader);
	const bool lead_swamp   = !lead_door && near_swamp(leader);
	const bool lead_passage = !lead_door && !lead_swamp && in_passage(leader);
	bool       door         = lead_door;
	bool       other        = lead_swamp || lead_passage;
	for (size_t i = 0; !door && i < validcnt; ++i) {
		if (valid[i] != leader && !valid[i]->is_dead()) {
			door  = near_door(valid[i]);
			other = other || (!door && (near_swamp(valid[i]) || in_passage(valid[i])));
		}
	}
	if (std::FILE* log = Party_log()) {
		{
			std::fprintf(
					log, "%d,%d,%d dir %d  leader: door %d swamp %d walls %d/%d  party: door %d other %d  left %d/%d\n",
					pos.tx, pos.ty, pos.tz, dir, lead_door, lead_swamp, wall_side(leader, (dir + 6) % 8),
					wall_side(leader, (dir + 2) % 8), door, other, single_file_left, Door_file_left);
			std::fflush(log);
		}
	}
	if (door) {
		Door_file_left = 4;
	} else if (Door_file_left > 0) {
		--Door_file_left;
	}
	if (door || other) {
		single_file_left = 4;
	} else if (single_file_left > 0) {
		--single_file_left;
	} else {
		return false;
	}
	// The order: the formation's (its slots, nearest the leader first).
	std::vector<Actor*> line;
	for (size_t i = 0; i < validcnt; ++i) {
		if (valid[i] != leader && !valid[i]->is_dead()) {
			line.push_back(valid[i]);
		}
	}
	std::stable_sort(line.begin(), line.end(), [this](const Actor* a, const Actor* b) {
		return formation_slot(a) < formation_slot(b);
	});
	if (Door_file_left > 0 || Leader_trail.size() < 2) {
		// Through a door: each into the tile the one ahead just left.
		Actor*     ahead = leader;
		Tile_coord left  = pos.get_neighbor((dir + 4) % 8);    // Where the leader came from.
		for (Actor* each : line) {
			const Tile_coord was = each->get_tile();
			if (each->in_queue()) {
				// Walking a path back to the party: it goes on, keeping its
				// place in the line.
				left  = was;
				ahead = each;
				continue;
			}
			step(each, ahead, each->get_direction(left), left);
			if (each->is_dead()) {
				break;
			}
			left  = was;
			ahead = each;
		}
		return true;
	}
	// Two columns along the trail.
	auto good_spot = [&](Actor* who, const Tile_coord& t) {
		return !is_wall(who, t) && !Poisons(who, t);
	};
	std::vector<Tile_coord> dest(line.size());
	std::vector<int>        rows(line.size());
	for (size_t k = 0, row = 1; k < line.size(); ++row) {
		const size_t      at     = std::min(2 * row, Leader_trail.size() - 1);
		const Tile_coord& center = Leader_trail[at];
		const Tile_coord& nearer = Leader_trail[at - 1];
		const int         tdir   = nearer == center ? dir : Get_direction(center.ty - nearer.ty, nearer.tx - center.tx);
		Tile_coord        lspot  = center.get_neighbor((tdir + 6) % 8);
		Tile_coord        rspot  = center.get_neighbor((tdir + 2) % 8);
		lspot.tz = rspot.tz = center.tz;
		if (k + 1 < line.size()) {
			// The one nearer the left spot takes it.
			const Tile_coord a = line[k]->get_tile();
			const Tile_coord b = line[k + 1]->get_tile();
			if (a.distance_2d(rspot) + b.distance_2d(lspot) < a.distance_2d(lspot) + b.distance_2d(rspot)) {
				std::swap(line[k], line[k + 1]);
			}
			if (good_spot(line[k], lspot) && good_spot(line[k + 1], rspot)) {
				dest[k] = lspot;
				dest[k + 1] = rspot;
				rows[k] = rows[k + 1] = static_cast<int>(row);
				k += 2;
				continue;
			}
		}
		dest[k] = center;
		rows[k] = static_cast<int>(row);
		++k;
	}
	// Front rows first, each following one in the row ahead.
	Actor* ahead     = leader;
	Actor* row_ahead = leader;
	int    cur_row   = 1;
	for (size_t k = 0; k < line.size(); ++k) {
		if (rows[k] != cur_row) {
			cur_row   = rows[k];
			row_ahead = ahead;
		}
		Actor* each = line[k];
		if (each->in_queue()) {
			ahead = each;    // Walking a path back to the party.
			continue;
		}
		step(each, row_ahead, each->get_direction(dest[k]), dest[k]);
		if (!each->is_dead()) {
			ahead = each;
		}
	}
	return true;
}

/*
 *  Get tile to step to, given destination tile (possibly more than 1
 *  step away), and the party's direction.
 */

inline Tile_coord Get_step_tile(
		const Tile_coord& pos,     // Current pos.
		const Tile_coord& dest,    // Desired dest.
		int               dir      // Dir. party is moving (0-7).
) {
	ignore_unused_variable_warning(dir);
	int dx = dest.tx - pos.tx;
	int dy = dest.ty - pos.ty;
	if (dx < -1) {
		dx = -1;    // Limit to 1 tile.
	} else if (dx > 1) {
		dx = 1;
	}
	if (dy < -1) {
		dy = -1;
	} else if (dy > 1) {
		dy = 1;
	}
	return pos + Tile_coord(dx, dy, 0);
}

/*
 *  Friend Mode: a follower ahead of the one it follows, in its way (as
 *  when the party turns around), steps out to the side rather than
 *  toward its spot through it.  The side is the one it is on, or else
 *  its spot's.
 *
 *  Output: the tile to step to, or 'pos' if it isn't in the way.
 */

static Tile_coord Aside_step(
		const Tile_coord& pos,     // Current pos.
		const Tile_coord& dest,    // Desired dest.
		const Tile_coord& lpos,    // Where the one it follows is.
		int               dir      // Which way that one walks (0-7).
) {
	static const int dirs[8][2] = {
			{ 0, -1},
            { 1, -1},
            { 1,  0},
            { 1,  1},
            { 0,  1},
            {-1,  1},
            {-1,  0},
            {-1, -1}
    };
	const int fx    = dirs[dir][0];
	const int fy    = dirs[dir][1];
	const int sx    = dirs[(dir + 2) % 8][0];
	const int sy    = dirs[(dir + 2) % 8][1];
	const int rx    = Tile_coord::delta(lpos.tx, pos.tx);
	const int ry    = Tile_coord::delta(lpos.ty, pos.ty);
	const int along = rx * fx + ry * fy;
	const int perp  = rx * sx + ry * sy;
	// Two tiles to the side (diagonals count double, about 3 for 2).
	const int clear = dir % 2 ? 3 : 2;
	if (along <= 0) {
		return pos;    // Behind it.
	}
	if (std::abs(perp) >= clear) {
		// Out of its way: back along the side while its spot is in the
		// way (heading for it would cut back in front of it).
		const int dperp = Tile_coord::delta(lpos.tx, dest.tx) * sx + Tile_coord::delta(lpos.ty, dest.ty) * sy;
		if (std::abs(dperp) < clear) {
			return pos + Tile_coord(-fx, -fy, 0);
		}
		return pos;
	}
	int side = perp > 0 ? 1 : (perp < 0 ? -1 : 0);
	if (!side) {
		const int dperp = Tile_coord::delta(lpos.tx, dest.tx) * sx + Tile_coord::delta(lpos.ty, dest.ty) * sy;
		side            = dperp < 0 ? -1 : 1;
	}
	return pos + Tile_coord(side * sx, side * sy, 0);
}

/*
 *  Find the party member occupying a given tile, starting with a given
 *  party #.
 *  Note:   Maybe it should check a rectangle of tiles someday if we want
 *      to have NPC's bigger than 1 tile.
 */

static Actor* Find_member_blocking(
		Tile_coord pos,     // Position to check.
		int        first    // Party ID to start with.
) {
	Game_window*   gwin  = Game_window::get_instance();
	Party_manager* pman  = gwin->get_party_man();
	const int      count = pman->get_count();

	for (int i = first; i < count; i++) {
		Actor* npc = gwin->get_npc(pman->get_member(i));
		assert(npc != nullptr);
		pos.tz = npc->get_lift();    // Use NPC's, since it might be up/dn
		//   by a step.
		if (npc->blocks(pos)) {
			return npc;    // Found.
		}
	}
	return nullptr;
}

/*
 *  Get the direction from a tile to NPC's position.
 */

inline int Get_dir_from(Actor* npc, Tile_coord& from) {
	const Tile_coord pos = npc->get_tile();
	return Get_direction(from.ty - pos.ty, pos.tx - from.tx);
}

/*
 *  Is the straight path to the leader clear, and less than 5 tiles?
 */

inline bool Clear_to_leader(
		Actor* npc, Actor* leader,
		Tile_coord from    // Start from here.
) {
	const Tile_coord dest = leader->get_tile();
	int              dist = leader->distance(from);
	if (dist > 4) {
		return false;    // Too far.
	}
	while (--dist) {    // Check tiles up to there.
		const int  dir  = Get_dir_from(leader, from);
		Tile_coord next = from.get_neighbor(dir);
		if (npc->is_blocked(next, &from)) {
			Actor* bnpc = Find_member_blocking(next, 0);
			if (!bnpc) {
				return false;    // Blocked by non-party-member.
			}
			next.tz = bnpc->get_lift();
		}
		from = next;
	}
	const int difftz = from.tz - dest.tz;    // Check diff. in z-coords.
	return difftz * difftz <= 1;             // Can't be more than 2.
}

/*
 *  Get a notion of 'cost' for stepping to a particular tile.
 *
 *  Output: Currently, 10000 if blocked, or (dist)**2.
 */

const int max_cost = 10000;

static int Get_cost(
		Actor*     npc,                       // NPC to take the step.
		Actor*            leader,                     // NPC he's following.
		Tile_coord        to,                         // Tile to step to.
		Actor**           find_blocking = nullptr,    // Returns blocking party member.
		const Tile_coord* dest          = nullptr     // Its spot in the formation.
) {
	int cost = 0;
	if (find_blocking) {
		*find_blocking = nullptr;
	}
	if (npc->is_blocked(to)) {    // (To.tz is updated.)
		// Can't go there.
		if (find_blocking) {
			// Find member we can swap with.
			*find_blocking = Find_member_blocking(to, 1 + npc->get_party_id());
			if (!*find_blocking) {
				return max_cost;
			}
			to.tz = (*find_blocking)->get_lift();
			cost += 1;    // Assess one point to swap.
		} else {
			return max_cost;
		}
	}
	const Tile_coord lpos   = leader->get_tile();
	const int        difftz = to.tz - lpos.tz;
	const int    // Measure closeness.
			diffty
			= Tile_coord::delta(to.ty, lpos.ty);
	const int difftx = Tile_coord::delta(to.tx, lpos.tx);
	// Get dist**2 in x-y plane.
	const int xydist2 = diffty * diffty + difftx * difftx;
	cost += difftz * difftz;
	if (dest) {
		// Friend Mode: toward its spot.  Going by closeness to the one it
		// follows, one on the formation's edge kept in its way (a bush,
		// say) stepped in toward the middle, then back out.
		const int dy = Tile_coord::delta(to.ty, dest->ty);
		const int dx = Tile_coord::delta(to.tx, dest->tx);
		cost += dx * dx + dy * dy;
	} else {
		cost += xydist2;
	}
	// Friend Mode: around swamp, rather than through it.
	if (Poisons(npc, to)) {
		cost += 32;
	}
	if (xydist2 > 2) {    // More than 1 tile away?
		// Check 1 more tile towards leader.
		if (!Clear_to_leader(npc, leader, to)) {
			cost += 16;    // If blocked, try to avoid.
		}
	}
	return cost;
}

/*
 *  Take best step to follow the leader.
 *
 *  Output: True if a step taken.
 */

static bool Take_best_step(
		Actor* npc, Actor* leader,
		Tile_coord& pos,           // Current pos.
		int         frame,         // Frame to show.
		int               dir,                // Direction we want to go.
		bool              swap = true,        // Trade places with one in the way?
		const Tile_coord* dest = nullptr      // Its spot (Friend Mode).
) {
	static const int deltadir[] = {0, 1, 7, 2, 6, 3, 5};

	int        best_cost = max_cost + 8;
	Tile_coord best(-1, -1, -1);
	Actor*     best_in_way = nullptr;
	for (const int i : deltadir) {
		const int        diri = (dir + i) % 8;
		const Tile_coord to   = pos.get_neighbor(diri);
		Actor*           in_way;    // Fudge cost with diff. in dir.
		const int        cost = Get_cost(npc, leader, to, swap ? &in_way : nullptr, dest);
		if (!swap) {
			in_way = nullptr;
		}
		if (cost < best_cost) {
			best_cost   = cost;
			best_in_way = in_way;
			best        = to;
		}
	}
	if (best_cost >= max_cost) {
		return false;
	}
	if (!best_in_way) {    // Nobody in way?
		return npc->step(best, frame);
	}
	best = best_in_way->get_tile();    // Swap positions.
	Game_object_shared npc_keep;
	Game_object_shared best_keep;
	npc->remove_this(&npc_keep);
	best_in_way->remove_this(&best_keep);
	npc->change_frame(frame);    // Appear to take a step.
	npc->move(best);
	best_in_way->move(pos);
	return true;
}

/*
 *  See if a step is reasonable.  This is the first test made.
 */

inline bool Is_step_okay(
		Actor*     npc,       // NPC to take the step.
		Actor*     leader,    // NPC he's following.
		Tile_coord to         // Tile to step to.
) {
	if (npc->is_pos_invalid() || npc->is_blocked(to)) {    // (To.tz is updated.)
		return false;
	}
	// Friend Mode: not into swamp from dry ground (the best step avoids it).
	if (Poisons(npc, to) && !Poisons(npc, npc->get_tile())) {
		return false;
	}
	int difftz = to.tz - leader->get_lift();
	difftz *= difftz;    // Deltaz squared.
	if (difftz > 4) {    // More than 2?
		return false;    // We'll want to find best dir.
	}
	// How close in XY?
	const int dist = leader->distance(to);
	if (dist == 1) {
		return difftz <= 1;    // 1 tile away, so want dz <= 1.
	}
	return Clear_to_leader(npc, leader, to);    // Couldn't take a 2nd step.
}

/*
 *  Move one follower to its destination (if possible).
 *
 *  Output: Direction (0-7) moved (or given 'dir' if we don't move).
 */

int Party_manager::step(
		Actor*            npc,
		Actor*            leader,    // Who NPC is following.
		int               dir,       // Direction we're walking (0-7).
		const Tile_coord& dest,      // Destination tile.
		bool              aside      // Out of the leader's way (Friend Mode).
) const {
	Tile_coord pos = npc->get_tile();    // Current position.
	// Out of the way of the party's leader (followers ahead of it when it
	// turns around), going by which way it really goes.
	Actor*     from = lead ? lead : leader;
	Tile_coord to   = aside && npc != from ? Aside_step(pos, dest, from->get_tile(), lead ? lead_dir : dir) : pos;
	if (to == pos) {
		to = Get_step_tile(pos, dest, dir);
	}
	if (to.tx == pos.tx && to.ty == pos.ty) {
		return dir;    // Not moving.
	}
	// TEST:
	if (npc->in_queue() || npc->is_moving()) {
		cout << npc->get_name() << " shouldn't be stepping!" << endl;
	}
	// Friend Mode: face the way it steps, one of four the way the Avatar's
	// are picked (Get_direction4: a diagonal faces east or west).  The
	// party's direction was used, and followers behind followers got it as
	// one of eight, halved, so walking southwest some faced south while
	// the Avatar faced west.
	// Stepping about the way the leader goes (within 45 degrees), it faces
	// as the leader does: going northeast, a step north or east (the
	// formation's steps are a mix) faces east, as the Avatar does.
	int       face  = static_cast<int>(Get_direction4(pos.ty - to.ty, to.tx - pos.tx));
	const int going = Get_direction(pos.ty - to.ty, to.tx - pos.tx);
	if (lead && std::min((going - lead_dir + 8) % 8, (lead_dir - going + 8) % 8) <= 1) {
		face = lead_face;
	}
	// Turning right around, a step facing sideways first.
	face = Turning_facing(npc->get_dir_facing(), face);
	Frames_sequence* frames     = npc->get_frames(face);
	int&             step_index = npc->get_step_index();
	if (!step_index) {    // First time?  Init.
		step_index = frames->find_unrotated(npc->get_framenum());
	}
	// Get next (updates step_index).
	const int frame = frames->get_next(step_index);
	// Want dz<=1, dx<=2, dy<=2.
	if (Is_step_okay(npc, leader, to) && npc->step(to, frame))
		;
	// Could have died from stepping on
	//   something.
	// Friend Mode: no trading places right after a turn (they flicked
	// through one another).
	else if (npc->is_dead()
			 || !Take_best_step(npc, leader, pos, frame, npc->get_direction(dest), turn_steps_left == 0, &dest)) {
		// Failed to take a step.
		cout << npc->get_name() << " failed to take a step" << endl;
		frames->decrement(step_index);
		return dir;
	}
	return Get_dir_from(npc, pos);
}

Actor* Party_manager::get_actor(int i) const {
	if (i == -1) {
		return gwin->get_main_actor();
	} else if (i < -1 || size_t(i) >= party_count) {
		return nullptr;
	}

	return gwin->get_npc(partyman->party[i]);
}
