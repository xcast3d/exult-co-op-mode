/*
 *  friendbattle.cc - Battle orders and enemy tactics.
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

// Orders: the person playing the Avatar can stop a fight for a moment and
// tell the party what to do.  Tactics: enemies fight by what they carry, and
// can lose their nerve.  All on the host, or in single player; see
// docs/friend-mode/battle-orders-plan.md.

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendbattle.h"

#include "Configuration.h"
#include "Gump_manager.h"
#include "actors.h"
#include "chunks.h"
#include "combat_opts.h"
#include "controller.h"
#include "effects.h"
#include "exult.h"
#include "friendclass.h"
#include "gamewin.h"
#include "monsters.h"
#include "monstinf.h"
#include "party.h"
#include "paths.h"
#include "schedule.h"
#include "shapeinf.h"
#include "tqueue.h"
#include "ucmachine.h"
#include "weaponinf.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

bool Friend_enemy_tactics = true;

// From exult.cc, where the party's Hold and the notices live.
bool         Friend_mode_party_holding();
void         Friend_mode_set_party_hold(bool hold);
void         Friend_mode_battle_notice(const string& text);
Game_object* Friend_mode_aim_for_orders(const string& prompt);

namespace {

Game_window* Gwin() {
	return Game_window::get_instance();
}

// The game's clock for all this: stops while the game is paused.
uint32 battle_clock = 0;
uint32 last_ticks   = 0;

void Tick_clock(bool paused) {
	const uint32 ticks = SDL_GetTicks();
	const uint32 gap   = ticks - last_ticks;
	last_ticks         = ticks;
	if (!paused && gap <= 250) {
		battle_clock += gap;
	}
}

bool Is_party(const Actor* actor) {
	return actor && (actor->is_in_party() || actor == Gwin()->get_main_actor());
}

/*
 *  Orders.
 */

enum class Order {
	freely,
	attack,
	protect,
	fall_back
};

constexpr int protect_foes_tiles = 5;     // Hostiles this near the ward count.
constexpr int protect_stay_tiles = 3;     // With none, stay this near.
constexpr int protect_look_tiles = 16;    // How far to look for its attackers.
constexpr int fall_back_tiles    = 3;     // This near the leader is back.
constexpr int order_lost_tiles   = 40;    // A target this far off is gone.
constexpr int idle_ms            = 400;   // Waiting, between looks.
constexpr int recall_look_tiles  = 12;    // A companion that gave up looks this far.

Order            current_order = Order::freely;
Game_object_weak order_target;    // The foe, or the one to protect.
bool             menu_wanted = false;

Actor* Order_target() {
	const Game_object_shared obj = order_target.lock();
	return obj ? obj->as_actor() : nullptr;
}

void Clear_order() {
	current_order = Order::freely;
	order_target  = Game_object_weak();
}

// A companion nobody plays.
bool Is_free_companion(const Actor* npc) {
	Game_window* gwin = Gwin();
	return npc && npc->is_in_party() && npc != gwin->get_main_actor() && npc != gwin->get_controlled_actor()
		   && !npc->is_dead() && Friend_mode_player_playing(npc) < 0 && npc->get_effective_alignment() == Actor::good;
}

// And fighting.  Not one held (its schedule is 'wait').
bool Obeys(const Actor* npc) {
	return Is_free_companion(npc) && npc->get_schedule_type() == Schedule::combat;
}

// With no hostile about, or no path, a companion gives up and goes back to
// following (Combat_schedule::now_what) though combat is still on: an order to
// attack or protect someone brings it back.
void Recall_companions() {
	Actor*    party[9];
	const int count = Gwin()->get_party(party, 1);
	for (int i = 0; i < count; ++i) {
		if (Is_free_companion(party[i]) && party[i]->get_schedule_type() == Schedule::follow_avatar) {
			party[i]->set_schedule_type(Schedule::combat);
		}
	}
}

// Companions change at once, not when their current walk ends.
void Redirect_party(Actor* target) {
	Actor*    party[9];
	const int count = Gwin()->get_party(party, 1);
	for (int i = 0; i < count; ++i) {
		if (Obeys(party[i])) {
			party[i]->set_target(target);
			party[i]->stop();
			party[i]->start_std();
		}
	}
}

