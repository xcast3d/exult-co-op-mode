# Battle orders and enemy tactics (plan)

Decided 2026-11-06; being built on the `feature/battle-orders` branch.
The code is in `friendbattle.cc` and `friendbattle.h`, with hooks in
`combat.cc`, `exult.cc` and `friendclass.cc`.

## The problem

U7's combat is a mess once it starts: companions and enemies each do what they
like, and the player mostly waits and hopes. From reading the code:

- **The player's target doesn't steer the party.** A companion only takes the
  Avatar's target when it can find no other opponent (`find_opponents`).
- **Fighters keep a target** until it dies, turns invisible or goes off screen
  (`Need_new_opponent`), however the fight changes around them.
- **Nobody coordinates.** Each fighter picks alone, often the nearest.
- **A failed path becomes a random walk** of up to about 8 tiles (`approach_foe`).
- **Monsters roll a hidden attack mode when they spawn** (`monsters.cc`), so two
  identical trolls can behave differently, with no way to tell which is which.

U7's attack-mode button doesn't fix this. It only sets how a companion chooses
its next target, so a change does little until the current target is gone.

## The idea

Combat starts and runs as usual, and orders are optional. The person playing the
Avatar (the host, or a single player) can stop the fight for a moment and tell
the party what to do. Enemies behave by role, so there is something readable to
respond to, and they can lose their nerve.

No turn-based mode and no pausing to fight: a pause lasts only as long as giving
an order.

## Battle orders

### Giving one

1. In combat, the Hold / Follow button under the minimap reads **Orders**. It
   doesn't say what the current order is, to keep the text down; hovering says.
   **Ctrl+C** does the same, and **LT + Y** on a pad (where that button is hidden).
2. In single player the game pauses. In co-op nothing stops: see Co-op below.
3. A short list, the same window as the item menu (`Controller_choose`). The
   order in force has a star. Escape cancels and combat resumes.
4. Simple orders apply at once and combat resumes.
5. Orders on someone open the crosshair. Space takes the current Q pick; Q and E
   cycle, or click. Escape cancels.

Outside combat the button is Hold / Follow as before, Y keeps doing Hold in and
out of combat, and LT + Y is the utilities ring. Guests have no orders, and keep
their own button.

### The orders

| Order | Who | What companions do | Ends |
| --- | --- | --- | --- |
| Fight freely | - | As usual, by each one's attack mode and Class AI; also lets go of Hold | The default |
| Attack target | Anyone alive outside the party, peaceful or not; anything else is refused with "That isn't a foe" | All go for it, at once | It dies or is gone, or another order |
| Protect | A party member | Go for whoever is attacking them, or hostiles within 5 tiles of them; with none, stay within 3 tiles | They die, or another order |
| Fall back | - | Stop fighting and walk to the leader; there, strike only what is within weapon reach | Another order |
| Hold / Follow | - | The existing Hold; it replaces any other order | Another order |

**Who obeys:** companions nobody plays, in the combat schedule. Not the Avatar,
the companion being led, a companion a guest plays, or a charmed companion. An
order overrides a companion's attack mode for choosing a foe, but a companion
set to Flee still flees, and one with almost no health still runs. Class AI
skills and placing carry on: a healer keeps healing rather than charging.

A companion that gave up and went back to following while combat is still on (no
hostile about, or no path) is brought back by Attack and Protect, at once and then
about once a second while the order lasts. With no order too, a companion that
gave up comes back when a hostile is within 12 tiles and in the open, whether or
not that foe is fighting (the log says "back in the fight").

The end of combat clears the order. Orders aren't saved.

## Enemy tactics

A host option, **Enemy tactics** (Game menu, Combat System; on by default, kept in
`config/friend/combat/enemy_tactics`; it was on the Class System page, kept in
`config/friend/enemy_tactics`, until 2026-11-09). Off means U7's own behaviour. It runs only
on the host, for hostiles in the combat schedule; duels, tournaments and berserk
monsters keep U7's behaviour.

### Roles, by what they carry

Set by weapon rather than the hidden roll, so the same enemy with the same weapon
does the same thing each time.

| Role | Who | Chooses | Moves |
| --- | --- | --- | --- |
| Melee | Anything else | The nearest, counting foes already crowded by its allies as further away, so a pack spreads out instead of queueing | Closes in |
| Ranged | A weapon with reach over 3 | The nearest | Steps away from a foe within 2 tiles, then shoots |
| Caster | A spellbook | The party member with the least health | As ranged |

