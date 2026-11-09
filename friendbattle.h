/*
 *  friendbattle.h - Battle orders and enemy tactics.
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

#ifndef FRIENDBATTLE_H
#define FRIENDBATTLE_H

// See docs/friend-mode/battle-orders-plan.md.

#include <string>

class Actor;
class Game_object;
class Tile_coord;

// The Combat System page (game menu; 2026-11-09), all on by default.  The
// host's rules: battle orders, enemy tactics (enemies fight by role and can
// lose their nerve), positional damage (Combat::positional_damage) and
// friendly fire (Combat::friendly_fire, off: party blasts spare the party).
// Each machine's own: walking pace in combat.
extern bool Friend_battle_orders;
extern bool Friend_enemy_tactics;
extern bool Friend_combat_walk_pace;

void Friend_battle_load_settings();
void Friend_battle_save_settings();
void Friend_combat_options();    // Show the options page.

// The Avatar's speed in combat (Mouse's factors): walking pace, or U7's
// slower step with the Combat System's walking pace off.
int Friend_combat_speed_factor();

// Each frame, from the main loop: opens a requested menu, ends orders that
// are over, and watches enemy morale.
void Friend_battle_frame();

// Orders can be given now: in combat, on the host or in single player, with
// companions.
bool Friend_orders_available();
// Ctrl+C, the button, LT + Y: the menu opens on the next frame.
void Friend_orders_request();
// The menu or its crosshair is up: guests are told why the game stopped.
// For the button's hint: "Fight freely", "Attack Troll"...
std::string Friend_orders_describe();

// What Combat_schedule::now_what does next, asked each step.
enum Friend_battle_steer_result {
	friend_steer_none,          // Carry on.
	friend_steer_busy,          // A walk or a wait was started: nothing more now.
	friend_steer_new_target,    // The target changed: back to approaching.
};
// 'has_spellbook': it's fighting from a readied spellbook.
Friend_battle_steer_result Friend_battle_steer(Actor* npc, bool has_spellbook);

// No path to its foe: true when it should wait and try again, not wander.
bool Friend_battle_no_path(Actor* npc);

// Positional damage (a melee blow, with the option on): where 'attacker'
// stands from 'target', by the way the target faces.
enum Friend_strike_position {
	friend_strike_front,
	friend_strike_side,
	friend_strike_back,
};
Friend_strike_position Friend_battle_position(const Actor* attacker, Game_object* target);
int                    Friend_battle_hit_bonus(Friend_strike_position position);
// A blow from behind on someone who isn't fighting (or is asleep or
// paralyzed): it always lands (2026-11-09).
bool Friend_battle_sure_back(Friend_strike_position position, Game_object* target);
// Around the blow landing: Actor::apply_damage and the hit numbers ask.
void        Friend_battle_set_strike(Friend_strike_position position);
int         Friend_battle_strike_damage();
const char* Friend_battle_strike_label();
// A blow from the side or back landed: "*flanked*" or "*back stab*" over 'target'.
void        Friend_battle_announce_strike(Actor* target, Friend_strike_position position);

// Breaking morale: 'pos' becomes a spot away from the party.
void Friend_battle_flee_spot(const Actor* npc, Tile_coord& pos);

#endif