// Other orders take over from Hold (held companions aren't fighting).
void Release_hold() {
	if (Friend_mode_party_holding()) {
		Friend_mode_set_party_hold(false);
	}
}

void Give_order(Order order, Actor* target) {
	Release_hold();
	current_order = order;
	order_target  = target ? weak_from_obj(target) : Game_object_weak();
	if (target) {
		Recall_companions();
	}
	Redirect_party(order == Order::attack ? target : nullptr);
}

void Order_attack(Game_object* obj) {
	Actor*       foe  = obj ? obj->as_actor() : nullptr;
	Actor*       self = Gwin()->get_main_actor();
	if (!obj) {
		return;    // Escape.
	}
	// Anyone alive outside the party, peaceful or not: it's the Avatar's call.
	if (!foe || !Friend_mode_is_attackable(self, foe)) {
		Friend_mode_battle_notice("That isn't a foe");
		return;
	}
	Give_order(Order::attack, foe);
	Friend_mode_battle_notice("Attack " + foe->get_npc_name());
}

void Order_protect(Game_object* obj) {
	Actor*       ward = obj ? obj->as_actor() : nullptr;
	if (!obj) {
		return;    // Escape.
	}
	if (!ward || ward->is_dead() || !Is_party(ward)) {
		Friend_mode_battle_notice("That isn't one of the party");
		return;
	}
	Give_order(Order::protect, ward);
	Friend_mode_battle_notice("Protect " + ward->get_npc_name());
}

enum Menu_choice {
	choose_freely,
	choose_attack,
	choose_protect,
	choose_fall_back,
	choose_hold
};

vector<string> Menu_labels() {
	const bool holding = Friend_mode_party_holding();
	// The star: the order in force.
	const auto entry = [holding](const char* name, Order order) {
		return string(!holding && current_order == order ? "* " : "") + name;
	};
	return {entry("Fight freely", Order::freely), entry("Attack target", Order::attack), entry("Protect", Order::protect),
			entry("Fall back", Order::fall_back), holding ? "* Follow" : "Hold"};
}

// The orders that need no aiming.
void Apply_choice(int choice) {
	switch (choice) {
	case choose_freely:
		Give_order(Order::freely, nullptr);
		break;
	case choose_fall_back:
		Give_order(Order::fall_back, nullptr);
		Friend_mode_battle_notice("Fall back");
		break;
	case choose_hold: {
		const bool holding = Friend_mode_party_holding();
		Clear_order();
		Friend_mode_set_party_hold(!holding);
		break;
	}
	default:
		break;    // Escape.
	}
}

void Open_menu() {
	const int choice = Controller_choose("Orders", Menu_labels());
	if (choice == choose_attack) {
		Order_attack(Friend_mode_aim_for_orders("Attack who?"));
	} else if (choice == choose_protect) {
		Order_protect(Friend_mode_aim_for_orders("Protect who?"));
	} else {
		Apply_choice(choice);
	}
}

// Its orders end, with a line when the game ended them.
void Check_order() {
	if (current_order == Order::freely) {
		return;
	}
	Game_window* gwin = Gwin();
	if (!gwin->in_combat()) {
		Clear_order();
		return;
	}
	if (current_order == Order::fall_back) {
		return;
	}
	Actor*       target = Order_target();
	const Actor* leader = gwin->get_party_leader();
	if (!target || target->is_dead() || !target->get_chunk() || !leader || target->get_map() != leader->get_map()
		|| target->distance(leader) > order_lost_tiles) {
		const bool attacking = current_order == Order::attack;
		Clear_order();
		Friend_mode_battle_notice(attacking ? "Target gone: fighting freely" : "Fighting freely");
		return;
	}
	static uint32 next_recall = 0;
	const uint32  now         = SDL_GetTicks();
	if (now >= next_recall) {    // One may give up again, on a target it can't reach.
		next_recall = now + 1000;
		Recall_companions();
	}
}

// Walk to within 'within' tiles of 'dest'.
bool Walk_near(Actor* npc, const Tile_coord& dest, int within) {
	const Tile_coord spot = Map_chunk::find_spot(dest, 2, npc);
	if (spot.tx < 0 || spot == npc->get_tile()) {
		return false;
	}
	return npc->walk_path_to_tile(spot, Gwin()->get_std_delay(), 0, within) != 0;
}

