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
#include "chunks.h"
#include "game.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "palette.h"
#include "items.h"
#include "monsters.h"
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
#include <functional>
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

// The Class System's clock: real time that stops while the game is paused
// (menus, conversations, Exult's pause), so cooldowns, buffs and fields
// wait too.  Ticked every frame (Tick_clock); a frame that took a long time
// means the main loop was held up by a menu or a conversation.
uint32 clock_offset = 0;
uint32 clock_last   = 0;

uint32 Now() {
	return static_cast<uint32>(SDL_GetTicks()) - clock_offset;
}

void Tick_clock(bool paused) {
	const uint32 raw = static_cast<uint32>(SDL_GetTicks());
	if (clock_last != 0) {
		const uint32 gap = raw - clock_last;
		if (paused || gap > 250) {
			clock_offset += gap;
		}
	}
	clock_last = raw;
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
	ally,              // The party member in range with the least health left (the user too).
};

enum class Skill_effect {
	heal,      // Health up by 'amount', capped at strength.
	cure,      // Clears poison, paralysis and curse.
	flag,      // Sets 'flag' (protection, invisible), with U7's own timer.
	hinder,    // Paralyzes or knocks out for 'duration_ms', after a roll.
	volley,    // A real shot with the readied weapon at each target.
	bonus,     // +'amount' strength and combat for 'duration_ms'.
	armor,     // +'amount' armour for 'duration_ms' (Quick Fix, Sanctuary).
	mark,      // The target takes +'amount' damage from the party for 'duration_ms'.
	stance,    // Riposte: for 'duration_ms', a melee strike that misses the user is answered.
	bomb,      // Thrown at the pick; after 'fuse_ms', hostiles within 'blast' are hit 'hits' times.
	flash,     // Thrown at the pick; hostiles within 'blast' are hindered ('flag', 'duration_ms').
	field,     // At the user's feet: the party within 'range' heals 'amount' every second for 'duration_ms'.
	summon,    // A friendly 'summon_shape' fights beside the party for 'duration_ms'.
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
	int          duration_ms = 0;     // How long it lasts, for the timed effects.
	int          rider_flag  = -1;    // Volley: then hinder each target hit ('then_hinder').
	int          rider_ms    = 0;
	int          weaken_prop = -1;    // Volley: then lower a stat ('then_weaken').
	int          weaken_by   = 0;
	int          weaken_ms   = 0;
	int          pierce      = 0;     // Armour the user's strikes ignore meanwhile ('pierce_armor').
	int          blast       = 0;     // 'bomb' and 'flash': radius around the pick, in tiles.
	int          fuse_ms     = 0;     // 'bomb': time from landing to the blast.
	int          hits        = 1;     // 'bomb': how many blasts each hostile takes.
	int          summon_shape = -1;   // 'summon': the creature's shape.
	const char*  reaction     = nullptr;    // Over each foe it lands on ("*stunned*"); null: by effect.
	Skill_look   look;
	const char*  words = "";    // Over the user.
	const char*  description = "";    // For the bar.

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

	// Volley riders, applied as the shot lands (each target rolls for
	// 'then_hinder').
	Class_skill& then_hinder(int f, int ms) {
		rider_flag = f;
		rider_ms   = ms;
		return *this;
	}

	Class_skill& then_weaken(int prop, int by, int ms) {
		weaken_prop = prop;
		weaken_by   = by;
		weaken_ms   = ms;
		return *this;
	}

	Class_skill& pierce_armor(int points) {
		pierce = points;
		return *this;
	}

	Class_skill& armor(int points, int ms) {
		effect      = Skill_effect::armor;
		amount      = points;
		duration_ms = ms;
		return *this;
	}

	Class_skill& mark(int extra, int ms) {
		effect      = Skill_effect::mark;
		amount      = extra;
		duration_ms = ms;
		return *this;
	}

	Class_skill& stance(int ms) {
		effect      = Skill_effect::stance;
		duration_ms = ms;
		return *this;
	}

	Class_skill& bomb(int radius, int fuse, int blasts) {
		effect  = Skill_effect::bomb;
		blast   = radius;
		fuse_ms = fuse;
		hits    = blasts;
		return *this;
	}

	Class_skill& flash(int radius, int f, int ms) {
		effect      = Skill_effect::flash;
		blast       = radius;
		flag        = f;
		duration_ms = ms;
		return *this;
	}

	Class_skill& field(int health_per_second, int ms) {
		effect      = Skill_effect::field;
		amount      = health_per_second;
		duration_ms = ms;
		return *this;
	}

