# Friendly fire: sparing the party from explosions (plan)

Built on the `feature/battle-orders` branch (2026-11-06), not yet tested in a
fight. Written after looking into why a companion with a firedoom staff kills
half the party each battle.

## The problem

Exult has no friendly-fire setting. An explosion hurts everything in its
blast, whoever fired it: `Explosion_effect::handle_event` (`effects.cc`) finds
every object within half the sprite's width and calls `attacked` on each,
party and shooter included. The check at `Actor::fight_back` (`actors.cc`) only
stops allies from fighting back; the damage still lands.

The firedoom staff makes it worse in two ways:

- **It explodes.** `docs/weapons.txt` gives its first flag byte as 0x12, and
  bit 1 of that byte is "explodes" (`Weapon_info::read`). The fire wand's byte
  is 0x10, so it doesn't.
- **Companions choose it.** `Weapon_info::get_base_strength` scores an explosive
  ranged weapon at 3000, far above any ordinary weapon, so auto-combat picks it
  and then fires at foes standing next to the party.

## The rule

An explosion fired by a party member doesn't hurt the party (or their summoned
creatures). Everything else stays as it is.

| Case | Hurts the party? | Why |
| --- | --- | --- |
| Firedoom staff and other exploding weapons, fired by the party | No | The point of this plan |
| Burst arrows, fired by the party | No | Same code path |
| Exploding spells cast by the party | No, if they arrive as an `Explosion_effect` | To confirm in testing, see below |
| Exploding weapons and spells fired by enemies | Yes | Unchanged |
| Powder kegs, including one a party member set off | Yes | Kegs stay dangerous, and chain reactions stay a hazard |
| Traps and explosions with no living attacker | Yes | Unchanged |
| A charmed party member, as target | Yes | Effectively an enemy for now |
| A charmed party member, as attacker | Yes | Their shot shouldn't be safe |
| Townsfolk and other non-party NPCs | Yes | Hitting bystanders should still have consequences |
| Objects: chests, doors, kegs | Yes | The blast still breaks things and sets off kegs |
| Fire fields | Yes | Separate code (`objs/egg.cc`); walking into fire should still hurt |

## How it would work

In `Explosion_effect` (`effects.cc`), so Exult's files get only a small
change. Every source of an explosion goes through it: `Projectile_effect`,
`Actor::figure_hit_points`, the damage code for objects (`objs/objs.cc`),
`Combat_schedule::attack_target`.

1. **Record who really fired, in the constructor.** It replaces a missing or
   non-actor attacker with the Avatar ("Blame avatar"), so by damage time a trap
   looks as if the Avatar fired it. Keep a flag from before that replacement:
   the attacker was a living actor, in the party or the Avatar, and not charmed.
2. **Tell kegs apart by the weapon.** `ShapeID::get_info(weapon).is_explosive()`
   is true for a keg and false for a firedoom staff, whose shape only has
   the "explodes" weapon flag. The `explode` object can't be used for this:
   chain reactions pass it, but it is cleared just before the damage loop.
3. **Skip in the damage loop.** With the option on, the flag set and the weapon
   not a keg, skip any object that is an actor, in the party or the Avatar, and
   not charmed.

The test for "not charmed" should be the same on both sides:
`get_effective_alignment() == Actor::good`, as `set_charmed_combat` uses.

**Summoned allies.** The Class System's summons fight beside the party but
aren't in it, and they share their alignment with townsfolk, who are still hit.
So `Friend_class_is_summon` (`friendclass.cc`) checks the list of creatures the
party has summoned.

**Co-op.** The host runs every fight, so guests need no separate code. A
skipped target takes no hit and sends no hit message.

## The option

"Friendly fire" on the Combat System page (game menu; until 2026-11-09 in the
Game Engine options, as `config/gameplay/combat/friendly_fire`), kept as
`config/friend/combat/friendly_fire` (`Combat::friendly_fire`). **No**, the
default, spares the party; **Yes** is U7's behaviour. In co-op the host's setting
decides, because the host runs the fights, and guests don't see the line.

## To test

- [ ] A companion with a firedoom staff, with foes next to the party: foes are
      hurt, the party and the shooter aren't.
- [ ] Burst arrows, the same way.
- [ ] The Explosion and Delayed Blast spells, with and without the Keyring mod.
      Script damage that skips `Explosion_effect` won't be covered: if a spell
      does, note it here and decide then.
- [ ] Foes firing explosions still hurt the party.
- [ ] A keg the party shoots still hurts the party, and sets off other kegs.
- [ ] A keg set off by a firedoom blast still hurts the party.
- [ ] A charmed companion is hit by an ally's blast, and their own blast hurts
      the party.
- [ ] Summons survive the party's blasts.
- [ ] Bystanders are still hurt, and still call the guards.
- [ ] A chest, door or other breakable in the blast still takes damage.
- [ ] The option set to the original behaviour: blasts hurt the party again.
- [ ] Co-op: a guest playing the staff's user, and the host's setting applied.
- [ ] A save and load keeps the option.

## Later, if wanted

- **Holding fire.** With the original behaviour chosen, the AI could wait when an
  ally stands in the blast, instead of firing anyway.
- **The weapon score.** Spared parties make the 3000 score less of a problem, but
  it still means companions drop other weapons for the staff. It could become an
  ordinary damage-based score.

When built, add the player-facing line to `CHANGELOG.md` under Unreleased.