// The nearest hostile to 'npc' around 'spot', with 'radius'.
Actor* Nearest_hostile(const Actor* npc, const Tile_coord& spot, int radius) {
	Actor_vector near;
	Game_object::find_nearby_actors(near, spot, c_any_shapenum, radius);
	Actor* best      = nullptr;
	int    best_dist = INT_MAX;
	for (Actor* each : near) {
		if (each != npc && Friend_mode_is_hostile(npc, each)) {
			const int dist = npc->distance(each);
			if (dist < best_dist) {
				best_dist = dist;
				best      = each;
			}
		}
	}
	return best;
}

// Combat is still on, and a companion that gave up (Recall_companions) has a
// hostile near and in the open: back to the fight.  Otherwise it only comes back
// when someone is hit, and a foe that isn't fighting leaves everyone standing.
void Recall_for_near_foes() {
	Actor*    party[9];
	const int count = Gwin()->get_party(party, 1);
	for (int i = 0; i < count; ++i) {
		Actor* npc = party[i];
		if (!Is_free_companion(npc) || npc->get_schedule_type() != Schedule::follow_avatar) {
			continue;
		}
		Actor* foe = Nearest_hostile(npc, npc->get_tile(), recall_look_tiles);
		if (foe && Fast_pathfinder_client::is_straight_path(npc, foe)) {
			std::cout << "Friend Mode: " << npc->get_npc_name() << " back in the fight, for " << foe->get_npc_name() << std::endl;
			npc->set_schedule_type(Schedule::combat);
		}
	}
}

// How far its readied weapon reaches.
int Weapon_reach(Actor* npc) {
	int                points = 0;
	int                weapon = -1;
	Game_object*       obj    = nullptr;
	const Weapon_info* winf   = npc->get_weapon(points, weapon, obj);
	return winf ? std::max(1, winf->get_range()) : 1;
}

Friend_battle_steer_result Set_target(Actor* npc, Actor* foe) {
	if (npc->get_target() == foe) {
		return friend_steer_none;
	}
	npc->set_target(foe);
	return friend_steer_new_target;
}

Friend_battle_steer_result Wait_a_moment(Actor* npc) {
	npc->set_target(nullptr);
	npc->start(200, idle_ms);
	return friend_steer_busy;
}

Friend_battle_steer_result Steer_protect(Actor* npc, Actor* ward) {
	if (npc == ward) {
		return friend_steer_none;    // The one protected fights as usual.
	}
	// Whoever is on the ward, keeping the one it's already fighting.
	Actor_vector near;
	Game_object::find_nearby_actors(near, ward->get_tile(), c_any_shapenum, protect_look_tiles);
	const Game_object* current = npc->get_target();
	Actor*             best    = nullptr;
	int                closest = INT_MAX;
	for (Actor* each : near) {
		if (each->get_target() == ward && Friend_mode_is_hostile(npc, each)) {
			if (each == current) {
				return friend_steer_none;
			}
			const int dist = npc->distance(each);
			if (dist < closest) {
				closest = dist;
				best    = each;
			}
		}
	}
	if (!best) {
		best = Nearest_hostile(npc, ward->get_tile(), protect_foes_tiles);
	}
	if (best) {
		return Set_target(npc, best);
	}
	if (npc->distance(ward) > protect_stay_tiles) {
		npc->set_target(nullptr);
		if (Walk_near(npc, ward->get_tile(), protect_stay_tiles - 1)) {
			return friend_steer_busy;
		}
	}
	return Wait_a_moment(npc);
}

Friend_battle_steer_result Steer_fall_back(Actor* npc) {
	const Actor* leader = Gwin()->get_party_leader();
	if (!leader || leader == npc || leader->get_map() != npc->get_map()) {
		return friend_steer_none;
	}
	if (npc->distance(leader) > fall_back_tiles) {
		npc->set_target(nullptr);
		if (Walk_near(npc, leader->get_tile(), fall_back_tiles - 1)) {
			return friend_steer_busy;
		}
		return Wait_a_moment(npc);
	}
	// Back: strike only what comes within reach.
	if (Actor* foe = Nearest_hostile(npc, npc->get_tile(), Weapon_reach(npc))) {
		return Set_target(npc, foe);
	}
	return Wait_a_moment(npc);
}

