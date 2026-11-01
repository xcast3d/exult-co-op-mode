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
extern int  Friend_class_icon_style;       // Friend_class_icons (parchment).
extern bool Friend_class_ai;               // Companions nobody plays use skills (on).
extern bool Friend_class_require_items;    // Skills need the class item (off).
extern bool Friend_class_auto_foe;         // A skill on one foe with no pick chooses (off: the crosshair).
extern bool Friend_class_shared_exp;       // A kill's experience shared by the party near it (on).

// Host and single player: 'expval', the experience for 'killer' defeating
// 'victim' (Actor::attacked).  With shared experience, a kill by the party
// (or a creature one of them summoned) goes to everyone in the party
// near it, divided, with a small bonus per member; otherwise, and for
// anyone else's kills, all to the killer as in Exult.
void Friend_class_give_experience(Actor* killer, Actor* victim, int expval);

enum Friend_class_bar_place {
	friend_bar_above_status = 0,
	friend_bar_bottom_left,
	friend_bar_bottom_right,
	friend_bar_top_left,
	friend_bar_top_right,
	friend_bar_left_center,     // A column at the left edge, centred.
	friend_bar_right_center,    // A column at the right edge, centred.
	friend_bar_place_count
};

// The skill bar's pictures (data/friend/skills/), each machine's choice.
// Colour and parchment are both kept while they are compared; names show
// only when the file is missing.
enum Friend_class_icons { friend_icons_names = 0, friend_icons_colour, friend_icons_parchment, friend_icons_count };

void Friend_class_load_settings();
void Friend_class_save_settings();    // And tell guests, on the host.
void Friend_class_options();          // Show the options page.

// On, and a game it knows (Black Gate).
bool Friend_class_active();

// The top (game-view y) of the HUD rows along the bottom: the party's faces
// and the skill bar.  Friend Mode's bars and chat log sit above it.
int Friend_hud_bottom_y();

// Every frame, on every machine: the bar, the range circles, and on the
// host (or in single player) the cooldowns and the Class AI.
void Friend_class_frame();

// Ctrl+1 to Ctrl+3: use that slot of the bar shown (0 the first), as a
// click does.  False if there is no bar or no such slot.
bool Friend_class_use_slot(int index);
// Controller support (the skills ring): the bar's slot 'index' as the bar
// shows it: its name, seconds left recharging (0 when ready), whether it
// can be used now.  False if there is no bar or no such slot.
bool Friend_class_slot_info(int index, std::string& name, int& seconds, bool& usable);
class Image_buffer8;
// Its picture (a skill's icon, a spell's), centred on (cx, cy) in 'buf',
// dimmed while it can't be used.  False with no picture (names chosen, an
// empty slot).
bool Friend_class_paint_slot(int index, Image_buffer8* buf, int cx, int cy);
// Controller support (the spellbook on the pad): cast 'spell' from the
// Avatar's book as the spell bar does, aimed at the Target System's pick
// (no crosshair while there is one).  Works with the Class System off too.
void Friend_class_cast_spell(int spell);
// The Avatar's spell bar: slot 'index's spell (-1 empty), and putting one
// there (saved; "Fireball on slot 2").  The host's and single player's.
int  Friend_class_avatar_slot(int index);
void Friend_class_set_avatar_slot(int index, int spell);
// A spell's name ("Empty" for -1), and its picture centred on (cx, cy).
std::string Friend_class_spell_name(int spell);
void        Friend_class_paint_spell(int spell, Image_buffer8* buf, int cx, int cy);

class Gump_button;
// The Avatar's spell bar, dragging (drag.cc): the spell a drag from this
// button carries (a spellbook's spell, or a slot of the bar), else -1; and
// its drop at game point (x, y): onto a slot puts it there, slot to slot
// swaps, a slot off the bar clears it.
int  Friend_class_drag_spell(Gump_button* button);
void Friend_class_drop_spell(Gump_button* button, int spell, int x, int y);

// A new world (load, a guest's download): cooldowns and bonuses start over.
void Friend_class_world_loaded();

// Hide the status icons and range circles now (a full-screen menu, such as
// the companion picker, is about to show; the next frame puts them back).
void Friend_class_hide_overlays();
// Around a save: short sleeps and stuns are lifted for the save (and put
// back after), and summoned creatures are dismissed, so nothing of ours
// outlasts its time in a saved game.
void Friend_class_saving(bool before);

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
// Combat_schedule::attack_target: this strike always hits (a Riposte's answer).
bool Friend_class_sure_hit(const Game_object* attacker);
// Combat_schedule::attack_target: a skill's shot may reach this far, past
// the weapon's own range (0 when no skill is shooting).
int Friend_class_shot_reach();
// Combat_schedule::now_what: confused by Dirty Trick, so not attacking.
bool Friend_class_calmed(const Actor* npc);
// Actor::apply_damage: whoever shields 'target' from this melee blow (Shield
// Ally) takes it instead, or null.
Actor* Friend_class_guardian(const Actor* target, Game_object* attacker);

// What friendclass.cc needs from exult.cc.
Friend_mode_session& Friend_mode_get_session();
void                 Friend_mode_class_notice(const std::string& text);
// The notice near the top with a second line under it, as the Space hint.
void Friend_mode_class_hint(const std::string& text, const std::string& hint);
// Hostile: evil or chaotic, outside the party, alive, someone 'own' may fight.
bool Friend_mode_is_hostile(const Actor* own, const Actor* foe);
// A guest may fight it: a creature (animals, monsters), or a hostile NPC;
// townsfolk too with 'peaceful_ok' (after the guest confirmed).
bool Friend_mode_guest_may_attack(const Actor* own, const Actor* foe, bool peaceful_ok = false);
// Host: the guest playing this companion now (not following), or -1.
int Friend_mode_player_playing(const Actor* actor);
// Guest: its companion is in follow mode (the host's AI drives it).
bool Friend_mode_guest_following();
// Network identities for actors: (0, NPC number) or (1, creature number).
bool   Friend_mode_actor_net_id(const Actor* actor, int& kind, uint32& id);
Actor* Friend_mode_actor_by_net_id(int kind, uint32 id);

// Status badges for the status bar's faces (Face_stats.cc): the skill
// marks on 'actor' this frame (bits: 1 up, 2 shield, 4 down), and one
// painted 7x7 at (x, y) in the current render target.
constexpr int friend_status_mark_count = 3;
constexpr int friend_status_mark_size  = 7;
int  Friend_class_status_marks(const Actor* actor);
void Friend_class_paint_status_mark(int mark, int x, int y);

#endif
