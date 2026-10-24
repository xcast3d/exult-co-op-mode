# Leading a companion

Status: **all four steps built** (2026-10-21): steps 1 to 3 tried and
working (Shift+Tab and Shift+click on a face, the party following in
formation, roofs, Y holding); step 4 not tested yet.

The Class System gives every companion skills, but only guests can use them
by hand: the host and a single player play the Avatar, whose bar holds
spells, and the companions use their skills only through the Class AI. This
plan lets the host (and a single player) take a companion in hand the way a
guest does, use its skill bar, and have the party follow it.

## Decided

- **Like a guest, not a disguise** (2026-10-21). The host controls the real
  companion, as a guest does: its own stats, pack, paperdoll, level and
  weapon, so its skills work as they do for a guest. Swapping the Avatar's
  sprite for a companion's, or making the companion "the Avatar", was
  considered and dropped: the scripts, conversations, karma and saves all
  expect NPC 0 to be the Avatar, and a disguised Avatar would carry the
  Avatar's stats and pack into Shamino's skills, with a second, hidden
  Shamino for every script that looks for him.
- **The Avatar stays the story actor.** A companion you lead follows the
  guests' rules ([interaction policy](interaction-policy.md)): doors,
  levers, containers and items work; merchants and trainers talk to the
  companion; other people and story objects say "The Avatar must do this".
  (First decided "only the Avatar talks to anyone", then changed the same
  day to match guests exactly.)