Friend_battle_steer_result Steer_by_order(Actor* npc) {
	switch (current_order) {
	case Order::attack: {
		Actor* foe = Order_target();
		return foe && !foe->is_dead() ? Set_target(npc, foe) : friend_steer_none;
	}
	case Order::protect: {
		Actor* ward = Order_target();
		return ward && !ward->is_dead() ? Steer_protect(npc, ward) : friend_steer_none;
	}
	case Order::fall_back:
		return Steer_fall_back(npc);
	default:
		return friend_steer_none;
	}
}

/*
 *  Positional damage.
 */

constexpr int side_hit_bonus    = 3;    // On the d30 hit roll: 10%.
constexpr int back_hit_bonus    = 6;    // 20%.
constexpr int back_extra_damage = 2;    // Before armour.

constexpr uint32 announce_ms = 2000;    // Between lines over one target.

Friend_strike_position strike_position = friend_strike_front;
// Only compared, never followed.
std::map<const Actor*, uint32> next_announce;

/*
 *  Enemy tactics.
 */

constexpr int    keep_away_tiles   = 2;       // A foe this near: a shooter steps away.
constexpr int    step_away_tiles   = 4;       // How far.
constexpr int    look_tiles        = 24;      // Foes further than this aren't considered.
constexpr int    crowded_penalty   = 4;       // Each ally on a foe counts as this many tiles.
constexpr int    max_crowd_counted = 3;
constexpr int    ranged_reach      = 3;       // More than this is a ranged weapon.
constexpr uint32 decide_ms         = 2000;    // Between reviews of its choice.
constexpr uint32 step_away_ms      = 2500;    // Between steps away, to shoot meanwhile.
constexpr uint32 morale_ms         = 500;     // Between looks at a group's morale.
constexpr int    min_group         = 3;       // A smaller group has no morale to lose.
constexpr int    leader_break      = 50;      // Percent, when the leader falls.
constexpr int    rout_break        = 35;      // Percent, when half have gone.
constexpr uint32 flee_min_ms       = 6000;
constexpr uint32 flee_spread_ms    = 4000;
constexpr int    flee_tiles        = 12;

// Only compared, never followed: a fighter that's gone just leaves its entry.
std::map<const Actor*, uint32> next_decide;
std::map<const Actor*, uint32> next_step;

// A hostile the tactics apply to.  Duels and tournaments are left alone.
bool Is_tactician(const Actor* npc) {
	return Friend_enemy_tactics && npc && !Friend_mode_is_guest() && !Is_party(npc) && !npc->is_dead()
		   && npc->get_effective_alignment() >= Actor::evil && npc->get_schedule_type() == Schedule::combat
		   && !npc->get_flag(Obj_flags::tournament);
}

bool Can_walk(const Actor* npc) {
	return (npc->get_type_flags() & MOVE_ALL) != 0 && !npc->get_info().has_strange_movement();
}

// The party members it could fight.
vector<Actor*> Party_foes(Actor* npc) {
	Actor*         party[9];
	const int      count         = Gwin()->get_party(party, 1);
	const bool     see_invisible = npc->can_see_invisible();
	vector<Actor*> found;
	for (int i = 0; i < count; ++i) {
		Actor* each = party[i];
		if (each->is_dead() || !each->get_chunk() || each->get_map() != npc->get_map() || npc->distance(each) > look_tiles
			|| each->get_effective_alignment() != Actor::good || each->get_flag(Obj_flags::asleep)
			|| (!see_invisible && each->get_flag(Obj_flags::invisible))) {
			continue;
		}
		found.push_back(each);
	}
	return found;
}

// How many other tacticians are on 'foe'.
int Crowd_on(const Actor* npc, const Actor* foe, const vector<Actor*>& nearby) {
	int crowd = 0;
	for (const Actor* each : nearby) {
		if (each != npc && each->get_target() == foe && Is_tactician(each) && ++crowd >= max_crowd_counted) {
			break;
		}
	}
	return crowd;
}