Choices are reviewed about every 2 seconds, and a fighter already beside its foe
stays on it, so targets don't flicker. Only the party and Avatar are considered;
with none in reach, U7's own choice stands.

A path that fails no longer becomes a random walk, for tacticians and for
companions under an order: the fighter drops the target and tries again shortly.

### Morale

Checked about twice a second over the hostiles nearby, for a group that has had
at least 3:

- **The leader falls.** The leader is a foe at least 25% stronger (strength plus
  combat) than the next. Each one left has a 50% chance to break.
- **The rout.** Half the group is gone, once per fight: a 35% chance each.

Breaking means running from the party for 6 to 10 seconds, then coming back to
fight. Only those that can: thinking creatures (`is_sentient`), that can move, not
berserk and not unkillable. So undead and slimes hold.

U7's `flee` mode does the running, with its direction changed to lead away from
the nearest party member (it was random). The monster's own mode returns after.

## Positional damage

Decided 2026-11-06, off by default while it was tuned; on by default since
2026-11-09. **Positional damage** on the Combat System page, kept as
`config/friend/combat/positional_damage` (was in the Game Engine options, as
`config/gameplay/combat/positional_damage`). A game rule: the host's in
co-op, and guests don't see the line.

Before this, Exult's hit and damage rolls ignored position and facing entirely.

| The blow lands from | Chance to hit (on the d30 roll) | Extra damage, before armour |
| --- | --- | --- |
| The front (ahead, or an eighth of a turn off) | Unchanged | None |
| The side (a quarter turn) | +6, about 20% | None |
| The back (more than a quarter turn) | +12, about 40%; always, on someone not fighting, asleep or paralyzed | +2 |

Raised 2026-11-09 (were +3 and +6): in a test against unaware trolls, the
position was found for each first blow from behind, but most missed, and a miss
turns the target round, so sneaking up rarely paid. The notices didn't show either:
Exult adds no text over someone who already has some, so a troll's bark won; the
notice now replaces it (host and guest). Tested 2026-11-09 on host and guest, with
the side blows too.

Found in the same test: a guest didn't see its own companion swing until the
host moved it. After the guest's last step, every update from the host carries
that step's number and was taken for its echo, frames and all; the frame is now
shown while the companion stands on that step.

- **Melee only.** Shots, spells and explosions are unchanged.
- **Everyone.** Enemies get it against the party too, so being surrounded is
  dangerous. Slimes and others with strange movement have no back.
- **The target's facing** is its current frame (`get_dir_facing`), one of four
  ways. Fighters face what they strike, so a back is mostly offered by something
  fighting someone else, or walking away.
- **A target can turn.** U7's step-back when hit turns it toward the attacker, so
  repeated blows from behind aren't all back hits. Left as it is for now.
- **To see it:** a landed blow says `*flanked*` (side) or `*back stab*` (back) over
  the target, as `*stunned*` does, at most every 2 seconds for one target. Guests
  see it too. With "Show hits" on in the same options, the number says so instead:
  `-5(12) flanked`, `-5(12) back stab`.

## Co-op

The host runs all of this, and guests see the results. Giving an order doesn't
stop anyone in co-op (decided 2026-11-06, to keep interruptions down): the fight
goes on under the list and crosshair, on the host and for guests, with no message.
The Avatar stops walking while choosing. See "Cursors and short lists" in the
[README](README.md).

## Files

| File | Change |
| --- | --- |
| `friendbattle.cc`, `.h` | New: orders, roles, morale, the option |
| `combat.cc` | `now_what` asks the new code each step; `approach_foe` and `run_away` each get a hook |
| `exult.cc` | Ctrl+C, LT + Y, the button, the crosshair prompt, the frame call |
| `friendbattle.cc` | The Combat System page (`Friend_combat_options`) and its settings |
| `Makefile.am`, `Makefile.common`, `msvcstuff/vs2019/*` | The new file |

## The Combat System page (2026-11-09)

Game menu → Combat System, under Target System and Class System, so each
change to U7's combat can be turned off. All on by default:

| Line | What it turns on | Whose |
| --- | --- | --- |
| Battle orders | The Orders menu (Ctrl+C, LT + Y, the minimap's button). Off: the button is Hold again, and a standing order ends. | The host's |
| Enemy tactics | Enemies by role, packs, morale. | The host's |
| Positional damage | *flanked* and *back stab*. | The host's |
| Friendly fire | Off (the default): party blasts spare the party. | The host's |
| Walking pace | Walking pace in combat, not U7's slower step. | Each player's |

A guest sees only Walking pace ("The host sets the rest."). The settings are
kept under `config/friend/combat/`, so the old keys' saved values don't carry
over (positional damage was saved as off).

## Combat movement

Decided 2026-11-06. The Avatar and companions led by the keys or mouse move at
walking pace at the slowest in combat, and with hostiles near, instead of U7's
in-between speed (`Mouse::medium_combat_speed_factor`, 150; walking is 200).
The Combat System's **Walking pace** (on by default, each machine's own, kept as
`config/friend/combat/walk_pace`) turns it off (`Friend_combat_speed_factor`). Running
is still not allowed in combat, and the short mouse arrow still gives the slowest
speed. Nothing else about the fight's timing changes.

## To test

Test fights: in a build with `CPPFLAGS=-DFRIEND_TEST_KEYS`, Shift+F9 on the host.

Orders:

- [ ] The button reads Orders only in combat, on the host and in single player;
      a guest's button is unchanged; hovering says the current order.
- [ ] Ctrl+C and the button open the list, paused; Escape resumes with no change.
- [ ] Attack: all free companions turn on the target right away, and go back to
      fighting freely when it dies, with a line saying so.
- [ ] Attack: Space takes the Q pick; Q cycles; a click works; a peaceful creature
      such as a deer is accepted; a companion or an object is refused; an empty
      spot just resumes.
- [ ] With combat pause on: Space pauses, Ctrl+C gives an order while paused, and
      the game stays paused until Space again.
- [ ] Attack, with no hostile left and the party back to following: a peaceful
      target such as a deer still gets attacked by all of them.
- [ ] Attack in a mixed fight: when the target dies, the party goes on to the others
      and nobody stands around while a foe is left.
- [ ] Protect: companions turn on whoever is attacking the one chosen.
- [ ] Fall back: they run to the leader and only strike what is in reach.
- [ ] Hold from the list, then Fight freely; Y still holds in combat.
- [ ] A guest's companion and the led companion ignore orders. Going to follow mode
      makes a companion obey.
- [ ] Combat off clears the order.
- [ ] Co-op: the fight keeps moving under the list and crosshair, on the host and
      for a guest, who sees no message and keeps playing. Single player pauses.
- [ ] LT + Y on a pad, in and out of combat; the crosshair on the pad.

Enemies:

- [ ] Archers keep their distance; melee packs spread across the party.
- [ ] A caster goes for the most hurt. Monsters don't carry spellbooks, so mages
      and liches fight as ranged (with fire bolts); see Known limits.
- [ ] Morale: goblins or brigands break when the leader falls or half are gone, then
      return. Skeletons and slimes don't break.
- [ ] Enemy tactics off: U7's behaviour, with morale off too.
- [ ] Scripted fights, duels and the Fellowship's arena-style tournaments behave
      as before.
- [ ] Balance: fights with tactics on, in single player and co-op. See
      [co-op scaling](coop-scaling-plan.md).

Positional damage:

- [ ] Off: no lines, hit numbers carry no label, and fights play as before.
- [ ] On: walking round a foe busy with a companion shows `*flanked*`, then
      `*back stab*`, with back hits landing harder. It doesn't flood the screen.
- [ ] With Show hits on, the numbers carry the words instead; a guest sees them.
- [ ] Enemies get the same on the party, and guests' companions too.
- [ ] Shots and spells never show a label. Slimes never do.
- [ ] Off and on survive a restart; a guest doesn't see the line.

## Known limits

- A monster saved while running away keeps U7's flee mode after loading.
- Companions still have U7's random walk when there's no order in force.
- The caster role goes by a readied spellbook, and `equip.dat` gives no monster
  one, so in practice enemies are melee or ranged. Mages count as ranged.

## Later, if wanted

- **A marker showing who an enemy is after**, so the player can respond.
- **Orders for one companion**, chosen first.
- **Offer the step-back fix to Exult.** `Combat_schedule::back_off` (`combat.cc`)
  subtracted the attacker's `ty` from the fighter's `tx`, so a fighter that was hit
  could step the wrong way. Fixed on this branch; the bug comes from upstream.

Friendly fire is on this branch too, with its own [plan](friendly-fire-plan.md).
