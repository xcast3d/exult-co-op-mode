# Friend Mode development

Friend Mode is an experimental cooperative multiplayer extension for Exult. The
project vision and roadmap live in [the original project brief](../ultima-7-friend-mode.md).

Development starts from a verified, unmodified Exult build. Keep the story
Avatar, the human-controlled actor, and the network host as separate concepts;
the first proof of concept changes only the local controller and camera target.

## Current milestone

Phases 0 to 7 are built, and so is most of Phase 9 (WASD, the Target
System) and the Class System; the brief's [roadmap](../ultima-7-friend-mode.md#roadmap)
keeps the boxes. Next is **Phase 8**, the last step before a release:
lobby codes over WebRTC are built on the Mac (`friendrtc.cc`, see
[the lobby](lobby.md)) and need a hand test; then a TURN relay fallback,
the Windows side, an optional session password, and Mac and Windows builds
for release. Still
waiting for more testers: Phase 6's three-player exit test and two people
grabbing the same item.

Phase 1 (local possession), for the record:

- [x] Import the official Exult source and history.
- [x] Configure GitHub as `upstream` and Bitbucket as `origin`.
- [x] Configure and compile an unmodified Exult binary on macOS.
- [x] Launch with legally obtained Ultima VII data and record a baseline save.
- [x] Trace the initial input, camera, and party-AI control points.
- [x] Implement and manually verify local Avatar/Iolo possession.
- [x] Restore Avatar control automatically across save/load.
- [x] Restore Avatar control automatically across unsafe scripted transitions.
- [x] Verify dialogue interactions return to and use the canonical Avatar.
- [x] Allow companion-owned paperdolls, bags, ordinary containers, and item
  dragging while preserving companion control.
- [x] Restrict companion controllers to their own inventory while retaining
  Avatar access to all party inventories.

