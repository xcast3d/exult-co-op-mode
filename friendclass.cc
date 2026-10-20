/*
 * Friend Mode's Class System.  See friendclass.h and
 * docs/friend-mode/companion-skills.md.
 *
 * The pieces, in order:
 *  - the options and their config;
 *  - each companion's class and skills (the definitions);
 *  - the rules: may this skill be used now, and on whom;
 *  - the effects, carried out on the host through Exult's own verbs
 *    (health, flags, real shots through the combat code, sprites, speech),
 *    which already reach guests;
 *  - the Class AI: companions nobody plays use their skills by their
 *    class's rules;
 *  - the skill bar (a HUD gump, scaled like the shortcut bar), the Avatar's
 *    spell bar, and the range circles;
 *  - the messages between host and guests.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "friendclass.h"

#include "Audio.h"
#include "Configuration.h"
#include "Face_stats.h"
#include "Gump.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Modal_gump.h"
#include "Text_button.h"
#include "actors.h"
#include "combat.h"
#include "effects.h"
#include "exult.h"
#include "font.h"
#include "friendmap.h"
#include "game.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "items.h"
#include "mouse.h"
#include "party.h"
#include "shapeid.h"
#include "shapeinf.h"
#include "ammoinf.h"
#include "vgafile.h"
#include "Flex.h"
#include "spellbook.h"
#include "ucmachine.h"
#include "weaponinf.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>

using std::cout;
using std::endl;

bool Friend_class_enabled       = false;
int  Friend_class_bar_position  = friend_bar_above_status;
bool Friend_class_show_ranges   = true;
bool Friend_class_ai            = true;
bool Friend_class_require_items = false;

namespace {

Game_window* Gwin() {
	return Game_window::get_instance();
}

uint32 Now() {
	return static_cast<uint32>(SDL_GetTicks());
}

bool Is_host() {
	return Friend_mode_get_session().mode() == Friend_mode_session::Mode::host;
}

bool Is_guest() {
	return Friend_mode_get_session().mode() == Friend_mode_session::Mode::guest;
}

/*
 *  Definitions.
 */

enum class Skill_type { heal, cure, support, attack, control, self };

// Whom a skill touches.
enum class Skill_target {
	self,              // The user.
	party_area,        // The party within range (the Avatar too).
	hurt_party,        // The most hurt party member within range.
	pick,              // The user's foe (its combat target), or the nearest hostile.
	hostiles_nearest,  // The 'count' nearest hostiles within range.
};

enum class Skill_effect {
	heal,      // Health up by 'amount', capped at strength.
	cure,      // Clears poison, paralysis and curse.
	flag,      // Sets 'flag' (protection, invisible), with U7's own timer.
	hinder,    // Paralyzes or knocks out for 'duration_ms', after a roll.
	volley,    // A real shot with the readied weapon at each target.
	bonus,     // +'amount' strength and combat for 'duration_ms'.
	taunt,     // Hostiles turn on the user (not built yet).
	calm,      // Hostiles stop fighting (not built yet).
};

// Where a class likes to stand in a fight (Class AI; placing is not built
// yet, see the plan).
enum class Placement { ranged, support, front };

constexpr int cooldown_standard = 60;    // Seconds; shortened later if too long.

// How a skill looks and sounds, on top of whatever its effect shows: any of
// U7's sprite effects (sprites.vga), a missile picture (shapes.vga) and a
// sound effect.  -1 for none.
struct Skill_look {
	int on_targets = -1;    // Sprite on each target.
	int on_user    = -1;    // Sprite on the user.
	int missile    = -1;    // Flies from the user to each target (a picture only).
	int sound      = -1;    // Sound effect at the user.
};

// One skill.  Written as a chain of named settings (see Classes()); what
// isn't set keeps the default here.
struct Class_skill {
	int          id;                                 // Unique; used on the wire.
	const char*  name;
	int          icon        = 0;                    // A spellbook picture: the spell's number.
	int          cooldown    = cooldown_standard;    // Seconds.
	Skill_type   type        = Skill_type::self;
	Skill_target target      = Skill_target::self;
	int          range       = 0;        // Tiles: an area's radius, or a target's reach.
	bool         range_shown = false;    // Drawn as a circle, or silent.
	int          level       = 1;        // Character level that unlocks it.
	Skill_effect effect      = Skill_effect::heal;
	int          amount      = 0;     // Health, or a stat bonus.
	int          flag        = -1;    // For 'flag' and 'hinder'.
	int          count       = 1;     // Targets ('hostiles_nearest'), or the AI's minimum.
	int          duration_ms = 0;     // For 'hinder', 'bonus', 'taunt' and 'calm'.
	Skill_look   look;
	const char*  words = "";    // Over the user.

	Class_skill(int i, const char* n) : id(i), name(n) {}

	// What it is and whom it touches.
	Class_skill& icon_spell(int spell_number) {
		icon = spell_number;
		return *this;
	}

	Class_skill& kind(Skill_type t) {
		type = t;
		return *this;
	}

	Class_skill& aim(Skill_target t, int tiles = 0, bool shown = false) {
		target      = t;
		range       = tiles;
		range_shown = shown;
		return *this;
	}

	Class_skill& unlock(int at_level) {
		level = at_level;
		return *this;
	}

	Class_skill& recharge(int seconds) {
		cooldown = seconds;
		return *this;
	}

	// How many targets ('hostiles_nearest'), or how many the Class AI waits
	// for (two hurt for Mending Circle).
	Class_skill& targets(int n) {
		count = n;
		return *this;
	}

	// The effect: one of these.
	Class_skill& heal(int health) {
		effect = Skill_effect::heal;
		amount = health;
		return *this;
	}

	Class_skill& cure() {
		effect = Skill_effect::cure;
		return *this;
	}

	Class_skill& set_flag(int f) {
		effect = Skill_effect::flag;
		flag   = f;
		return *this;
	}

	Class_skill& hinder(int f, int ms) {
		effect      = Skill_effect::hinder;
		flag        = f;
		duration_ms = ms;
		return *this;
	}

	// 'extra' strength and combat on the user while it flies (Aimed Shot).
	Class_skill& volley(int extra = 0, int ms = 0) {
		effect      = Skill_effect::volley;
		amount      = extra;
		duration_ms = ms;
		return *this;
	}

	Class_skill& bonus(int plus, int ms) {
		effect      = Skill_effect::bonus;
		amount      = plus;
		duration_ms = ms;
		return *this;
	}

	Class_skill& taunt(int ms) {
		effect      = Skill_effect::taunt;
		duration_ms = ms;
		return *this;
	}

	Class_skill& calm(int ms) {
		effect      = Skill_effect::calm;
		duration_ms = ms;
		return *this;
	}

	// The look.
	Class_skill& on_targets(int sprite) {
		look.on_targets = sprite;
		return *this;
	}

	Class_skill& on_user(int sprite) {
		look.on_user = sprite;
		return *this;
	}

	Class_skill& missile(int shape) {
		look.missile = shape;
		return *this;
	}

	Class_skill& sound(int sfx) {
		look.sound = sfx;
		return *this;
	}

	Class_skill& says(const char* text) {
		words = text;
		return *this;
	}
};

struct Companion_class {
	const char*              npc_name;      // As Black Gate names them.
	const char*              class_name;
	Placement                placement;
	int                      item_shape;    // Class item, or -1 (none defined yet).
	const char*              item_name;
	std::vector<Class_skill> skills;
};

// Sprites (sprites.vga), as Black Gate's scripts use them.
constexpr int sprite_poof     = 9;
constexpr int sprite_bubbles  = 13;
constexpr int sprite_barrier  = 15;
constexpr int sprite_sparkles = 16;
constexpr int sprite_beads    = 18;
constexpr int sprite_music    = 24;

// Spell numbers (circle * 8 + place in the circle): the spellbook's
// pictures, used as icons.  Skills never run a spell's script.
constexpr int spell_fireworks    = 2;
constexpr int spell_fire_blast   = 18;
constexpr int spell_mass_cure    = 20;
constexpr int spell_protection   = 21;
constexpr int spell_heal         = 25;
constexpr int spell_paralyze     = 26;
constexpr int spell_protect_all  = 29;
constexpr int spell_sleep        = 30;
constexpr int spell_lightning    = 33;
constexpr int spell_dance        = 41;
constexpr int spell_great_heal   = 45;
constexpr int spell_invisibility = 46;
constexpr int spell_cause_fear   = 48;
constexpr int spell_death_bolt   = 57;
constexpr int spell_mass_might   = 62;
constexpr int spell_swordstrike  = 69;

