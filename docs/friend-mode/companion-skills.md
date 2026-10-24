# Companion skills: the Class System

Status: **being built, ahead of the release** (decided 2026-10-20: it needs
balancing and touches today's combat, so better early than late). First
written 2026-10-19 as an idea for after v1. The foundation is described in
"The Class System: foundation" below; the rest of this page is the design it
grows into.

A guest who plays Iolo or Jaana as their "main" today walks, picks a target
and lets Exult's combat swing the weapon. This idea gives each companion two
or three skills of their own, drawn from the class they had in Ultima IV, so
playing Iolo feels like playing a bard and playing Jaana feels like playing
a healer. Few skills, long cooldowns, simple effects: a flavour layer on
top of U7's combat, not a new combat system.

Decided (2026-10-19):

- **A small skill bar for guests only,** for the companion they play.
  Companions nobody plays (the party's AI, single player) use their skills
  on their own, by their class's rules and the same cooldowns. Nobody else
  triggers a companion's skills, the host included.
- **The Avatar gets a spell bar instead:** spells picked from the
  spellbook, cast with one key or click, aimed through the Target System
  instead of the crosshair.
- **The mouse first;** keys for the bars are still to work out.
- **Roughly one skill per battle:** cooldowns of about a minute or more,
  since most fights are over in under a minute.
- **Each companion's skills can need their own item,** found in the world:
  Iolo's lute in his home, so he gets his songs later in the story, not in
  Trinsic. Companions who join far from home carry theirs. An option, off
  at first (2026-10-20): making items and placing them in the world is a
  game master's call, for later.
- **The AI uses skills too, per companion by choice:** "Normal AI" (U7 as it
  is) or "Class AI", where the companion fights like its class (healers
  watch health, Iolo sings when the fight calls for it, Shamino keeps his
  distance).

## The Class System: foundation

Decided 2026-10-20. The pieces everything else is built on.

**Its own code.** The Class System lives in `friendclass.cc` and
`friendclass.h` (data, rules, AI, the bar and its options page), with a few
calls from `exult.cc` (each frame, the host's tick, messages) and from the
combat schedule (Class AI, later). Exult's own files only get hooks, so it
stays as separate from Exult as it can.

**Menu: Game menu → Class System**, next to Target System:

| Option | Values | Default | Who decides |
| --- | --- | --- | --- |
| Enable | Yes / No | No | The host, for everyone (a game rule); guests follow it |
| Skill bar | Above status bar, Bottom left, Bottom right, Top left, Top right, Left side, Right side (columns, centred) | Above status bar | Each machine |
| Show skill ranges | Yes / No | Yes | Each machine |
| Skills on a foe | Ask / Auto: with nothing picked (no Q), a skill on one foe asks with the crosshair, or chooses as the Class AI does (the user's foe, then the nearest; the strongest or the biggest group for some) | Ask | Each machine (2026-10-23) |
| Class AI | Yes / No | Yes | The host |

With Enable off the rest hides, as on the Target System page. Off means U7
as it was: no bars, no skills, U7's AI.

**Skill bars.** A HUD bar, scaled with the HUD like the shortcut bar and
face stats (the gumps size setting), at the position chosen. Icons are the
spellbook's own spell pictures, so nothing new is drawn. Each icon shows
how many seconds are left while it recharges and is greyed when it can't be
used; a click uses it and the reason shows near the top when it can't
("Restoration: 23 seconds", "Needs a hostile", "Not yet: level 3").

- A guest's bar holds its companion's skills.
- The host's bar holds the Avatar's spells: three slots the player fills
  by dragging spells from the spellbook (below; 2026-10-23, was a fixed
  Telekinesis, Lightning and Time Stop, before that Great Heal, Fire Blast
  and Lightning), each greyed while the Avatar doesn't know it or lacks
  the mana, reagents or level. A click casts through the spellbook's own
  code (mana, reagents, words of power as in U7); a spell that wants a
  target takes the Target System's pick instead of the crosshair:
  a spell that asks for one takes the Q or E pick, the crosshair only
  with no pick; any other just casts. A slot clicked is used on the next frame, not inside the click: a
  crosshair inside it crashed Exult (2026-10-23). A spell at a foe starts
  the Avatar's fight with it once the cast is over (fighting at once left
  the Avatar standing while the rest of the party fought).
- No bar for spectators, in follow mode, in conversations or menus.

**Definitions, one table per companion.** In `friendclass.cc`, by NPC
number. Each companion has a class name and up to four skills; each skill
has a number of its own (stable, used on the wire and in the config):

| Field | Meaning |
| --- | --- |
| id | The skill's number, unique (Iolo 10-19, Shamino 20-29...) |
| name | "Battle Hymn" |
| icon | A spellbook picture (spell number) |
| cooldown | Seconds; 60 for all at first |
| range | Tiles: for an area, its radius; for a target, how far it reaches |
| range shown | Drawn (a circle, for songs and heals) or silent (Stun Shot) |
| type | heal, cure, support (buff), attack, control (paralyze, sleep, taunt), self |
| target | self, party in range, most hurt in range, the pick, nearest hostiles |
| level | Character level that unlocks it (1 for the first; 4, 8 for later ones) |
| effect | What it does, from a small list the code knows (below), or any of U7's spells |
| look | Any of U7's sprite effects on the targets and on the user, a missile picture flying to each target, a sound |
| words | The line over the companion: "*plays a battle hymn*" |

Each skill is written as a chain of named settings in `friendclass.cc`, one
per line, so a new one reads like this:

```cpp
Class_skill(30, "Restoration")
		.describe("Heals the whole party")
		.recharge(90)
		.icon_spell(spell_restoration)
		.kind(T::heal)
		.aim(G::party_area)             // No range: the whole party
		.unlock(level_third)
		.heal(20)
		.targets(2)                     // The Class AI waits for two hurt
		.on_targets(sprite_sparkles)
		.says("Vas Mani Hur"),
```

A skill on foes gives its range, and `true` to draw it:
`.aim(G::hostiles_nearest, 12, true)` (Song of Peace).

Levels (2026-10-22, were 1, 3, 6): the first skill at level 1, the second
at 4, the third at 7 (`level_second`, `level_third` in friendclass.cc). U7's
level is 1 + log2(experience / 50), so level 7 takes 3200 experience and
level 8 would be near the end of Black Gate.

**Cooldowns** (after testing with testers, 2026-10-20): by place on the
bar, 30 seconds for the first skill, 60 for the second and 90 for the third,
with a few exceptions (Crippling Shot 30, Flashbang 60, Summon Dogs and
Summon Wolf 60). The third skills may go to 120 after more testing.
Kept on the host per companion and skill, sent to the guest who plays it;
not saved. After any skill, all of that character's skills wait 5 seconds
(was 1.5, 2026-10-22), counted down on the bar, so a high level can't fire
all three at once; each skill's own cooldown runs as well.

**Paused with the game.** Every timer (cooldowns, buffs, stuns, fields,
fuses, summons) runs on the Class System's own clock, which stops while
Exult's world is paused: menus, conversations, Exult's pause, and on a
guest while the host is paused or talking.

**Guests' picks.** A guest's skill carries the guest's Target System pick
(Q), so Stun Shot or Hunter's Mark land on what the guest chose. With no
pick (someone who doesn't use Q), a skill on one foe asks with the
crosshair, as a spell does, or with "Skills on a foe: Auto" chooses as the
Class AI does (2026-10-23; before, it took the companion's
foe, then the nearest hostile, which could mark the wrong one). The same
for the host, single player and a led companion. The Class AI still
chooses by itself. Helpful skills never ask: they choose by need.

**Who skills reach** (2026-10-23). Party buffs reach the whole party,
wherever it stands, with no range and no circle: Song of Warding, Battle
Hymn, Mass Cure, Restoration. Skills on one party member choose by need,
from the whole party: Greater Heal and Lay on Hands the most hurt, Quick
Fix the weakest, Shield Ally the ally a foe is on (else the most hurt)
within 6 tiles of Dupre, who must stay that close for the blows to land on
him. A party member picked with Q (the host, out of combat) is taken
instead. (A crosshair for them was tried the same day and dropped: the
choice by need is better.) Ranges are only for skills on foes (Song of
Peace's 12, the bows' 24, Challenge's 9...) and Shield Ally's 6. Circles
are drawn only for Song of Peace (12), Dirty Trick (12), Challenge (9) and
Set Trap (2 round Julia's feet, for as long as the caltrops lie).

**The strongest foe.** Hunter's Mark, Sleep Arrow, Crippling Shot and
Master's Strike, choosing for themselves (the Class AI, or Auto), take the
hostile in range with the most health at full, then now
(`strongest_foe`), not the nearest: marking, sleeping or crippling a rat
wastes them, and Master's Strike's armour piercing counts on the toughest.
Flashbang goes for the foe with the most hostiles within its blast
(`densest_foe`). Aimed Shot and Stun Shot keep the user's foe, then the
nearest: a burst on what one fights, and an interrupt on what closes in.

**Not while the Avatar talks.** Guests' skills are greyed and refused
("Wait for the Avatar") during a host conversation or pause.

**Saves.** Before any save (autosaves too) summoned creatures are
dismissed, and our short sleeps and stuns are lifted for the save and put
back after, so U7's longer timers never take them over in a saved game.

**Status icons** (2026-10-20; emotes would clash with chat): a small green
up arrow under whoever has a stat boost (Battle Hymn), a blue shield for
armour (Quick Fix), a shielded ally (Shield Ally), Riposte or protection, a
red down arrow for a weakened, marked or confused foe (Crippling Shot,
Hunter's Mark, Dirty Trick). Drawn like the
target arrow, from a few pixels, on every machine. Area skills show their
circle and sparkles instead.

**The bar's look** (after the first test, 2026-10-20): square slots in
Exult's own button style (as on the options pages) with the skill's name in
the tiny font, a tooltip pinned to the hovered slot (black, white letters:
name, what it does, why not), gone as the mouse leaves,
the seconds left in the corner, darker while unusable; flush with the edge
it is on; a row, or a column at the left or right side. Since 2026-10-23
there is no tooltip box: the mouse over a slot puts its name in the notice
near the top, and under it, in the smaller letters of the Space hint, "Use
Skill (Ctrl+1)" or "Use Spell (Ctrl+2)" ("Locked", "Not Learned"; an empty
slot of the Avatar's says "Empty Slot", then "Drag a spell here from
spellbook"). The Avatar's slots are the spellbook's picture of the spell,
on Exult's button a pixel larger all round.

**Fighting by the bar** (2026-10-20): an offensive skill or spell (an
attack or hold on the pick) on a hostile turns combat mode on; in combat
mode the Avatar's spells go through on anyone; in peace mode on someone
peaceful they ask first ("Fire Blast on Deer? Click again", within 4
seconds). Animals and other creatures never ask: the skill fires, combat
mode comes on and the attack on the pick starts, as Space would. Guests may
fight peaceful people too, after the same confirmation (2026-10-20).

**Ranges** (2026-10-20): area skills got about half again as much range:
Song of Peace and Dirty Trick 12 tiles, Challenge 9, Blade Dance 6, Flashbang 5 around where it lands, Set Trap
2 around Julia's feet. Bow skills (2026-10-23): 24 tiles (were 16, and
Volley 12), Shamino's and Tseramed's; U7's bow reaches 20, so a skill's
shot reaches as far as the skill (`Friend_class_shot_reach` in
`Combat_schedule::attack_target`). Spark's sling skills keep 16 and 14,
the sling's own 16, so bows outrange slings.

**Strength is health.** U7's maximum health is strength, so Battle Hymn's
+3 strength gives +3 health too, taken back when it ends.

**Boosts reach everyone.** Every fight is worked out on the host, guests'
companions included, so a boost on anyone counts wherever they are played;
guests' stats windows show U7's own numbers (decided: fine, the boosts are
short).

**Range system.** Like targeting's range, in tiles, measured the way Exult
measures distance. Two kinds:

- **Shown:** a faint circle around the user while the mouse is on its icon
  and for 2 seconds after it is used, on every machine (Iolo's songs,
  Song of Peace, Challenge). "Show skill ranges" turns it off.
- **Silent:** checked, never drawn (Stun Shot, Aimed Shot, Hunter's Mark): too
  far gives "Too far".

**Effects the code knows** (each skill is one of these, with numbers):

| Effect | Does | Skills |
| --- | --- | --- |
| heal | health up, capped at strength | Greater Heal (full), Restoration, Lay on Hands |
| cure | clears poison, paralysis, curse | Mass Cure |
| flag | sets a U7 flag on whoever it targets: protection, invisible | Song of Warding, Vanish |
| hinder | paralyze or knock out each target for a few seconds, with a roll each | Song of Peace (sleep) |
| volley riders | a shot that, as it lands, also hinders (`then_hinder`) or lowers a stat (`then_weaken`); `pierce_armor` makes the user's strikes ignore some armour; `dashing` darts the user beside each target in turn before a melee strike | Stun Shot, Sleep Arrow, Crippling Shot, Master's Strike, Blade Dance |
| armor | timed extra armour: strong on one ally, or smaller on everyone in the circle | Quick Fix |
| mark | the target takes extra damage from the party for a while | Hunter's Mark |
| stance | for a while, a melee strike that misses the user is answered with one of its own, which always hits | Riposte |
| guard | for a while, melee blows on one ally (the one a hostile is on, else the most hurt, within 6; or the Q pick) land on the user instead, while the user is within 6 tiles with a shield readied | Shield Ally |
| flash | thrown at the pick; hostiles where it lands are paralyzed briefly (each rolls) | Flashbang |
| bomb | thrown at the pick; after a fuse (sparks meanwhile), hostiles where it lands take the powder keg's blast, with its picture and sound; the party is spared | (none since Time Bomb went) |
| field | at the user's feet, a circle that heals the party standing in it every second | (none since Healing Field went) |
| trap | at the user's feet, a circle where hostiles lose health every second (armour doesn't help); the party is spared | Set Trap |
| summon | one or more friendly creatures (good alignment, combat schedule) fight beside the party, then leave with a poof | Summon Dogs, Summon Wolf, Summon Bee |
| volley | a real shot (the readied bow, sling or crossbow and its ammunition, U7's hit and damage) at each target | Volley, Trick Shot |
| bonus | timed +N strength and combat | Battle Hymn, Hunter's Mark |
| taunt | hostiles in range turn on the user and are kept on it for the duration | Challenge |
| calm | hostiles don't attack for a few seconds, with a roll each (combat against intelligence); they stand and keep their target | Dirty Trick |

**U7's looks, not its scripts** (decided 2026-10-20). Skills never run a
spell's script, so they can't fight the game's own scripts or their
assumptions (that the Avatar casts, the party is spared, and so on). What
a skill does is always our effect above; what it looks and sounds like can
be any of U7's own pieces: `on_targets(sprite)`, `on_user(sprite)`,
`missile(shape)`, `sound(sfx)`. Sprites are sprites.vga's (poof 9, green
bubbles 13, barrier 15, sparkles 16, blue beads 18, music 24, and the rest;
see the effect preview below); missiles are pictures only, flying on every
machine; the harm is the effect's. Spell numbers are used only for the
icons (the spellbook's pictures). The Avatar's spell bar is different: it
casts the Avatar's real spells, as the spellbook does.

**Reactions.** Whoever a skill lands on shows an emote-style line, as
emotes show over companions, on every machine: *stunned*, *asleep*,
*dazed*, *crippled*, *marked*, *enraged*, or *resisted* when the roll fails.
Each comes from the effect; a skill can set its own with `reacts("*drowsy*")`.

**Effect preview** (Class System page → Effect preview): a panel at the
top of the view, with the game running, that steps through U7's sprite
effects, the missile pictures weapons and ammunition fly as, and the sound
effects, playing each on one's character (a missile flies at the Q pick, or
six tiles ahead). It shows the number to use (`on_targets(16)`,
`missile(…)`, `sound(…)`), and for sprites how many frames. Kind switches
between the three; << and >> step by ten; the host's log lists each one
played. Works with the Class System off, and in single player.

**Class AI.** For companions nobody plays, when "Class AI" is on: a tick on
the host about twice a second for each such companion in the party, in
this order:

1. **Watch:** the party's health (who is below half, below a quarter),
   status (poisoned, paralyzed, cursed), the hostiles near (how many, how
   close, who they are on), and the companion's own health and weapon.
2. **Choose a skill:** go through the companion's skills in its class's
   order; the first that is ready (level, cooldown, item) and whose rule
   says "now" is used. The rules by type:
   - heal: someone below half (Restoration: two or more);
   - cure: someone in range poisoned, paralyzed or cursed;
   - support: three or more hostiles near, or the party taking a beating;
   - attack: enough targets in range (Volley: two or more);
   - control: a hostile on a hurt party member;
   - self: own health below half.
3. **Place itself:** each class has a place to stand:
   - **ranged** (Shamino, Iolo, Spark): keep 4-8 tiles from the nearest
     hostile; a foe beside it means step away first, then shoot;
   - **support** (Jaana): stay near the middle of the party, out of the
     front; when someone needs a heal out of range, walk until in range;
   - **front** (Dupre, Sentri): close in, as U7 does now.
4. **Use it:** turn to the target, the casting pose, the effect, the words,
   the cooldown; the same code a player's click runs.
5. Otherwise fight as U7 does (its combat schedule, its target).

Placing is built (2026-10-20): `Friend_class_place`, called by the combat
schedule (`Combat_schedule::now_what`) before it closes in on a foe. When it
starts a walk the schedule waits for it, then goes on as U7 does:

- **ranged** (Iolo, Shamino, Spark): with a ranged weapon readied and a
  hostile within 2 tiles, step 4 tiles away from it, then shoot as usual;
- **support** (Jaana): when as many companions are below half as her heal
  waits for (two), and the worst is out of its range, walk to within range
  less 2 tiles; with a hostile within 2 tiles, fall back to the middle of the
  party;
- **front** (Dupre, Sentri): U7's own closing in;
- never further than 12 tiles from the Avatar, and at most one placing walk
  every 1.5 seconds, so they shoot and heal in between;
- only for companions nobody plays, with Class AI on, on the host.

"Normal AI" is U7's, untouched.

**Network** (protocol 32): a guest's "use skill" (skill id); the host's
class state (enabled, require items, Class AI, and for each guest's
companion its skills' seconds left), sent when it changes and once a second
while anything recharges; and a "skill used" event to everyone (who, which
skill) so every machine shows the circle. The effects themselves travel as
today (health, flags, missiles, sprites, chat).

**Also needed** (not menu options):

- Log lines on the host for balancing: who used what, on whom, how much.
- For testing: `config/friend/class/short_cooldowns` (yes/no, in the cfg,
  not on the page) makes every cooldown 5 seconds.
- The test checklist section, and the brief's roadmap.

## The skills

The first four companions are the ones in the original idea. The rest come
later, once the first set has been played.

| Companion | U4 class | Skills (level, cooldown) |
| --- | --- | --- |
| Iolo | Bard | **Song of Peace** (1, 30 s): hostiles within 12 fall asleep for 5 s, each rolls to resist. **Song of Warding** (4, 60 s): protection for the whole party. **Battle Hymn** (7, 90 s): +3 strength (so +3 health) and +3 combat for the whole party, for 30 s. |
| Shamino | Ranger | **Vanish** (1, 30 s): invisible for a moment. **Aimed Shot** (4, 60 s): one harder shot at the pick, within 24. **Volley** (7, 90 s): arrows at the 3 nearest hostiles within 24. |
| Jaana | Druid | She says words of power as she uses them (2026-10-23). **Greater Heal** (1, 30 s, "Vas Mani"): a full heal on the most hurt in the party. **Mass Cure** (4, 60 s, "Vas An Nox"; was Purify): cures poison, paralysis and curses on the whole party. **Restoration** (7, 90 s, "Vas Mani Hur"; was Mending Circle): +20 health for the whole party. Her second and third are U7's spells of those names, which reach the whole party too. |
| Dupre | Paladin | **Lay on Hands** (1, 30 s): +25 health on the most hurt in the party. **Shield Ally** (4, 60 s): for 15 s, melee blows on an ally within 6 (the one a foe is on) land on Dupre instead while he stays within 6 tiles (shield required). **Challenge** (7, 90 s): hostiles within 9 turn on him for 6 s. |
| Spark | (boy) | With his sling. **Stun Shot** (1, 30 s): a stone, then the pick stops for 3 s, within 16. **Trick Shot** (4, 60 s): stones at the 2 nearest hostiles within 14. **Dirty Trick** (7, 90 s): mud at hostiles within 12; they don't attack for 3 s. |
| Sentri | Swordmaster (agile duelist) | **Riposte** (1, 30 s): for 10 s, every melee strike that misses him is answered with a strike that always hits. **Master's Strike** (4, 60 s): he lunges beside the pick, within 4, and strikes a harder blow that ignores 6 armour. **Blade Dance** (7, 90 s): he darts to every foe within 6 in turn, striking each. |
| Tseramed | Huntsman (disabling archer, beside Shamino's attacking one) | **Sleep Arrow** (1, 30 s): the pick sleeps for 5 s, within 24. **Crippling Shot** (4, 30 s): the pick loses 10 dexterity for 10 s, within 24 (U7 has no slowed walking, so it attacks less often). **Hunter's Mark** (7, 90 s): the pick takes +4 damage from party hits for 20 s, within 24. |
| Julia | Tinker (combat engineer) | **Set Trap** (1, 30 s): caltrops at her feet, 2 tiles round, for 5 s; hostiles in them lose 3 health a second, never the party. **Quick Fix** (4, 60 s): +5 armour on the weakest ally in the party, for 30 s. **Flashbang** (7, 60 s): thrown up to 12; hostiles within 5 of where it lands are stunned for 2.5 s. |
| Katrina | Shepherd (gentle support) | **Summon Dogs** (1, 60 s): two of her sheepdogs fight for 30 s. **Summon Wolf** (4, 60 s): a wolf fights for her for 30 s. **Summon Bee** (7, 90 s): a giant bee fights for her for 30 s. |

Changed 2026-10-22 (feedback round): skill icons 24x24; unlock levels 1, 4, 7 (Iolo and the
Avatar start at level 3, so the second skill was open from the start); Iolo's
Song of Peace first, Song of Warding second; Master's Strike a lunge (range
4, `dashing`), since U7's melee often stood a tile off and refused it; the
wait after any skill 5 seconds. The levels below read 3 and 6 in the history.

Changed after testing with testers (2026-10-20): the skills reordered so
each bar runs level 1, 3, 6 (1, 4, 7 since 2026-10-22); cooldowns 30/60/90 by level; Jaana's Entangle
became Greater Heal; Dupre's Stand Fast became Shield Ally; Spark got Dirty
Trick; Julia's Time Bomb became Set Trap; Katrina's Healing Field and
Sanctuary became summons. New skill numbers: Greater Heal 33, Shield Ally
43, Dirty Trick 52, Set Trap 83, Summon Wolf 93, Summon Bee 94 (Summon
Dogs keeps 91); 32, 40, 82, 90 and 92 are retired.

Decided 2026-10-20 (the user's design for the last four): some overlap is
fine, since nobody can take every companion along; several heal and several
support. Defence boosts differ by spread: one ally strongly (Quick Fix) or
everyone in a circle a little (Song of Warding). No new art: each
skill's look is U7's own sprites, missile pictures and sounds (the keg flies
for Flashbang; any shape can, a cannonball or a clock, chosen
with the effect preview).

The Avatar gets no class skills: it has the spellbook, and a spell bar
for it (below).

A skill announces itself the way emotes do, over the companion and in the
log: "*plays a battle hymn*", "*looses a volley*", "*calls on the earth*".
The companion plays its casting frame, and a sprite effect marks who was
touched (U7 already has sparkles, glows and the heal effect).

## Class items

Each companion's skills come from one item of their own: Iolo's lute,
Shamino's bow, Jaana's staff or herb pouch, Dupre's shield, Spark's
sling, and so on. With it in their pack or hands the skill bar lights up;
without it the bar is grey and says "Needs Iolo's Lute". It gives the
feature a small piece of story: the first visit to a companion's home
becomes worth making, and later companions are "complete" when they join.

**Where they lie.** In each companion's home where they have one; on the
companion when they join away from home or late. To check in the map and
the game's scripts before placing anything (Exult Studio shows where each
NPC lives and sleeps):

| Companion | Where the item could be | Check |
| --- | --- | --- |
| Iolo | His home | Where Iolo has a home or shop in Black Gate |
| Shamino | His home | Same |
| Dupre | His home, or on him | Where he joins and whether he has a home |
| Jaana | Her house in Cove | Her house in Cove |
| Spark | His father Christopher's house in Trinsic | He joins there, so it is found at once; fine for a boy with a sling |
| Sentri, Tseramed, Julia, Katrina | Their homes, or on them | Where each joins and lives |

**Stock Black Gate only, so no new shapes.** Friend Mode plays the
unmodified game, so the items are existing shapes (U7 has a lute, bows,
staves, shields, slings) marked as special by a quality number of our own,
and named by Friend Mode when clicked ("Iolo's Lute"). The host places them
into the world once per game, the first time a game is played with skills
on, like the F9 test kit; a flag in the save says it is done, so they are
never placed twice and are saved like any other item. They can be sold, lost
or stolen like anything else; a lost one can be put back from the options
page ("Restore class items").

To decide: whether a companion without the item has one starter skill (so
Iolo isn't plain from Trinsic to his home), or none. Leaning none, as
decided, with the item easy to reach for the first four.

## Cooldowns

About one skill per fight, so cooldowns run from about 60 seconds (heals,
the common songs) to 3 minutes (Lay on Hands, Song of Peace). Kept on the
host per companion, so switching companions with J doesn't reset them; not
saved (a load clears them). The bar shades each skill until it is ready.
Possible later: Jaana's skills also cost mana.

## The skill bar

Guests only: a small bar, two or three icons, under or beside the status
bar, for the companion the guest plays.

- Each icon shades as it cools down, grey with the reason when it can't be
  used ("Needs Iolo's Lute", "No arrows", "Needs a hostile").
- Clicked with the mouse to start with; keys later (see Keys below).
- Hidden when the option is off, for spectators, and in follow mode (the
  companion's AI uses its skills then).
- No bar on the host: the Avatar has the spell bar, and the host's other
  companions use their skills on their own.

## The Avatar's spell bar

The host's counterpart: a bar of the Avatar's chosen spells, so casting is
one press instead of opening the spellbook, finding the page, clicking the
spell and then clicking a target. Works in single player too, so it is one
of the parts that could be [offered to Exult](upstreaming.md).

- **Choosing spells:** in the spellbook, a spell is put on the bar (a
  button by each spell, or dragged to a slot); six to eight slots. The bar
  shows only spells the Avatar knows, greyed when there isn't the mana or
  the reagents (`Spellbook_object::can_do_spell`).
- **Casting:** a click on the slot (keys later) casts it through the
  spellbook's own code (`Spellbook_object::do_spell`), so mana, reagents,
  the words of power and failures stay U7's.
- **Aiming with the Target System:** a spell that asks for a target (the
  crosshair) takes the current pick instead: Q for a foe or person, E for
  an object or container, the party for healing. Exult's usecode machine
  already answers crosshair clicks from a queue
  (`queued_click_items`, used today for a guest's painting and keys), so
  the pick is put there before the spell runs. With no pick that fits, the
  crosshair comes up as before.
- **Self and party spells** (Heal on the Avatar, Protect All) need no
  pick. Heal and Cure on another party member take the Q party pick (Q
  offers the party out of combat), or the Avatar with none.
- The bar is kept with the game, so it is still there after a load.

Exult already has a bookmark (one spell per book) and casts the bookmarked
spell when the Avatar fights with a spellbook readied; the bar is the same
idea with more slots and a target.

### Spells dragged onto the bar (decided and built 2026-10-23, not tested)

Like Ultima Online's spell shortcuts: the player drags a spell from the
open spellbook onto a slot of the Avatar's bar, and the slot casts it from
then on. It replaces the fixed Telekinesis, Lightning and Time Stop, which
not everyone wants. The Avatar only: the companions' bars keep their
skills.

Decided with the user:

- **Three slots,** as the companions' bars, on Ctrl+1 to Ctrl+3.
- **Kept with the save game:** each playthrough has its own bar, and
  loading an old save brings back its spells. (A small file in gamedat,
  added to the files Exult saves; read on load, empty when missing.)
- **A new game starts empty:** an empty slot shows a dimmed frame and the
  tooltip "Drag a spell here".
- **Dragging:** a spell from the book onto a slot puts it there, replacing
  what was there; a slot dragged onto another swaps them; a slot dragged
  off the bar onto the game view clears it. The spell's picture follows
  the mouse while dragging. A plain click on a spell in the book still
  sets the bookmark, and a double-click still casts.

Built as below: the drag in `Dragging_info` (drag.cc: `drag_spell`, the
picture in the dragged item's layer, `Friend_class_drag_spell` and
`Friend_class_drop_spell`), the file `FRIENDBARDAT`
(`<GAMEDAT>/friendbar.dat`, three spell numbers, -1 empty) in gamedat.cc's
save and clear lists (saved only when present, so older saves load with
an empty bar), the 72 names in `spell_names`, and
`Usecode_machine::friend_calls_intrinsic` for "asks for a target". A slot
shows the spellbook's picture of its spell on Exult's button; an empty one
is a plain button with "Empty" on it in small letters; hovering it says "Empty Slot" near the top, then "Drag a
spell here from spellbook".

How, in the code (the plan):

- The drag starts on the book's `Spell_button` (gumps/Spellbook_gump.cc);
  Exult's drag already holds a pushed button (`Dragging_info::button`).
  On the release, `Dragging_info::drop` asks the Class System whether the
  mouse is over a slot of the bar (a hook such as
  `Friend_class_drop_spell(spell, x, y)`), before the button's own click.
  Slot to slot and slot off the bar are the bar's own buttons.
- Casting stays as today: the spellbook's code (mana, reagents, words of
  power), the click on the next frame, and the Target System's pick in
  place of the crosshair.
- **Aiming, layered on the Target System** (the user's call, 2026-10-23;
  a table of all 72 spells was dropped): a spell that asks for a target
  gets the current pick, Q's or E's (they clear each other, so there is
  one at most); with no pick, the crosshair. A spell that asks for none
  (Time Stop, Mass Might) just casts, with no crosshair and no asking,
  whatever is picked: the pick only answers a crosshair. Since the
  bar can't tell a heal from a bolt, a pick of a peaceful person (not the
  party, not a creature) asks once ("Lightning on Finnigan? Again"),
  whatever the spell, and the Avatar takes up the fight after the cast
  only on a hostile pick. Whether a spell asks for a target is read from
  its own script (does it call `click_on_item`?) when the game loads, not
  a list written by hand, so an untargeted spell never asks.
- Greyed as today: not in the book ("Not Learned"), no mana, reagents or
  level. A slot could also show how many casts the reagents allow, as the
  spellbook does.
- Host and single player only; nothing new on the wire.

## Keys

**Ctrl+1, Ctrl+2 and Ctrl+3** use the bar's first three slots, as a click
does (2026-10-23), on the host, single player and guests alike; with no
bar, a red X. (Ctrl+number had no binding in Exult's keys. On a Mac whose
Mission Control uses Ctrl+number to switch desktops, the system takes them
first.)

Decided 2026-10-20: **the mouse is enough for now; keys are the last phase
of this feature.** The reason for keys was a game controller one day, and a
controller would get its own scheme anyway (a radial menu for skills and
spells, see the [controller plan](controller-plan.md)), so keyboard keys
needn't be bent to fit it. Notes for that phase:

The mouse comes first; keys are still to think through. Most letters are
taken (Q, E, Space, C, Y, J, L, X, Tab, WASD), and the numbers are already
crowded: 1-8 paperdolls, Shift+1-8 backpacks, so Alt+1-8 would be a third
layer on the same keys. Ideas so far:

- **Cycle and use:** one key moves a highlight along the bar, another uses
  the highlighted slot, as Q and Space do for targets. Two keys for any
  number of slots, at the cost of a press or two more. U (free since the
  world map was shelved) and O look free.
- **Alt+1-8** for the slots, if a third layer on the numbers isn't too much.
- F-keys where free.

A gamepad fits the cycle-and-use idea well: a shoulder button to cycle, a
face button to use (see the [controller plan](controller-plan.md)). The
Space hint line could also say "Volley ready" when a skill is.

## Class AI

Companions nobody plays use their skills on their own: the party's AI in
co-op, a guest's companion in follow mode, and every companion in single
player. They keep the same cooldowns as a player and follow their class's
rules (below), so they don't fire every skill the moment a fight starts.

Each companion has a setting, **Normal AI** or **Class AI**, for whenever
nobody is playing it. Normal AI is U7's, with no skills.
Set by the host, from the companion's paperdoll or the options page, and
kept with the game (a small Friend Mode file in the save, like the journal).
A guest playing a companion drives it and its skills itself; when it goes
to follow mode, its setting applies.

Class AI changes two things: **when skills are used**, and **where the
companion stands**.

| Companion | Class AI |
| --- | --- |
| Iolo | Sings by need: Song of Warding when two or more hostiles are near, Song of Peace when a hostile is on a badly hurt companion, Battle Hymn when three or more are near. Keeps a few tiles back with his crossbow. |
| Shamino | Always fights from a distance: steps away from a foe that comes beside him, then shoots; Vanish when he is hurt and a foe is close, Aimed Shot at his foe, Volley when two or more hostiles are in range. Falls back to his sword with no arrows. |
| Jaana | Watches health: stays near the middle of the party, out of the front line; Greater Heal when someone is below half, Mass Cure when someone is poisoned or paralyzed, Restoration when two or more are badly hurt; fights only when nobody needs her. |
| Dupre | Front line: Lay on Hands on the most hurt when someone is below half, Shield Ally on an ally a foe is beating on (the Avatar, or anyone below three quarters), then stays within reach of that ally while it lasts; Challenge when a foe is on a hurt party member. |
| Spark | Hangs back with his sling; Stun Shot on a foe beside a hurt party member; Dirty Trick the same way, on everything near. |
| Sentri | Front line; Riposte when a foe is beside him; Master's Strike on his foe; Blade Dance with two or more foes within 6. |
| Tseramed | Keeps his distance; Sleep Arrow or Crippling Shot on a foe beside a hurt party member, Hunter's Mark on his foe. |
| Julia | Keeps her distance; Set Trap when a hostile is within 3, Quick Fix on the weakest when hostiles gather, Flashbang where two or more hostiles stand. |
| Katrina | Stays mid-party; with two or more hostiles near, calls one creature at a time, the strongest that is ready (the bee, then the wolf, then her dogs), and another only once it has gone. |

**How.** Exult's combat schedule decides each step in
`Combat_schedule::now_what` and picks foes in `find_foe` by the actor's
attack mode (nearest, weakest, strongest, berserk, protect, defend, flank,
flee, random, manual). Class AI is a layer on the host, checked about twice
a second for each companion set to it:

- **Skills:** the class's rules above, read from the party's health and
  flags and the hostiles near; when one fires, it goes through the same
  code as a player's skill (same checks, cooldowns, effects).
- **Standing:** where U7 has a mode that fits, use it (Jaana in "defend",
  Dupre in "protect" or "nearest"); where it doesn't, a small step of our
  own in `now_what` before the usual approach: a ranged fighter with a foe
  beside it steps away first (Shamino, Iolo, Spark); a healer with nobody
  to heal moves toward the middle of the party.
- The companion's own attack mode is kept and put back when Class AI is
  turned off, and saves write it unchanged.

The Keyring mod's `aiCastHealing` (`content/bgkeyring/src/spells/spell_ai.uc`)
is a good model of a healer's priorities: the poisoned, cursed, paralyzed,
then the badly wounded, wounded and hurt.

## How skills work

**On the host, in the engine.** Skills are C++ on the host (a new
`friendskills.cc`), not usecode. A guest sends "use skill N, target T"; the
host checks it and carries it out; everything it changes already reaches
guests:

- buffs as flags: protection, invisibility, paralysis (party status already
  sends them);
- heals and cures: health and the poison, paralysis and curse flags (party
  status and creature health already sent);
- arrows and stones: `Projectile_effect` from the companion to each target
  (missiles already sent; the host does the hits through the usual combat
  code);
- the look: `Sprites_effect` on each actor touched (already sent);
- the words: the emote path (`Friend_mode_emote`), already sent.

So the network part is small: one request message (skill, target), the
cooldowns back to the player who owns them, the class AI settings, and
timed bonuses in party status (below).

**Built from what U7 already has.** Each skill is a short recipe over Exult's
own verbs, so hit chance, damage, armour and saves stay U7's:

- Song of Warding, Stand Fast: set `protection` (+3 defence, and no poison
  or curse from hits; `Actor::try_to_hit`, `Actor::figure_hit_points`).
- Vanish: set `invisible` with a short timer.
- Entangle, Snare: set `paralyzed` with a short timer, with a roll against
  the target's strength, as hits with powers do (`roll_to_win`).
- Stun Shot: a real shot, then `paralyzed` for a moment, with the same kind of roll.
- Mass Cure (was Purify): clear `poisoned`, `paralyzed`, `cursed` (like the Cure spell).
- Mending Circle, Lay on Hands, Patch Up, Second Wind: raise health, capped
  at strength (as the Heal spells do).
- Volley, Aimed Shot, Trick Shot: fire the companion's readied bow,
  crossbow or sling with its ammunition, through the same attack code a
  normal shot uses, one piece of ammunition per shot. With fewer hostiles,
  fewer shots.
- Challenge: point each hostile in the circle at Dupre as its opponent
  for a few seconds.
- Song of Peace, Calm Beasts: drop those hostiles' opponent and keep them
  from picking a new one for a few seconds (not a change of alignment:
  they are still hostile afterwards).
- Firebomb: an explosion at the pick (`Explosion_effect`, as powder kegs),
  which already damages and is sent.

**What U7 lacks: timed bonuses.** Battle Hymn and Hunter's Mark want "+N to
strength and combat for 30 seconds". U7's only stat buff is might, which
doubles strength, dexterity and intelligence for one to two minutes: far too
strong for a song. So add a small list of timed bonuses per actor on the
host (property, amount, ends at), read where Exult works out effective
stats (`Actor::get_effective_prop`, which combat reads through
`Get_effective_prop` in actors.cc) and sent with party status. Not saved: a
load drops them. U7's maximum health is strength, so "more health" is a
strength bonus, and health over the maximum when the song ends is cut back.

**The circle.** Area skills use a radius of their own (about 6 to 8 tiles,
much smaller than targeting's 40), drawn with the range circle code while
the key is held and for a moment after. Single-target attacks use the Q
pick. A healer's single-target skills (Lay on Hands, Patch Up) take the most
hurt party member in the circle, since guests can't Q the party.

**When.** Not during a host conversation or pause, not asleep, paralyzed or
dead; attacks only in combat mode and on hostiles unless the player chose
the fight with C, as targeting does now (so no crimes by accident). Heals
and cures work any time.

**An option.** Game menu → Target System (or its own page): "Companion
skills", off by default, set by the host for the whole session (it is a game
rule, not a control). Off means U7 as it was: no bar, no class AI, items
stay ordinary items. Guests are told when it changes.

## The Keyring mod

The mod remembered from years ago is Marzo Junior's Keyring mod for Black
Gate, which ships with Exult (`content/bgkeyring`). It gives each companion
a spellcasting item (Shamino's Ankh, Dupre's Amulet, Julia's hammer and
others), cast from a conversation with mana and reagents, with classes such
as `BARD_CLASS` (half-level casting) and `HEALER`, and new NPC-only spells.

Taken from it: the class idea, a personal item per companion, and the
healing priorities. Not taken: it is a usecode mod with its own map, items
and shapes, so everyone would need to run it, and its casting is a
conversation, too slow for a fight and one that would pause guests.

## Steps

1. **Spike: Mending Circle on the host.** A debug key heals the party in a
   circle around Jaana, with the emote and a sprite effect. Proves the recipe
   shape, the circle and the sync with nothing new on the wire.
2. **One guest skill end to end.** The request message, the host's checks,
   cooldowns back to the guest, the "not ready" notice. Shamino's Volley,
   since it touches combat, missiles and ammunition.
3. **The guests' skill bar,** with the mouse.
4. **Timed bonuses** (effective stats, party status) and Battle Hymn.
5. **The first five companions** (Iolo, Shamino, Jaana, Dupre, Spark), all
   their skills, and the option.
6. **Class items:** find the homes, place the items once per game, the
   names, the grey bar, "Restore class items".
7. **Class AI** for the first five, and the per-companion setting.
8. **The Avatar's spell bar:** choosing spells, casting from the bar, the
   pick answering the crosshair. Independent of the rest; could come first,
   since it helps single player too.
9. **Keys** for both bars: the last phase (the mouse is enough until then);
   a controller gets its own radial scheme.
10. **Play it** with a host and two guests, then tune cooldowns and amounts.
11. **The other four companions.**

## Backlog

- **Skill icons (the user may draw them).** One `.shp` file, for example
  `data/friend_icons.shp`, one frame per skill, 8-bit in Ultima VII's
  palette (index 255 transparent), about 24 x 24 or 32 x 32 game pixels
  (the HUD scales them). Made with Aseprite and Exult's plug-in
  (`tools/aseprite_plugin`: Sprite → Convert to U7 Palette, File → Export
  to U7 SHP), or as PNGs in the U7 palette packed with Exult's `ipack`
  (`tools/ipack.txt`). In the code: each skill gets an icon frame number;
  the bar loads the file from `<DATA>` and falls back to the name when there
  is no file or no frame. The Avatar's spells can keep the spellbook's own
  pictures or get their own.

- **Greyed-out slots look glitchy** (seen 2026-10-20): the every-other-pixel
  shading turns noisy once the HUD is scaled up. Use a cleaner look, for
  example a solid dark panel with the icon dimmed through the palette, or
  only the icon's outline.

## Open questions

- Keys for both bars: cycle-and-use, Alt+numbers, or something else.
- The spell bar: how many slots, and whether spells go on it by a button in
  the spellbook or by dragging.
- A starter skill without the item, or none.
- Which existing shape stands for each item, and the homes (the table above).
- Numbers: how much Battle Hymn adds, durations, radius, each cooldown.
- Song of Peace and Calm Beasts: is "no opponent for a few seconds" enough,
  or do they need a real "calm" state that survives the combat schedule
  picking again?
- Class AI default for a new game: Normal (U7 as it was) or Class.
- Serpent Isle (Phase 10) has its own companions (Boydon, Petra, Windrel and
  others): their own skills and items, or Black Gate only at first.
