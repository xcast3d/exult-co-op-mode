# Interaction authority policy

Friend Mode separates physical world interactions from story authority. The
host remains authoritative for both, but a guest-controlled companion may
request a limited set of safe actions without becoming the story Avatar.

## Initial permission matrix

| Interaction | Avatar controller | Companion controller | Notes |
| --- | --- | --- | --- |
| View own paperdoll and inventory | Allow | Allow | Never changes story identity. |
| Equip or move own inventory items | Allow | Allow | Host validates ownership, capacity, and equipment rules. |
| View another party member's paperdoll | Allow | Deny | The Avatar is the party inventory administrator. |
| Open ordinary containers | Allow | Allow | Host validates reach, locks, and container state. |
| Take, move, or drop ordinary items | Allow | Allow | Synchronize object ownership and resolve simultaneous actions. |
| Buy, sell and train with merchants and trainers | Allow | Planned (Phase 7) | The conversation runs on the guest's copy only; the host carries out the purchase or training with the party's gold. |
| General NPC conversation | Allow | Deny | Companion input must not advance story or expose quest choices. |
| Quest decisions and story dialogue | Allow | Deny | Always attributed to the canonical Avatar. |
| Everyday items with a script | Allow | Allow, from a list | Food, potions, bottles, torches, the pocketwatch, keys and lockpicks, the brush; since 2026-10-22 the bucket, pools of water (a drink, on the drinker) and the gavel (on anything in reach); since 2026-11-08 the bale of wool, spindle of thread and dough (on a spinning wheel, loom or baking hearth). The pocketwatch, plaques (not Lord British's) and other read-only things run on the guest's own copy (`Friend_mode_guest_local_item`); the strength tester is used by the host for the guest like a lever. `Friend_mode_guest_item_use` in exult.cc; a companion the host leads uses the same list. |
| Story-critical object Usecode | Allow | Deny | Requires an explicit allowlist or classification before guests can invoke it. |
| Attack a creature or hostile NPC | Allow | Allow | The guest picks the foe; the host runs the companion's combat schedule against it. |
| Attack a peaceful NPC | Allow | Allow, after confirming | Decided 2026-10-20: part of the game's freedom. The guest presses or clicks again within a few seconds ("Attack Finnigan? Again to attack"); the host checks the confirmation. Guards and fights back hit the whole party, as for the Avatar. Talking stays with the Avatar. |
| Read and write the journal | Allow | Allow | One writer per note; the host's journal is authoritative. |

## Implementation direction

Do not classify every double-click as story Usecode. Route requests through an
interaction policy that considers the assigned actor, target type, interaction
kind, distance, ownership, and current scripted state.

The first safe-interaction slice should distinguish:

1. UI-only actions such as opening a party paperdoll.
2. Ordinary object and container actions.
3. Commerce sessions with a shopkeeper.
4. General or story-bearing Usecode, which remains Avatar-only.

Shop access needs a narrow commerce mode rather than permission to run an NPC's
entire conversation tree. If existing Usecode does not expose a reliable way to
identify commerce-only branches, keep guest shop dialogue disabled until a
host-side adapter or explicit allowlist exists.
