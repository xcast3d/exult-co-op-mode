/*
 * Friend Mode's Class System: a few skills for each companion, drawn from
 * the class they had in Ultima IV (Iolo's songs, Jaana's heals, Shamino's
 * volley), a skill bar, a spell bar for the Avatar, skill ranges, and a
 * "Class AI" for companions nobody plays.  See
 * docs/friend-mode/companion-skills.md.
 *
 * It is kept in friendclass.cc, apart from Exult: the rest of the game only
 * calls the hooks below.  The host carries every skill out; a guest asks.
 */
#ifndef FRIENDCLASS_H
#define FRIENDCLASS_H

#include "friendnet.h"

#include <string>
#include <vector>

class Actor;
class Game_object;

// The Class System options page (Game menu -> Class System).
extern bool Friend_class_enabled;          // The host's game rule (off).
extern int  Friend_class_bar_position;     // Friend_class_bar_place (above the status bar).
extern bool Friend_class_show_ranges;      // Circles for shown ranges (on).
extern bool Friend_class_ai;               // Companions nobody plays use skills (on).
extern bool Friend_class_require_items;    // Skills need the class item (off).

enum Friend_class_bar_place {
	friend_bar_above_status = 0,
	friend_bar_bottom_left,
	friend_bar_bottom_right,
	friend_bar_top_left,
	friend_bar_top_right,
	friend_bar_place_count
};

void Friend_class_load_settings();
void Friend_class_save_settings();    // And tell guests, on the host.
void Friend_class_options();          // Show the options page.

// On, and a game it knows (Black Gate).
bool Friend_class_active();

// Every frame, on every machine: the bar, the range circles, and on the
// host (or in single player) the cooldowns and the Class AI.
void Friend_class_frame();

// A new world (load, a guest's download): cooldowns and bonuses start over.
void Friend_class_world_loaded();

// Messages.  True if the blob was the Class System's.
bool Friend_class_host_blob(Friend_mode_session::Blob_kind kind, const std::vector<unsigned char>& data, int player);
bool Friend_class_guest_blob(Friend_mode_session::Blob_kind kind, const std::vector<unsigned char>& data);
// Host: a guest joined; send it the rules.
void Friend_class_guest_joined(int player);

// Class AI placing, from the combat schedule before it closes in on a foe:
// a ranged companion keeps its distance, a healer walks into range or falls
// back to the party.  True if it started a walk instead.
bool Friend_class_place(Actor* npc);

// Timed stat bonuses and penalties (Battle Hymn, Crippling Shot), added to
// Actor::get_effective_prop (strength, combat, dexterity).
int Friend_class_bonus(const Actor* actor, int prop);
// Actor::apply_damage: armour boosts (Quick Fix, Sanctuary), armour the
// attacker ignores (Master's Strike), extra damage on a marked foe from the
// party (Hunter's Mark).
void Friend_class_adjust_damage(const Actor* target, Game_object* attacker, int& damage, int& armor);
// A melee strike missed 'target' (Combat_schedule::attack_target): Riposte.
void Friend_class_note_miss(Game_object* attacker, Game_object* target);

// What friendclass.cc needs from exult.cc.
Friend_mode_session& Friend_mode_get_session();
void                 Friend_mode_class_notice(const std::string& text);
// Hostile: evil or chaotic, outside the party, alive, someone 'own' may fight.
bool Friend_mode_is_hostile(const Actor* own, const Actor* foe);
// Host: the guest playing this companion now (not following), or -1.
int Friend_mode_player_playing(const Actor* actor);
// Guest: its companion is in follow mode (the host's AI drives it).
bool Friend_mode_guest_following();

#endif