- **Fighting anyone, as guests may** (2026-10-21): creatures and hostiles
  at once, peaceful people after confirming ("Attack Finnigan? Again to
  attack"), the same as a guest's companion.
- **The party follows whoever you lead, the Avatar included** (2026-10-21).
  Leaving the party behind would be simpler (scouting, laying caltrops,
  shooting from range), but going on into a dungeon would mean switching
  back to the Avatar every time. So the Avatar and the other companions
  follow the companion you lead, as they follow the Avatar today.
- **Y holds the party** on the host and in single player: everyone you
  don't lead stops and waits; Y again sends them back to following. For
  creeping in alone with Shamino, vanishing and striking. (On a guest Y
  stays follow mode.)
- **The Avatar fights as it always has** while it follows (2026-10-21):
  U7's own combat for the party, no Class AI placing.
- **A guest in follow mode follows the leader** (2026-10-21): the party's
  AI walks its companion behind whoever the host leads, as it walks the
  rest of the party. While the host's party holds, it holds too.

## How it plays

- **Shift+Tab** (Tab is the player list) cycles whom you lead: the Avatar,
  then each companion in the party that no guest is playing, then back to
  the Avatar. The notice near the top says "Leading Shamino" (and
  "Leading the Avatar").
- **Shift+click on a face** in Exult's status bars leads that one directly,
  the Avatar's face too (2026-10-21). A guest's companion says "Iolo is
  another player's"; one asleep or paralyzed, "can't be led now". On a
  guest (2026-10-23) it takes that companion if it is free, as Shift+Tab
  does for the next one. Hovering a face says who it is in the context
  notice near the top ("Iolo", "Iolo (Marc)" for another player's), with
  "Switch Character (Shift+Click)" under it, except on the one played now
  and, on a guest, the Avatar (2026-10-23).
- The camera, roofs, night lighting, the Target System (Q, E, Space) and
  the minimap follow whoever you lead; most of them already follow the
  controlled actor.
- The skill bar is that companion's, as a guest's is; back on the Avatar,
  the spell bar returns.
- The Avatar and the others walk behind you in formation. In a fight they
  fight as U7's party does: the Class AI's skills and placing for the
  companions, U7's own combat for the Avatar.
- **Y** holds them where they stand ("The party holds"); Y again, "The
  party follows". A held party fights back if attacked, standing its
  ground.
- Leading someone, you stay within reach of the Avatar, as guests do (the
  leash). That only matters while the party holds; otherwise the Avatar is
  right behind you.

## What there is to build on

- **Local possession** (Phase 1, `Game_window::toggle_friend_mode_control`,
  Ctrl+F as a development key): taking a companion in hand, the camera
  following it, its AI paused, and control going back to the Avatar before
  saves and scripted scenes.
- **Guests' companions on the host** (Phase 2 to 6): eggs, teleporters,
  waking sleepers, creatures kept awake near them, item reach and theft,
  the leash, and the interaction policy.
- **The Class System**: the bar for any companion (`Bar_actor`), and the
  Class AI already leaves alone whoever is controlled.

## Issues and how to meet them

1. **Following a leader that isn't the Avatar** (the biggest piece). Two
   things move the party today, and both are tied to the Avatar:
   - `Follow_avatar_schedule::now_what` (schedule.cc) aims every follower
     at `gwin->get_main_actor()`;
   - the formation steps: each step of the Avatar (`Main_actor::step`)
     calls `Party_manager::get_followers`, which moves the followers
     (`move_followers`) from the Avatar's step. A companion's steps don't.

   Both get a **leader**: the Avatar normally, the companion you lead
   otherwise (`Game_window::get_party_leader`, say). The led companion's
   steps then call the formation code the Avatar's call. `Actor::follow`
   already takes a leader.
2. **The Avatar following.** When you lead someone else, the Avatar must
   walk like a follower: put it on a follow schedule toward the leader (and
   take it off when you switch back). Exult moves the Avatar by schedule
   and path in cutscenes, so the pieces exist, but not for long stretches;
   test it hard (doors, stairs, teleporters, combat starting and ending).
   The formation places members by party number (the Avatar is -1), so the
   Avatar needs a place in it too.
3. **Holding.** `Party_manager::get_followers` already skips members on
   the `wait` schedule, so a hold is the wait schedule for everyone but the
   leader, and back to follow on release. Also:
   - saves write held companions as following (the same fix as guests'
     companions saved waiting, 2026-10-17);
   - a map change (teleporter, moongate, stairs to another map) ends the
     hold, or someone is left behind on another map;
   - held companions fight back when attacked, without walking off to chase.
4. **Scripts and eggs.** Script, music and weather eggs fire for the Avatar
   only, as for guests: lead Shamino into a room and its scene plays when
   the Avatar arrives. That suits scouting ahead. Traps and monster eggs
   fire for the companion you lead, as for guests' companions.
5. **Back to the Avatar.** Control already goes back to the Avatar before a
   save and when a script takes over. Also: when someone starts a
   conversation with the Avatar (Batlin walking up, a guard), and when the
   companion you lead dies, leaves the party or is knocked out (the Avatar
   takes over; the notice says why).
6. **Co-op.** The host can lead only companions no guest plays; guests'
   pickers leave out the one the host leads; a guest joining sees "Marc
   leads Shamino" in the player list. A guest's companion in follow mode
   follows the leader (the follow schedule's leader, so nothing extra) and
   holds when the party holds. Y means "hold" on the host and
   "follow mode" on a guest; the help line under each says so.
7. **Keys.** Shift+Tab to cycle and Y to hold are free on the host. Ctrl+F
   (the development key) became the same as Shift+Tab (2026-10-21).

## Steps

1. **Lead and the bar.** Shift+Tab cycles whom you lead, on today's local
   possession, with the skill bar following; the guests' interaction rules
   for the companion you lead. Nobody follows yet (the party stands, as
   Phase 1 left it). Enough to use caltrops or Volley from the host.
   Built 2026-10-21 (`Friend_mode_lead_next`, `Friend_mode_led_companion`
   in exult.cc; the rules in `Game_window::double_clicked_object`):
   targeting and double-clicks in combat attack with the led companion
   (on the combat schedule, as the host fights for a guest's companion;
   walking, clearing the target or combat ending stand it again); its
   skills take the Q pick; a companion left in combat mode goes back into
   the fight. Brought forward from later steps: the leash (no one follows
   yet, so it was needed now) and guests' pickers leaving out the led
   companion. Ctrl+F still takes the first companion and back.
2. **The party follows the leader.** The leader setting, the formation
   driven by the led companion's steps, the Avatar on a follow schedule.
   The risky step; test it through doors, stairs, teleporters and fights.
   Built 2026-10-21: `Game_window::get_party_leader` (the companion one
   leads, else the Avatar) is whom `Follow_avatar_schedule` and
   `Game_window::get_leader_followers` aim at; each walk the led companion
   starts calls the followers, as the Avatar's walks do. Formation walking
   (Exult's default) works with the led companion as its root: the first
   try used Exult's "traditional" walking instead (each follower paths
   after the leader) and looked chaotic, so `Party_manager::get_followers`
   now starts the formation from the leader and puts the Avatar in the
   leader's place in it (`Party_manager::formation_slot`), the same shape
   as when the Avatar leads. The Avatar, like the others, is brought near
   when it falls far behind. Its own
   steps no longer set roofs and dungeon views unless the camera is on it.
   `teleport_party` takes the led companion along (it is on the wait
   schedule while led).
3. **Y holds.** The wait schedule for everyone else, the leash, saves, and
   the end of a hold on a map change.
   Built 2026-10-21 (`Friend_mode_set_hold` and the rest in exult.cc): Y
   holds everyone but the one played, the Avatar too while a companion
   is led, and a guest's companion in follow mode; Y again, or the leader
   reaching another map, and they follow ("The party holds", "The party
   follows"). Combat mode leaves the held alone (U7 skips the wait
   schedule; the Avatar is kept out of `toggle_combat`'s change too), so
   they stand their ground by a check of our own: every 2 seconds each
   strikes the nearest hostile within its weapon's reach without moving
   (a bow reaches far), at once one attacking it, any once combat mode is
   on. Shift+Tab during a hold: the one now led leaves the hold, the one
   left joins it. Saves write the held as following. `teleport_party`
   takes them along. The leash (from step 1) measures from the Avatar,
   held or not.
4. **Co-op and the edges.** Pickers, the player list, back to the Avatar on
   conversations and on losing the companion, and the test checklist.
   Built 2026-10-21:
   - **Saves keep the lead.** Phase 1 handed control back before every
     save, and in co-op saves are frequent (the 5-minute autosave, the
     world sent to each joining guest). `Game_window::write` now writes the
     led companion following, as it does a guest's companion, and keeps it
     led.
   - **Spoken to, the Avatar answers.** When a conversation shows its first
     face while a companion is led (someone speaks to the Avatar, a
     scene), the Avatar takes over for it, and the companion is led again
     once the outermost script ends (`Friend_mode_lead_talk_begins`,
     `_ends`). Not for the companion's own merchant talk.
   - **Losing the one led** (Phase 1's safety check gives the Avatar back):
     "Iolo has fallen", "has left the party" or "is out of the fight";
     leading the Avatar.
   - **The player list and the minimap** show the host on the companion it
     leads (guests see "Marc: Shamino"), and the list names companions by
     their own names ("Sentri", not "fighter").
   - Pickers leaving out the led companion came in step 1.
   - **Afterwards, the same day:** Ctrl+F (Phase 1's development key) does
     what Shift+Tab does; theft by the led companion has it as the thief
     (witnesses look at it, as for a guest's); a companion given back by
     the safety check during a hold joins the hold, and one given back
     "wait" otherwise follows.

## Open questions

None for now. Answered 2026-10-21: merchants and trainers talk to the
companion one leads, as to a guest's (on the host it is the real
conversation, so `Friend_mode_lead_talk_to_merchant` puts the story flags
and the merchant's "met" back after); the Avatar fights by U7's own combat
while it follows; a guest's companion in follow mode follows the leader,
and holds with the party.