Actor* Pick_melee(Actor* npc, const vector<Actor*>& foes) {
	vector<Actor*> nearby;
	Gwin()->get_nearby_npcs(nearby);
	const Game_object* current    = npc->get_target();
	Actor*             best       = nullptr;
	int                best_score = INT_MAX;
	for (Actor* foe : foes) {
		int score = npc->distance(foe) + crowded_penalty * Crowd_on(npc, foe, nearby);
		if (foe == current) {
			score -= crowded_penalty;    // No flicking between two near the same.
		}
		if (score < best_score) {
			best_score = score;
			best       = foe;
		}
	}
	return best;
}

Actor* Pick_ranged(Actor* npc, const vector<Actor*>& foes) {
	Actor* best      = nullptr;
	int    best_dist = INT_MAX;
	for (Actor* foe : foes) {
		const int dist = npc->distance(foe);
		if (dist < best_dist) {
			best_dist = dist;
			best      = foe;
		}
	}
	return best;
}

Actor* Pick_caster(const vector<Actor*>& foes) {
	Actor* best = nullptr;
	for (Actor* foe : foes) {
		if (!best || foe->get_property(Actor::health) < best->get_property(Actor::health)) {
			best = foe;
		}
	}
	return best;
}

bool Step_away(Actor* npc, const Actor* foe) {
	const Tile_coord at   = npc->get_tile();
	const Tile_coord from = foe->get_tile();
	int              dx   = at.tx - from.tx;
	int              dy   = at.ty - from.ty;
	if (dx == 0 && dy == 0) {
		dx = 1;
	}
	const int        len = std::max(std::abs(dx), std::abs(dy));
	const Tile_coord dest(at.tx + dx * step_away_tiles / len, at.ty + dy * step_away_tiles / len, at.tz);
	return Walk_near(npc, dest, 0);
}

Friend_battle_steer_result Steer_by_role(Actor* npc, bool has_spellbook) {
	if (npc->get_attack_mode() == Actor::berserk) {
		return friend_steer_none;
	}
	const vector<Actor*> foes = Party_foes(npc);
	if (foes.empty()) {
		return friend_steer_none;    // U7's own choice stands.
	}
	const bool ranged = has_spellbook || Weapon_reach(npc) > ranged_reach;
	// A shooter with a foe at its throat moves first.
	if (ranged && Can_walk(npc) && battle_clock >= next_step[npc]) {
		if (const Actor* close = Pick_ranged(npc, foes);
			close && npc->distance(close) <= keep_away_tiles && Step_away(npc, close)) {
			next_step[npc] = battle_clock + step_away_ms;
			return friend_steer_busy;
		}
	}
	const Game_object* current = npc->get_target();
	if (current && battle_clock < next_decide[npc]) {
		return friend_steer_none;
	}
	next_decide[npc] = battle_clock + decide_ms;
	// Beside its foe, a melee fighter stays on it.
	if (!ranged && current && npc->distance(current) <= 2) {
		const Actor* foe = current->as_actor();
		if (foe && !foe->is_dead()) {
			return friend_steer_none;
		}
	}
	Actor* pick = has_spellbook ? Pick_caster(foes) : ranged ? Pick_ranged(npc, foes) : Pick_melee(npc, foes);
	return pick ? Set_target(npc, pick) : friend_steer_none;
}

/*
 *  Morale: a group that has lost its leader, or half its number, may break.
 */

struct Runner {
	Game_object_weak   who;
	Actor::Attack_mode mode;
	uint32             until;
};

vector<Runner>   runners;
Game_object_weak group_leader;
bool             group_had_leader = false;
bool             group_lost_leader = false;
bool             group_routed     = false;
int              group_peak       = 0;
uint32           next_morale      = 0;

int Fighting_strength(Actor* npc) {
	return npc->get_property(Actor::strength) + npc->get_property(Actor::combat);
}

bool Can_break(const Actor* npc) {
	const Monster_info* info = npc->get_info().get_monster_info();
	return (!info || !info->cant_die()) && npc->is_sentient() && Can_walk(npc) && npc->get_attack_mode() != Actor::berserk
		   && npc->get_attack_mode() != Actor::flee && !npc->get_flag(Obj_flags::asleep);
}

void Break_group(const vector<Actor*>& group, int percent) {
	for (Actor* npc : group) {
		if (Can_break(npc) && std::rand() % 100 < percent) {
			runners.push_back({weak_from_obj(npc), npc->get_attack_mode(),
							   battle_clock + flee_min_ms + static_cast<uint32>(std::rand()) % flee_spread_ms});
			npc->set_attack_mode(Actor::flee);
			npc->set_target(nullptr);
			npc->stop();
			npc->start_std();
		}
	}
}

