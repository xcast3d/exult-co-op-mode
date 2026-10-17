# Shops and trainers for guests (Phase 7)

Goal: a guest can buy from merchants and train with trainers, using the
party's gold, without moving the story. See Phase 7 in the
[project brief](../ultima-7-friend-mode.md).

## What Black Gate's scripts do

Found with `tools/friend-mode/ucread.py` (a small reader for the usecode
file, built on ucxt's opcode tables) and
`tools/friend-mode/shops_and_trainers.py`, which lists every NPC whose
script calls a routine that takes party gold:

- **Shops (43 NPCs).** The "buy" answer offers categories (weapons, armour,
  provisions...), each calling a merchant's own shop routine, which lists
  items and prices and ends in one shared routine, **08F8**: it counts the
  party's gold, asks how many, then `add_party_items` (the item) and
  `remove_party_items` (the gold).
- **Trainers (17 NPCs).** The "train" answer asks "Which of you wishes to
  train?" (0920/0921), takes gold, and raises stats through **0912**, which
  sets a character's properties (strength, dexterity, combat, training
  points...).
- **Spell sellers (5), healers (9), sellers to the party (3) and other paid
  services (6)** use the same building blocks.
- **Gates.** For almost every NPC, "buy" or "train" appears after asking
  "job" in the same conversation. Many merchants also check that they are
  at work ("Come to my shop during business hours"). Only 7 NPCs check a
  story flag right before offering a service (Jaana, Lord British, Rudyom,
  Rayburt, Owen, Reyna, Sarpling).

## Plan

1. **Conversation on the guest only.** Double-clicking one of these NPCs
   runs that NPC's own conversation on the guest's copy, in full, so "job"
   leads to "buy" as usual. Nothing is sent to the host or other guests,
   and story flags it sets stay on the guest. Other NPCs keep "The Avatar
   must do this".
2. **The companion's portrait** in place of the Avatar's on the guest's
   side of the conversation.
3. **Answer like the host's world.** The host sends each nearby NPC's
   schedule (so shop hours are right) and the story flags (when they
   change), so the guest's copy offers the same options the Avatar would
   get.
4. **Purchases and training happen on the host.** During such a
   conversation the guest catches `add_party_items`, `remove_party_items`
   and the stat changes, and sends them to the host as one request; the
   host checks the party's gold, applies it, and the usual sync brings the
   items, gold and stats back. Money is the party's; no separate purse.
   Training is for the guest's own companion only.
5. **Party stats sync.** Today only health, food and status effects reach
   guests; training also changes strength, dexterity, intelligence, combat,
   magic and training points, so those join the party status.

First shops and trainers; spells, healers and the rest after.

## Built: shops (2026-10-17, protocol version 21)

Tested: buying works from host and guest, the party's gold is taken either
way, and a purchase the party can't afford is refused. Fixed after that
test: a guest's purchase went to the Avatar's pack (Exult gives to the
first party member with room); it now goes to the guest's companion
(`Usecode_internal::add_items_to_buyer`), and to the rest of the party only
if the companion's pack is full. Retested and working.

- A guest double-clicking one of the 42 merchants (the "shop" rows below;
  Inforlem waits for trainers) runs that merchant's conversation on the
  guest's copy (`Friend_mode_guest_talk_to_merchant`). The host and other
  guests see nothing.
- The companion's portrait replaces the Avatar's on the guest's side
  (`Usecode_machine::set_friend_shopper`, used in
  `Conversation::show_avatar_choices`).
- While that runs, `add_party_items` is remembered and the next
  `remove_party_items` (the gold) sends both to the host as one purchase.
  The host accepts only gold as payment, checks the party has it, takes it
  and gives the item (`Usecode_internal::friend_party_trade`), then resends
  every party pack so a refused purchase goes back on the guest. Payments
  without an item (rooms, training) are not sent yet.
- Spells: `add_spell` fails for a guest with "The Avatar must do this";
  spell shops take no gold when that happens.
- The host's NPC schedules ride in the upper bits of each NPC state's frame,
  and `get_schedule_type` answers with them on guests, so shop hours match.
  The story flags go out as bits when they change (twice a second at most),
  held back while a guest's merchant conversation is open.

## Built: trainers (2026-10-17, protocol version 24)

Tested with Markus in Trinsic: refused when the companion had no training
points (Iolo), trained Spark, and the stats matched on host and guest.

- The 17 trainers (Inforlem included, who also sells) talk to a guest's
  companion the same way as merchants.
- "Which of you wishes to train?" lists only the guest's companion: inside
  Black Gate's trainee pickers (090D, 0921), `get_party_list` answers with
  the companion alone (`Usecode_internal::running_function` checks the
  call stack). The rest of the conversation sees the whole party.
- Training pays first, then raises stats through 0912 (`set_npc_prop`). A
  payment with nothing bought is held; the companion's stat changes that
  follow are collected, and both go to the host as one training session
  when the next payment comes or the conversation ends. The host checks the
  gold, takes it, and applies the changes to the guest's own companion only
  (strength, dexterity, intelligence, combat, mana, magic and training
  points, at most 10 each). The stat sync brings them back.