// Each companion's class and skills.  Levels: the first at 1, the next at
// 4, the last at 8.
const std::vector<Companion_class>& Classes() {
	using T = Skill_type;
	using G = Skill_target;
	static const std::vector<Companion_class> classes = {
			{"Iolo",
			 "Bard",
			 Placement::ranged,
			 -1,
			 "Iolo's Lute",
			 {
					 Class_skill(11, "Song of Warding")
							 .icon_spell(spell_protect_all)
							 .kind(T::support)
							 .aim(G::party_area, 8, true)
							 .set_flag(Obj_flags::protection)
							 .targets(2)
							 .on_targets(sprite_music)
							 .says("*sings a song of warding*"),
					 Class_skill(10, "Battle Hymn")
							 .icon_spell(spell_mass_might)
							 .kind(T::support)
							 .aim(G::party_area, 8, true)
							 .unlock(4)
							 .bonus(3, 30000)
							 .targets(3)
							 .on_targets(sprite_music)
							 .says("*plays a battle hymn*"),
					 // Hostiles in the circle drowse for a few seconds (each rolls).
					 Class_skill(12, "Song of Peace")
							 .icon_spell(spell_dance)
							 .kind(T::control)
							 .aim(G::hostiles_nearest, 8, true)
							 .unlock(8)
							 .hinder(Obj_flags::asleep, 5000)
							 .targets(8)
							 .on_user(sprite_music)
							 .on_targets(sprite_music)
							 .says("*plays a song of peace*"),
			 }},
			{"Shamino",
			 "Ranger",
			 Placement::ranged,
			 -1,
			 "Shamino's Bow",
			 {
					 Class_skill(20, "Volley")
							 .icon_spell(spell_swordstrike)
							 .kind(T::attack)
							 .aim(G::hostiles_nearest, 12)
							 .volley()
							 .targets(3)
							 .says("*looses a volley*"),
					 Class_skill(21, "Aimed Shot")
							 .icon_spell(spell_death_bolt)
							 .kind(T::attack)
							 .aim(G::pick, 14)
							 .unlock(4)
							 .volley(6, 3000)
							 .says("*takes careful aim*"),
					 Class_skill(22, "Vanish")
							 .icon_spell(spell_invisibility)
							 .kind(T::self)
							 .unlock(8)
							 .set_flag(Obj_flags::invisible)
							 .on_targets(sprite_poof)
							 .says("*melts into the shadows*"),
			 }},
			{"Jaana",
			 "Druid",
			 Placement::support,
			 -1,
			 "Jaana's Staff",
			 {
					 Class_skill(30, "Mending Circle")
							 .icon_spell(spell_great_heal)
							 .kind(T::heal)
							 .aim(G::party_area, 8, true)
							 .heal(10)
							 .targets(2)
							 .on_targets(sprite_sparkles)
							 .says("*calls on the earth to mend*"),
					 Class_skill(31, "Purify")
							 .icon_spell(spell_mass_cure)
							 .kind(T::cure)
							 .aim(G::party_area, 8, true)
							 .unlock(4)
							 .cure()
							 .on_targets(sprite_bubbles)
							 .says("*purifies the air*"),
					 Class_skill(32, "Entangle")
							 .icon_spell(spell_paralyze)
							 .kind(T::control)
							 .aim(G::pick, 10)
							 .unlock(8)
							 .hinder(Obj_flags::paralyzed, 6000)
							 .on_targets(sprite_beads)
							 .says("*calls up grasping roots*"),
			 }},
			{"Dupre",
			 "Paladin",
			 Placement::front,
			 -1,
			 "Dupre's Shield",
			 {
					 Class_skill(40, "Stand Fast")
							 .icon_spell(spell_protection)
							 .kind(T::self)
							 .set_flag(Obj_flags::protection)
							 .on_targets(sprite_barrier)
							 .says("*stands fast*"),
					 Class_skill(41, "Lay on Hands")
							 .icon_spell(spell_heal)
							 .kind(T::heal)
							 .aim(G::hurt_party, 3)
							 .unlock(4)
							 .heal(25)
							 .on_targets(sprite_sparkles)
							 .says("*lays on hands*"),
					 Class_skill(42, "Challenge")
							 .icon_spell(spell_cause_fear)
							 .kind(T::control)
							 .aim(G::hostiles_nearest, 6, true)
							 .unlock(8)
							 .taunt(6000)
							 .targets(8)
							 .says("*issues a challenge*"),
			 }},
			{"Spark",
			 "Boy with a sling",
			 Placement::ranged,
			 -1,
			 "Spark's Sling",
			 {
					 Class_skill(50, "Knockdown")
							 .icon_spell(spell_sleep)
							 .kind(T::control)
							 .aim(G::pick, 10)
							 .hinder(Obj_flags::asleep, 4000)
							 .says("*lets fly a stone*"),
					 Class_skill(51, "Trick Shot")
							 .icon_spell(spell_fireworks)
							 .kind(T::attack)
							 .aim(G::hostiles_nearest, 10)
							 .unlock(4)
							 .volley()
							 .targets(2)
							 .says("*tries a trick shot*"),
			 }},
	};
	return classes;
}

const Companion_class* Class_of(const Actor* actor) {
	if (!actor || !GAME_BG || actor == Gwin()->get_main_actor()) {
		return nullptr;
	}
	const std::string name = actor->get_npc_name();
	for (const auto& each : Classes()) {
		if (name == each.npc_name) {
			return &each;
		}
	}
	return nullptr;
}

const Class_skill* Skill_by_id(const Companion_class* cls, int id) {
	if (cls) {
		for (const auto& skill : cls->skills) {
			if (skill.id == id) {
				return &skill;
			}
		}
	}
	return nullptr;
}

bool Effect_built(const Class_skill& skill) {
	return skill.effect != Skill_effect::taunt && skill.effect != Skill_effect::calm;
}

/*
 *  State.  Cooldowns are the host's (a guest keeps a copy from the host's
 *  messages); none of it is saved.
 */

constexpr uint32 global_wait_ms = 1500;    // After any skill, before the next.
constexpr uint32 ai_period_ms   = 500;     // How often the Class AI looks.

struct Cooldown_key {
	int npc;
	int skill;
	bool operator<(const Cooldown_key& other) const {
		return npc != other.npc ? npc < other.npc : skill < other.skill;
	}
};
std::map<Cooldown_key, uint32> cooldowns;     // Ready again at (ticks).
std::map<int, uint32>          global_waits;  // By NPC: may act again at.

struct Timed_flag {
	Game_object_weak actor;
	int              flag;
	uint32           until;
};
std::vector<Timed_flag> timed_flags;    // Host: short paralysis and knockdowns.

struct Timed_bonus {
	Game_object_weak actor;
	int              amount;
	uint32           until;
};
std::vector<Timed_bonus> bonuses;    // Host: strength and combat bonuses.

struct Range_circle {
	int    npc;
	int    radius;    // Tiles.
	int    color;     // Palette index.
	uint32 until;
};
std::vector<Range_circle> circles;    // Every machine: circles to show.

uint32 Seconds_left(int npc, int skill) {
	const auto found = cooldowns.find({npc, skill});
	const uint32 now = Now();
	if (found == cooldowns.end() || found->second <= now) {
		return 0;
	}
	return (found->second - now + 999) / 1000;
}

bool Short_cooldowns() {
	static bool checked = false;
	static bool on      = false;
	if (!checked) {
		// For testing only, not on the options page.
		config->value("config/friend/class/short_cooldowns", on, false);
		checked = true;
	}
	return on;
}

int Cooldown_ms(const Class_skill& skill) {
	return (Short_cooldowns() ? 5 : skill.cooldown) * 1000;
}

/*
 *  The rules.
 */

int Health(const Actor* actor) {
	return actor->get_property(Actor::health);
}

int Max_health(const Actor* actor) {
	return std::max(1, actor->get_property(Actor::strength));
}

bool Able(const Actor* actor) {
	return actor && actor->get_chunk() && !actor->is_dead() && !actor->get_flag(Obj_flags::asleep)
		   && !actor->get_flag(Obj_flags::paralyzed) && !actor->is_knocked_out();
}

// The Avatar and the party, alive.
std::vector<Actor*> Party() {
	std::vector<Actor*> party;
	Game_window*        gwin   = Gwin();
	Actor*              avatar = gwin->get_main_actor();
	if (avatar && !avatar->is_dead()) {
		party.push_back(avatar);
	}
	Party_manager* partyman = gwin->get_party_man();
	for (int i = 0; i < partyman->get_count(); ++i) {
		Actor* member = gwin->get_npc(partyman->get_member(i));
		if (member && !member->is_dead()) {
			party.push_back(member);
		}
	}
	return party;
}

std::vector<Actor*> Party_in_range(const Actor* user, int range) {
	std::vector<Actor*> found;
	for (Actor* member : Party()) {
		if (member->get_map() == user->get_map() && user->distance(member) <= range) {
			found.push_back(member);
		}
	}
	return found;
}

// Hostiles within 'range' of the user, nearest first.
std::vector<Actor*> Hostiles(const Actor* user, int range) {
	Actor_vector near;
	user->find_nearby_actors(near, c_any_shapenum, range);
	std::vector<Actor*> found;
	for (Actor* each : near) {
		if (Friend_mode_is_hostile(user, each) && user->distance(each) <= range) {
			found.push_back(each);
		}
	}
	std::sort(found.begin(), found.end(), [user](const Actor* a, const Actor* b) {
		return user->distance(a) < user->distance(b);
	});
	return found;
}

bool Has_class_item(const Actor* user, const Companion_class& cls) {
	if (!Friend_class_require_items || cls.item_shape < 0) {
		return true;    // Not asked for, or no item made yet.
	}
	return const_cast<Actor*>(user)->find_item(cls.item_shape, c_any_qual, c_any_framenum) != nullptr;
}