// Those who ran come back to fight (also with tactics turned off meanwhile).
void Return_runners() {
	runners.erase(std::remove_if(runners.begin(), runners.end(),
						  [](const Runner& runner) {
							  const Game_object_shared obj = runner.who.lock();
							  Actor*                   npc = obj ? obj->as_actor() : nullptr;
							  if (!npc || npc->is_dead()) {
								  return true;
							  }
							  if (battle_clock < runner.until) {
								  return false;
							  }
							  if (npc->get_attack_mode() == Actor::flee) {
								  npc->set_attack_mode(runner.mode);
							  }
							  return true;
						  }),
				  runners.end());
}

void Reset_group() {
	group_leader     = Game_object_weak();
	group_had_leader = false;
	group_lost_leader = false;
	group_routed     = false;
	group_peak       = 0;
	next_decide.clear();
	next_step.clear();
}

void Watch_morale() {
	if (battle_clock < next_morale) {
		return;
	}
	next_morale = battle_clock + morale_ms;
	vector<Actor*> nearby;
	Gwin()->get_nearby_npcs(nearby);
	vector<Actor*> group;
	for (Actor* npc : nearby) {
		if (Is_tactician(npc)) {
			group.push_back(npc);
		}
	}
	if (group.empty()) {
		Reset_group();
		return;
	}
	group_peak = std::max(group_peak, static_cast<int>(group.size()));
	if (group_peak < min_group) {
		return;
	}
	// The leader fell.
	if (group_had_leader) {
		const Game_object_shared obj    = group_leader.lock();
		const Actor*             leader = obj ? obj->as_actor() : nullptr;
		if (!leader || leader->is_dead()) {
			group_leader     = Game_object_weak();
			group_had_leader = false;
			group_lost_leader = true;    // Once: a group breaks over it one time.
			Break_group(group, leader_break);
			return;
		}
	} else if (!group_lost_leader && group.size() >= 2) {
		// Clearly the strongest, while it stands: one leader to each fight.
		Actor* first  = nullptr;
		int    best   = 0;
		int    second = 0;
		for (Actor* npc : group) {
			const int strength = Fighting_strength(npc);
			if (strength > best) {
				second = best;
				best   = strength;
				first  = npc;
			} else if (strength > second) {
				second = strength;
			}
		}
		if (first && best * 4 >= second * 5) {
			group_leader     = weak_from_obj(first);
			group_had_leader = true;
		}
	}
	// Half have gone.
	if (!group_routed && static_cast<int>(group.size()) * 2 <= group_peak) {
		group_routed = true;
		Break_group(group, rout_break);
	}
}

}    // namespace

Friend_strike_position Friend_battle_position(const Actor* attacker, Game_object* target) {
	const Actor* foe = target ? target->as_actor() : nullptr;
	if (!Combat::positional_damage || !attacker || !foe || foe == attacker || foe->is_dead()
		|| foe->get_info().has_strange_movement()) {    // Slimes and the like have no back.
		return friend_strike_front;
	}
	// 0 straight ahead, in eighths of a turn; facing is one of the four main ways.
	const int turn = (target->get_direction(const_cast<Actor*>(attacker)) - foe->get_dir_facing() + 8) % 8;
	const int away = std::min(turn, 8 - turn);
	return away <= 1 ? friend_strike_front : away == 2 ? friend_strike_side : friend_strike_back;
}

int Friend_battle_hit_bonus(Friend_strike_position position) {
	return position == friend_strike_back ? back_hit_bonus : position == friend_strike_side ? side_hit_bonus : 0;
}

void Friend_battle_set_strike(Friend_strike_position position) {
	strike_position = position;
}

int Friend_battle_strike_damage() {
	return strike_position == friend_strike_back ? back_extra_damage : 0;
}

const char* Friend_battle_strike_label() {
	return strike_position == friend_strike_back ? " back stab" : strike_position == friend_strike_side ? " flanked" : "";
}

