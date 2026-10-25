# Co-op scaling (possible future addition)

Not built. Written 2026-10-25 after a talk with Marcus: with companion
skills and the better combat AI, fights may be too easy, more so with
several players. Play more first; build this only if testing shows the
need.

## Idea

Enemies get harder with each guest, the way many co-op games scale: mostly
more health, a little more damage. A multiplier per guest, not a new
difficulty level.

## How it differs from Exult's difficulty

Exult's Game Engine options already have two:

- **Combat difficulty** (Easiest -3 to Hardest +3): only fights between
  the party and others. Each step moves hit chance by 2 on the d30 roll
  (about 6.7 points; party targets easier to hit, enemies harder), adds 1
  damage to hits on the party and takes 1 off hits by the party, and moves
  the target's armour the other way by 1. A straight-line ramp, no stat
  changes; spells and script damage are untouched (`combat.cc`
  `Combat_schedule::attack_target`, `Actor::figure_hit_points`,
  `Actor::apply_damage`).
- **Combat charmed difficulty** (Normal, Hard): on Hard a charmed Avatar
  turns on the party and leaves the player's control, and charmed party
  members fight the party (`Actor::get_effective_alignment`,
  `Actor::set_charmed_combat`).

Neither counts players or touches health. Co-op scaling would stack on top
of them.

## Why smaller numbers than other games

In most co-op games each player adds a body, so enemies need more health
to keep up. Here a guest adds no one: the companion is in the party either
way, and the guest plays it better (and uses its skills). So the steps
should be small.

## Plan

- **Effective health, not stored health.** Don't raise strength or health
  (they would be saved and outlast the session). The host shrinks each hit
  on someone outside the party: at 1.5x, a 6-damage hit takes 4. Fractions
  round up or down at random so small hits still land. Health bars stay
  correct and simply fall slower; a guest leaving changes the next hit.
- **A little more damage to the party:** a small multiplier on hits that
  land on party members.
- **Which damage:** weapon hits and spell damage. Not damage from game
  scripts (scripted deaths, story events) or traps, so the story is safe.
- **Who counts:** guests with a companion; spectators don't. The host is
  the baseline.
- **Settings:** host only (the host works out every hit), on the Class
  System or Game Engine page. "Co-op scaling" on/off, enemy health per
  guest (start +25%), enemy damage per guest (start +5%). Three guests at
  the defaults: enemies with 1.75x health, hitting 15% harder. Linear per
  guest.
- **Single player:** the Class AI makes single player easier too. If that
  shows in play, a baseline for "Class System on" in the same setting,
  rather than anything per guest.

## Where it would go

Host side only, no protocol change: a helper that returns the multipliers
from the number of guests with a companion, used in `Actor::apply_damage`
(weapon hits) and the spell damage path, gated off for script damage.