// Whether 'skill' may be used by 'user' at all now (not who it would
// touch).  'why' says why not.
bool May_use(const Actor* user, const Companion_class& cls, const Class_skill& skill, std::string& why) {
	if (!Friend_class_active()) {
		why = "The Class System is off";
		return false;
	}
	if (!Effect_built(skill)) {
		why = std::string(skill.name) + " is not ready yet";
		return false;
	}
	if (user->get_level() < skill.level) {
		why = std::string(skill.name) + ": level " + std::to_string(skill.level);
		return false;
	}
	if (!Has_class_item(user, cls)) {
		why = std::string("Needs ") + cls.item_name;
		return false;
	}
	if (!Able(user)) {
		why = std::string(user->get_npc_name()) + " can't now";
		return false;
	}
	if (const uint32 left = Seconds_left(user->get_npc_num(), skill.id)) {
		why = std::string(skill.name) + ": " + std::to_string(left) + (left == 1 ? " second" : " seconds");
		return false;
	}
	const auto wait = global_waits.find(user->get_npc_num());
	if (wait != global_waits.end() && wait->second > Now()) {
		why = "Not so fast";
		return false;
	}
	return true;
}

// Who the skill would touch now.
std::vector<Actor*> Targets(Actor* user, const Class_skill& skill) {
	switch (skill.target) {
	case Skill_target::self:
		return {user};
	case Skill_target::party_area:
		return Party_in_range(user, skill.range);
	case Skill_target::hurt_party: {
		Actor* worst = nullptr;
		for (Actor* member : Party_in_range(user, skill.range)) {
			if (Health(member) < Max_health(member)
				&& (!worst || Health(member) * Max_health(worst) < Health(worst) * Max_health(member))) {
				worst = member;
			}
		}
		if (worst) {
			return {worst};
		}
		return {};
	}
	case Skill_target::pick: {
		Game_object* target = user->get_target();
		Actor*       foe    = target ? target->as_actor() : nullptr;
		if (foe && Friend_mode_is_hostile(user, foe) && user->distance(foe) <= skill.range) {
			return {foe};
		}
		std::vector<Actor*> near = Hostiles(user, skill.range);
		if (near.empty()) {
			return {};
		}
		return {near.front()};
	}
	case Skill_target::hostiles_nearest: {
		std::vector<Actor*> near = Hostiles(user, skill.range);
		if (static_cast<int>(near.size()) > skill.count) {
			near.resize(skill.count);
		}
		return near;
	}
	}
	return {};
}

// Who of 'targets' the skill would do something for: nobody hurt means a
// heal is wasted, and so on.
std::vector<Actor*> Worth_it(const Class_skill& skill, const std::vector<Actor*>& targets) {
	std::vector<Actor*> worth;
	for (Actor* each : targets) {
		bool use = true;
		if (skill.effect == Skill_effect::heal) {
			use = Health(each) < Max_health(each);
		} else if (skill.effect == Skill_effect::cure) {
			use = each->get_flag(Obj_flags::poisoned) || each->get_flag(Obj_flags::paralyzed)
				  || each->get_flag(Obj_flags::cursed);
		}
		if (use) {
			worth.push_back(each);
		}
	}
	return worth;
}

int Circle_color(const Class_skill& skill) {
	Shape_manager* sman = Shape_manager::get_instance();
	switch (skill.type) {
	case Skill_type::heal:
	case Skill_type::cure:
		return sman->get_special_pixel(POISON_PIXEL);    // Green.
	case Skill_type::attack:
	case Skill_type::control:
		return sman->get_special_pixel(HIT_PIXEL);    // Red.
	default:
		return sman->get_special_pixel(PROTECT_PIXEL);    // Blue.
	}
}

void Show_circle(int npc, const Class_skill& skill, uint32 ms) {
	if (!skill.range_shown || skill.range <= 0) {
		return;
	}
	circles.erase(std::remove_if(circles.begin(), circles.end(),
						  [npc](const Range_circle& c) {
							  return c.npc == npc;
						  }),
				  circles.end());
	circles.push_back({npc, skill.range, Circle_color(skill), Now() + ms});
}

/*
 *  Effects (host only).
 */

void Say(Actor* actor, const char* words) {
	Gwin()->get_effects()->add_text(words, actor);
	Friend_mode_note_speech(actor, words);
}

void Sprite_on(Actor* actor, int sprite) {
	if (sprite >= 0) {
		Gwin()->get_effects()->add_effect(std::make_unique<Sprites_effect>(sprite, actor, 0, 0, 0, 0));
	}
}

// A missile picture from the user to the target, on every machine (the
// effect does any harm; this is only the look).
void Missile_to(Actor* user, Actor* target, int shape) {
	if (shape < 0) {
		return;
	}
	const Tile_coord from = user->get_tile();
	const Tile_coord to   = target->get_tile();
	Gwin()->get_effects()->add_effect(std::make_unique<Projectile_effect>(from, to, shape, shape, -1));
	Friend_mode_note_projectile(from, to, shape, shape, -1);
}

void Sound_at(Actor* actor, int sfx) {
	if (sfx >= 0) {
		Audio::get_ptr()->play_sound_effect(sfx, actor);
		Friend_mode_note_sound(sfx, actor);
	}
}

void Add_bonus(Actor* actor, int amount, int ms) {
	bonuses.push_back({weak_from_obj(actor), amount, Now() + static_cast<uint32>(ms)});
}

// A shot with the user's readied weapon, through the combat code (range,
// ammunition, hit and damage as U7).  False if it couldn't be made.
bool Shoot(Actor* user, Actor* target) {
	int                points = 0;
	int                weapon = -1;
	Game_object*       obj    = nullptr;
	const Weapon_info* winf   = user->get_weapon(points, weapon, obj);
	if (!winf || weapon < 0) {
		return false;
	}
	user->change_frame(user->get_dir_framenum(user->get_direction(target), Actor::ready_frame));
	return Combat_schedule::attack_target(user, target, target->get_tile(), weapon);
}

// Carry the skill out.  'why' says why not.
bool Perform(Actor* user, const Companion_class& cls, const Class_skill& skill, std::string& why) {
	if (!May_use(user, cls, skill, why)) {
		return false;
	}
	std::vector<Actor*> targets = Worth_it(skill, Targets(user, skill));
	if (targets.empty()) {
		switch (skill.type) {
		case Skill_type::heal:
			why = "Nobody needs healing";
			break;
		case Skill_type::cure:
			why = "Nobody needs curing";
			break;
		case Skill_type::attack:
		case Skill_type::control:
			why = "No hostile in range";
			break;
		default:
			why = "Nobody in range";
			break;
		}
		return false;
	}
	int done = 0;
	switch (skill.effect) {
	case Skill_effect::heal:
		for (Actor* each : targets) {
			const int before = Health(each);
			each->set_property(Actor::health, std::min(Max_health(each), before + skill.amount));
			done += Health(each) - before;
		}
		break;
	case Skill_effect::cure:
		for (Actor* each : targets) {
			each->clear_flag(Obj_flags::poisoned);
			each->clear_flag(Obj_flags::paralyzed);
			each->clear_flag(Obj_flags::cursed);
			++done;
		}
		break;
	case Skill_effect::flag:
		for (Actor* each : targets) {
			each->set_flag(skill.flag);
			++done;
		}
		break;
	case Skill_effect::hinder:
		for (Actor* each : targets) {
			// As hits with powers roll (Actor::figure_hit_points).
			// Some creatures can't be put to sleep or paralyzed: no flag.
			const bool had = each->get_flag(skill.flag);
			if (!had
				&& Actor::roll_to_win(user->get_effective_prop(Actor::combat), each->get_effective_prop(Actor::strength))) {
				each->set_flag(skill.flag);
			}
			if (!had && each->get_flag(skill.flag)) {
				timed_flags.push_back({weak_from_obj(each), skill.flag, Now() + static_cast<uint32>(skill.duration_ms)});
				++done;
			} else {
				Gwin()->get_effects()->add_text("Resisted", each);
				Friend_mode_note_speech(each, "Resisted");
			}
		}
		break;
	case Skill_effect::volley:
		if (skill.amount > 0) {
			Add_bonus(user, skill.amount, skill.duration_ms);    // Aimed Shot: while it flies.
		}
		for (Actor* each : targets) {
			if (Shoot(user, each)) {
				++done;
			}
		}
		if (done == 0) {
			why = "Needs a bow, crossbow or sling, and ammunition";
			return false;
		}
		break;
	case Skill_effect::bonus:
		for (Actor* each : targets) {
			Add_bonus(each, skill.amount, skill.duration_ms);
			++done;
		}
		break;
	case Skill_effect::taunt:
	case Skill_effect::calm:
		why = std::string(skill.name) + " is not ready yet";
		return false;
	}
	const uint32 now = Now();
	cooldowns[{user->get_npc_num(), skill.id}] = now + static_cast<uint32>(Cooldown_ms(skill));
	global_waits[user->get_npc_num()]          = now + global_wait_ms;
	if (skill.effect != Skill_effect::volley) {
		user->change_frame(user->get_dir_framenum(Actor::raise1_frame));
	}
	if (skill.words[0]) {
		Say(user, skill.words);
	}
	Sprite_on(user, skill.look.on_user);
	Sound_at(user, skill.look.sound);
	for (Actor* each : targets) {
		Sprite_on(each, skill.look.on_targets);
		if (each != user) {
			Missile_to(user, each, skill.look.missile);
		}
	}
	Show_circle(user->get_npc_num(), skill, 2000);
	if (Friend_mode_get_session().connected()) {
		std::vector<unsigned char> event;
		const int                  npc = user->get_npc_num();
		event.push_back(static_cast<unsigned char>(npc & 0xff));
		event.push_back(static_cast<unsigned char>(npc >> 8));
		event.push_back(static_cast<unsigned char>(skill.id & 0xff));
		event.push_back(static_cast<unsigned char>(skill.id >> 8));
		Friend_mode_get_session().send_blob(Friend_mode_session::Blob_kind::class_event, event);
	}
	cout << "Friend Class: " << user->get_npc_name() << " uses " << skill.name << " on " << targets.size()
		 << (targets.size() == 1 ? " target" : " targets");
	if (skill.effect == Skill_effect::heal) {
		cout << " (+" << done << " health)";
	} else if (skill.effect == Skill_effect::hinder || skill.effect == Skill_effect::volley) {
		cout << " (" << done << " landed)";
	}
	cout << endl;
	return true;
}