void Friend_battle_announce_strike(Actor* target, Friend_strike_position position) {
	if (!target || position == friend_strike_front) {
		return;
	}
	if (next_announce.size() > 64) {
		next_announce.clear();    // Those long gone.
	}
	uint32& next = next_announce[target];
	if (battle_clock < next) {
		return;
	}
	next = battle_clock + announce_ms;
	const char* text = position == friend_strike_back ? "*back stab*" : "*flanked*";
	// With hit numbers on, the number here already says so (Actor::attacked).
	if (!Combat::show_hits) {
		Gwin()->get_effects()->add_text(text, target);
	}
	Friend_mode_note_speech(target, text);    // Guests see it too.
}

void Friend_battle_load_settings() {
	config->value("config/friend/enemy_tactics", Friend_enemy_tactics, true);
}

void Friend_battle_save_settings() {
	if (!Friend_mode_is_guest()) {
		config->set("config/friend/enemy_tactics", Friend_enemy_tactics ? "yes" : "no", false);
	}
}

void Friend_battle_frame() {
	Game_window* gwin = Gwin();
	if (Friend_mode_is_guest() || !gwin->get_main_actor()) {
		menu_wanted = false;
		return;
	}
	const bool paused = gwin->get_tqueue()->is_paused();
	Tick_clock(paused);
	if (std::exchange(menu_wanted, false) && (!paused || Combat::is_paused()) && Friend_orders_available()
		&& !gwin->get_gump_man()->modal_gump_mode() && !gwin->get_usecode()->in_usecode()) {
		Open_menu();
		return;
	}
	if (paused) {
		return;
	}
	Check_order();
	static uint32 next_foe_look = 0;
	if (const uint32 now = SDL_GetTicks();
		now >= next_foe_look && gwin->in_combat() && !gwin->get_usecode()->in_usecode()) {
		next_foe_look = now + 1000;
		Recall_for_near_foes();
	}
	Return_runners();
	if (Friend_enemy_tactics) {
		Watch_morale();
	}
}

bool Friend_orders_available() {
	Game_window* gwin = Gwin();
	return !Friend_mode_is_guest() && gwin->in_combat() && gwin->get_party_man()->get_count() > 0;
}

void Friend_orders_request() {
	menu_wanted = true;
}

string Friend_orders_describe() {
	if (Friend_mode_party_holding()) {
		return "The party holds";
	}
	const Actor* target = Order_target();
	switch (current_order) {
	case Order::attack:
		return target ? "Attack " + target->get_npc_name() : "Attack";
	case Order::protect:
		return target ? "Protect " + target->get_npc_name() : "Protect";
	case Order::fall_back:
		return "Fall back";
	default:
		return "Fight freely";
	}
}

Friend_battle_steer_result Friend_battle_steer(Actor* npc, bool has_spellbook) {
	if (current_order != Order::freely && Obeys(npc)) {
		return Steer_by_order(npc);
	}
	if (Is_tactician(npc)) {
		return Steer_by_role(npc, has_spellbook);
	}
	return friend_steer_none;
}

bool Friend_battle_no_path(Actor* npc) {
	return Is_tactician(npc) || (current_order != Order::freely && Obeys(npc));
}

void Friend_battle_flee_spot(const Actor* npc, Tile_coord& pos) {
	const bool ran = std::any_of(runners.begin(), runners.end(), [npc](const Runner& runner) {
		return runner.who.lock().get() == npc;
	});
	if (!ran) {
		return;    // U7's own running away.
	}
	Actor*       party[9];
	const int    count   = Gwin()->get_party(party, 1);
	const Actor* nearest = nullptr;
	int          closest = INT_MAX;
	for (int i = 0; i < count; ++i) {
		if (!party[i]->is_dead() && party[i]->get_map() == npc->get_map() && npc->distance(party[i]) < closest) {
			closest = npc->distance(party[i]);
			nearest = party[i];
		}
	}
	if (!nearest) {
		return;
	}
	const Tile_coord at   = npc->get_tile();
	const Tile_coord from = nearest->get_tile();
	int              dx   = at.tx - from.tx;
	int              dy   = at.ty - from.ty;
	if (dx == 0 && dy == 0) {
		dx = 1;
	}
	const int len = std::max(std::abs(dx), std::abs(dy));
	pos.tx        = at.tx + dx * flee_tiles / len;
	pos.ty        = at.ty + dy * flee_tiles / len;
}