- A payment followed by no stat changes (an inn room, say) still stays on
  the guest only.

## Built: undoing the rest (2026-10-17, protocol version 25)

When a guest's merchant or trainer conversation ends, anything else it
changed on the guest's copy is thrown away: the guest puts back the host's
story flags (the last ones received), and asks the host (`resync_request`)
to resend every party pack and the items near the players, so a free gift
or anything else the host never had goes away.

## The NPCs

Names come from `npc.dat` (best effort). Routine numbers are usecode
functions; shop hours means the script checks the merchant is at work.

| NPC | Name | Offers (routine) | Shop hours |
| --- | --- | --- | --- |
| 5 | Jaana | heal/resurrect 089E |  |
| 6 | Trellek | pay 08EE |  |
| 7 | Sentri | train 08E5 | yes |
| 17 | Chantu | heal/resurrect 0860 |  |
| 18 | Dell | shop 0872 0873 0874 | yes |
| 19 | Apollonia | shop 0842 | yes |
| 20 | Markus | train 08BD |  |
| 23 | British | heal/resurrect 08B4 |  |
| 24 | Nystul | shop 08C6; spells 08C5 |  |
| 35 | Csil | heal/resurrect 0870 |  |
| 36 | Zella | train 0950 |  |
| 37 | Lucy | shop 08B7 | yes |
| 38 | Greg | shop 0899 |  |
| 47 | Jeanette | shop 08A0 | yes |
| 49 | Denby | train 0875 |  |
| 50 | Fred | shop 088C | yes |
| 51 | Kelly | shop 08A7 |  |
| 52 | Willy | sell-to-party 0948; shop 0946 | yes |
| 53 | Gaye | shop 088E | yes |
| 54 | Coop | shop 0863 |  |
| 55 | Grayson | shop 0897 0898 |  |
| 59 | Sean | shop 08E3 | yes |
| 60 | Brownie | sell-to-party 0857 |  |
| 61 | Mack | sell-to-party 08B8 |  |
| 67 | Boots | shop 0854 |  |
| 74 | Rudyom | shop 08DC; spells 08DB |  |
| 76 | Rayburt | train 08D0 | yes |
| 79 | Zinaida | shop 0951 |  |
| 83 | Margareta | pay 08BA |  |
| 87 | Zorn | shop 0952 0953 | yes |
| 92 | Rutherford | shop 08DE | yes |
| 94 | Karenna | train 08A6 | yes |
| 95 | Jakher | train 089F |  |
| 98 | Saralek | pay 08E0 |  |
| 100 | Tavenor | pay 08ED |  |
| 101 | Salamon | pay 08DF |  |
| 102 | Nicodemus | shop 08C4; spells 08C3 |  |
| 104 | Bradman | train 0856 |  |
| 108 | Reyna | heal/resurrect 08D2 |  |
| 115 | Penni | train 08C8 |  |
| 118 | Carlyn | shop 085C 085D 085E |  |
| 119 | Snel | train 0878 |  |
| 121 | Kliftin | shop 08AA 08A9 |  |
| 123 | Daphne | shop 0871 |  |
| 130 | Boris | shop 0855 | yes |
| 142 | Mordra | pay 08EF |  |
| 143 | Rowena | heal/resurrect 091F |  |
| 153 | Mariah | shop 08BC; spells 08BB |  |
| 159 | Jillian | train 08A2 |  |
| 161 | Chad | train 085F | yes |
| 162 | Elad | heal/resurrect 0879 |  |
| 163 | Phearcy | shop 08CB 08CC |  |
| 179 | Polly | shop 08CD | yes |
| 181 | Inforlem | shop 089C; train 089A 089B |  |
| 182 | Inmanilem | heal/resurrect 089D |  |
| 188 | Sarpling | shop 08E1 08E2 |  |
| 189 | Forbrak | shop 0889 |  |
| 190 | Betra | shop 0853 |  |
| 192 | Menion | train 08BE |  |
| 194 | Jehanne | shop 08A1 |  |
| 196 | Richter | shop 08D3 08D4 |  |
| 198 | Jordan | shop 08A3 08A4 |  |
| 199 | Denton | shop 0876 |  |
| 201 | Leigh | heal/resurrect 08AC |  |
| 205 | Zaksam | train 094F |  |
| 206 | Eldroth | shop 087A |  |
| 207 | Yongi | shop 094D 094E |  |
| 215 | Ansikart | shop 0841 |  |
| 216 | Sur | shop 0949 094A; spells 094C |  |
| 219 | Aurvidlem | shop 084C |  |
| 222 | Glenno | shop 088F |  |
| 228 | Lucky | train 08B6 | yes |
| 229 | Budo | shop 0858 0859 085A |  |
| 231 | Mandy | shop 08B9 |  |
| 237 | Kessler | shop 08A8 |  |
| 238 | Perrin | train 08CA |  |