// Host: end short paralysis and knockdowns, and old bonuses.
void Expire() {
	const uint32 now = Now();
	timed_flags.erase(std::remove_if(timed_flags.begin(), timed_flags.end(),
							  [now](const Timed_flag& each) {
								  if (each.until > now) {
									  return false;
								  }
								  if (const Game_object_shared obj = each.actor.lock()) {
									  Actor* actor = obj->as_actor();
									  if (actor && !actor->is_dead() && actor->get_flag(each.flag)) {
										  actor->clear_flag(each.flag);
									  }
								  }
								  return true;
							  }),
					  timed_flags.end());
	const size_t before = bonuses.size();
	bonuses.erase(std::remove_if(bonuses.begin(), bonuses.end(),
						  [now](const Timed_bonus& each) {
							  return each.until <= now || each.actor.expired();
						  }),
				  bonuses.end());
	if (bonuses.size() != before) {
		// Health over the new maximum is cut back.
		for (Actor* member : Party()) {
			if (Health(member) > Max_health(member)) {
				member->set_property(Actor::health, Max_health(member));
			}
		}
	}
	for (auto it = cooldowns.begin(); it != cooldowns.end();) {
		it = it->second <= now ? cooldowns.erase(it) : std::next(it);
	}
}

/*
 *  Class AI (host).  Companions nobody plays use their skills by their
 *  class's rules; placing them (keeping distance, walking into range) comes
 *  next, in the combat schedule.
 */

bool Ai_wants(Actor* user, const Class_skill& skill) {
	const bool fighting = Gwin()->in_combat() || user->get_schedule_type() == Schedule::combat;
	switch (skill.type) {
	case Skill_type::heal: {
		// Below half, as many as the skill asks (Mending Circle: two).
		int hurt = 0;
		for (Actor* each : Targets(user, skill)) {
			if (Health(each) * 2 < Max_health(each)) {
				++hurt;
			}
		}
		return hurt >= std::max(1, skill.count);
	}
	case Skill_type::cure:
		return !Worth_it(skill, Targets(user, skill)).empty();
	case Skill_type::support: {
		if (!fighting) {
			return false;
		}
		// Enough hostiles near to make a song worth it.
		return static_cast<int>(Hostiles(user, 12).size()) >= std::max(2, skill.count);
	}
	case Skill_type::attack: {
		if (!fighting) {
			return false;
		}
		const int want = skill.target == Skill_target::hostiles_nearest ? std::min(2, skill.count) : 1;
		return static_cast<int>(Targets(user, skill).size()) >= want;
	}
	case Skill_type::control: {
		if (!fighting) {
			return false;
		}
		// A hostile on a party member who is below half.
		for (Actor* foe : Targets(user, skill)) {
			Game_object* on     = foe->get_target();
			Actor*       victim = on ? on->as_actor() : nullptr;
			if (victim && (victim->is_in_party() || victim == Gwin()->get_main_actor())
				&& Health(victim) * 2 < Max_health(victim)) {
				return true;
			}
		}
		return false;
	}
	case Skill_type::self:
		return fighting && Health(user) * 2 < Max_health(user) && !Hostiles(user, 3).empty();
	}
	return false;
}

void Class_ai() {
	static uint32 next = 0;
	const uint32  now  = Now();
	if (now < next) {
		return;
	}
	next              = now + ai_period_ms;
	Game_window* gwin = Gwin();
	if (!Friend_class_ai || gwin->get_usecode()->in_usecode() || gwin->is_network_host_paused()) {
		return;
	}
	for (Actor* member : Party()) {
		const Companion_class* cls = Class_of(member);
		if (!cls || member == gwin->get_controlled_actor() || Friend_mode_player_playing(member) >= 0) {
			continue;    // A player uses its skills.
		}
		for (const auto& skill : cls->skills) {
			std::string why;
			if (May_use(member, *cls, skill, why) && Ai_wants(member, skill) && Perform(member, *cls, skill, why)) {
				break;    // One at a time.
			}
		}
	}
}

/*
 *  Host: the rules and each guest's cooldowns, to guests.
 */

std::map<int, std::vector<unsigned char>> sent_states;    // By player.
bool                                      rules_changed = false;

std::vector<unsigned char> State_for(int player) {
	std::vector<unsigned char> state;
	state.push_back(static_cast<unsigned char>((Friend_class_enabled ? 1 : 0) | (Friend_class_require_items ? 2 : 0)
											   | (Friend_class_ai ? 4 : 0)));
	Actor* actor = nullptr;
	for (Actor* member : Party()) {
		if (Friend_mode_player_playing(member) == player) {
			actor = member;
		}
	}
	const Companion_class* cls = Class_of(actor);
	const int              npc = actor ? actor->get_npc_num() : 0xffff;
	state.push_back(static_cast<unsigned char>(npc & 0xff));
	state.push_back(static_cast<unsigned char>(npc >> 8));
	state.push_back(static_cast<unsigned char>(cls ? cls->skills.size() : 0));
	if (cls) {
		for (const auto& skill : cls->skills) {
			const uint32 left = Seconds_left(npc, skill.id);
			state.push_back(static_cast<unsigned char>(skill.id & 0xff));
			state.push_back(static_cast<unsigned char>(skill.id >> 8));
			state.push_back(static_cast<unsigned char>(std::min<uint32>(left, 255)));
		}
	}
	return state;
}

void Send_states() {
	static uint32 next = 0;
	const uint32  now  = Now();
	if (now < next && !rules_changed) {
		return;
	}
	next                     = now + 250;
	rules_changed            = false;
	Friend_mode_session& net = Friend_mode_get_session();
	if (!net.connected()) {
		sent_states.clear();
		return;
	}
	for (const int player : net.guest_players()) {
		std::vector<unsigned char> state = State_for(player);
		auto&                      sent  = sent_states[player];
		if (state != sent) {
			net.send_blob(Friend_mode_session::Blob_kind::class_state, state, player);
			sent = std::move(state);
		}
	}
}

/*
 *  Guest: the host's word.
 */

int guest_state_npc = -1;    // Its companion, as the host last said.

void Take_state(const std::vector<unsigned char>& data) {
	if (data.size() < 4) {
		return;
	}
	Friend_class_enabled       = (data[0] & 1) != 0;
	Friend_class_require_items = (data[0] & 2) != 0;
	Friend_class_ai            = (data[0] & 4) != 0;
	const int npc              = data[1] | (data[2] << 8);
	guest_state_npc            = npc == 0xffff ? -1 : npc;
	const int count            = data[3];
	const uint32 now           = Now();
	for (int i = 0; i < count && 4 + 3 * i + 2 < static_cast<int>(data.size()); ++i) {
		const unsigned char* at    = &data[4 + 3 * i];
		const int            skill = at[0] | (at[1] << 8);
		const int            left  = at[2];
		if (left > 0) {
			// Keep counting down between messages: only move a clock
			// that is a second or more out.
			uint32&      until = cooldowns[{npc, skill}];
			const uint32 want  = now + static_cast<uint32>(left) * 1000;
			if (until + 1000 < want || until > want + 1000) {
				until = want;
			}
		} else {
			cooldowns.erase({npc, skill});
		}
	}
}

void Take_event(const std::vector<unsigned char>& data) {
	if (data.size() < 4) {
		return;
	}
	const int    npc   = data[0] | (data[1] << 8);
	const int    id    = data[2] | (data[3] << 8);
	Actor*       actor = Gwin()->get_npc(npc);
	const auto*  skill = Skill_by_id(Class_of(actor), id);
	if (skill) {
		Show_circle(npc, *skill, 2000);
		if (npc == guest_state_npc) {
			global_waits[npc] = Now() + global_wait_ms;
		}
	}
}

/*
 *  The bars.
 */

// The Avatar's spell bar: chosen spells, cast in one click.
const std::array<int, 3> avatar_spells = {spell_great_heal, spell_fire_blast, spell_lightning};

const char* Spell_name(int spell) {
	switch (spell) {
	case spell_great_heal:
		return "Great Heal";
	case spell_fire_blast:
		return "Fire Blast";
	case spell_lightning:
		return "Lightning";
	default:
		return "Spell";
	}
}

bool Spell_heals(int spell) {
	return spell == spell_great_heal;
}

Spellbook_object* Avatar_spellbook() {
	Actor* avatar = Gwin()->get_main_actor();
	if (!avatar) {
		return nullptr;
	}
	Game_object_vector all;
	avatar->get_objects(all, c_any_shapenum, c_any_qual, c_any_framenum);
	for (Game_object* obj : all) {
		if (Spellbook_object* book = obj->as_spellbook()) {
			return book;
		}
	}
	return nullptr;
}

// One slot of a bar: a skill of the companion this machine plays, or one of
// the Avatar's spells.
struct Bar_slot {
	int  skill = -1;    // Skill id, or -1.
	int  spell = -1;    // Spell number, or -1.
	int  icon  = 0;     // Spell number for the picture.
	bool usable  = false;
	int  seconds = 0;    // Recharging.
	int  level   = 0;    // Locked until this level.
	std::string name;
	std::string why;     // Why not usable.
};