See [macOS setup](macos-setup.md) and [control-point notes](control-points.md).
To play with someone outside your network, see [internet play](internet-play.md);
for Windows, the [Windows build plan](windows-build.md); for the browser
version, [playing in a web browser](web-browser.md).
For a repeatable two-player pass, use the [test checklist](test-checklist.md);
In a test build (made with `CPPFLAGS=-DFRIEND_TEST_KEYS`; release builds
have none of this since 2026-10-26), on the host, **Ctrl+F9** places a test
kit of usable objects around the Avatar, **F9** and **F8** gather two groups
of companions into the party (levelled for the Class System, with reagents
and a full spellbook), **Shift+F9** places a fight (two each of troll,
headless and skeleton) and **Shift+F8** a rat, a wolf and a rabbit.
For now (2026-10-28, demo videos) every build has a temporary **F9** that
places that fight in any game, co-op or not, ahead of the test keys; it is
removed before a release ([Before a
release](macos-setup.md#before-a-release-mac-and-windows)).
The guest-action boundary is defined in the [interaction authority
policy](interaction-policy.md). The Target System's item groups are in
[targeting groups](targeting-groups.md); a gamepad plan in [controller
support](controller-plan.md); the Phase 8 [lobby](lobby.md); what is
protected and what is still open in [security](security.md); how the work splits into layers for forks in [branching](branching.md); class skills for companions in [companion skills](companion-skills.md);
the host leading a companion in [leading a
companion](leading-companions.md); what could go to
upstream Exult in
[upstreaming notes](upstreaming.md).

## Phase 2 checkpoint

The first loopback networking slice is in place. A host can listen on a local
TCP port and a guest can use a dedicated **Connect to Host** title-menu entry.
The peers exchange a versioned handshake and reject incompatible game variants.
This has been verified with two isolated Exult instances on one Mac.

The temporary local guest profile and copied runtime data live under
`.friend-mode-local/`, which is ignored by Git.

The next assignment slice now transfers the host's eligible companion roster,
shows the guest a **Select Your Character** carousel using the companions'
existing framed portraits, names, short descriptions, and Exult arrow artwork,
and has the host validate the requested assignment. The guest then takes local
input and camera control of the accepted companion. This has been verified with
Iolo over two local instances. Moving connection-address entry and this selector
into one uninterrupted pre-game flow remains a later UI refinement.

The current synchronization prototype has also been verified for Avatar and
companion position/facing, nearby NPC and creature position/facing, companion
movement across building thresholds, client-specific roofs, and disconnect
notification. The guest camera and movement cursor remain anchored to the
assigned companion. Guest-side NPC simulation is suspended so it does not fight
host snapshots.

Inspection labels from single clicks remain local to the client that generated
them. Network guests can use their own inventory and ordinary containers, but
NPC/sign activations that could start Avatar-owned story Usecode are currently
blocked without revoking companion control.

Host-generated NPC remarks and animal barks are now replicated to the guest.
Host-started conversations also place the guest in a temporary spectator
state: movement and interaction are paused, portraits, text, and response
choices are mirrored, and only the host can choose a response. The guest
resumes control of the same companion when the conversation ends. All four
behaviors have been verified together over two local instances.

## Next session

**Test round, 2026-10-25 (tested on host and guest; no protocol change).**
Labels from a click (names, remarks, "Locked") sit centred just above
the head as chat does, not over it at the picture's top left as in Exult
(`Text_effect::Figure_text_pos`; things get no drop, their pictures have no
headroom). "/" opens the chat line with "/" typed. Pools of water work on
guests: the fountain animates on each machine by itself, so the guest's
frame rarely matched the host's and the host refused the drink as "not
found"; a map path to an animated thing now matches any frame on its tile
(`Friend_read_path`). Dupre's Shield Ally is first (level 1, 30 s) and Lay
on Hands second (level 4, 60 s); icons swapped to match.

**Feedback round, 2026-10-23: batch G built and tested (no protocol
change).** After a full live test ([the feedback
round](feedback-2026-10-23.md)): the guest's hold button says "Stop" (was
"Take over", clipped); E in windows goes top left to bottom right as items
show, and starts again in a newly opened window; Space opening a container
picks its first item (so E, Space, Shift+Space takes all); Escape clears
the pick before closing windows or opening the menu (a pick in a window
clears and closes it in one press); Shift+click on a face
on a guest takes that companion. Batch H, same day (tested): R toggles
running (Caps Lock no longer does; the status bar style is Shift+R); the
skill bar on the left or right sits inside the status bar's side columns.
Space out of combat does nothing on a
creature with no conversation (a rabbit said "The Avatar must do this" on
a guest). Batch I, same day (tested): the test kit's companions in two groups,
F9 (Iolo, Shamino, Dupre, Jaana, Spark) and F8 (Sentri, Julia, Katrina,
Tseramed), at most seven; gathering leaves alone the companions players
have (a guest's one fought the follow AI). Then skill changes (tested): bow skills
reach 24 tiles (past the bow's own 20), Spark's sling keeps 16; Jaana says
words of power. Second round (tested): Jaana's are Greater Heal, Mass
Cure and Restoration (U7's spells, whole party); party buffs reach the
whole party with no range; skills on one ally (Greater Heal, Lay on Hands,
Quick Fix, Shield Ally within 6) choose by need from the whole party; with
no Q pick, skills on one foe ask with the crosshair, or choose for
themselves with the Class System page's "Skills on a foe: Auto" (Hunter's
Mark, Sleep Arrow, Crippling Shot and Master's Strike the strongest foe,
Flashbang the biggest group). Shift+F9 places a fight (two each of troll,
headless and skeleton); the old rat, wolf and rabbit are Shift+F8.

**Feedback round, 2026-10-22: batch A built and tested (no protocol
change).** Unlock levels 1, 4, 7 (were 1, 3, 6); Iolo's Song of Peace
first; Sentri's Master's Strike a lunge (range 4, darting beside the pick);
skill icons 36x36 (were 40); after any skill every skill of that character
waits 5 seconds (was 1.5), counted down on the bar. Batches B to F (badges,
the arrow over windows, the preview crash, Space rules, Caps Lock to run,
the sort and take-all icons, the hold button, gavel, bucket and fountains,
conversation choices by key) are planned in [the feedback
round](feedback-2026-10-22.md).

**Batch B built and tested the same day** (then the armour badge made blue and lined up with U7's badges): the skill status icons moved from
under people to badges on the status bar's faces (party only; foes keep
the words over their heads); the target arrow shows over a window when the
pick is inside it; the effect preview's crash at sprite 20 (an empty
sprite, as is 25) is fixed in `Sprites_effect` itself.

**Batches C and D built and tested the same day:** Space uses what has a
use on the map (books, the gavel, the bucket) and Shift+Space takes it; in
someone else's window Space takes, Shift+Space takes all, Alt+Space splits;
Alt+Space drops some of one's own. Keys walk at the second fastest speed and
Caps Lock runs; Shift+S no longer opens Save/Restore. Small Sort and Take
all icons on container windows (Sort on every container now); a Hold /
Follow button under the minimap (or in the corner), the same as Y; the
minimap and enemy health bars moved to Game Display, with a Small / Large
minimap.

**Batch E built the same day (tested on the host; guests not yet):** guests and a led companion can
use the bucket, the gavel and pools of water (Black Gate's fountains; E
offers them with decor on, Space drinks); the window icons a little lower,
the hold button wider, skill icons 24x24.

**Batch F built the same day (tested on the host; guests not yet):** in conversations E and Shift+E
move a white highlight through the choices and Space picks it; on a guest
watching the Avatar talk, Space suggests the highlighted choice. E no
longer picks the 14th choice.

**Fixes after that, same day (host tested; guests not yet):** a companion
stands in for the Avatar in the item scripts it runs (the bucket, walks
and the scripts they schedule), so a guest or a led companion fills the
bucket itself; the gavel reaches 8 tiles; Shift+Tab on a guest takes the
next free companion; a dot in each player's minimap colour on the face of
the character they play; the minimap and hold button under conversations;
Q, E and Space skip people and things hidden under a roof.

**Leading a companion, step 1, built 2026-10-21 (no protocol change;
played since in the 2026-10-22 and 10-23 rounds, checklist not ticked).** The host and a single player press Shift+Tab to lead the next
companion (one no guest plays), with its skill bar and the Q pick; combat
through targeting and double-clicks uses it; it follows the guests' rules
("The Avatar must do this" for people, merchants included); the guests'
leash applies. Tested: Shift+Tab cycling. Step 2, built the same day
(tested: formation, roofs): the party, the Avatar included, follows the
companion one leads (`Game_window::get_party_leader`). Step 3 (tested): Y
on the host and in single player holds the party; held companions strike
hostiles in reach without moving; Shift+click on a face leads that one.
Step 4 (untested): saves keep the lead, the Avatar takes over when spoken
to and the companion is led again after, notices when the one led falls,
and guests see the host on the companion it leads. See [leading a
companion](leading-companions.md) and the [test checklist](test-checklist.md).

**Skill icons, built 2026-10-21 (untested in a long session).** 40x40 (36x36 since 2026-10-22) pictures per skill in `data/friend/skills/`, colour
(transparent) and parchment, chosen on the Class System page ("Skill
icons", each machine); the name shows in the tooltip, unusable ones are
dimmed. Built from `scratch/class-skill-icons/` (not in the repo) with
`build.py`. Credits for the game-icons.net motifs (CC BY 3.0) are in
`data/friend/skills/CREDITS.txt`. Installed with the game: `Makefile.mingw`
(`install`, `debug`, and so the GitHub Windows build), the macOS bundle
(`macosx/macosx.am`) and `make install` (`data/Makefile.am`) copy the
folder to `data/friend/skills/`. Since 2026-10-22 they are 24x24.

**Pauses in co-op, built 2026-10-20 (no protocol change; untested).**
Only the host pauses the game. In co-op, windows (backpacks, paperdolls)
never pause the world on any machine (`Gump_manager::gumps_dont_pause_game`
says so while `Friend_mode_in_session`; each window remembers whether it
paused, so turning co-op on or off with windows open can't unbalance the
queue). Combat pause does nothing in co-op, host included (decided: no
pausing in a co-op game; the only exceptions are Exult's menus and the
Avatar's conversations, which stop Exult's main loop and can't be
changed). While a guest has one of
Exult's menus open (they stop its main loop), the host's AI drives its
companion quietly (guest_follow 2 and 3: no bar, no notice), and hands it
back when the menu closes. The host's menus still pause everyone.

**Skills changed after testing, built 2026-10-20 (protocol version 35);
played since, and changed again in the 2026-10-22 and 10-23 rounds.** From the user's sheet after playing with testers; the
table is in [companion skills](companion-skills.md), the checks in "Class
System" in the [test checklist](test-checklist.md).

- Every bar runs level 1, 3, 6; cooldowns 30, 60, 90 by level (a few
  exceptions). Iolo, Shamino, Jaana, Dupre, Sentri reordered.
- **New skills:** Jaana's Greater Heal (full heal on the most hurt),
  Dupre's Shield Ally (melee blows on an ally land on him for 15 s, shield
  needed; the Class AI keeps him near his ward, `Friend_class_guardian` in
  `Actor::apply_damage`), Spark's Dirty Trick (hostiles within 12 don't
  attack for 3 s, `Friend_class_calmed` in `Combat_schedule::now_what`),
  Julia's Set Trap (caltrops at her feet hurt hostiles 5 s), Katrina's
  Summon Dogs (two), Summon Wolf and Summon Bee. Gone: Entangle, Stand
  Fast, Time Bomb, Healing Field, Sanctuary.
- Riposte's answer always hits (`Friend_class_sure_hit`); Blade Dance
  reaches 6 tiles by darting Sentri beside each foe in turn.

**Class System, built 2026-10-20 (protocol version 33); played and
tuned since** (the 2026-10-22 and 10-23 feedback rounds; the checklist's
Class System boxes aren't ticked). See "Class System" in the
[test checklist](test-checklist.md) and the plan in
[companion skills](companion-skills.md).

- **Its own files:** `friendclass.cc` and `friendclass.h`; hooks in
  `exult.cc` (each frame, messages, world loaded, settings), the game menu,
  `Actor::get_effective_prop` (timed bonuses), `Gump::is_hud` (the bar
  draws at the HUD's scale like the shortcut bar), and an `aim` argument
  for `Spellbook_object::do_spell` (the spell bar's pick answers the
  crosshair).
- **Game menu → Class System:** Enable (on by default since 2026-10-24), Skill bar position (above the
  status bar, four corners), Show skill ranges, Skills on a foe (Ask /
  Auto, 2026-10-23), Class AI, Require class
  items (off; no items exist yet).
- **Skill bar** for a guest's companion; **spell bar** for the Avatar
  (three slots filled by dragging spells from the spellbook, kept with the
  save; since 2026-10-23). Mouse, or Ctrl+1 to Ctrl+3.
- **Definitions** per companion (Iolo, Shamino, Jaana, Dupre, Spark): id,
  name, icon (a spellbook picture), cooldown (60 s), range and whether it
  is drawn, type, target, unlock level (1, 4, 8), effect, words.
- **Effects built:** heal, cure, flags (protection, invisibility), hinder
  (short paralysis or knockdown, with a roll), volley (real shots with the
  readied weapon through the combat code), timed strength and combat
  bonuses. Skills never run a spell's script; each has a look made of
  U7's own pieces instead: sprites on the user and targets, a missile
  picture, a sound. Song of Peace puts hostiles in its circle to sleep for
  5 seconds (each rolls); Dupre's Challenge turns hostiles in range on
  him for 6 seconds.
- **The last four companions** (the user's design, 2026-10-20): Sentri
  the Swordmaster (Riposte, Blade Dance, Master's Strike), Tseramed the
  Huntsman (Sleep Arrow, Crippling Shot, Hunter's Mark), Julia the Tinker
  (Quick Fix, Flashbang, Time Bomb), Katrina the Shepherd (Healing Field,
  Summon Creature, Sanctuary). New effects for them: shot riders, armour
  boosts and piercing, marks, a riposte stance, thrown flash and bomb, a
  healing field, a summoned creature; hooks in `Actor::apply_damage` and
  `Combat_schedule::attack_target`. Spark's Knockdown became Stun Shot.
- **After the review** (protocol 33): guests' picks go with their skills;
  no skills during host conversations or pauses; levels 1, 3, 6;
  cooldowns 15/30/60 by kind; a clock that stops while the game is paused;
  saves dismiss summons and lift short stuns; status icons under people
  (green up arrow, blue shield, red down arrow), sent to guests; a line on
  what each skill does; Battle Hymn's strength raises health; "Require
  class items" off the page.
- **Effect preview** on the Class System page (removed 2026-10-26, once
  every skill's look was chosen): stepped through sprites, missile pictures
  and sounds, playing each on your character with its number.
- **Class AI:** chooses and uses skills by type (health, status, how many
  hostiles, a hostile on a hurt companion), and places companions in a
  fight (`Friend_class_place`, from `Combat_schedule::now_what`): ranged
  ones step away from a foe beside them, Jaana walks into heal range or
  falls back to the party, the front line closes in as U7 does.

**Fixes from the first internet test, built 2026-10-19 (protocol
version 31); not tested yet.** A second person joined the host's public
address from his own home (Mac host, his PC and Mac). See "Fixes from the
first internet test" in the [test checklist](test-checklist.md).

- **Join screen flickered** on Mac and PC: each frame was shown black
  first (`clear_screen(true)`). The picker had the same.
- **Join remembers** the last player name and host address
  (`config/friend/player_name`, `config/friend/last_host`).
- **Guest stuck in "The Avatar is having a conversation"** after the host
  talked to Chantu, the Trinsic healer (ended with Escape, which picks
  "bye" as a click does), through switching, spectating and rejoining. The
  cause wasn't found, so two failsafes and log lines: the host's main loop
  only runs outside conversations, so a conversation still open there gets
  its end sent; and a guest whose Avatar moves 2 seconds after the last
  conversation message ends it. Both log, as do the host's begin and end.
- **Running as a guest stuttered over the internet** (fine locally, fine
  slowly). The host echoed every step it accepted, and the guest moved
  its companion to the echo a round trip late, pulling it back while
  running. And the host kept only a guest's latest move, so steps that
  arrived together looked like a jump it refused. Moves now carry a step
  number and queue on the host (up to 3 tiles apart); the actor state
  carries the last step handled, and the guest ignores echoes of steps it
  has walked past. Corrections (refusals, leash, teleporters, the host's
  AI) still move it.
- **Knocked out stopped the attack.** Exult stops everyone attacking an
  actor that falls unconscious; a pick being attacked now stays attacked
  until it dies (the Avatar's, and a guest's companion on the host), then
  the nearest hostile on screen is next, never a peaceful one.
- **Minimap tiny at large resolutions**: it is drawn in its own layer at
  Exult's HUD scale (`UiLayerHudGumps`), not one game pixel per cell.
- **Guests' merchant conversations** offer only name, job, bye and the
  answers leading to buying, selling or training (and a shop's "nothing"),
  per NPC, made by `tools/friend-mode/merchant_answers.py`; answers the
  shop routines add (lists, yes/no) all show. Only "buy" and "train"
  wasn't enough: 17 shops sell through "food", "weapons", "room" and the
  like.
- **Range at large resolutions.** Exult's combat dropped a foe off the
  host's screen while the attacker was on it (`Need_new_opponent`), so a
  guest seeing more than the host couldn't fight what only it saw. Now part
  of the targeting batch below.

**Targeting for everything, built 2026-10-19 (same protocol); tested on
one Mac and working** (Q, Space, C, E, Y, merchants, the minimap and the
range frame; walking as a guest still needs a try over the internet). See the brief's Phase 9 and "Targeting for everything" in the [test
checklist](test-checklist.md).

- **Space** out of combat talks to or uses the pick
  (`Game_window::double_clicked_object`, the double-click without the
  mouse); in combat it attacks. Q out of combat also offers the party,
  last (the Avatar only).
- **E** cycles containers and bodies (`Friend_mode_cycle_object`), green
  arrow, Space opens. Guests' follow moved to **Y**.
- **Range**: on screen and at most 40 tiles each way
  (`Game_window::friend_target_range`). Bigger screens get a faint
  square frame (two layers, fill and border, by layer alpha) in combat
  and after Q or E. The host's combat keeps a guest companion's foe while
  in range of the companion (`Out_of_sight` in combat.cc); the guest's
  item and creature sync radius went from 2 to 3 chunks.

**Serpent Isle (decided 2026-10-18):** not supported yet. Friend Mode is
Black Gate only; the co-op core is game-independent, but guests' world use
(shape lists), shops and trainers (`GAME_BG`), picker descriptions, the
test kit and Serpent Isle's companion-heavy story need their own pass.
Planned as Phase 10, after the Phase 8 release (see the brief's roadmap).

**Three quality-of-life features, built 2026-10-18 (protocol version
30); not tested yet.** See "Main Menu, Suggest a reply, WASD" in the
[test checklist](test-checklist.md).

- **Main Menu** in the game menu (Escape), above Quit, with "Return to the
  main menu?". Exult can't run its title menu again inside one game, so
  it closes the co-op session (guests see the host leave; a guest's
  companion is held for a rejoin), then starts Exult again with the same
  arguments minus `--friend-host`/`--friend-join`/`--friend-port`, plus
  `--skip-intro`, and lands on the title menu.
- **Suggest a reply.** While the Avatar talks, a guest clicks one of the
  choices. The host checks it is on offer, draws it in that player's
  color (their minimap dot color) and passes it to every guest; the log
  says "Marc suggests: Name". One suggestion per player, cleared when the
  Avatar picks. New text message `Text_kind::suggestion`; the host now
  reads guests' messages while waiting for a click (`Get_click`), since
  its main loop is not running during a conversation.
- **WASD movement** (Game Input → "WASD movement", on by default since 2026-10-24, kept
  in the config). W, A, S, D walk like the arrow keys, two at once
  diagonally; Shift with one of them does what the key alone did
  (Shift+W the watch, Shift+S Save/Restore). Guests walk through the
  usual host requests.

First try (host only): WASD lagged, the game seemed to freeze, mostly
when attacking or toggling combat, and Shift+W did nothing. Found: a new
direction waited for the current walk (about 6 tiles) to finish; keys were
tracked by their key-up events, so a W let go while Shift, Cmd or a menu
had the keyboard stayed held and the Avatar kept walking (stuck in combat);
and Shift+W could arrive as a capital W, which nothing is bound to. Now the
main loop reads W, A, S, D by keyboard position every frame and re-aims at
once; Shift+W and Shift+S are passed on as the plain keys. The single-click
attack is taken out until target cycling (next step). The host logs
"main loop waited N ms" for any pass over 750 ms, to catch freezes.

**Target cycling with Q** (with WASD movement on), built after that: Q
picks the next person or creature on screen who isn't in the party,
nearest first, Shift+Q the previous; it gets a lasting red outline (drawn
in `Actor::paint`, like the hit flash; `Game_window::set_action_target`).
First version attacked every pick; the user liked the feel and asked for
targeting only: out of combat Q just marks. In combat mode Q cycles only
hostiles and attacks the pick through the usual combat code (the
Avatar's target, or the guest's attack request), and turning combat on
(`toggle_combat` → `Friend_mode_combat_started`) attacks a hostile pick;
a peaceful one is left alone, so combat never starts a crime by accident
(double-click still attacks anyone). Tested and liked; then the red outline
(confused with the hit flash) became gold, which still read as a status
effect, so the pick now gets a small arrow over its head instead: white
out of combat, red in combat mode, in its own layer placed over the target
each frame (`Friend_mode_paint_target_arrow`). Q past the last target
clears the pick, the next Q starting again. "No target" replaces the
"Targeting" line when the pick clears (Q past the end, or the target dies
or leaves). **Space** with a target picked attacks it, peaceful or not,
turning combat on (`Friend_mode_force_attack`); in a fight against a
hostile pick it keeps pausing combat. The user loves the targeting and
thinks it may be enough without reworking combat.

Then: targeting works with WASD off too, and for guests (double-click was
hard on moving foes). Fighting through targeting puts the Avatar in
Exult's "manual" attack mode (the combat AI picks no foes itself) until
combat ends (`Friend_mode_take_manual`, `Friend_mode_combat_ended` from
`toggle_combat`); clearing the target or walking stops the attack, and
Space takes it up again (Space pauses combat only while the Avatar is
already attacking the pick). A save writes the Avatar's own mode. A
guest clearing its target in combat sends a stop (`friend_attack_stop`
in the guest attack message), and the host ends its companion's fight.

Then: in combat mode Q also picks peaceful people (hostiles first, then
the rest, nearest first within each), attacking only a hostile pick at
once; a peaceful pick waits for Space, and whatever was being attacked is
let go. The notice says "Targeting Deer" while only marked and "Attacking
Deer" once the attack starts (and "Targeting" again after walking stops
it). Before, a peaceful target in combat meant leaving combat, picking,
and coming back. Tested on a guest too ("Iolo attacks wolf for guest").

Targeting got its own Game Input option, "Targeting (Q, Space)", on by
default, separate from WASD movement; turning it off drops any pick.

Also tested and working: clearing the target or walking stops the
attack; Main Menu returns to the title menu; WASD and targeting on a
guest; Suggest a reply with two guests (two players suggesting the same
choice: it shows the later one's color; the log has both).

Found with two guests: chat sent while the Avatar was in a conversation
reached nobody until it ended (the host's conversation waits in
`Get_click`, outside the main loop). The chat handling is now one
function (`Friend_mode_take_chats`), also called from the host's click
wait, so chat, whispers and emotes go through during conversations. Retested:
it works; then, by request, chat during a conversation no longer shows
over the speaker (it sat on top of the conversation), only in the log.
The host couldn't chat during its own conversation either: its keys went
to the conversation's click loop. Typing in the chat line is now one
function (`Friend_mode_chat_event`), also called from that loop, and the
chat line has its own layer above the conversation (it was drawn into the
game view, under the conversation's layer). Enter is free there: Exult's
conversations pick choices with 1-9 and a-z (the Nth choice), not Enter;
while the chat line is open, typing goes to it, not to the choices.
Each pick shows "Targeting Wolf" near the top (one such line at a time). The outline goes when the target dies, leaves or is more
than 30 tiles off. A Q with no hostile on screen flashes the red X. Next
step: taking the controlled character's fight away from the party AI.

Second try: walking is fluid, and combat no longer gets stuck (walking
slows in combat, as in U7: Exult's combat walking speed). The only long
passes logged were outside combat (menus).

**Test round, 2026-10-18 (no protocol change).** Host and two guests on
one Mac.

Second pass, working: minimap dots, dungeon hiding and the setting kept;
U doing nothing; whispers; no arrows to other players; the player list
(Tab); J picker; Tab completion; rejoin within 3 minutes (straight back
into Spark); the spectator camera (X); autosave rotating through slots
997-999; dismissal in conversation; a host load without a guest's
companion; a co-op save loaded in single player. Found and changed:

- **Knocked out or asleep sent the guest to spectating.** The guest's own
  safety check (`Game_window::enforce_friend_mode_control_safety`, made
  for local possession) dropped control while its companion was asleep
  (which is also how Exult knocks people out) or paralyzed. A network
  guest now keeps it: the host refuses its moves meanwhile and frees it
  only if it dies or leaves the party. A rejoin or host load also keeps
  a companion that is asleep (`Friend_actor_is_eligible(..., keeping)`),
  and the guest takes it back asleep (`take_friend_mode_control`).
- **Emote asterisks were invisible:** U7's font 0 (overhead text) has a
  blank '*'. `Font::add_missing_asterisk()` draws one in the font's yellow
  and black outline when the fonts load.
- **L** now says "Chat Log Disabled" / "Chat Log Enabled".
- **The chat log showed at the top left, too big, and vanished when you
  walked.** Its layer was placed right, but fonts paint into the current
  render target, not the buffer they are given, so the lines went into the
  game view's top left (wiped by the next repaint) and the layer stayed
  empty. It now switches the render target to its layer, as overhead text
  does, so it stays pinned bottom left at the overhead text's size.

- **Notices were anchored in the world too** (overhead-style text at a
  map spot near the top, left behind as you walked). Notices, and
  spectator chat or whispers while the log is hidden, are now pinned like
  the log: centred near the top, 3 lines at most, 4 seconds each
  (`Friend_mode_paint_pinned` paints both). The log's bottom moved to 20
  pixels above the bottom of the view.

- **The chat log overlapped the status bar** ("Spectating. J: choose a
  companion...", following, paused). The bars stay where they are (clear
  of conversations); the log now stacks above whichever shows, as it does
  above the chat line.
- **Picker descriptions ran off the card** ("Longtime friend of the
  Avatar"). The card now wraps each line to its width.
- **Session notices showed twice** (near the top and in the log). Joins,
  leaves, companion changes and the host's "Autosaved" now go to the log
  while it is shown, and near the top only while it is hidden, like
  whispers (`Friend_mode_session_notice`). Notices for this machine alone
  ("Joined Marc's world", "Chat Log Enabled") stay near the top.
- **Backlog:** Return to Main Menu (in the brief's "Co-op quality of
  life").

Retested and working after these fixes: asleep companions stay the
guest's; L's notices; the chat log pinned bottom left; notices pinned
near the top; session notices in one place; the log above the status
bar; the picker card's wrapped text.

Also working after the commit: emote asterisks; the chat log's size and
whisper color; a rejoin and a host load while the companion is asleep;
the minimap (no slowdown).

Still to test: a companion dying (not just knocked out), knocked out and
paralyzed; "Iolo is free again" after 3 minutes; held items in saves and
in a newcomer's world; four players and a fifth window spectating;
scripted events; the Phase 7 leftovers (undoing a guest's merchant or
trainer conversation, spell shops, shop hours, clocks and the
telescope/sextant).

First pass, working: the corner minimap, chat, the chat log, whispers.
Changed after that (retested in the second pass unless noted):

- **World map on U shelved.** With the current leash everyone is near the
  Avatar, so the corner minimap is enough. The code is kept, unbuilt, in
  [shelved/world-map.cc](shelved/world-map.cc), with how to bring it back.
  U does nothing now.
- **Arrows to other players removed** (messy, and the minimap shows where
  everyone is). The arrow back to the Avatar stays.
- **Minimap dots halved:** 2x2 in the player's color with a black edge.
- **Chat log restyled:** its own layer, scaled like overhead chat (the
  text-effect UI size) instead of with the game view, white letters with
  the font's dark shadow, whispers in purple, its bottom 30 pixels above
  the bottom of the view (it was 44).
- **Emotes in the log** read "Marc bows." instead of "Marc: *bows*" (still
  "*bows*" over the actor). Guests recognize an emote by its text, so a
  player typing "*bows*" gets the same.

**Maps, built 2026-10-17; not tested yet** (no protocol change). Both are
drawn from the ground under each map chunk, so towns show as floors
without walls ([friendmap.cc](../../friendmap.cc)).

- **U: world map.** The whole world with a dot for every player (the
  Avatar and each player's companion, in their arrow colors) and a key
  naming them. Works in single player too. The first time it fills in over
  a second or so instead of freezing the game.
- **Corner minimap** (Game Engine options: "Corner minimap", off by
  default): the ground around the view, top right, with player dots.
  Overworld only; it hides in dungeons. Showing only the dungeon parts the
  party has explored is a later step.

**Co-op quality of life, built 2026-10-17 (protocol version 29); not
tested yet.** See "Co-op quality of life" in the
[test checklist](test-checklist.md).

- **Keys:** **U** world map (since shelved); hold **Tab** for the player list (Player, Character,
  Health); **J** opens the companion picker (it was Tab); **L** hides or
  shows the chat log; **X** (spectating) watches the next player; **E** (now **Y**)
  follows the Avatar. In the chat line, **Tab** completes a player's name
  (or a command after "/"); Tab again goes to the next match.
- **Rejoin:** a guest that drops keeps its companion for 3 minutes (the
  party's AI drives it and nobody else can pick it); reconnecting with the
  same player name goes straight back into it.
- **Chat log, whispers, emotes:** a small fading log, bottom left, of
  chat, whispers and session notices only. `/w name text` whispers through
  the host to one player. `/wave`, `/bow`, `/point`, `/cheer`, `/kneel` and
  `/sit` pose the speaker's actor on the host, with "*waves*" over it for
  everyone.
- **Arrows to players:** off-screen player companions get an edge arrow
  with a colored mark and the player's name.
- **Autosave:** every 5 minutes while a guest is connected, into slots
  997-999 ("Co-Op Autosave"), over the oldest; never a player's own saves.
- **Held items stay in saves.** An item a guest held mid-drag was missing
  from saves (now including autosaves) and from a joining guest's world;
  saves now put it back where it came from for the moment of saving.

**Phase 6, M5, built 2026-10-17 (protocol version 28); not tested yet.**
Follow mode, Spectate and Enter in the journal were tested and work, and
so did Iolo following again after the saved-waiting fix. "Watch" is now
called **Spectate** (the picker choice, the notices and the bar: "You are
spectating. Press Tab to choose a companion.").

- **Losing a companion.** When a guest's companion dies, leaves the party
  (a host conversation can dismiss it) or isn't in the party in a save the
  host loads, the host frees it and everyone sees "Iolo has fallen; Marc
  is now spectating" (or "has left the party", "is not in the party in
  this save"). The guest spectates and its picker opens by itself with the
  free companions, after any host conversation ends. Asleep or paralyzed
  companions stay their guest's.
- **Four players.** `.friend-mode-local/guest3.cfg` is a third guest (own
  gamedat and saves, audio off), for host plus three guests.

**Follow mode and Spectate, built and tested 2026-10-17 (protocol version
28).** For stepping away without losing your companion, or giving it up:

- **E: follow the Avatar** (moved to **Y** on 2026-10-19). The party's own AI takes your companion: it
  follows the Avatar and fights when the party fights, but stays yours
  (nobody else can pick it). A bar says "Following the Avatar. Move to take
  over." Walking, attacking, switching or pressing E again hands it back to
  you. E is `toggle_eggs` in the default keys, a cheat that guests can't
  use; the journal and the chat line still get the letter.
- **Enter in the journal** starts a new line again; the chat line opens
  with Enter only while the journal is closed.
- **Companions saved waiting (fixed after testing).** A companion a guest
  holds waits for that guest on the host, and the host's saves (and the
  world sent to joining guests) wrote it that way. Loaded later, it stood
  still for good: Iolo didn't follow after Spectate. Saves now write a guest's
  companion with its party schedule, and a released companion always
  follows (or fights, in combat mode), which also repairs older saves.
- **Spectate.** The Tab picker has a Spectate choice under the companion card
  (only while you have one): your companion goes back to the party, and
  everyone sees "<name> is now spectating".

**Phase 6, M3 and M4, built and tested 2026-10-17 (protocol version 27).**
The only problem found: Enter opened the chat line while writing in the
journal (fixed in version 28, below).

- **Notices on every machine.** Joining, entering the world as a
  companion, switching and leaving show on the host and on every guest (a
  newcomer gets its own "Joined ...'s world" instead). A guest joining
  while the host is in a menu now sees the pause bar.
- **Switching between guests.** Tab offers your companion and the free
  ones. If another guest takes the one you picked first, you see "That
  companion is taken" and the picker opens again with what is free now.
  The companion you leave goes back to the party's AI, or straight into
  the fight if the Avatar is in combat mode, and on your machine it moves
  as the host says. Switching waits while the Avatar is in a conversation.

**Phase 6, M1 and M2, built and tested 2026-10-17 (protocol version 26).**
Three windows on one Mac (host, guests on Iolo and Spark): movement, chat,
opening and closing windows, teleporting and a host load with both guests
connected all work. The host takes several guests:

- **One connection interface.** The socket code is now in `friendtcp.cc`
  behind `Friend_connection` and `Friend_listener` (`friendconn.h`), a
  reliable byte stream, so a WebRTC data channel (Phase 8) can slot in
  without touching the messages.
- **One session, several peers.** `Friend_mode_session` keeps a `Peer` per
  connection (framing, outgoing queue, name, half-arrived text and blobs,
  the latest requests). Guests get a player number (host 0, guests 1 and
  up) when accepted. Every send goes to every guest unless it names one;
  what the host reads says which guest sent it. Speech, chat, names and
  dialogue text share one text message.
- **A record per guest on the host** (`Friend_guest` in `exult.cc`):
  companion, last position sent, held item, leash timer, attack, journal
  window. `Game_window` keeps a list of reserved companions, and combat,
  eggs, teleporters, waking sleepers, item sync and "creatures stay awake
  near guests" work for each of them.
- **Player list.** The host sends every player's name and companion when it
  changes, so guests label each other's companions and name chat senders.
  Chat from a guest is shown on the host and passed on to the other guests.
- The picker offers each guest only companions nobody else has. Guests
  beyond one per companion plus 4 watching are turned away with "The
  host's game is full".
- The "last sent" caches stay shared: changes go to every guest, and a new
  guest's world download resends everything to everyone.

Fixed after that test: a lit lamp post (or any animated object) made item
sync resend its chunk four times a second, because its frame keeps
changing; animated objects now count as the first frame of their cycle.
Guests get their player number when accepted, so port probes no longer use
numbers up.

To test with three windows, see "Two guests" in the
[test checklist](test-checklist.md). The second guest uses
`.friend-mode-local/guest2.cfg` (own gamedat and saves, audio off).

Next: M3–M5 in the [multi-guest plan](multi-guest-plan.md) (join/leave
notices on every machine, Tab switching between guests, party-change edge
cases).

## Earlier sessions

The item pick-up model, the host pause bar and chest windows staying open on
the guest were tested on one Mac on 2026-10-16 and work. The two-tester
contention checks have not been run with a second person yet.

Also tested on 2026-10-16 and working: guests using food, potions (including
poison, which now shows on the guest), keys, lockpicks and teleporters;
"Locked" and "Unlocked" on the guest; and the larger **F9** test kit. Real
in-game teleporters still need a try, to see how the leash behaves after
long jumps.

The guest's wait bar now ends with every host script, so a one-line remark
with a portrait (no "bye") no longer leaves it stuck; tested and working.

Tested 2026-10-16 and working: creature sync (existing and new creatures,
deaths and bodies) and clock sync (join, Ctrl-T skips, hunger). Clouds are
random on each machine and keep drifting on the guest while the host is
paused; decided 2026-10-17 to leave them unsynced (nobody will notice). Protocol version 13.

Also tested 2026-10-16 and working: guests can't save, load or use cheats;
the host loading a save sends the new world to the guest; bodies stay near
either player and go when both are far away. Not separately tested: script,
missile-trap and button eggs doing nothing on the guest.

Found in testing:

- A guest whose companion isn't in the party after a host load is left
  without a body: the camera doesn't follow the Avatar, creatures don't show,
  and there is no way to pick a companion again (even after the host loads a
  save that has one). Chat still works and shows at the top of the host's
  view.
- The cheats option still shows on guests (it can't be turned on); hide it
  like Load/Save.
- Guests can switch to combat mode but their companion doesn't attack
  (built since; see below).
- Bottles, torches and other everyday objects with scripts say "The Avatar
  must do this"; guests should be able to use the safe ones.
- An inactive window can leave a stale mouse arrow drawn until it is clicked;
  cosmetic.

Tested 2026-10-16 and working (protocol version 14): everything in the list
below except grandfather clocks and the telescope/sextant (none found yet).
The map key (M) needs a map in the companion's pack, as in U7.

Built and tested 2026-10-16 (fixes for what that test round found):

- **Theft by guests.** When the host carries out a guest's item move, it
  runs Exult's theft check the way it does for the Avatar's drags (an item
  not okay to take, from outside the party, that really moved, outside
  dungeons), but with witnesses looking at the guest's companion: "heard
  something", warnings, then guards. `Game_window::theft` and
  `find_witness` take an optional thief for this.
- **Waking sleepers.** A sleeping NPC can be woken by the guest's companion
  walking close by, as by the Avatar (same distance, clear line and chance).
  The check only runs for NPCs near the host's view, which the leash mostly
  covers.
- **Painting** (rebuilt after testing; retested and working). The brush script
  asks for two clicks: double-click the brush (frames 2 and up of shape
  823), click the pigments (frames 0-1), then the easel. The guest picks
  both, and the host answers both clicks in order (the usecode machine has a
  small queue of click answers for this). Double-clicking the pigments alone
  gets a remark. Double-clicking the easel runs on the guest only.
- **Coin flip** (tested, left as is). Gold's script is a fixed exchange
  between the Avatar and Iolo, who must be nearby, with the text over them;
  it runs on the guest only because it closes open windows.
  The F9 kit now has an easel, and a brush and pigments in the supplies bag.

Built 2026-10-16:

- **Spectating and picking a companion again.** A guest without a companion
  follows the Avatar's view (roofs too), keeps getting items, creatures and
  the clock, and sees a bar: "You are watching. Press Tab to choose a
  companion." **Tab** opens the companion picker at any time (not during a
  host conversation or pause); picking another companion hands the old one
  back to the party's AI and the host says "<name> switches to <companion>".
  The cheats option is hidden on guests.
- **Everyday objects for guests.** Black Gate's object scripts were checked
  (the decompiler in tools/ucxt can't read the data, so a small script listed
  each script's engine calls and flags). Guests may now use bottles (the same
  script as food), torches (light and put out), the pocketwatch and
  grandfather clocks (say the time). Eating or drinking something that isn't
  yours calls the guards, as in single-player.
- **Read-only objects open on the guest only.** Their scripts only show
  something, so a guest runs them on its own copy and the host never sees
  it: signs, tombstones, books, scrolls, the cloth map, telescope, sextant,
  sundial, orrery viewer, crystal ball, and the pot and stuffed toy remarks.
  Three books and one scroll set a story flag when read; for those the story
  only counts when the Avatar reads them, so a guest can find one and point
  the Avatar to it. The mirror (sets a flag) and beds (sleep moves time and
  the party) stay Avatar-only.
- **Shortcut keys on guests** (F food, W watch, P lockpick, K keys and the
  like) used to run item scripts on the guest's own copy. They now use only
  the companion's own items and go through the host like a double-click; the
  cloth map (M) still opens locally.

Built and tested 2026-10-17:

- **Creatures appear around the guest.** Monster eggs hatch for the
  guest's companion as for the Avatar (`Egg_object::is_active`: "cached
  in" and "Avatar near" eggs of the monster type only). Script, music,
  weather and other eggs stay the Avatar's. So creatures such as the
  Trinsic stable horse appear wherever the guest walks inside the leash.
- **Creatures near the guest stay awake.** Exult stops creatures and NPCs
  that are off the host's screen and more than a screenful from the
  Avatar, and wakes them only when the host draws them. Now they also stay
  awake within a screenful of the guest's companion
  (`Game_window::near_remote_friend`), and the host wakes those four times
  a second (`Npc_actor::wake`), so they walk, fight and send their moves to
  the guest.

- **Missiles and spell effects on the guest** (protocol version 18).
  Every missile the host fires (arrows, bolts, thrown weapons, missile
  traps, spells cast as missiles) is sent with its start, end, picture and
  speed, and flies on the guest as a picture only
  (`Projectile_effect`'s visual-only constructor); the host does the hit
  and anything left behind. Sprite effects (spells, explosions, blood) are
  sent the same way, following the actor they belong to.
- **Health on the guest** (protocol version 19). The host sends the
  health of creatures and NPCs near the players when it changes. Exult
  shows it in two optional places: after a creature's name when clicked in
  combat mode (only when the combat mode option isn't "Original"), and as
  "-6(2)" (damage, health left) over whoever is hit, when Game Engine
  options has "combat shows hits" on. Each hit is now sent to the guest
  whatever the host's setting, and shown if the guest's own setting is on.
- **NPCs near the guest send their moves** even when the host isn't
  drawing them (the host used to send only those in Exult's 'nearby' list,
  around its own view).

Built and tested 2026-10-17 (protocol version 24): **guests can train**
with trainers; the trainee list shows only the guest's companion and the
host applies the training with the party's gold. Details:
[shops and trainers](shops-and-trainers.md).

Built and tested 2026-10-17 (protocol version 22): **stats,
experience and levels reach guests.** Party status used to carry only
health, food level and status effects; it now also has strength,
dexterity, intelligence, combat, mana, magic, training points and
experience for every party member. The level is worked out from
experience, so level-ups show in the guest's stats window too (Exult shows
no level-up message of its own). Tested: everything matches, and the
guest's companion's experience goes up from its fights.

Fixed after that test (protocol version 23): new creatures showed a hurt
health bar on the guest. The guest's copy of a creature rolls its own
random strength (the most health), which differed from the host's; the
host now sends strength with health. Retested and working.

Built and tested 2026-10-17 (protocol version 21): **guests can shop.**
Double-clicking a merchant runs the conversation on the guest only, with
the companion's portrait; each purchase is made on the host with the
party's gold. Story flags and NPC schedules now reach guests. Details:
[shops and trainers](shops-and-trainers.md).

Built and tested 2026-10-17 (protocol version 20). Also checked while
testing: a guest's companion dying leaves the guest watching, and Tab picks
another; the Avatar dying reloaded the last save (early in the story, before
the healer resurrection), and the guest followed the host into the reloaded
world.

- **World sounds on the guest.** Each side hears the right sound whoever
  caused it, quieter the farther away. The host sends the sounds the world
  makes: scripted object sounds (doors, levers, secret walls, food and
  potions), hits, animation and pickup sounds, and explosions, placed where
  they happen. Ambient sounds (birds, crickets) and menu clicks are each
  machine's own and aren't sent. Sounds loop only on the host.
- **Enemy health bars** (an option, off by default: Game Engine →
  "Enemy health bars"). A small green and red bar, like Ultima Online's,
  over creatures and NPCs outside the party that are hurt; unhurt townsfolk
  get none. Works on host and guest alike (the guest gets health from the
  host).
- **Faster messages and a connect time limit.** Both sides set
  `TCP_NODELAY`, so a step goes out at once instead of waiting up to about
  200 ms to be bundled. Joining gives up after 5 seconds per address
  instead of freezing for a minute (checked: a test connect and handshake
  took 0.05 s; an unreachable address gave up after 5.0 s).

Tested 2026-10-17 and working: creatures appear and move near the guest
(the Trinsic horse); missiles from the guest's companion and from the
Avatar show on the guest; "combat shows hits" labels show on the guest.
The Trinsic cat near the guest is awake (it meows) but stays put until the
Avatar comes close: its schedule follows the Avatar, as in single-player.


Tested 2026-10-17 and working: **Co-Op Mode in the menus**, so no
special launch options are needed. On the title menu, **Co-Op Mode** (below
Journey Onward) opens three choices: **Host a Game** switches hosting on
and returns to the menu (the line then reads "Co-Op Mode: Hosting"; pick it
again to stop), after which Start New Game or Journey Onward work as usual;
**Join a Game** goes straight to the player name and host address, then
downloads the host's world; **Back** (or Escape). In a running game, the
Escape menu has **Open to Co-Op** to start hosting there and then; guests
can join at once, as they already join mid-game. Either way the host sees
"Co-Op Mode is open on port 1992". `--friend-host` and `--friend-join`
still work as shortcuts. The default port is 1992, the year Ultima VII
came out (`--friend-port` changes it). The Join player name starts empty.

Next (see the roadmap in the [project brief](../ultima-7-friend-mode.md)):
Phase 6, more than one guest; Phase 7, shops and trainers for guests;
Phase 8, lobby codes over WebRTC and the release. The Windows build, guest
egg hatching, sounds and the rest of Phase 5 are done.

Tested 2026-10-16/17 and working (protocol version 17): guest combat,
the shared journal and the journal bar, and music from the host, as
described below.

- **Journal bar.** While one side has the journal open, the other sees a
  bar like the pause bar: "Someone is writing in the party journal."
- **Music from the host.** Every track the host starts or stops (region
  music, combat, victory, scripts) is sent to the guest and played there,
  including the current track when the guest joins. While connected, the
  guest's own copy of the world starts no music of its own, since its eggs
  and fights would pick differently. The guest needs "digital music" off
  unless OGG music files are installed; with it on and no files, Exult
  plays no music at all.
- **Host audio off.** The host's combat music checks (danger, attacked,
  guards) use the track the game last asked for
  (`Audio::current_music_track`), not the host's music player, which
  doesn't exist with audio off, so guests still get the music. Background
  music for towns, weather and dungeons still follows the host's own synth:
  Exult only plays it with digital music or an MT-32.
- **Battle music for the guest's fights.** The guest's companion counts
  like the Avatar: its first strike starts the battle tune and killing a
  hostile (evil) monster plays the victory tune. Peaceful animals (a horse)
  give none, as in U7. The host sends "no music" only when music stops,
  not every second.
- **Sound effects** need a Black Gate digital pack in `data/`
  (`sqsfxbg.flx`, Roland, or `jmsfx.flx`, from Exult's download page; both
  are git-ignored). This Mac now has the Roland pack. This build has no MIDI
  sound effects. Sounds made by the other machine's actions (a door the
  host opens) are not sent yet.

Built and tested 2026-10-16 (protocol version 15):

- **Guest combat** (start of Phase 5). In combat mode, a guest's
  double-click on a foe sends it to the host, which puts the companion on
  Exult's own combat schedule against that foe. The companion walks up and
  fights with whatever it has readied; its moves and attack frames come back
  as usual. It stops and waits for the guest again when the foe dies or is
  lost (out of the host's view, invisible, or the combat schedule picks
  another), or as soon as the guest walks. Foes are creatures and hostile
  NPCs (evil or chaotic) within 24 tiles; anyone peaceful gets "The Avatar
  must do this". Combat mode on a guest only changes what double-clicks do:
  the guest's own copies of the Avatar and party no longer start fights of
  their own. Creature health still isn't shown on the guest.
- **Shared journal.** The host's journal is the real one, and guests can
  read and write it (N). Writing in a note starts with the first change to
  it and ends when the cursor leaves it or the journal closes. The writer
  holds the note: the others see their name next to the note's date and
  get a red X if they try to type in it. The host decides who got a note
  first, and a guest that loses the race gets the finished text back. A
  guest's finished note (changed, or new) goes to the host, which applies
  it and sends the whole journal back; the host's own notes and automatic
  story notes go out the same way, checked twice a second. A guest's new
  note lands after the shared ones.


Waiting for a second tester: the two-tester contention checks and a real
in-game teleporter with the leash.

Untested: a guest double-clicking a scripted object it can't use (the bottle
or torch in the test kit's chest should say "The Avatar must do this").
NPCs saying it was tested and works.

Still to test:

- Undoing a guest's conversation (protocol version 25): after a merchant
  or trainer conversation, the host's story flags come back on the guest
  and the host resends the party's packs; a free gift from an NPC should
  vanish on the guest. Also a quick shop and training round on this build.
- Grandfather clocks and the telescope/sextant from a guest.
- Spell shops: a guest trying to buy a spell gets "The Avatar must do
  this" and pays nothing.
- Shop hours: a merchant outside business hours turns the guest away, as
  for the Avatar.

Later: a 30-minute soak test with two or more guests, and scripted events
with several guests (see "Scripted events" in the
[test checklist](test-checklist.md)).

Decided: the Avatar is the game master. It can open and change every party
member's inventory, including giving items to a guest's companion. Guests are
limited to their own character and cannot give items to others.

## Phase 3 checkpoint: player identity and overhead chat

**Connect to Host** now asks for a player name (up to 20 characters) and a host
address before loading. The host uses the Avatar's name. Both peers exchange
names after the handshake, and clicking a player-controlled actor or its party
portrait shows `Character (Player)`.

While connected, press **Enter** to open the `Say:` line, **Enter** again to
send, or **Escape** to cancel. Messages are limited to 80 printable ASCII
characters and are filtered again on receipt. Text appears centred above the
speaker in the shadowed font, wraps by pixel width (long words are split), and
stays up longer for longer messages. Up to three recent messages per speaker
stack upward; older ones are dropped. Players without an actor (spectators)
appear near the top of the view as `Name: text`.

Chat packets carry the sender's NPC number. The host ignores the guest's claim
and uses its own assignment; guests trust the host. Chat never runs Usecode.
The protocol version is now 5. Relaying chat between multiple guests waits on
multi-peer sessions.

### Joining with the host's world

After **Connect to Host**, the guest connects from the title menu and asks for
the host's world. The host writes its live game to a temporary save (the
normal save code, about 190 KB) and sends it as one variable-length message:
a header packet with the length, then raw bytes. The guest shows download
progress, unpacks the save, and loads it, so party, quest flags, items, doors
and NPCs match the host at the moment of joining. Protocol version is now 7.

Since 2026-10-24 the guest unpacks it into a folder of its own beside its
gamedat (`gamedat.coop`), never into the gamedat itself, so the guest's own
game in progress is untouched. Only the host keeps the game: a guest can't
save or load, and makes no crash save. The folder is deleted the next time
Exult starts (after Quit, Main Menu or a crash), and JOURNEY ONWARD then
continues the guest's own game.

### Doors, shutters and lamps

The host shares every change a game script makes to a world object's shape or
frame, hooked in `Usecode_internal::set_item_frame` and `set_item_shape`. That
covers doors opened by the Avatar, NPCs or scripts, shutters, lamps, levers and
so on. Decorative animation (torches, fountains) uses `change_frame` directly
and is not sent. Ultima VII opens doors by swapping shape (closed 270/376 to
open 432/433), so an object is identified by tile plus old shape and frame,
falling back to any door at that tile.

Guests may use doors and a short list of world toggles (Black Gate only for
now): shutters 290/291/322/372, light sources 336/338, sconces 435/481,
curtains 657/678, levers 787, switches 788, secret walls 828/845 and lamp posts (889 unlit, which swaps to 526 lit lamp when turned on). The guest's double-click asks the host,
which checks the shape and reach (the normal double-click reach rule) and then
uses it: doors the way NPCs do (quality zeroed so only the door's own toggle
runs), the others with a normal activation. Locked doors show "Locked". If the object's
script walks someone there first (levers do), the guest's companion walks, not
the Avatar (`Friend_mode_usecode_walker`). Night lighting is measured from the
actor the view follows instead of always the Avatar, so a guest sees light
around their own companion. Levers
and switches are allowed so guests can help with puzzles; their scripts can
open gates or trigger effects elsewhere, which is accepted. Protocol version is 9.

### Items on the map and in containers

The host reconciles movable objects with the guest by chunk (see the
[world sync plan](world-sync-plan.md), W3): every 250 ms it fingerprints the
objects and container contents near the players and sends any chunk that
changed; the guest makes its copy match. Party inventories are reconciled the
same way. A guest's drag and drop happens locally and is also sent to the host
as a move request; the host checks reach, ownership, weight and room, makes
the move, and resends so both sides match (W4). When only a container's contents
change, the guest updates them in place, so a chest or pack window stays open.

When the host opens a menu or a dialog (including the quantity prompt when
splitting a stack), Exult pauses its world, so the host tells guests: their
movement and interaction pause, with a bar at the bottom saying "The host has
paused the game." until the host returns. Chat still works.

Picking an item up takes it out of everyone's world until it is dropped or
put back: the host's drags show up on guests at once, and a guest's pick-up is
held by the host for that guest (see the world sync plan). Two people can't end
up with the same item. Protocol
version is now 10.

### Food, potions, keys and lockpicks

A guest can double-click food or a potion to use it on their own companion,
and a key or lockpick to get a crosshair and pick a door or chest. The guest
sends the item and the target to the host. The host checks that the item is
the guest's or within reach (and the target too), then runs the item's
normal script with the target click already answered, so eating, drinking,
unlocking and breaking picks all behave as in the game. The results come back
through the usual item and door sync. Black Gate shapes: food 377, potion 340,
key 641, lockpick 627. Protocol version is now 11.

Anything else a guest double-clicks that would run a script, including people
outside the party, shows "The Avatar must do this" instead. Locked chests show
"Locked".

Text spoken by doors, chests and other objects ("Locked", "Unlocked") is sent
to guests with the object's tile and shape. The host also sends each party
member's health, food level and status effects (poison, sleep, paralysis,
charm, curse, protection, might) when they change. The guest copies them
without starting its own timers, since poison and the rest run on the host,
and flashes the red outline when health drops. Protocol version is now 12.

### Teleporters

When the host accepts a guest's step, it fires the eggs on that tile the way
it would for any party member. A teleporter triggered by the guest's companion
sends only that companion, not the Avatar and the party. The guest's own copy
of the world never fires teleporters itself; the host's move comes back as
the companion's new position. When the Avatar takes a teleporter, the guest's
companion goes with the party. Leash rules still apply on arrival.

### Creatures

Monsters (creatures without an NPC number) get a network ID from the host the
first time they are near the players, in the same chunks item sync covers.
When the set of creatures there changes (one appears, dies or leaves), the
host sends the whole list: ID, shape, frame, tile, alignment and invisibility.
The guest creates the ones it lacks, standing still, and deletes the rest,
including creatures its own map loading brings in. It checks this four times
a second, so a creature lost to the guest's own map unloading comes back.
Moves go out as creature states by ID, only when a creature moved or changed
frame. Monster eggs do nothing on the guest; the host's eggs make the
creatures (and items, which item sync shares). Health and combat are still
the host's alone (Phase 5). **Shift+F8** on the host places a rat, a wolf and
a rabbit near the Avatar (Shift+F9 until 2026-10-23; it is a fight now).

### Clock and weather

The host sends its day, hour, minute, tick and current weather (from
`Effects_manager::get_weather`) when they change, checked twice a second, and
right after sending its world to a joining guest. The guest's clock stops
advancing on its own, so hunger, healing and hourly NPC schedule changes stay
with the host, and jumps to the host's time, updating the palette. A change
of weather starts the same effect on the guest (snow, storm, fog, clouds,
sparkles) until the host's weather changes again. Weather eggs do nothing on
the guest. Protocol version is now 13.

### Party mirroring and spectating

The host sends its party list when a guest joins and whenever the party
changes, and the guest updates its local party to match before taking control.
This lets a guest control a companion recruited after the guest's copy of the
world was made (for example Spark). A guest without a companion is a
spectator: movement and double-clicks are blocked so it cannot move its local
copy of the Avatar, and the camera follows the host's Avatar. Protocol version
is now 6.

### Leash

Guest companions stay near the Avatar, which keeps them inside the area the
host simulates (see the [independent-views research](independent-views-research.md)).
Distance is measured horizontally from the Avatar:

- Beyond one screen (20 tiles): "Straying from the Avatar", and a green arrow
  at the screen edge points back to the Avatar.
- At 32 tiles the guest cannot step further ("You must return to the Avatar").
  The guest checks before stepping, and the host refuses such moves and resends
  the true position. Steps back toward the Avatar are always allowed.
- If the Avatar moves away and leaves the companion beyond 32 tiles, the guest
  sees "The Avatar needs you"; after 5 seconds the host moves the companion
  beside the Avatar ("You are beckoned by the Avatar"). A change of map moves
  it at once.

Raising the radius later is the path toward free roaming.

**To explore: a longer leash.** Today it warns past 20 tiles (one screen)
and stops guests at 32. Could it be 3-4 screens (about 60-80 tiles)? Check
what the host simulates that far from the Avatar: chunk caching and
deleting around guests (`Game_window` keeps 3 chunks around each guest's
companion), item sync radius (2 chunks around a guest), keeping creatures
and NPCs awake near guests, schedules and egg hatching, and bandwidth. Not
urgent; a modest increase is enough.

### Session notices

The host shows notices near the top of the view when a guest joins, enters the
world as a companion, and leaves (the companion returns to party AI). The guest
sees a notice when it joins the host's world. Connections that are not Friend
Mode clients, such as local port probes, are logged and closed.

## Local possession prototype

Launch the modified build with `./exult --bg --nocrc`, load a save with an
active companion, and press `Control-F`. Local movement and camera focus switch
to the first eligible party companion. Press `Control-F` again to restore
Avatar control and the companion's prior AI schedule.

Saving or loading while controlling a companion automatically hands control
back to the Avatar first, preventing the temporary suspended-AI schedule from
being written into a save.

Scripted control, incapacitation, party removal, and story-bearing Usecode
interactions revoke companion control. Dialogue therefore remains anchored to
the canonical Avatar.

Safe local interactions are now separated from that boundary. A controlled
companion can open only its own paperdoll and nested bags, open ordinary
non-scripted containers, and move items using its own reach. The Avatar remains
the party administrator and can inspect every party member. Pressing `I` while
controlling a companion opens that companion directly; Avatar control retains
normal inventory cycling.

The `--nocrc` flag is required because the development key binding changes the
generated Exult resource archive. It does not disable validation of the
original Ultima VII game data.

## Dialogue authority

Network dialogue is host-authoritative. When the Avatar begins a conversation,
connected guests temporarily enter a spectator state: movement and world
interaction pause, the host's conversation text and portraits are mirrored,
and only the host may choose responses. A bar at the bottom of the guest's
screen, clear of the conversation, says "The Avatar is having a conversation.
Please wait." Guest control resumes on the same companion when dialogue ends.

Inspection labels, including single-click object names, remain local to each
client and are not replicated. Spoken world text—including scheduled NPC
remarks, animal barks, and scripted item speech—is host-authoritative and is
replicated to guests.

## Known prototype gaps

- Guests start from the host's world and items, doors and party inventories
  stay in sync near the players, but theft by guests and giving items to other
  party members are not handled yet; see the
  [world sync plan](world-sync-plan.md).
- A guest whose host disconnects sees a notice and Exult closes after a few
  seconds: Exult can't tear a game down and go back to the title menu in one
  process (the host's Main Menu button restarts Exult instead).
- Lobby codes (Phase 8) work on the Mac build only so far; the Windows
  build joins by address (port forwarding or Tailscale over the internet).
