# Interaction authority policy

Friend Mode separates physical world interactions from story authority. The
host remains authoritative for both, but a guest-controlled companion may
request a limited set of safe actions without becoming the story Avatar.

## Initial permission matrix

| Interaction | Avatar controller | Companion controller | Notes |
| --- | --- | --- | --- |
| View own paperdoll and inventory | Allow | Allow | Never changes story identity. |
| Equip or move own inventory items | Allow | Allow | Host validates ownership, capacity, and equipment rules. |
| View another party member's paperdoll | Allow | Allow read-only initially | Revisit item transfers separately. |
| Open ordinary containers | Allow | Allow | Host validates reach, locks, and container state. |
| Take, move, or drop ordinary items | Allow | Allow | Synchronize object ownership and resolve simultaneous actions. |
| Buy and sell with a shopkeeper | Allow | Allow | Restrict the conversation to commerce paths and the acting companion's inventory/funds. |
| General NPC conversation | Allow | Deny | Companion input must not advance story or expose quest choices. |
| Quest decisions and story dialogue | Allow | Deny | Always attributed to the canonical Avatar. |
| Story-critical object Usecode | Allow | Deny | Requires an explicit allowlist or classification before guests can invoke it. |
| Combat actions | Allow | Later phase | Host-authoritative combat synchronization is required first. |

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