// Which actor's bar this machine shows, if any: a guest's companion, or the
// Avatar on the host and in single player.
Actor* Bar_actor() {
	Game_window* gwin = Gwin();
	if (!Friend_class_active() || gwin->is_in_exult_menu() || !gwin->get_main_actor()) {
		return nullptr;
	}
	if (Is_guest()) {
		Actor* own = gwin->get_controlled_actor();
		if (!gwin->has_network_friend_control() || gwin->is_network_spectating() || Friend_mode_guest_following()
			|| !Class_of(own)) {
			return nullptr;
		}
		return own;
	}
	return gwin->get_main_actor();
}

std::vector<Bar_slot> Bar_slots(Actor* actor) {
	std::vector<Bar_slot> slots;
	if (!actor) {
		return slots;
	}
	if (actor == Gwin()->get_main_actor()) {
		Spellbook_object* book = Avatar_spellbook();
		for (const int spell : avatar_spells) {
			Bar_slot slot;
			slot.spell = spell;
			slot.icon  = spell;
			slot.name  = Spell_name(spell);
			if (!book) {
				slot.why = "No spellbook";
			} else if (!book->has_spell(spell)) {
				slot.why = std::string("You don't know ") + slot.name;
			} else if (!book->can_do_spell(actor, spell)) {
				slot.why = slot.name + ": not enough mana, reagents or experience";
			} else {
				slot.usable = true;
			}
			slots.push_back(slot);
		}
		return slots;
	}
	const Companion_class* cls = Class_of(actor);
	if (!cls) {
		return slots;
	}
	for (const auto& skill : cls->skills) {
		Bar_slot slot;
		slot.skill   = skill.id;
		slot.icon    = skill.icon;
		slot.name    = skill.name;
		slot.seconds = static_cast<int>(Seconds_left(actor->get_npc_num(), skill.id));
		slot.level   = actor->get_level() < skill.level ? skill.level : 0;
		slot.usable  = May_use(actor, *cls, skill, slot.why);
		slots.push_back(slot);
	}
	return slots;
}

void Use_slot(const Bar_slot& slot) {
	Game_window* gwin  = Gwin();
	Actor*       actor = Bar_actor();
	if (!actor) {
		return;
	}
	if (slot.spell >= 0) {
		// The Avatar's spell, through the spellbook (mana, reagents, words
		// of power); the Target System's pick answers the crosshair.
		Spellbook_object* book = Avatar_spellbook();
		if (!slot.usable || !book) {
			Friend_mode_class_notice(slot.why);
			return;
		}
		Actor*       pick = gwin->get_action_target();
		Game_object* aim  = nullptr;
		if (Spell_heals(slot.spell)) {
			aim = pick && (pick->is_in_party() || pick == actor) ? pick : actor;
		} else if (pick && !pick->is_in_party()) {
			aim = pick;
		}
		book->do_spell(actor, slot.spell, false, false, aim);
		return;
	}
	const Companion_class* cls   = Class_of(actor);
	const Class_skill*     skill = Skill_by_id(cls, slot.skill);
	if (!skill) {
		return;
	}
	std::string why;
	if (!May_use(actor, *cls, *skill, why)) {
		Friend_mode_class_notice(why);
		return;
	}
	if (Is_guest()) {
		std::vector<unsigned char> request = {static_cast<unsigned char>(skill->id & 0xff),
											  static_cast<unsigned char>(skill->id >> 8)};
		Friend_mode_get_session().send_blob(Friend_mode_session::Blob_kind::class_use, request);
		global_waits[actor->get_npc_num()] = Now() + global_wait_ms;
		return;
	}
	if (!Perform(actor, *cls, *skill, why)) {
		Friend_mode_class_notice(why);
	}
}

class Skill_bar;
Skill_bar* the_bar = nullptr;

// A slot's button: the gump paints it; this only takes the click.
class Skill_button : public Gump_button {
	int index;

public:
	Skill_button(Gump* par, int icon, int px, int py, int i)
			: Gump_button(par, 33 + icon % 8, px, py, SF_GUMPS_VGA), index(i) {
		set_frame(icon / 8);
	}

	bool activate(MouseButton button) override;

	bool push(MouseButton button) override {
		return button == MouseButton::Left;
	}

	void unpush(MouseButton button) override {
		ignore_unused_variable_warning(button);
	}

	void paint() override {}

	bool is_draggable() const override {
		return false;
	}

	int get_index() const {
		return index;
	}
};

constexpr int slot_gap    = 2;
constexpr int slot_pad    = 2;
constexpr int name_height = 10;    // A line for the hovered skill's name.

// The bar: a HUD gump, drawn into its own layer at the HUD's scale (as the
// shortcut bar and the party's face stats are).
class Skill_bar : public Gump {
	std::vector<Bar_slot>                      slots;
	std::vector<std::unique_ptr<Skill_button>> buttons;
	TileRect                                   region{0, 0, 0, 0};
	int                                        slot_w  = 0;
	int                                        slot_h  = 0;
	int                                        hovered = -1;

public:
	Skill_bar() : Gump(nullptr, 0, 0, 0, SF_GUMPS_VGA) {}

	~Skill_bar() override {
		if (the_bar == this) {
			the_bar = nullptr;
		}
		Gwin()->set_all_dirty();
	}

	bool is_persistent() const override {
		return true;
	}

	bool is_draggable() const override {
		return false;
	}

	bool no_handcursor() const override {
		return true;
	}

	bool is_hud() const override {
		return true;
	}

	TileRect get_rect() const override {
		return region;
	}

	TileRect get_dirty() override {
		return region;
	}

	bool has_point(int mx, int my) const override {
		return region.has_point(mx, my);
	}

	Gump_button* on_button(int mx, int my) override {
		for (auto& button : buttons) {
			if (button->on_button(mx, my)) {
				return button.get();
			}
		}
		return nullptr;
	}

	const Bar_slot* slot(int index) const {
		return index >= 0 && index < static_cast<int>(slots.size()) ? &slots[index] : nullptr;
	}

	void set_hovered(int index) {
		if (index != hovered) {
			hovered = index;
			Gwin()->set_all_dirty();
		}
	}

	int get_hovered() const {
		return hovered;
	}

	// New slots (another companion, a level, a cooldown tick) and place.
	void update(std::vector<Bar_slot> fresh) {
		const bool same_skills
				= fresh.size() == slots.size()
				  && std::equal(fresh.begin(), fresh.end(), slots.begin(), [](const Bar_slot& a, const Bar_slot& b) {
						 return a.skill == b.skill && a.spell == b.spell;
					 });
		const bool changed = !same_skills
							 || !std::equal(fresh.begin(), fresh.end(), slots.begin(),
									 [](const Bar_slot& a, const Bar_slot& b) {
										 return a.usable == b.usable && a.seconds == b.seconds && a.level == b.level;
									 });
		slots = std::move(fresh);
		if (!same_skills) {
			make_buttons();
		}
		if (place() || changed) {
			Gwin()->set_all_dirty();
		}
	}

	void paint() override;

private:
	void make_buttons() {
		buttons.clear();
		slot_w = 0;
		slot_h = 0;
		for (const auto& each : slots) {
			ShapeID      id(33 + each.icon % 8, each.icon / 8, SF_GUMPS_VGA);
			Shape_frame* frame = id.get_shape();
			if (frame) {
				slot_w = std::max(slot_w, frame->get_width());
				slot_h = std::max(slot_h, frame->get_height());
			}
		}
		slot_w += 2 * slot_pad;
		slot_h += 2 * slot_pad;
		for (size_t i = 0; i < slots.size(); ++i) {
			ShapeID      id(33 + slots[i].icon % 8, slots[i].icon / 8, SF_GUMPS_VGA);
			Shape_frame* frame = id.get_shape();
			const int    sx    = static_cast<int>(i) * (slot_w + slot_gap);
			const int    px    = sx + (frame ? (slot_w - frame->get_width()) / 2 + frame->get_xleft() : 0);
			const int    py    = name_height + (frame ? (slot_h - frame->get_height()) / 2 + frame->get_yabove() : 0);
			buttons.push_back(std::make_unique<Skill_button>(this, slots[i].icon, px, py, static_cast<int>(i)));
		}
	}

	// Where the chosen position puts the bar, in game coordinates.  True if
	// it moved.
	bool place() {
		Image_window* iwin = Gwin()->get_win();
		const int     n    = static_cast<int>(slots.size());
		const int     w    = std::max(1, n * slot_w + (n - 1) * slot_gap);
		const int     h    = name_height + slot_h;
		const int     left = iwin->get_start_x();
		const int     top  = iwin->get_start_y();
		const int     right  = iwin->get_end_x();
		const int     bottom = iwin->get_end_y();
		constexpr int edge   = 4;
		// The party's faces along the bottom (Exult's status bars).
		TileRect faces(0, 0, 0, 0);
		for (auto it = Gwin()->get_gump_man()->begin(); it != Gwin()->get_gump_man()->end(); ++it) {
			if (auto* stats = dynamic_cast<Face_stats*>(*it)) {
				faces = stats->get_rect();
			}
		}
		const bool faces_at_bottom = faces.w > 0 && faces.w > faces.h;
		int        bx              = 0;
		int        by              = 0;
		switch (Friend_class_bar_position) {
		case friend_bar_bottom_left:
			bx = left + edge;
			by = bottom - h - edge;
			break;
		case friend_bar_bottom_right:
			bx = right - w - edge;
			by = bottom - h - edge;
			break;
		case friend_bar_top_left:
			bx = left + edge;
			by = top + edge;
			break;
		case friend_bar_top_right:
			bx = right - w - edge;
			by = top + edge + (Friend_minimap_enabled ? 58 : 0);    // Under the minimap.
			break;
		default:    // Above the status bar, bottom centre.
			bx = left + (right - left - w) / 2;
			by = bottom - h - edge;
			break;
		}
		// Never on the faces: above them.
		if (faces_at_bottom && by + h > faces.y && bx < faces.x + faces.w && bx + w > faces.x) {
			by = faces.y - h - 2;
		}
		const TileRect fresh(bx, by, w, h);
		if (fresh.x == region.x && fresh.y == region.y && fresh.w == region.w && fresh.h == region.h) {
			return false;
		}
		region = fresh;
		x      = bx;
		y      = by;
		return true;
	}
};