	Class_skill& summon(int shape, int ms) {
		effect       = Skill_effect::summon;
		summon_shape = shape;
		duration_ms  = ms;
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

	// The emote over each foe it lands on, instead of the effect's own
	// ("*stunned*", "*asleep*", "*crippled*"...).
	Class_skill& reacts(const char* text) {
		reaction = text;
		return *this;
	}

	// One line for the bar, under the name: what it does.
	Class_skill& describe(const char* text) {
		description = text;
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
constexpr int sprite_small_blast = 5;
constexpr int sprite_teleport    = 7;
constexpr int sprite_fireworks   = 12;
constexpr int sprite_burst_arrow = 19;
constexpr int sprite_purple      = 21;
constexpr int sprite_sword       = 23;
constexpr int sprite_poof     = 9;
constexpr int sprite_bubbles  = 13;
constexpr int sprite_barrier  = 15;
constexpr int sprite_sparkles = 16;
constexpr int sprite_beads    = 18;
constexpr int sprite_music    = 24;

// Shapes (shapes.vga).
constexpr int shape_dog        = 496;    // Katrina's summoned sheepdog.
constexpr int shape_powder_keg = 704;    // Julia's Time Bomb: flies, and its blast.

// Spell numbers (circle * 8 + place in the circle), checked against Black
// Gate's own spell scripts (0x640 + number, by their words of power; the
// Keyring mod's lists are in another order): the spellbook's pictures, used
// as icons, and the Avatar's spell bar.  Skills never run a spell's script.
constexpr int spell_fireworks    = 3;
constexpr int spell_fire_blast   = 18;
constexpr int spell_mass_cure    = 20;
constexpr int spell_protection   = 21;
constexpr int spell_heal         = 25;
constexpr int spell_paralyze     = 28;
constexpr int spell_protect_all  = 27;
constexpr int spell_sleep        = 31;
constexpr int spell_lightning    = 33;
constexpr int spell_dance        = 41;
constexpr int spell_great_heal   = 44;
constexpr int spell_invisibility = 45;
constexpr int spell_cause_fear   = 48;
constexpr int spell_death_bolt   = 57;
constexpr int spell_mass_might   = 62;
constexpr int spell_swordstrike  = 70;
constexpr int spell_curse        = 24;
constexpr int spell_mass_curse   = 35;
constexpr int spell_explosion    = 43;
constexpr int spell_delayed      = 58;
constexpr int spell_restoration  = 63;
constexpr int spell_summon       = 69;
constexpr int spell_protect      = 21;
constexpr int spell_flame_strike = 51;
constexpr int spell_tremor       = 55;

// Each companion's class and skills.  Levels: the first at 1, the next at
// 3, the last at 6 (U7's levels: 1 + log2(experience / 50)).
// Cooldowns: 15 s for a heal on one, 30 s for group heals and single
// shots or holds, 60 s (the default) for big damage and support.
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
							 .describe("Protection for the party in the circle")
							 .icon_spell(spell_protect_all)
							 .kind(T::support)
							 .aim(G::party_area, 8, true)
							 .set_flag(Obj_flags::protection)
							 .targets(2)
							 .on_targets(sprite_music)
							 .says("*sings a song of warding*"),
					 Class_skill(10, "Battle Hymn")
							 .describe("The party nearby hits harder and has more health")
							 .icon_spell(spell_mass_might)
							 .kind(T::support)
							 .aim(G::party_area, 8, true)
							 .unlock(3)
							 .bonus(3, 30000)
							 .targets(3)
							 .on_targets(sprite_music)
							 .says("*plays a battle hymn*"),
					 // Hostiles in the circle drowse for a few seconds (each rolls).
					 Class_skill(12, "Song of Peace")
							 .describe("Hostiles in the circle drowse a few seconds")
							 .icon_spell(spell_dance)
							 .kind(T::control)
							 .aim(G::hostiles_nearest, 8, true)
							 .unlock(6)
							 .hinder(Obj_flags::asleep, 5000)
							 .reacts("*drowsy*")
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
							 .describe("Arrows at the three nearest hostiles")
							 .icon_spell(spell_swordstrike)
							 .kind(T::attack)
							 .aim(G::hostiles_nearest, 12)
							 .volley()
							 .targets(3)
							 .says("*looses a volley*"),
					 Class_skill(21, "Aimed Shot")
							 .describe("One careful shot that hits harder")
							 .icon_spell(spell_death_bolt)
							 .kind(T::attack)
							 .aim(G::pick, 14)
							 .unlock(3)
							 .volley(6, 3000)
							 .says("*takes careful aim*"),
					 Class_skill(22, "Vanish")
							 .describe("Invisible for a moment; foes lose him")
							 .icon_spell(spell_invisibility)
							 .kind(T::self)
							 .unlock(6)
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
							 .describe("Heals everyone in the circle")
							 .recharge(30)
							 .icon_spell(spell_great_heal)
							 .kind(T::heal)
							 .aim(G::party_area, 8, true)
							 .heal(10)
							 .targets(2)
							 .on_targets(sprite_sparkles)
							 .says("*calls on the earth to mend*"),
					 Class_skill(31, "Purify")
							 .describe("Cures poison, paralysis and curses nearby")
							 .recharge(30)
							 .icon_spell(spell_mass_cure)
							 .kind(T::cure)
							 .aim(G::party_area, 8, true)
							 .unlock(3)
							 .cure()
							 .on_targets(sprite_bubbles)
							 .says("*purifies the air*"),
					 Class_skill(32, "Entangle")
							 .describe("Roots the target in place for six seconds")
							 .recharge(30)
							 .icon_spell(spell_paralyze)
							 .kind(T::control)
							 .aim(G::pick, 10)
							 .unlock(6)
							 .hinder(Obj_flags::paralyzed, 6000)
							 .reacts("*entangled*")
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
							 .describe("Protection for Dupre himself")
							 .recharge(30)
							 .icon_spell(spell_protection)
							 .kind(T::self)
							 .set_flag(Obj_flags::protection)
							 .on_targets(sprite_barrier)
							 .says("*stands fast*"),
					 Class_skill(41, "Lay on Hands")
							 .describe("A big heal on the most hurt nearby")
							 .recharge(15)
							 .icon_spell(spell_heal)
							 .kind(T::heal)
							 .aim(G::hurt_party, 3)
							 .unlock(3)
							 .heal(25)
							 .on_targets(sprite_sparkles)
							 .says("*lays on hands*"),
					 Class_skill(42, "Challenge")
							 .describe("Hostiles in the circle turn on Dupre")
							 .icon_spell(spell_cause_fear)
							 .kind(T::control)
							 .aim(G::hostiles_nearest, 6, true)
							 .unlock(6)
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
					 // A stone that stops the target in its tracks (decided 2026-10-20).
					 Class_skill(50, "Stun Shot")
							 .describe("A stone that stops the target for three seconds")
							 .recharge(30)
							 .icon_spell(spell_paralyze)
							 .kind(T::control)
							 .aim(G::pick, 10)
							 .volley()
							 .then_hinder(Obj_flags::paralyzed, 3000)
							 .says("*lets fly a stone*"),
					 Class_skill(51, "Trick Shot")
							 .describe("Stones at the two nearest hostiles")
							 .icon_spell(spell_fireworks)
							 .kind(T::attack)
							 .aim(G::hostiles_nearest, 10)
							 .unlock(3)
							 .volley()
							 .targets(2)
							 .says("*tries a trick shot*"),
			 }},
			// Agile melee duelist: counterattacks, speed, precision.
			{"Sentri",
			 "Swordmaster",
			 Placement::front,
			 -1,
			 "Sentri's Blade",
			 {
					 Class_skill(60, "Riposte")
							 .describe("For ten seconds, answers every melee miss")
							 .recharge(30)
							 .icon_spell(spell_swordstrike)
							 .kind(T::self)
							 .stance(10000)
							 .on_user(sprite_barrier)
							 .on_targets(sprite_sword)    // On each foe answered.
							 .says("*takes a dueling stance*"),
					 Class_skill(61, "Blade Dance")
							 .describe("A strike at every foe beside him")
							 .icon_spell(spell_tremor)
							 .kind(T::attack)
							 .aim(G::hostiles_nearest, 1)
							 .unlock(3)
							 .volley()
							 .targets(8)
							 .on_targets(sprite_sword)
							 .says("*whirls into a blade dance*"),
					 Class_skill(62, "Master's Strike")
							 .describe("A precise blow that ignores some armour")
							 .icon_spell(spell_flame_strike)
							 .kind(T::attack)
							 .aim(G::pick, 1)
							 .unlock(6)
							 .volley(6, 2000)
							 .pierce_armor(6)
							 .on_targets(sprite_fireworks)
							 .says("*strikes with perfect precision*"),
			 }},
			// Tactical archer: disabling foes and weakening priority targets.
			{"Tseramed",
			 "Huntsman",
			 Placement::ranged,
			 -1,
			 "Tseramed's Bow",
			 {
					 Class_skill(70, "Sleep Arrow")
							 .describe("An arrow that puts the target to sleep")
							 .recharge(30)
							 .icon_spell(spell_sleep)
							 .kind(T::control)
							 .aim(G::pick, 14)
							 .volley()
							 .then_hinder(Obj_flags::asleep, 5000)
							 .says("*nocks a sleeping arrow*"),
					 // U7 has no slowed walking: the target attacks less often.
					 Class_skill(71, "Crippling Shot")
							 .describe("An arrow that makes the target attack less")
							 .recharge(30)
							 .icon_spell(spell_curse)
							 .kind(T::control)
							 .aim(G::pick, 14)
							 .unlock(3)
							 .volley()
							 .then_weaken(Actor::dexterity, -10, 10000)
							 .on_targets(sprite_burst_arrow)
							 .says("*aims for the legs*"),
					 Class_skill(72, "Hunter's Mark")
							 .describe("The party's hits on the target do more")
							 .icon_spell(spell_mass_curse)
							 .kind(T::attack)
							 .aim(G::pick, 14)
							 .unlock(6)
							 .mark(4, 20000)
							 .on_targets(sprite_purple)
							 .says("*marks the quarry*"),
			 }},
			// Combat engineer: gadgets and explosives.
			{"Julia",
			 "Tinker",
			 Placement::ranged,
			 -1,
			 "Julia's Hammer",
			 {
					 // One ally, a strong boost (Sanctuary: everyone, smaller).
					 Class_skill(80, "Quick Fix")
							 .describe("Much more armour for the weakest ally nearby")
							 .recharge(30)
							 .icon_spell(spell_protect)
							 .kind(T::support)
							 .aim(G::ally, 4)
							 .armor(5, 30000)
							 .targets(1)
							 .on_targets(sprite_barrier)
							 .says("*tightens a few straps*"),
					 Class_skill(81, "Flashbang")
							 .describe("Thrown; hostiles where it lands are stunned")
							 .icon_spell(spell_explosion)
							 .kind(T::control)
							 .aim(G::pick, 10)
							 .unlock(3)
							 .flash(3, Obj_flags::paralyzed, 2500)
							 .missile(shape_powder_keg)
							 .on_targets(sprite_fireworks)
							 .says("*throws a flashbang*"),
					 Class_skill(82, "Time Bomb")
							 .describe("Thrown; a few seconds later, it explodes")
							 .icon_spell(spell_delayed)
							 .kind(T::attack)
							 .aim(G::pick, 10)
							 .unlock(6)
							 .bomb(3, 3000, 2)
							 .missile(shape_powder_keg)
							 .targets(2)
							 .says("*lights a fuse*"),
			 }},
			// Gentle support: healing over time, a summoned friend, protection.
			{"Katrina",
			 "Shepherd",
			 Placement::support,
			 -1,
			 "Katrina's Crook",
			 {
					 Class_skill(90, "Healing Field")
							 .describe("A field that heals whoever stands in it")
							 .recharge(30)
							 .icon_spell(spell_restoration)
							 .kind(T::heal)
							 .aim(G::party_area, 4, true)
							 .field(2, 12000)
							 .targets(2)
							 .on_targets(sprite_sparkles)
							 .says("*calls down a healing field*"),
					 Class_skill(91, "Summon Creature")
							 .describe("Her sheepdog fights beside you a while")
							 .icon_spell(spell_summon)
							 .kind(T::support)
							 .unlock(3)
							 .summon(shape_dog, 30000)
							 .targets(2)
							 .on_targets(sprite_teleport)
							 .says("*whistles for her sheepdog*"),
					 Class_skill(92, "Sanctuary")
							 .describe("More armour for everyone in the circle")
							 .icon_spell(spell_protect_all)
							 .kind(T::support)
							 .aim(G::party_area, 6, true)
							 .unlock(6)
							 .armor(2, 20000)
							 .targets(2)
							 .on_targets(sprite_barrier)
							 .says("*prays for sanctuary*"),
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
	return skill.effect != Skill_effect::calm;
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

// Timed bonuses (and penalties): U7 stats, and a few of our own.
enum Bonus_kind {
	bonus_armor        = 1001,    // Added to armour (Quick Fix, Sanctuary).
	bonus_damage_taken = 1002,    // Extra damage from the party (Hunter's Mark).
	bonus_pierce       = 1003,    // Armour its strikes ignore (Master's Strike).
	bonus_riposte      = 1004,    // Answers melee strikes that miss (Riposte).
};

struct Timed_bonus {
	Game_object_weak actor;
	int              prop;    // Actor::strength, combat, dexterity, or a Bonus_kind.
	int              amount;
	uint32           until;
	int              skill = -1;    // The skill that gave it (for Riposte's look).
};
std::vector<Timed_bonus> bonuses;    // Host.

int Bonus_of(const Actor* actor, int prop) {
	if (bonuses.empty() || !actor) {
		return 0;
	}
	const uint32 now   = Now();
	int          total = 0;
	for (const Timed_bonus& each : bonuses) {
		if (each.prop == prop && each.until > now) {
			const Game_object_shared obj = each.actor.lock();
			if (obj.get() == actor) {
				total += each.amount;
			}
		}
	}
	return total;
}

// Things to do a moment later: a shot landing, a fuse, a field's tick, a
// summoned creature leaving.  Host only; cleared with the world.
struct Pending {
	uint32                at;
	std::function<void()> run;
};
std::vector<Pending> pending;

std::vector<Game_object_weak> summons;    // Host: summoned creatures still about.

// The pick a guest sent with its skill (the Target System's Q), for the
// skill being carried out now.
Game_object_weak chosen_pick;

void Later(uint32 ms, std::function<void()> run) {
	pending.push_back({Now() + ms, std::move(run)});
}

struct Timed_taunt {
	Game_object_weak foe;
	Game_object_weak user;    // Who the foe must fight.
	uint32           until;
};
std::vector<Timed_taunt> taunts;    // Host: Challenge.

struct Range_circle {
	int        npc;
	int        radius;    // Tiles.
	int        color;     // Palette index.
	uint32     until;
	Tile_coord at{-1, -1, -1};    // A field's spot; else around the NPC.
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

// U7's maximum health is strength, so a strength bonus raises it too.
int Max_health(const Actor* actor) {
	return std::max(1, actor->get_property(Actor::strength) + Bonus_of(actor, Actor::strength));
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
	if (Is_guest() && Gwin()->network_input_blocked()) {
		why = "Wait for the Avatar";
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
	case Skill_target::ally: {
		Actor* weakest = nullptr;
		for (Actor* member : Party_in_range(user, skill.range)) {
			if (!weakest || Health(member) * Max_health(weakest) < Health(weakest) * Max_health(member)) {
				weakest = member;
			}
		}
		if (weakest) {
			return {weakest};
		}
		return {};
	}
	case Skill_target::pick: {
		// The guest's own pick first, then the companion's foe.
		if (const Game_object_shared chosen = chosen_pick.lock()) {
			Actor* foe = chosen->as_actor();
			if (foe && Friend_mode_is_hostile(user, foe) && user->distance(foe) <= skill.range) {
				return {foe};
			}
		}
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
							  return c.npc == npc && c.at.tx < 0;    // Fields stay.
						  }),
				  circles.end());
	circles.push_back({npc, skill.range, Circle_color(skill), Now() + ms});
}

// A field's circle stays on its spot for as long as it lasts.
void Show_field(int npc, const Class_skill& skill, const Tile_coord& spot) {
	Range_circle circle{npc, skill.range, Circle_color(skill), Now() + static_cast<uint32>(skill.duration_ms)};
	circle.at = spot;
	circles.push_back(circle);
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

void Add_bonus(Actor* actor, int prop, int amount, int ms, int skill = -1) {
	bonuses.push_back({weak_from_obj(actor), prop, amount, Now() + static_cast<uint32>(ms), skill});
}

// +'amount' strength and combat (Battle Hymn, Aimed Shot).
void Add_might(Actor* actor, int amount, int ms) {
	Add_bonus(actor, Actor::strength, amount, ms);
	Add_bonus(actor, Actor::combat, amount, ms);
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
	// A swing beside the foe, else the ready pose of a shot.
	const int pose = user->distance(target) <= 1 ? Actor::strike1_frame : Actor::ready_frame;
	user->change_frame(user->get_dir_framenum(user->get_direction(target), pose));
	return Combat_schedule::attack_target(user, target, target->get_tile(), weapon);
}

// Paralyze or put to sleep for 'ms', after a roll as hits with powers make
// (Actor::figure_hit_points).  Some creatures can't be: no flag.
// An emote-style line over someone a skill landed on (or didn't), on every
// machine, as emotes show over companions.
void React(Actor* actor, const char* text) {
	if (actor && text && text[0]) {
		Gwin()->get_effects()->add_text(text, actor);
		Friend_mode_note_speech(actor, text);
	}
}

const char* Hinder_reaction(int flag) {
	return flag == Obj_flags::asleep ? "*asleep*" : "*stunned*";
}

bool Try_hinder(Actor* user, Actor* each, int flag, int ms, const char* reaction = nullptr) {
	if (!each || each->is_dead()) {
		return false;
	}
	const bool had = each->get_flag(flag);
	if (!had && Actor::roll_to_win(user->get_effective_prop(Actor::combat), each->get_effective_prop(Actor::strength))) {
		each->set_flag(flag);
	}
	if (!had && each->get_flag(flag)) {
		timed_flags.push_back({weak_from_obj(each), flag, Now() + static_cast<uint32>(ms)});
		React(each, reaction ? reaction : Hinder_reaction(flag));
		return true;
	}
	React(each, "*resisted*");
	return false;
}

// Hostiles to 'user' within 'radius' tiles of 'spot'.
std::vector<Actor*> Hostiles_near(const Actor* user, const Tile_coord& spot, int radius) {
	Actor_vector near;
	Game_object::find_nearby_actors(near, spot, c_any_shapenum, radius);
	std::vector<Actor*> found;
	for (Actor* each : near) {
		if (Friend_mode_is_hostile(user, each) && each->distance(spot) <= radius) {
			found.push_back(each);
		}
	}
	return found;
}

// Roughly when a thrown thing or a shot lands.
uint32 Flight_ms(const Actor* user, const Tile_coord& spot) {
	return 300 + 60 * static_cast<uint32>(user->distance(spot));
}

void Sprite_at(const Tile_coord& spot, int sprite) {
	if (sprite >= 0) {
		Gwin()->get_effects()->add_effect(std::make_unique<Sprites_effect>(sprite, spot, 0, 0, 0, 0, -1));
	}
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
			if (Try_hinder(user, each, skill.flag, skill.duration_ms, skill.reaction)) {
				++done;
			}
		}
		break;
	case Skill_effect::volley:
		if (skill.amount > 0) {
			Add_might(user, skill.amount, skill.duration_ms);    // Aimed Shot: while it flies.
		}
		if (skill.pierce > 0) {
			Add_bonus(user, bonus_pierce, skill.pierce, std::max(skill.duration_ms, 1000));
		}
		for (Actor* each : targets) {
			if (!Shoot(user, each)) {
				continue;
			}
			++done;
			if (skill.rider_flag < 0 && skill.weaken_prop < 0) {
				continue;
			}
			// What rides on the shot, as it lands.
			const Game_object_weak foe_ref  = weak_from_obj(each);
			const Game_object_weak user_ref = weak_from_obj(user);
			const int              flag     = skill.rider_flag;
			const int              flag_ms  = skill.rider_ms;
			const int              prop     = skill.weaken_prop;
			const int              by       = skill.weaken_by;
			const int              by_ms    = skill.weaken_ms;
			const char*            reaction = skill.reaction;
			Later(Flight_ms(user, each->get_tile()), [=]() {
				const Game_object_shared foe_obj  = foe_ref.lock();
				const Game_object_shared user_obj = user_ref.lock();
				Actor* foe   = foe_obj ? foe_obj->as_actor() : nullptr;
				Actor* owner = user_obj ? user_obj->as_actor() : nullptr;
				if (!foe || !owner || foe->is_dead()) {
					return;
				}
				if (flag >= 0) {
					Try_hinder(owner, foe, flag, flag_ms, reaction);
				}
				if (prop >= 0) {
					Add_bonus(foe, prop, by, by_ms);
					React(foe, reaction ? reaction : (by < 0 ? "*crippled*" : "*emboldened*"));
				}
			});
		}
		if (done == 0) {
			why = skill.range <= 1 ? "Needs a weapon in hand" : "Needs a bow, crossbow or sling, and ammunition";
			return false;
		}
		break;
	case Skill_effect::bonus:
		// More strength, so more health: given now, taken back when it ends.
		for (Actor* each : targets) {
			Add_might(each, skill.amount, skill.duration_ms);
			each->set_property(Actor::health, std::min(Max_health(each), Health(each) + skill.amount));
			++done;
		}
		break;
	case Skill_effect::taunt:
		// Each hostile turns on the user, and is kept on it a while.
		for (Actor* each : targets) {
			each->set_target(user, true);
			React(each, skill.reaction ? skill.reaction : "*enraged*");
			taunts.push_back({weak_from_obj(each), weak_from_obj(user), Now() + static_cast<uint32>(skill.duration_ms)});
			++done;
		}
		break;
	case Skill_effect::armor:
		for (Actor* each : targets) {
			Add_bonus(each, bonus_armor, skill.amount, skill.duration_ms);
			++done;
		}
		break;
	case Skill_effect::mark:
		for (Actor* each : targets) {
			Add_bonus(each, bonus_damage_taken, skill.amount, skill.duration_ms);
			React(each, skill.reaction ? skill.reaction : "*marked*");
			++done;
		}
		break;
	case Skill_effect::stance:
		Add_bonus(user, bonus_riposte, 1, skill.duration_ms, skill.id);
		done = 1;
		break;
	case Skill_effect::flash:
	case Skill_effect::bomb: {
		// Thrown at the pick's spot; what it does happens where it lands.
		const Tile_coord       spot     = targets.front()->get_tile();
		const Game_object_weak user_ref = weak_from_obj(user);
		const Class_skill*     what     = &skill;    // The table lives for the game.
		const uint32           land     = Flight_ms(user, spot);
		if (skill.effect == Skill_effect::flash) {
			Later(land, [=]() {
				const Game_object_shared user_obj = user_ref.lock();
				Actor*                   owner    = user_obj ? user_obj->as_actor() : nullptr;
				if (!owner) {
					return;
				}
				Sprite_at(spot, what->look.on_targets);
				Sound_at(owner, what->look.sound);
				int dazed = 0;
				for (Actor* foe : Hostiles_near(owner, spot, what->blast)) {
					dazed += Try_hinder(owner, foe, what->flag, what->duration_ms,
									 what->reaction ? what->reaction : "*dazed*")
									 ? 1
									 : 0;
				}
				cout << "Friend Class: " << what->name << " dazed " << dazed << endl;
			});
		} else {
			// A few sparks while the fuse burns, then the blast.
			for (uint32 t = 1000; t < static_cast<uint32>(skill.fuse_ms); t += 1000) {
				Later(land + t, [=]() {
					Sprite_at(spot, sprite_small_blast);
				});
			}
			Later(land + static_cast<uint32>(skill.fuse_ms), [=]() {
				const Game_object_shared user_obj = user_ref.lock();
				Actor*                   owner    = user_obj ? user_obj->as_actor() : nullptr;
				// The keg's own blast picture and sound.
				const Shape_info& keg = ShapeID::get_info(shape_powder_keg);
				Sprite_at(spot, keg.get_explosion_sprite());
				const int boom = keg.get_explosion_sfx();
				Audio::get_ptr()->play_sound_effect(boom, spot, AUDIO_MAX_VOLUME);
				Friend_mode_note_sound(boom, nullptr, &spot);
				if (!owner) {
					return;
				}
				// Only hostiles: the party is spared.  The keg's own damage
				// (U7's numbers), through Actor::attacked.
				int hit = 0;
				for (Actor* foe : Hostiles_near(owner, spot, what->blast)) {
					for (int i = 0; i < what->hits && !foe->is_dead(); ++i) {
						foe->attacked(owner, shape_powder_keg, -1, true);
					}
					++hit;
				}
				cout << "Friend Class: " << what->name << " hit " << hit << endl;
			});
		}
		done = 1;
		break;
	}
	case Skill_effect::field: {
		// At the user's feet, ticking each second.
		const Tile_coord   spot = user->get_tile();
		const Class_skill* what = &skill;
		const int          map  = user->get_map_num();
		for (uint32 t = 1000; t <= static_cast<uint32>(skill.duration_ms); t += 1000) {
			const bool show = (t / 1000) % 2 == 0;    // Sparkles every other tick.
			Later(t, [=]() {
				for (Actor* member : Party()) {
					if (member->get_map_num() == map && member->distance(spot) <= what->range
						&& Health(member) < Max_health(member)) {
						member->set_property(Actor::health, std::min(Max_health(member), Health(member) + what->amount));
						if (show) {
							Sprite_on(member, what->look.on_targets);
						}
					}
				}
			});
		}
		Show_field(user->get_npc_num(), skill, spot);
		done = static_cast<int>(targets.size());
		break;
	}
	case Skill_effect::summon: {
		const Tile_coord spot = Map_chunk::find_spot(user->get_tile(), 3, skill.summon_shape, 0, 1);
		if (spot.tx < 0) {
			why = "No room";
			return false;
		}
		const Game_object_shared made
				= Monster_actor::create(skill.summon_shape, spot, Schedule::combat, Actor::good);
		if (!made) {
			why = "Nothing came";
			return false;
		}
		Sprite_at(spot, skill.look.on_targets);
		summons.push_back(made);
		const Game_object_weak creature = made;
		Later(static_cast<uint32>(skill.duration_ms), [=]() {
			if (const Game_object_shared obj = creature.lock()) {
				Actor* actor = obj->as_actor();
				if (actor && !actor->is_dead() && obj->get_chunk()) {
					Sprite_at(obj->get_tile(), sprite_poof);
					obj->remove_this();
				}
			}
		});
		done = 1;
		break;
	}
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
	const bool own_look = skill.effect == Skill_effect::flash || skill.effect == Skill_effect::bomb
						  || skill.effect == Skill_effect::field || skill.effect == Skill_effect::summon
						  || skill.effect == Skill_effect::stance;
	for (Actor* each : targets) {
		if (!own_look) {
			Sprite_on(each, skill.look.on_targets);
		}
		if (each != user) {
			Missile_to(user, each, skill.look.missile);
		}
	}
	if (skill.effect != Skill_effect::field) {    // A field has shown its own.
		Show_circle(user->get_npc_num(), skill, 2000);
	}
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

// Host: end short paralysis and knockdowns, and old bonuses; run what
// was put off.
void Expire() {
	const uint32 now = Now();
	if (!pending.empty()) {
		std::vector<Pending> due;
		for (auto it = pending.begin(); it != pending.end();) {
			if (it->at <= now) {
				due.push_back(std::move(*it));
				it = pending.erase(it);
			} else {
				++it;
			}
		}
		for (auto& each : due) {
			each.run();    // May add more.
		}
	}
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
	// Challenged foes stay on the challenger while it stands.
	taunts.erase(std::remove_if(taunts.begin(), taunts.end(),
						 [now](const Timed_taunt& each) {
							 const Game_object_shared foe_obj  = each.foe.lock();
							 const Game_object_shared user_obj = each.user.lock();
							 Actor* foe  = foe_obj ? foe_obj->as_actor() : nullptr;
							 Actor* user = user_obj ? user_obj->as_actor() : nullptr;
							 if (each.until <= now || !foe || !user || foe->is_dead() || user->is_dead()) {
								 return true;
							 }
							 if (foe->get_target() != user) {
								 foe->set_target(user, true);
							 }
							 return false;
						 }),
				 taunts.end());
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
	// Thrown things: worth it with enough hostiles where it lands.
	if (skill.effect == Skill_effect::bomb || skill.effect == Skill_effect::flash) {
		const std::vector<Actor*> picks = Targets(user, skill);
		return fighting && !picks.empty()
			   && static_cast<int>(Hostiles_near(user, picks.front()->get_tile(), skill.blast).size()) >= 2;
	}
	// Riposte: when something is at its throat.
	if (skill.effect == Skill_effect::stance) {
		return fighting && !Hostiles(user, 1).empty();
	}
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
 *  Class AI placing (host): where a companion nobody plays stands in a
 *  fight.  Called from the combat schedule before it closes in on a foe;
 *  true means a walk was started instead.
 */

constexpr int    keep_away_tiles  = 2;       // A foe this close: step away.
constexpr int    step_away_tiles  = 4;       // How far.
constexpr int    max_from_avatar  = 12;      // Never wander further than this.
constexpr uint32 place_period_ms  = 1500;    // Between placing walks, to shoot meanwhile.
std::map<int, uint32> next_place;            // By NPC.

// A ranged weapon readied, with what it needs.
bool Has_ranged_weapon(Actor* npc) {
	int                points = 0;
	int                weapon = -1;
	Game_object*       obj    = nullptr;
	const Weapon_info* winf   = npc->get_weapon(points, weapon, obj);
	return winf && winf->get_range() > 3;
}

// The middle of the party (the Avatar and the members on this map).
Tile_coord Party_middle(const Actor* npc) {
	int                 sx = 0;
	int                 sy = 0;
	int                 n  = 0;
	for (Actor* member : Party()) {
		if (member != npc && member->get_map() == npc->get_map()) {
			const Tile_coord t = member->get_tile();
			sx += t.tx;
			sy += t.ty;
			++n;
		}
	}
	const Tile_coord at = npc->get_tile();
	return n ? Tile_coord(sx / n, sy / n, at.tz) : at;
}

// Walk toward 'dest' (to within 'within' tiles), staying near the Avatar.
bool Walk_to(Actor* npc, Tile_coord dest, int within) {
	Actor* avatar = Gwin()->get_main_actor();
	if (avatar && avatar->get_map() == npc->get_map()) {
		const Tile_coord a = avatar->get_tile();
		dest.tx            = std::clamp<int>(dest.tx, a.tx - max_from_avatar, a.tx + max_from_avatar);
		dest.ty            = std::clamp<int>(dest.ty, a.ty - max_from_avatar, a.ty + max_from_avatar);
	}
	const Tile_coord spot = Map_chunk::find_spot(dest, 2, npc);
	if (spot.tx < 0 || spot == npc->get_tile()) {
		return false;
	}
	return npc->walk_path_to_tile(spot, Gwin()->get_std_delay(), 0, within) != 0;
}

// Step away from 'foe', toward the party's side.
bool Step_away(Actor* npc, const Actor* foe) {
	const Tile_coord at   = npc->get_tile();
	const Tile_coord from = foe->get_tile();
	int              dx   = at.tx - from.tx;
	int              dy   = at.ty - from.ty;
	if (dx == 0 && dy == 0) {
		dx = 1;
	}
	const int len = std::max(std::abs(dx), std::abs(dy));
	Tile_coord dest(at.tx + dx * step_away_tiles / len, at.ty + dy * step_away_tiles / len, at.tz);
	return Walk_to(npc, dest, 0);
}

bool Place(Actor* npc) {
	const Companion_class* cls = Class_of(npc);
	if (!cls || cls->placement == Placement::front) {
		return false;    // The front line fights as U7 does.
	}
	const uint32 now  = Now();
	auto&        next = next_place[npc->get_npc_num()];
	if (now < next) {
		return false;
	}
	const std::vector<Actor*> near = Hostiles(npc, 12);
	if (cls->placement == Placement::ranged) {
		// Keep a distance: a foe beside it means step away, then shoot.
		if (!near.empty() && npc->distance(near.front()) <= keep_away_tiles && Has_ranged_weapon(npc)
			&& Step_away(npc, near.front())) {
			next = now + place_period_ms;
			cout << "Friend Class: " << npc->get_npc_name() << " keeps its distance" << endl;
			return true;
		}
		return false;
	}
	// Support: walk into range of whoever needs healing most; otherwise
	// keep out of the front, near the middle of the party.
	const Class_skill* heal_skill = nullptr;
	for (const auto& skill : cls->skills) {
		if (skill.type == Skill_type::heal && npc->get_level() >= skill.level && !Seconds_left(npc->get_npc_num(), skill.id)) {
			heal_skill = &skill;
			break;
		}
	}
	if (heal_skill) {
		// As many hurt as the heal waits for (Mending Circle: two), so it
		// is used on arrival.
		Actor* worst = nullptr;
		int    hurt  = 0;
		for (Actor* member : Party()) {
			if (member->get_map() == npc->get_map() && Health(member) * 2 < Max_health(member)) {
				++hurt;
				if (member != npc
					&& (!worst || Health(member) * Max_health(worst) < Health(worst) * Max_health(member))) {
					worst = member;
				}
			}
		}
		if (worst && hurt >= std::max(1, heal_skill->count) && npc->distance(worst) > heal_skill->range
			&& Walk_to(npc, worst->get_tile(), std::max(1, heal_skill->range - 2))) {
			next = now + place_period_ms;
			cout << "Friend Class: " << npc->get_npc_name() << " moves to heal " << worst->get_npc_name() << endl;
			return true;
		}
	}
	if (!near.empty() && npc->distance(near.front()) <= keep_away_tiles) {
		const Tile_coord middle = Party_middle(npc);
		if (npc->distance(middle) > 2 && Walk_to(npc, middle, 1)) {
			next = now + place_period_ms;
			cout << "Friend Class: " << npc->get_npc_name() << " falls back to the party" << endl;
			return true;
		}
	}
	return false;
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
	if (skill && actor && skill->effect == Skill_effect::field) {
		Show_field(npc, *skill, actor->get_tile());
	} else if (skill) {
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

const char* Spell_description(int spell) {
	switch (spell) {
	case spell_great_heal:
		return "Heals fully: the Avatar, or the party member picked";
	case spell_fire_blast:
		return "A bolt of fire at the pick";
	case spell_lightning:
		return "Lightning at the pick";
	default:
		return "";
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
	std::string description;
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
			slot.description = Spell_description(spell);
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
		slot.description = skill.description;
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
	cout << "Friend Class: bar click on " << slot.name << (slot.usable ? "" : " (not usable: " + slot.why + ")") << endl;
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
		// The skill, and the Target System's pick (kind 2: none).
		std::vector<unsigned char> request = {static_cast<unsigned char>(skill->id & 0xff),
											  static_cast<unsigned char>(skill->id >> 8)};
		int    kind = 2;
		uint32 id   = 0;
		if (!Friend_mode_actor_net_id(gwin->get_action_target(), kind, id)) {
			kind = 2;
		}
		request.push_back(static_cast<unsigned char>(kind));
		for (int b = 0; b < 4; ++b) {
			request.push_back(static_cast<unsigned char>(id >> (8 * b)));
		}
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

	// "Draggable" stays on: Exult's click handling (Dragging_info) only
	// carries a button's press through to its release when it is.

	int get_index() const {
		return index;
	}
};

constexpr int slot_gap    = 2;
constexpr int slot_pad    = 2;
constexpr int name_height = 20;    // Two lines over the bar: the hovered skill's name and what it does.

// The bar: a HUD gump, drawn into its own layer at the HUD's scale (as the
// shortcut bar and the party's face stats are).
class Skill_bar : public Gump {
	std::vector<Bar_slot>                      slots;
	std::vector<std::unique_ptr<Skill_button>> buttons;
	TileRect                                   region{0, 0, 0, 0};
	int                                        slot_w  = 0;
	int                                        slot_h  = 0;
	int                                        bar_w   = 1;    // The icons, or the widest line over them.
	int                                        icons_x = 0;    // Where the icons start in it.
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

	// Only the icons take clicks; the lines over them don't hide the world.
	bool has_point(int mx, int my) const override {
		const int icons_w = static_cast<int>(slots.size()) * (slot_w + slot_gap) - slot_gap;
		return TileRect(region.x + icons_x, region.y + name_height, icons_w, slot_h).has_point(mx, my);
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
		Shape_manager* sman    = Shape_manager::get_instance();
		const int      n       = static_cast<int>(slots.size());
		const int      icons_w = std::max(1, n * slot_w + (n - 1) * slot_gap);
		int            text_w  = 0;
		for (const auto& each : slots) {
			text_w = std::max({text_w, sman->get_text_width(0, each.name.c_str()),
							   sman->get_text_width(0, each.description.c_str())});
		}
		bar_w   = std::max(icons_w, text_w + 4);
		icons_x = (bar_w - icons_w) / 2;
		for (size_t i = 0; i < slots.size(); ++i) {
			ShapeID      id(33 + slots[i].icon % 8, slots[i].icon / 8, SF_GUMPS_VGA);
			Shape_frame* frame = id.get_shape();
			const int    sx    = icons_x + static_cast<int>(i) * (slot_w + slot_gap);
			const int    px    = sx + (frame ? (slot_w - frame->get_width()) / 2 + frame->get_xleft() : 0);
			const int    py    = name_height + (frame ? (slot_h - frame->get_height()) / 2 + frame->get_yabove() : 0);
			buttons.push_back(std::make_unique<Skill_button>(this, slots[i].icon, px, py, static_cast<int>(i)));
		}
	}

	// Where the chosen position puts the bar, in game coordinates.  True if
	// it moved.
	bool place() {
		Image_window* iwin = Gwin()->get_win();
		const int     w    = bar_w;
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
		int             sx   = icons_x + static_cast<int>(i) * (slot_w + slot_gap);
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
		const int dw = sman->get_text_width(0, each->description.c_str());
		sman->paint_text(0, each->description.c_str(), nx + (region.w - dw) / 2, ny + name_height / 2);
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

/*
 *  Status icons: small pictures under whoever a timed skill is on, drawn
 *  like the target arrow (in a layer, from a few pixels), on every machine.
 *  A green up arrow for a stat boost, a blue shield for armour (and
 *  Riposte, and U7's protection), a red down arrow for a weakened or marked
 *  foe.  Emotes would clash with chat, so these are pictures.
 */

enum Status_mark { mark_up = 1, mark_shield = 2, mark_down = 4 };

// Host: what each person with a skill on them shows.
std::vector<std::pair<Game_object_weak, int>> Host_marks() {
	std::vector<std::pair<Game_object_weak, int>> found;
	auto add = [&found](Game_object* obj, int mark) {
		for (auto& each : found) {
			if (each.first.lock().get() == obj) {
				each.second |= mark;
				return;
			}
		}
		found.emplace_back(weak_from_obj(obj), mark);
	};
	const uint32 now = Now();
	for (const Timed_bonus& each : bonuses) {
		const Game_object_shared obj = each.actor.lock();
		if (!obj || each.until <= now) {
			continue;
		}
		if (each.prop == bonus_armor || each.prop == bonus_riposte) {
			add(obj.get(), mark_shield);
		} else if (each.prop == bonus_damage_taken || each.amount < 0) {
			add(obj.get(), mark_down);
		} else if (each.prop == Actor::strength || each.prop == Actor::combat || each.prop == Actor::dexterity) {
			add(obj.get(), mark_up);
		}
	}
	for (Actor* member : Party()) {
		if (member->get_flag(Obj_flags::protection)) {
			add(member, mark_shield);
		}
	}
	return found;
}

struct Guest_mark {
	int    kind;
	uint32 id;
	int    mark;
};
std::vector<Guest_mark> guest_marks;    // Guest: as the host last said.

// Host: tell guests when the marks change.
void Send_marks(const std::vector<std::pair<Game_object_weak, int>>& marks) {
	static std::vector<unsigned char> sent;
	Friend_mode_session&              net = Friend_mode_get_session();
	if (!net.connected()) {
		sent.clear();
		return;
	}
	std::vector<unsigned char> data;
	int                        count = 0;
	data.push_back(0);
	data.push_back(0);
	for (const auto& [ref, mark] : marks) {
		const Game_object_shared obj   = ref.lock();
		Actor*                   actor = obj ? obj->as_actor() : nullptr;
		int                      kind  = 0;
		uint32                   id    = 0;
		if (!actor || !Friend_mode_actor_net_id(actor, kind, id) || count == 255) {
			continue;
		}
		data.push_back(static_cast<unsigned char>(kind));
		for (int b = 0; b < 4; ++b) {
			data.push_back(static_cast<unsigned char>(id >> (8 * b)));
		}
		data.push_back(static_cast<unsigned char>(mark));
		++count;
	}
	data[0] = static_cast<unsigned char>(count);
	if (data != sent) {
		net.send_blob(Friend_mode_session::Blob_kind::class_marks, data);
		sent = std::move(data);
	}
}

void Take_marks(const std::vector<unsigned char>& data) {
	guest_marks.clear();
	const int count = data.empty() ? 0 : data[0];
	for (int i = 0; i < count && 2 + 6 * i + 5 < static_cast<int>(data.size()); ++i) {
		const unsigned char* at = &data[2 + 6 * i];
		const uint32         id = at[1] | (at[2] << 8) | (at[3] << 16) | (static_cast<uint32>(at[4]) << 24);
		guest_marks.push_back({at[0], id, at[5]});
	}
}

constexpr int icon_size = 7;

// The three pictures: B black edge, X the colour, '.' clear.
const char* const* Icon_rows(int mark) {
	static const char* const up[icon_size]
			= {"...B...", "..BXB..", ".BXXXB.", "BXXXXXB", "BBXXXBB", "..BXB..", "..BBB.."};
	static const char* const shield[icon_size]
			= {"BBBBBBB", "BXXXXXB", "BXXXXXB", "BXXXXXB", ".BXXXB.", "..BXB..", "...B..."};
	static const char* const down[icon_size]
			= {"..BBB..", "..BXB..", "BBXXXBB", "BXXXXXB", ".BXXXB.", "..BXB..", "...B..."};
	return mark == mark_up ? up : mark == mark_shield ? shield : down;
}

struct Icon_layer {
	int layer = -1;
	int mark  = 0;    // The picture it holds.
};
std::array<Icon_layer, 24> icon_layers;

void Paint_marks() {
	Game_window*  gwin = Gwin();
	Image_window* iwin = gwin->get_win();
	// Who shows what, on this machine.
	std::vector<std::pair<Actor*, int>> shown;
	if (Friend_class_active() && !gwin->is_in_exult_menu()) {
		if (Is_guest()) {
			for (const Guest_mark& each : guest_marks) {
				if (Actor* actor = Friend_mode_actor_by_net_id(each.kind, each.id)) {
					shown.emplace_back(actor, each.mark);
				}
			}
		} else {
			const auto marks = Host_marks();
			if (Is_host()) {
				Send_marks(marks);
			}
			for (const auto& [ref, mark] : marks) {
				const Game_object_shared obj = ref.lock();
				if (Actor* actor = obj ? obj->as_actor() : nullptr) {
					shown.emplace_back(actor, mark);
				}
			}
		}
	}
	size_t        used         = 0;
	constexpr int text_layer_z = (1 << 18) + (1 << 16);
	for (const auto& [actor, marks] : shown) {
		if (!actor->get_chunk() || actor->is_dead()) {
			continue;
		}
		int pictures[3];
		int n = 0;
		for (const int mark : {mark_up, mark_shield, mark_down}) {
			if (marks & mark) {
				pictures[n++] = mark;
			}
		}
		int x;
		int y;
		gwin->get_shape_location(actor, x, y);
		// A row under the feet, centred.
		const int row_w = n * (icon_size + 1) - 1;
		int       left  = x - c_tilesize / 2 - row_w / 2;
		for (int i = 0; i < n && used < icon_layers.size(); ++i, ++used) {
			Icon_layer& slot = icon_layers[used];
			if (slot.layer < 0) {
				slot.layer = gwin->create_layer("Skill status icon", icon_size, icon_size, 255, 0, text_layer_z - 7);
				slot.mark  = 0;
				if (slot.layer < 0) {
					return;
				}
			}
			if (slot.mark != pictures[i]) {
				Image_buffer8* buf = gwin->get_layer_ibuf(slot.layer);
				if (!buf) {
					return;
				}
				Shape_manager*      sman  = Shape_manager::get_instance();
				const unsigned char color = pictures[i] == mark_up
													? static_cast<unsigned char>(gwin->get_pal()->find_color(15, 58, 15))
											: pictures[i] == mark_shield ? sman->get_special_pixel(PROTECT_PIXEL)
																		 : sman->get_special_pixel(HIT_PIXEL);
				const char* const* rows = Icon_rows(pictures[i]);
				buf->fill8(255);
				for (int py = 0; py < icon_size; ++py) {
					for (int px = 0; px < icon_size; ++px) {
						const char c = rows[py][px];
						if (c == 'B') {
							buf->put_pixel8(0, px, py);
						} else if (c == 'X') {
							buf->put_pixel8(color, px, py);
						}
					}
				}
				gwin->layer_set_dirty(slot.layer);
				slot.mark = pictures[i];
			}
			int sx0;
			int sy0;
			int sx1;
			int sy1;
			iwin->game_to_screen(left, y + 1, false, sx0, sy0);
			iwin->game_to_screen(left + icon_size, y + 1 + icon_size, false, sx1, sy1);
			gwin->layer_set_dest(slot.layer, sx0, sy0, sx1 - sx0, sy1 - sy0);
			if (!iwin->layer_is_visible(slot.layer)) {
				gwin->layer_set_visible(slot.layer, true);
			}
			left += icon_size + 1;
		}
	}
	bool hid = false;
	for (size_t i = used; i < icon_layers.size(); ++i) {
		if (icon_layers[i].layer >= 0 && iwin->layer_is_visible(icon_layers[i].layer)) {
			gwin->layer_set_visible(icon_layers[i].layer, false);
			hid = true;
		}
	}
	if (used > 0 || hid) {
		gwin->set_painted();
	}
}

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
			Actor*     actor = gwin->get_npc(each.npc);
			const bool fixed = each.at.tx >= 0;    // A field, on its spot.
			if (used >= circle_layers.size() || (!fixed && (!actor || !actor->get_chunk()))) {
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
			if (fixed) {
				gwin->get_shape_location(each.at, x, y);
			} else {
				gwin->get_shape_location(actor, x, y);
			}
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
	// "Require class items" is kept in the config but off the page until
	// there are items (a story question, decided later).
	enum setting_ids { set_enabled, set_bar, set_ranges, set_ai, set_count };

	enum button_ids { id_ok, id_cancel, id_preview, id_first_setting, id_count = id_first_setting + set_count };

	std::array<int, set_count>                         settings{};
	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	static const char* label(int setting) {
		static const char* const labels[set_count]
				= {"Enable:", "Skill bar:", "Show skill ranges:", "Class AI:"};
		return labels[setting];
	}

	// Guests: the game rules are the host's.
	static bool host_only(int setting) {
		return setting == set_enabled || setting == set_ai;
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
					Friend_class_ai ? 1 : 0};
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
				&Class_options_gump::toggle<2>, &Class_options_gump::toggle<3>};
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

// The top, in game-view coordinates, of the HUD along the bottom of the
// screen: the party's faces (Exult's status bars) and the skill bar, each
// where its layer really is (they scale with the HUD, not the game view).
int Friend_hud_bottom_y() {
	Game_window*  gwin  = Gwin();
	Image_window* iwin  = gwin->get_win();
	int           top   = gwin->get_height();
	auto          check = [&](Gump* gump) {
        const int layer = gump ? gump->get_render_layer() : -1;
        if (layer < 0 || !iwin->layer_is_visible(layer)) {
            return;
        }
        const TileRect rect = gump->get_rect();
        if (rect.w <= 0 || rect.w < rect.h) {
            return;    // Not a row (Exult's vertical face stats).
        }
        int sx;
        int sy;
        if (!iwin->layer_to_screen(layer, rect.x - gump->layer_bounds.x, rect.y - gump->layer_bounds.y, sx, sy)) {
            return;
        }
        int gx;
        int gy;
        iwin->screen_to_game(sx, sy, false, gx, gy);
        if (gy > gwin->get_height() / 2) {    // Along the bottom.
            top = std::min(top, gy);
        }
	};
	for (auto it = gwin->get_gump_man()->begin(); it != gwin->get_gump_man()->end(); ++it) {
		if (dynamic_cast<Face_stats*>(*it) || *it == the_bar) {
			check(*it);
		}
	}
	return top;
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
	Game_window* gwin = Gwin();
	if (!gwin->get_main_actor()) {
		return;
	}
	Tick_clock(gwin->get_tqueue()->is_paused()
			   || (Is_guest() && (gwin->is_network_host_paused() || gwin->is_network_dialogue_paused())));
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
	Paint_marks();
}

void Friend_class_world_loaded() {
	cooldowns.clear();
	global_waits.clear();
	timed_flags.clear();
	bonuses.clear();
	taunts.clear();
	pending.clear();
	summons.clear();
	circles.clear();
	sent_states.clear();
	guest_marks.clear();
}

void Friend_class_saving(bool before) {
	if (before) {
		// Dismissed: a saved game would keep them for good.
		for (const Game_object_weak& each : summons) {
			if (const Game_object_shared obj = each.lock()) {
				if (obj->get_chunk()) {
					Sprite_at(obj->get_tile(), sprite_poof);
					obj->remove_this();
				}
			}
		}
		summons.clear();
	}
	// U7's own timers for sleep and paralysis are long: lift ours for the
	// save, and put them back after it.
	for (const Timed_flag& each : timed_flags) {
		const Game_object_shared obj   = each.actor.lock();
		Actor*                   actor = obj ? obj->as_actor() : nullptr;
		if (!actor || actor->is_dead()) {
			continue;
		}
		if (before) {
			actor->clear_flag(each.flag);
		} else if (each.until > Now()) {
			actor->set_flag(each.flag);
		}
	}
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
	if (data.size() >= 7 && data[2] < 2) {
		const uint32 pick = data[3] | (data[4] << 8) | (data[5] << 16) | (static_cast<uint32>(data[6]) << 24);
		chosen_pick       = weak_from_obj(Friend_mode_actor_by_net_id(data[2], pick));
	}
	// Not while the Avatar talks or the host is in a menu.
	const bool waiting = Gwin()->get_usecode()->in_usecode() || Gwin()->get_tqueue()->is_paused();
	if (waiting) {
		why = "Wait for the Avatar";
	}
	const bool done = !waiting && skill && Perform(actor, *cls, *skill, why);
	chosen_pick.reset();
	if (done) {
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
	case Friend_mode_session::Blob_kind::class_marks:
		Take_marks(data);
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

bool Friend_class_place(Actor* npc) {
	Game_window* gwin = Gwin();
	if (!Friend_class_active() || !Friend_class_ai || Is_guest() || !npc || npc == gwin->get_main_actor()
		|| npc == gwin->get_controlled_actor() || !npc->is_in_party() || Friend_mode_player_playing(npc) >= 0
		|| !Able(npc)) {
		return false;
	}
	return Place(npc);
}

int Friend_class_bonus(const Actor* actor, int prop) {
	if (prop != Actor::strength && prop != Actor::combat && prop != Actor::dexterity) {
		return 0;
	}
	return Bonus_of(actor, prop);
}

void Friend_class_adjust_damage(const Actor* target, Game_object* attacker, int& damage, int& armor) {
	if (bonuses.empty()) {
		return;
	}
	Actor*     by    = attacker ? attacker->as_actor() : nullptr;
	const bool party = by && (by->is_in_party() || by == Gwin()->get_main_actor());
	armor += Bonus_of(target, bonus_armor);
	if (by) {
		armor = std::max(0, armor - Bonus_of(by, bonus_pierce));
	}
	if (party) {
		damage += Bonus_of(target, bonus_damage_taken);
	}
}

void Friend_class_note_miss(Game_object* attacker, Game_object* target) {
	static bool answering = false;    // A riposte never answers a riposte.
	Actor*      user      = target ? target->as_actor() : nullptr;
	Actor*      foe       = attacker ? attacker->as_actor() : nullptr;
	if (answering || !user || !foe || foe->is_dead() || !Able(user) || user->distance(foe) > 1
		|| Bonus_of(user, bonus_riposte) <= 0) {
		return;
	}
	int                points = 0;
	int                weapon = -1;
	Game_object*       obj    = nullptr;
	const Weapon_info* winf   = user->get_weapon(points, weapon, obj);
	if (!winf || weapon < 0) {
		return;
	}
	answering = true;
	user->change_frame(user->get_dir_framenum(user->get_direction(foe), Actor::strike1_frame));
	Combat_schedule::attack_target(user, foe, foe->get_tile(), weapon);
	answering = false;
	React(user, "*ripostes*");
	for (const Timed_bonus& each : bonuses) {
		if (each.prop == bonus_riposte && each.actor.lock().get() == user) {
			if (const Class_skill* skill = Skill_by_id(Class_of(user), each.skill)) {
				Sprite_on(foe, skill->look.on_targets);
			}
			break;
		}
	}
}