bool Skill_button::activate(MouseButton button) {
	if (button != MouseButton::Left || !the_bar) {
		return false;
	}
	if (const Bar_slot* slot = the_bar->slot(index)) {
		Use_slot(*slot);
	}
	return true;
}

// Every other pixel of a rectangle in black: greyed out.
void Shade(Image_buffer8* buf, int x0, int y0, int w, int h) {
	for (int yy = 0; yy < h; ++yy) {
		for (int xx = (yy & 1); xx < w; xx += 2) {
			buf->put_pixel8(0, x0 + xx, y0 + yy);
		}
	}
}

void Skill_bar::paint() {
	Shape_manager* sman = Shape_manager::get_instance();
	Image_buffer8* buf  = Gwin()->get_win()->get_ib8();
	const int      line = sman->get_special_pixel(PROTECT_PIXEL);
	for (size_t i = 0; i < slots.size(); ++i) {
		const Bar_slot& each = slots[i];
		int             sx   = static_cast<int>(i) * (slot_w + slot_gap);
		int             sy   = name_height;
		local_to_screen(sx, sy);
		// A dark box with an edge, blue under the mouse.
		buf->fill8(0, slot_w, slot_h, sx, sy);
		if (static_cast<int>(i) == hovered) {
			buf->fill8(line, slot_w, 1, sx, sy);
			buf->fill8(line, slot_w, 1, sx, sy + slot_h - 1);
			buf->fill8(line, 1, slot_h, sx, sy);
			buf->fill8(line, 1, slot_h, sx + slot_w - 1, sy);
		}
		ShapeID      id(33 + each.icon % 8, each.icon / 8, SF_GUMPS_VGA);
		Shape_frame* frame = id.get_shape();
		if (frame) {
			const int ix = sx + (slot_w - frame->get_width()) / 2 + frame->get_xleft();
			const int iy = sy + (slot_h - frame->get_height()) / 2 + frame->get_yabove();
			sman->paint_shape(ix, iy, frame);
		}
		if (!each.usable) {
			Shade(buf, sx + 1, sy + 1, slot_w - 2, slot_h - 2);
		}
		char text[8] = "";
		if (each.level > 0) {
			std::snprintf(text, sizeof(text), "L%d", each.level);
		} else if (each.seconds > 0) {
			std::snprintf(text, sizeof(text), "%d", each.seconds);
		}
		if (text[0]) {
			const int tw = sman->get_text_width(0, text);
			const int th = sman->get_text_height(0);
			sman->paint_text(0, text, sx + (slot_w - tw) / 2, sy + (slot_h - th) / 2);
		}
	}
	if (const Bar_slot* each = slot(hovered)) {
		int nx = 0;
		int ny = 0;
		local_to_screen(nx, ny);
		const int tw = sman->get_text_width(0, each->name.c_str());
		sman->paint_text(0, each->name.c_str(), nx + (region.w - tw) / 2, ny);
	}
}

// Show, refresh or hide this machine's bar.
void Update_bar() {
	Game_window*  gwin  = Gwin();
	Gump_manager* gumps = gwin->get_gump_man();
	Actor*        actor = Bar_actor();
	if (the_bar) {
		bool open = false;
		for (auto it = gumps->begin(); it != gumps->end(); ++it) {
			open = open || *it == the_bar;
		}
		if (!open) {
			the_bar = nullptr;    // Closed (and deleted) with the others.
		}
	}
	if (!actor) {
		if (the_bar) {
			gumps->close_gump(the_bar);
			the_bar = nullptr;
		}
		return;
	}
	std::vector<Bar_slot> slots = Bar_slots(actor);
	if (slots.empty()) {
		if (the_bar) {
			gumps->close_gump(the_bar);
			the_bar = nullptr;
		}
		return;
	}
	if (!the_bar) {
		the_bar = new Skill_bar();
		the_bar->update(std::move(slots));
		gumps->add_gump(the_bar);
	} else {
		the_bar->update(std::move(slots));
	}
	// The mouse over a slot: its name, and its range around the user.
	int hovered = -1;
	if (!gumps->modal_gump_mode()) {
		const int mx = Mouse::mouse()->get_mousex();
		const int my = Mouse::mouse()->get_mousey();
		if (gumps->find_gump(mx, my) == the_bar) {
			int lx = mx;
			int ly = my;
			gumps->map_game_to_gump(the_bar, mx, my, lx, ly);
			if (auto* button = dynamic_cast<Skill_button*>(the_bar->on_button(lx, ly))) {
				hovered = button->get_index();
			}
		}
	}
	the_bar->set_hovered(hovered);
	if (const Bar_slot* slot = the_bar->slot(hovered)) {
		if (const Class_skill* skill = Skill_by_id(Class_of(actor), slot->skill)) {
			Show_circle(actor->get_npc_num(), *skill, 100);
		}
	}
}

/*
 *  Range circles: a faint disc with a border around the user, as the
 *  targeting range is drawn.  A few at once (one per user).
 */

struct Circle_layers {
	int fill   = -1;
	int border = -1;
	int radius = -1;    // Drawn for, in pixels.
	int color  = -1;
};
std::array<Circle_layers, 4> circle_layers;

void Paint_circles() {
	Game_window*  gwin = Gwin();
	Image_window* iwin = gwin->get_win();
	const uint32  now  = Now();
	circles.erase(std::remove_if(circles.begin(), circles.end(),
						  [now](const Range_circle& c) {
							  return c.until <= now;
						  }),
				  circles.end());
	size_t used = 0;
	if (Friend_class_show_ranges && Friend_class_active() && !gwin->is_in_exult_menu()) {
		for (const Range_circle& each : circles) {
			Actor* actor = gwin->get_npc(each.npc);
			if (used >= circle_layers.size() || !actor || !actor->get_chunk()) {
				continue;
			}
			Circle_layers& layers = circle_layers[used++];
			const int      radius = each.radius * c_tilesize;
			const int      size   = 2 * radius + 2;
			if (layers.fill < 0 || layers.radius != radius || layers.color != each.color) {
				for (int* layer : {&layers.fill, &layers.border}) {
					if (*layer >= 0) {
						gwin->destroy_layer(*layer);
						*layer = -1;
					}
				}
				constexpr int text_layer_z = (1 << 18) + (1 << 16);    // As effects.cc's text.
				layers.fill   = gwin->create_layer("Skill range fill", size, size, 255, 0, text_layer_z - 6);
				layers.border = gwin->create_layer("Skill range border", size, size, 255, 0, text_layer_z - 5);
				if (layers.fill < 0 || layers.border < 0) {
					return;
				}
				gwin->layer_set_alpha(layers.fill, 24);
				gwin->layer_set_alpha(layers.border, 150);
				Image_buffer8* fill  = gwin->get_layer_ibuf(layers.fill);
				Image_buffer8* line  = gwin->get_layer_ibuf(layers.border);
				if (!fill || !line) {
					return;
				}
				fill->fill8(255);
				line->fill8(255);
				const int inner = radius - 2;
				for (int py = 0; py < size; ++py) {
					const int dy = 2 * py + 1 - size;
					for (int px = 0; px < size; ++px) {
						const int dx = 2 * px + 1 - size;
						const int d2 = dx * dx + dy * dy;
						if (d2 <= 4 * inner * inner) {
							fill->put_pixel8(static_cast<unsigned char>(each.color), px, py);
						} else if (d2 <= 4 * radius * radius) {
							line->put_pixel8(static_cast<unsigned char>(each.color), px, py);
						}
					}
				}
				gwin->layer_set_dirty(layers.fill);
				gwin->layer_set_dirty(layers.border);
				layers.radius = radius;
				layers.color  = each.color;
			}
			int x;
			int y;
			gwin->get_shape_location(actor, x, y);
			const int cx = x - c_tilesize / 2;
			const int cy = y - c_tilesize / 2;
			int       sx0;
			int       sy0;
			int       sx1;
			int       sy1;
			iwin->game_to_screen(cx - size / 2, cy - size / 2, false, sx0, sy0);
			iwin->game_to_screen(cx + size / 2, cy + size / 2, false, sx1, sy1);
			for (const int layer : {layers.fill, layers.border}) {
				gwin->layer_set_dest(layer, sx0, sy0, sx1 - sx0, sy1 - sy0);
				gwin->layer_set_visible(layer, true);
			}
			gwin->set_painted();
		}
	}
	for (size_t i = used; i < circle_layers.size(); ++i) {
		for (const int layer : {circle_layers[i].fill, circle_layers[i].border}) {
			if (layer >= 0 && iwin->layer_is_visible(layer)) {
				gwin->layer_set_visible(layer, false);
				gwin->set_painted();
			}
		}
	}
}

/*
 *  Effect preview: steps through U7's sprite effects, missile pictures and
 *  sound effects, playing each on one's character, with its number, so a
 *  skill's look can be chosen by number.  Opened from the Class System page;
 *  a HUD panel at the top of the view, so the game keeps running.
 */

class Effect_preview;
Effect_preview* the_preview = nullptr;
bool            open_preview = false;    // Asked for from the options page.

enum class Preview_mode { sprites, missiles, sounds };

// The missile pictures weapons and ammunition fly as (shapes.vga).
const std::vector<int>& Missile_shapes() {
	static std::vector<int> shapes;
	if (shapes.empty()) {
		Vga_file& file = Shape_manager::get_instance()->get_file(SF_SHAPES_VGA);
		for (int shape = 0; shape < file.get_num_shapes(); ++shape) {
			const Shape_info& info = ShapeID::get_info(shape);
			if (const Weapon_info* winf = info.get_weapon_info()) {
				const int proj = winf->get_projectile();
				shapes.push_back(proj == -3 ? shape : proj);    // -3: the weapon itself flies.
			}
			if (const Ammo_info* ainf = info.get_ammo_info()) {
				const int sprite = ainf->get_sprite_shape();
				shapes.push_back(sprite == -3 ? shape : sprite);
			}
		}
		shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
							 [&file](int shape) {
								 return shape < 0 || shape >= file.get_num_shapes();
							 }),
					 shapes.end());
		std::sort(shapes.begin(), shapes.end());
		shapes.erase(std::unique(shapes.begin(), shapes.end()), shapes.end());
	}
	return shapes;
}

class Effect_preview : public Gump {
	using Button = CallbackTextButton<Effect_preview>;
	enum { b_mode, b_back10, b_back, b_play, b_next, b_next10, b_close, b_count };
	std::array<std::unique_ptr<Gump_button>, b_count> buttons;
	Preview_mode                                      mode  = Preview_mode::sprites;
	std::array<int, 3>                                place = {0, 0, 0};    // Each mode's position.
	TileRect                                          region{0, 0, 0, 0};
	static constexpr int                              width  = 196;
	static constexpr int                              height = 44;

	int count() const {
		switch (mode) {
		case Preview_mode::sprites:
			return Shape_manager::get_instance()->get_file(SF_SPRITES_VGA).get_num_shapes();
		case Preview_mode::missiles:
			return static_cast<int>(Missile_shapes().size());
		case Preview_mode::sounds: {
			Audio* audio = Audio::get_ptr();
			Flex*  sfx   = audio ? audio->get_sfx_file() : nullptr;
			return sfx ? static_cast<int>(sfx->number_of_objects()) : 0;
		}
		}
		return 0;
	}

	int& at() {
		return place[static_cast<int>(mode)];
	}

	// The number to put in a skill's definition.
	int number() {
		if (mode == Preview_mode::missiles) {
			const auto& shapes = Missile_shapes();
			return at() < static_cast<int>(shapes.size()) ? shapes[at()] : -1;
		}
		return at();
	}

	void step(int by) {
		const int n = count();
		if (n <= 0) {
			return;
		}
		at() = ((at() + by) % n + n) % n;
		play();
	}

	void back10() {
		step(-10);
	}

	void back() {
		step(-1);
	}

	void next() {
		step(1);
	}

	void next10() {
		step(10);
	}

	void switch_mode() {
		mode = mode == Preview_mode::sprites    ? Preview_mode::missiles
			   : mode == Preview_mode::missiles ? Preview_mode::sounds
												: Preview_mode::sprites;
		Gwin()->set_all_dirty();
	}

	void close_preview() {
		Gwin()->get_gump_man()->close_gump(this);
	}

public:
	Effect_preview() : Gump(nullptr, 0, 0, 0, SF_GUMPS_VGA) {
		buttons[b_mode]   = std::make_unique<Button>(this, &Effect_preview::switch_mode, "Kind", 4, 26, 34, 12);
		buttons[b_back10] = std::make_unique<Button>(this, &Effect_preview::back10, "<<", 40, 26, 20, 12);
		buttons[b_back]   = std::make_unique<Button>(this, &Effect_preview::back, "<", 62, 26, 20, 12);
		buttons[b_play]   = std::make_unique<Button>(this, &Effect_preview::play, "Play", 84, 26, 30, 12);
		buttons[b_next]   = std::make_unique<Button>(this, &Effect_preview::next, ">", 116, 26, 20, 12);
		buttons[b_next10] = std::make_unique<Button>(this, &Effect_preview::next10, ">>", 138, 26, 20, 12);
		buttons[b_close]  = std::make_unique<Button>(this, &Effect_preview::close_preview, "Close", 160, 26, 32, 12);
		place_panel();
	}

	~Effect_preview() override {
		if (the_preview == this) {
			the_preview = nullptr;
		}
		Gwin()->set_all_dirty();
	}

	bool is_persistent() const override {
		return true;
	}

	bool is_draggable() const override {
		return false;
	}

	bool is_hud() const override {
		return true;
	}

	TileRect get_rect() const override {
		return region;
	}

	TileRect get_dirty() override {
		return region;
	}

	bool has_point(int mx, int my) const override {
		return region.has_point(mx, my);
	}

	Gump_button* on_button(int mx, int my) override {
		for (auto& button : buttons) {
			if (Gump_button* found = button->on_button(mx, my)) {
				return found;
			}
		}
		return nullptr;
	}

	// Top centre of the view.
	void place_panel() {
		Image_window* iwin = Gwin()->get_win();
		const int     bx   = iwin->get_start_x() + (iwin->get_end_x() - iwin->get_start_x() - width) / 2;
		const int     by   = iwin->get_start_y() + 4;
		if (bx != region.x || by != region.y) {
			region = TileRect(bx, by, width, height);
			x      = bx;
			y      = by;
			Gwin()->set_all_dirty();
		}
	}

	// Play the current one on one's character.
	void play() {
		Game_window* gwin  = Gwin();
		Actor*       actor = gwin->get_controlled_actor() ? gwin->get_controlled_actor() : gwin->get_main_actor();
		const int    num   = number();
		if (!actor || num < 0) {
			return;
		}
		switch (mode) {
		case Preview_mode::sprites:
			gwin->get_effects()->add_effect(std::make_unique<Sprites_effect>(num, actor, 0, 0, 0, 0));
			break;
		case Preview_mode::missiles: {
			// At the pick, or six tiles the way the character faces.
			Actor*     pick = gwin->get_action_target();
			Tile_coord to   = actor->get_tile();
			if (pick) {
				to = pick->get_tile();
			} else {
				const int dir = actor->get_dir_facing();    // 0 north, clockwise, by eighths.
				static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
				static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
				to.tx += 6 * dx[dir & 7];
				to.ty += 6 * dy[dir & 7];
			}
			const Tile_coord from = actor->get_tile();
			gwin->get_effects()->add_effect(std::make_unique<Projectile_effect>(from, to, num, num, -1));
			Friend_mode_note_projectile(from, to, num, num, -1);
			break;
		}
		case Preview_mode::sounds:
			if (Audio* audio = Audio::get_ptr()) {
				audio->play_sound_effect(num, actor);
			}
			Friend_mode_note_sound(num, actor);
			break;
		}
		cout << "Friend Class: preview "
			 << (mode == Preview_mode::sprites ? "sprite " : mode == Preview_mode::missiles ? "missile " : "sound ") << num
			 << endl;
		gwin->set_all_dirty();
	}

	void paint() override {
		Shape_manager* sman = Shape_manager::get_instance();
		Image_buffer8* buf  = Gwin()->get_win()->get_ib8();
		int            sx   = 0;
		int            sy   = 0;
		local_to_screen(sx, sy);
		buf->fill8(0, width, height, sx, sy);
		const int line = sman->get_special_pixel(PROTECT_PIXEL);
		buf->fill8(line, width, 1, sx, sy);
		buf->fill8(line, width, 1, sx, sy + height - 1);
		buf->fill8(line, 1, height, sx, sy);
		buf->fill8(line, 1, height, sx + width - 1, sy);
		const int n   = count();
		const int num = number();
		char      text[80];
		switch (mode) {
		case Preview_mode::sprites:
			std::snprintf(text, sizeof(text), "Sprite %d  (%d of %d, %d frames)", num, at() + 1, n,
						  n > 0 ? sman->get_file(SF_SPRITES_VGA).get_num_frames(num) : 0);
			break;
		case Preview_mode::missiles:
			std::snprintf(text, sizeof(text), "Missile %d  (%d of %d)", num, at() + 1, n);
			break;
		case Preview_mode::sounds:
			if (n > 0) {
				std::snprintf(text, sizeof(text), "Sound %d  (%d of %d)", num, at() + 1, n);
			} else {
				std::snprintf(text, sizeof(text), "No sound effects installed");
			}
			break;
		}
		sman->paint_text(0, text, sx + 6, sy + 4);
		sman->paint_text(0, "Kind: sprites, missiles, sounds", sx + 6, sy + 14);
		for (auto& button : buttons) {
			button->paint();
		}
	}
};

void Update_preview() {
	Gump_manager* gumps = Gwin()->get_gump_man();
	if (the_preview) {
		bool open = false;
		for (auto it = gumps->begin(); it != gumps->end(); ++it) {
			open = open || *it == the_preview;
		}
		if (!open) {
			the_preview = nullptr;
		}
	}
	if (open_preview) {
		open_preview = false;
		if (!the_preview && !Gwin()->is_in_exult_menu()) {
			the_preview = new Effect_preview();
			gumps->add_gump(the_preview);
		}
	}
	if (the_preview) {
		the_preview->place_panel();
	}
}

/*
 *  The options page.
 */

class Class_options_gump : public Modal_gump {
	enum setting_ids { set_enabled, set_bar, set_ranges, set_ai, set_items, set_count };

	enum button_ids { id_ok, id_cancel, id_preview, id_first_setting, id_count = id_first_setting + set_count };

	std::array<int, set_count>                         settings{};
	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	static const char* label(int setting) {
		static const char* const labels[set_count]
				= {"Enable:", "Skill bar:", "Show skill ranges:", "Class AI:", "Require class items:"};
		return labels[setting];
	}

	// Guests: the game rules are the host's.
	static bool host_only(int setting) {
		return setting == set_enabled || setting == set_ai || setting == set_items;
	}

	bool shown(int id) const {
		if (id < id_first_setting) {
			return true;
		}
		const int setting = id - id_first_setting;
		if (Is_guest() && host_only(setting)) {
			return false;
		}
		return setting == set_enabled || settings[set_enabled] != 0 || Is_guest();
	}

	using Button = CallbackTextButton<Class_options_gump>;
	using Toggle = CallbackToggleTextButton<Class_options_gump>;

	template <int setting>
	void toggle(int state) {
		settings[setting] = state;
		if (setting == set_enabled) {
			Gwin()->set_all_dirty();
		}
	}

public:
	Class_options_gump() : Modal_gump(nullptr, -1) {
		SetProceduralBackground(TileRect(0, 0, 100, yForRow(set_count + 3)), -1);
		settings = {Friend_class_enabled ? 1 : 0, Friend_class_bar_position, Friend_class_show_ranges ? 1 : 0,
					Friend_class_ai ? 1 : 0, Friend_class_require_items ? 1 : 0};
		buttons[id_ok]     = std::make_unique<Button>(this, &Class_options_gump::close, GumpStrings::OK(), 25,
                                                  yForRow(set_count + 2), 50);
		buttons[id_cancel] = std::make_unique<Button>(this, &Class_options_gump::cancel, GumpStrings::CANCEL(), 75,
													  yForRow(set_count + 2), 50);
		buttons[id_preview] = std::make_unique<Button>(
				this, &Class_options_gump::preview, "Effect preview", 25, yForRow(set_count + 1), 100);
		const std::vector<std::string> yesNo = {GumpStrings::No(), GumpStrings::Yes()};
		const std::vector<std::string> places
				= {"Above status bar", "Bottom left", "Bottom right", "Top left", "Top right"};
		using Callback          = void (Class_options_gump::*)(int);
		Callback toggles[set_count] = {&Class_options_gump::toggle<0>, &Class_options_gump::toggle<1>,
				&Class_options_gump::toggle<2>, &Class_options_gump::toggle<3>, &Class_options_gump::toggle<4>};
		for (int i = 0; i < set_count; ++i) {
			buttons[id_first_setting + i] = std::make_unique<Toggle>(this, std::move(toggles[i]),
					i == set_bar ? places : yesNo, settings[i], get_button_pos_for_label(label(i)), yForRow(i),
					i == set_bar ? 85 : 44);
		}
		ResizeWidthToFitWidgets(tcb::span(buttons.data(), id_count));
		HorizontalArrangeWidgets(tcb::span(buttons.data() + id_ok, 2));
		RightAlignWidgets(tcb::span(buttons.data() + id_first_setting, set_count));
	}

	void close() override {
		if (!Is_guest()) {
			Friend_class_enabled       = settings[set_enabled] != 0;
			Friend_class_ai            = settings[set_ai] != 0;
			Friend_class_require_items = settings[set_items] != 0;
		}
		Friend_class_bar_position = settings[set_bar];
		Friend_class_show_ranges  = settings[set_ranges] != 0;
		Friend_class_save_settings();
		done = true;
	}

	void cancel() {
		done = true;
	}

	// Close the page and open the effect preview panel.
	void preview() {
		open_preview = true;
		done         = true;
	}

	Gump_button* on_button(int mx, int my) override {
		for (int id = 0; id < id_count; ++id) {
			auto& btn   = buttons[id];
			auto  found = btn && shown(id) ? btn->on_button(mx, my) : nullptr;
			if (found) {
				return found;
			}
		}
		return Modal_gump::on_button(mx, my);
	}

	void paint() override {
		Modal_gump::paint();
		for (int id = 0; id < id_count; ++id) {
			if (buttons[id] && shown(id)) {
				buttons[id]->paint();
			}
		}
		Image_window8* iwin = Gwin()->get_win();
		for (int i = 0; i < set_count; ++i) {
			if (shown(id_first_setting + i)) {
				font->paint_text(iwin->get_ib8(), label(i), x + label_margin, y + yForRow(i) + 1);
			}
		}
		if (Is_guest()) {
			font->paint_text(iwin->get_ib8(), "The host sets the rest.", x + label_margin,
							 y + yForRow(set_count) + 1);
		}
		Gwin()->set_painted();
	}
};

}    // namespace

/*
 *  The hooks.
 */

bool Friend_class_active() {
	return Friend_class_enabled && GAME_BG;
}

void Friend_class_load_settings() {
	config->value("config/friend/class/enabled", Friend_class_enabled, false);
	config->value("config/friend/class/bar_position", Friend_class_bar_position, friend_bar_above_status);
	config->value("config/friend/class/show_ranges", Friend_class_show_ranges, true);
	config->value("config/friend/class/ai", Friend_class_ai, true);
	config->value("config/friend/class/require_items", Friend_class_require_items, false);
	if (Friend_class_bar_position < 0 || Friend_class_bar_position >= friend_bar_place_count) {
		Friend_class_bar_position = friend_bar_above_status;
	}
}

void Friend_class_save_settings() {
	auto put = [](const char* key, bool on) {
		config->set(key, on ? "yes" : "no", false);
	};
	// A guest's game rules are the host's, so only the host keeps its own.
	if (!Is_guest()) {
		put("config/friend/class/enabled", Friend_class_enabled);
		put("config/friend/class/ai", Friend_class_ai);
		put("config/friend/class/require_items", Friend_class_require_items);
	}
	config->set("config/friend/class/bar_position", Friend_class_bar_position, false);
	put("config/friend/class/show_ranges", Friend_class_show_ranges);
	config->write_back();
	rules_changed = true;
	Gwin()->set_all_dirty();
}

void Friend_class_options() {
	Class_options_gump options;
	Gwin()->get_gump_man()->do_modal_gump(&options, Mouse::hand);
}

void Friend_class_frame() {
	if (!Gwin()->get_main_actor()) {
		return;
	}
	if (!Is_guest()) {
		Expire();
		if (Friend_class_active()) {
			Class_ai();
		}
		if (Is_host()) {
			Send_states();
		}
	}
	Update_bar();
	Update_preview();
	Paint_circles();
}

void Friend_class_world_loaded() {
	cooldowns.clear();
	global_waits.clear();
	timed_flags.clear();
	bonuses.clear();
	circles.clear();
	sent_states.clear();
}

bool Friend_class_host_blob(Friend_mode_session::Blob_kind kind, const std::vector<unsigned char>& data, int player) {
	if (kind != Friend_mode_session::Blob_kind::class_use) {
		return false;
	}
	if (data.size() < 2) {
		return true;
	}
	const int id    = data[0] | (data[1] << 8);
	Actor*    actor = nullptr;
	for (Actor* member : Party()) {
		if (Friend_mode_player_playing(member) == player) {
			actor = member;
		}
	}
	const Companion_class* cls   = Class_of(actor);
	const Class_skill*     skill = Skill_by_id(cls, id);
	std::string            why   = "That skill isn't yours";
	if (skill && Perform(actor, *cls, *skill, why)) {
		return true;
	}
	cout << "Friend Class: refused a guest's skill (" << why << ')' << endl;
	Friend_mode_get_session().send_blob(
			Friend_mode_session::Blob_kind::class_refused, std::vector<unsigned char>(why.begin(), why.end()), player);
	return true;
}

bool Friend_class_guest_blob(Friend_mode_session::Blob_kind kind, const std::vector<unsigned char>& data) {
	switch (kind) {
	case Friend_mode_session::Blob_kind::class_state:
		Take_state(data);
		return true;
	case Friend_mode_session::Blob_kind::class_event:
		Take_event(data);
		return true;
	case Friend_mode_session::Blob_kind::class_refused: {
		std::string text;
		for (const unsigned char chr : data) {
			if (chr >= 32 && chr < 127) {
				text += static_cast<char>(chr);
			}
		}
		Friend_mode_class_notice(text);
		return true;
	}
	default:
		return false;
	}
}

void Friend_class_guest_joined(int player) {
	sent_states.erase(player);
}

int Friend_class_bonus(const Actor* actor, int prop) {
	if (bonuses.empty() || (prop != Actor::strength && prop != Actor::combat)) {
		return 0;
	}
	const uint32 now   = Now();
	int          total = 0;
	for (const Timed_bonus& each : bonuses) {
		if (each.until > now) {
			const Game_object_shared obj = each.actor.lock();
			if (obj.get() == actor) {
				total += each.amount;
			}
		}
	}
	return total;
}
