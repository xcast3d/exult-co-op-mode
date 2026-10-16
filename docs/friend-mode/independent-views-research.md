# Independent views: can guests go their own way? (research spike)

**Status:** research only; no code changed. Written against branch
`feature/friend-mode` (HEAD 15cc3e6a5). All `file:line` references are to that tree.

**Question:** how feasible is it for the host Exult process to simulate several
separate areas at once (the Avatar plus each guest-controlled companion,
possibly on opposite sides of town or the map), instead of only the area around
the Avatar/camera?

## Bottom line

Exult has no "active region" object. "Active" is the emergent result of four
independent mechanisms, and **every one of them is keyed to the host's single
camera/view or to the Avatar**:

1. The on-screen rectangle (`get_win_tile_rect()`, `add_dirty()` clipping,
   `Npc_actor::paint()` waking dormant NPCs).
2. The camera actor's distance (`get_camera_actor()`).
3. Which superchunks have been read in (`schunk_read[]`, filled only by
   `read_map_data()` for the view), and the Avatar-keyed cache-out.
4. The Avatar identity itself (`get_main_actor()`), used by egg triggers,
   Usecode intrinsics, and sleeping-NPC wake-ups.

There is a lot of it (roughly 20 distinct places, listed below), and none of it
is behind an abstraction. But the engine's core (time queue, pathfinding,
combat) is global and does not care where things are, so the work is
"widen several view checks into 'any observer', and keep several superchunks
resident", not "write a second simulator".

Verdicts (detail in the last sections):

| Option | Verdict | Rough effort |
| --- | --- | --- |
| (A) Host simulates several active regions | Feasible, moderate risk, fidelity good | ~4-8 weeks of focused work for "walk anywhere in a town/map, NPCs and eggs behave" |
| (B) Guests simulate their own regions, report to host | **Not recommended**; fails authority model | Very large; effectively a rewrite of the sync design |
| (C) Keep guests inside the host's single active region | Cheap and robust; a design constraint, not a feature | ~1 week (leash + door/object replication) |

**Recommended path:** do (C) first because it unblocks door replication and
object sync, which are needed in every option. Then grow into (A) in stages
(observer list, keep guest superchunks resident, widen NPC/egg activation),
gated by a distance cap you raise over time. Do not pursue (B).

## Important finding up front: the host is already exposed

The host accepts a guest move if the companion is within 2 tiles of the
request and then calls `actor->move()` directly (`exult.cc:1970-1982`, the
"host accepted move" block). Nothing checks that the destination superchunk is
loaded on the host:

- `Game_map::ensure_chunk_read` is called from exactly one place, a
  search-by-shape helper (`gamemap.cc:1642`). `read_map_data()` is called from
  `set_scrolls` (`gamewin.cc:1105`), the paint paths (`gamerend.cc:61, 421`)
  and a few `gamewin.cc` helpers (1820-1888), all of which follow the **host's**
  view.
- `Game_object::move`/`Actor::move` fetch the chunk with `get_chunk_safely`,
  which creates an empty `Map_chunk` on demand (`gamemap.h:153-170`) without
  reading the superchunk's objects.

So a guest companion walking out of the host's loaded superchunks stands in a
region where the host has **no walls, doors, eggs or scenery** (terrain flats
may exist, objects do not). The host snapshot then reports positions that the
host cannot collision-check, and any later door/egg logic would silently run
against an empty world. Even with option (C) the guest must be confined to
superchunks the host has loaded; at present that confinement does not exist.

Second exposure: the Avatar's movement triggers `emulate_cache`
(`actors.cc:4810` -> `gamewin.cc:3255` -> `Game_map::cache_out`,
`gamemap.cc:1752-1791`). That keeps only the 3x3 superchunks around the
**Avatar** and writes every other unmodified superchunk out to
`schunk_cache[]`, removing non-actor objects and calling `Actor::cache_out()`
on every actor in it (`gamemap.cc:1793-1865`, `actors.cc:1963-1968`). A
guest-controlled companion standing outside that 3x3 has its surroundings
cached out from under it and its schedule reset (`set_schedule_type`).
`schunk_modified` is only set for editor/ifix edits (`gamemap.h:145-148`), so
in normal play it does not protect anything.

## 1. What decides which NPCs run schedules/AI

**Not the camera, not the Avatar directly: there is no per-tick "who is active"
set.** The time queue is global (`gwin->get_tqueue()->activate(ticks)`,
`exult.cc:2227`; also 3461, 3580). Every NPC with an entry in the queue gets
`Npc_actor::handle_event` (`actors.cc:5166-5222`) regardless of location. What
varies is whether the NPC is *dormant*:

- `Npc_actor::handle_event`: if the NPC has no action, it calls
  `schedule->now_what()` only when `!dormant`, otherwise
  `schedule->im_dormant()` (`actors.cc:5195-5210`). It also goes dormant if
  `get_map() != gwin->get_map()` (`actors.cc:5180-5185`).
- **Going dormant** happens in `Npc_actor::step` (and `Monster_actor::step`,
  `monsters.cc:419-425`) when a non-party NPC steps off screen *and* is more
  than a screenful (`1 + c_screen_tile_size`) from **`get_camera_actor()`**
  (`actors.cc:5266-5272, 5290-5299`). Also when blocked off screen. Also in
  `Combat_schedule::now_what`, which sets dormant if the NPC is >50 tiles from
  the camera actor (`combat.cc:1559`).
- **Waking** happens only in `Npc_actor::paint()` (`actors.cc:5131-5148`): if
  an NPC is drawn on screen and dormant, it is re-queued 0.5 s later. Painting
  means being in the host's view. A guest-side NPC the host never paints never
  wakes.
- Also dormant when its chunk was not read in at schedule-set time
  (`actors.cc:1948-1953`).
- **Hourly schedule changes** are *not* view-dependent for selection:
  `Game_window::schedule_npcs` loops all NPCs (`gamewin.cc:2772-2790`, called
  from `gameclk.cc:257, 296, 326`) and calls `update_schedule`. But execution
  depends on view: `Actor::set_schedule_and_loc` calls
  `teleport_offscreen_to_schedule`, which **teleports** the NPC straight to its
  schedule spot if neither its position nor the destination is in a read
  superchunk (`actors.cc:1972-1986`, 2000-2008). Otherwise it creates a
  `Walk_to_schedule` and walks (real pathfinding, on loaded terrain only).
  So "far away" NPCs are not simulated at all; they teleport when the hour
  turns. This is what the player experiences as "the town is alive around me".
- `Npc_proximity_handler` (`gamewin.cc:2736-2766`, `npcnear.cc`) is **not** the
  AI scheduler. It only fires barks/"npc_proximity" Usecode and sleeper
  wake-ups for NPCs on screen (+10 tiles): `npcnear.cc:97-103` drops anyone off
  `get_win_tile_rect().enlarge(10)`; sleeper wake uses
  `npc->distance(gwin->get_main_actor()) < 6` (`npcnear.cc:109`). Membership is
  populated by `Npc_actor::paint` and monster-egg creation
  (`actors.cc:5145-5147`, `objs/egg.cc:306`).
  `get_nearby_npcs()` (`gamewin.cc:2764`) returns that same set, which is why
  your host snapshot (`exult.cc:2007`) only covers NPCs near the host view.
- Party members follow the Avatar and teleport/approach when more than about a
  screen away from the **view rect** (`actors.cc:1462-1470`,
  `schedule.cc:5992-6000`, `schedule.cc:819-830`). A guest-reserved companion
  is parked in `Schedule::wait` (`gamewin.cc:1229-1241`), which avoids this.

Consumers of `get_nearby_npcs` besides the snapshot: combat music logic
(`combat.cc:224, 245, 260, 401`), field damage (`objs/egg.cc:1601`).

**Implication for (A):** NPC "liveness" is a function of (a) being painted by
the host renderer and (b) being near the camera actor. To keep NPCs alive
around a second observer, you must give the host a set of *observer points*
and replace the screen/camera tests with "within R of any observer". The paint
wake-up is the hard one: you either simulate a hidden paint pass over each
observer's rect, or reimplement the wake as a distance test in a new per-tick
pass.

## 2. Chunk/map loading and caching

- Map is 12x12 superchunks, 16x16 chunks each, 16x16 tiles per chunk
  (256x256 tiles per superchunk). `Game_map` holds `schunk_read[144]`,
  `schunk_modified[144]`, `schunk_cache[144]` (`gamemap.h:70-75`).
- **Residency is per superchunk**, set by `get_superchunk_objects`
  (`gamemap.cc:1199-1206`): terrain objects, ifix, ireg. `read_map_data`
  (`gamemap.cc:305-340`) reads every superchunk intersecting the host's view
  rectangle only.
- **Several regions can stay resident**: nothing prevents `schunk_read[]` from
  being true for arbitrary superchunks; the data structure is already
  sparse. What prevents it is the Avatar-keyed cache-out above
  (`gamewin.cc:3255-3347`, `gamemap.cc:1752-1791`), which unloads everything
  outside the Avatar's 3x3. Cached-out ireg objects are serialized in memory
  (`gamemap.cc:1833-1840`), so *moved/changed* movable objects survive. I did
  not verify whether runtime frame changes on **ifix** objects (most doors and
  scenery) persist across a cache-out. Treat as a risk; test before relying on
  it.
- Cheaper chunk state: `Map_chunk::setup_cache()` builds the per-chunk
  blocking/egg cache. `Main_actor::switched_chunks` sets it up for the 3x3
  chunks around the Avatar (`actors.cc:4763-4811`). `Npc_actor::step` calls
  `gmap->get_chunk_safely` and `is_blocked`, which lazily set up caches; but
  the superchunk object read is the real gate.
- `emulate_cache` also cancels Usecode scripts >4 chunks from the Avatar
  (`gamewin.cc:3261-3265` via `Usecode_script::purge`, `usecode/ucsched.cc:260`)
  and clears weather eggs >120 tiles away. That would cancel scripted door
  animations and egg scripts near a far guest.
- `emulate_is_move_allowed` blocks `Monster_actor::step` outside 1 superchunk of
  the **camera actor** (`gamewin.cc:3351-3370`, `monsters.cc:386`), so monsters
  near a second observer would freeze unless widened.

**Implication:** keeping N regions resident is a small, local change (a set of
"pinned" superchunks consulted by `cache_out` and read on demand when any
observer moves). Memory cost is a few MB per superchunk; the engine was
designed for 3x3. The riskiest bit is the interaction between cache-out and
eggs/scripts/NPC schedule resets, not memory.

## 3. Egg activation

Eggs are triggered by **actor movement**, tile-based, then filtered by
`Egg_object::is_active` (`objs/egg.cc:829-922`). Call sites of
`activate_eggs`:

- `Main_actor::step` (`actors.cc:4754`): the Avatar.
- `Npc_actor::step` (`actors.cc:5288`): any NPC, including companions walking
  by AI. **Not** `Actor::move` (teleports) - important: the host's
  guest-move handler uses `actor->move()` (`exult.cc:1978`), which does **not**
  trigger eggs. Guest-controlled companions never hatch eggs today.
- `Game_window::read` (`gamewin.cc:3194`): initial egg under the Avatar.
- `barge.cc:884`: barges activate eggs for the **main actor**.
- `drag.cc:759, 917`: objects dropped/picked up on an egg tile.
- `unhatch_eggs` on Avatar departure (`actors.cc:4735`).

What the criteria are measured against, in `is_active`:

- `cached_in` ("anywhere in square"), `avatar_near`, `avatar_far`,
  `avatar_footpad`: strictly `obj == gwin->get_main_actor()`
  (`egg.cc:862, 873, 898, 905`).
- `party_near`, `party_footpad`: any actor with `Obj_flags::in_party`
  (`egg.cc:880-882, 906-912`). Companions work here, **provided something calls
  `activate_eggs` for them**.
- `something_on`: any non-actor.
- Missile (trap) eggs additionally require the egg to be inside the host
  window +10 tiles and, if range-limited, the **main actor** within range
  (`egg.cc:100-107`); they fire at party members (`egg.cc:121-129`).
- Monster eggs create monsters via `Monster_actor::create`
  (`egg.cc:280-310`); these monsters are then subject to the view-keyed dormancy
  and `emulate_is_move_allowed` (camera-superchunk) rules above.

**Implication:** eggs need two changes for guests: (1) call `activate_eggs`
(and `unhatch_eggs`) in the guest-move handler; (2) decide policy for
Avatar-only criteria. Avatar-only eggs are mostly story/cutscene triggers and
should stay Avatar-only. Party-near eggs (including teleporters, jukeboxes,
and intermap teleports such as `teleport_party` at `egg.cc:517-519`, which
would drag the **whole party** including the Avatar) need an explicit per-egg
decision.

## 4. Combat, pathfinding, Usecode assuming Avatar/view

Engine pieces keyed to the view rect (`get_win_tile_rect`) or camera:

- `Combat_schedule::start_battle` runs only when `camera_actor == main_actor`
  (`combat.cc:198`); `now_what` goes dormant when >50 tiles from the camera
  (`combat.cc:1559`); `failures > 5` give-up skips the camera actor
  (`combat.cc:1687`).
- Pathfinder client for enemies has a screen-rect hint (`paths.cc:236, 247-249`).
- Schedules: Follow_avatar (`schedule.cc:5992`), wander/approach
  (`schedule.cc:438, 819`) all look at the host window.
- `Actor::teleport`/summon uses the view rect to decide visibility
  (`actors.cc:1507`).
- `Frame animators` use the camera lift for audibility (`objs/animate.cc:52-53`).

Usecode intrinsics (`usecode/intrinsics.cc`):

- View-rect: `find_nearby ... -359` "find on map" (line 875), `on_screen`/
  `is_npc_near`-style check (981), `armageddon` (2104), the edge-of-screen
  helper (3783).
- Avatar: about 40 `get_main_actor()` uses, e.g. line 501 (AVATAR global),
  998 (`npc->distance(main_actor) < 40`), 1143 (intercept tile), 1259, 1432,
  1534, 2109 (nearby actors around the Avatar), 3901-3905 (approach Avatar),
  4067, 4186 (reachability from Avatar). Many story functions also read the
  Avatar through script constants, not intrinsics.

**Implication:** none of this prevents a *second* region from running; it just
means story/combat Usecode would sometimes evaluate against the Avatar, not
the guest. For a co-op sandbox this is acceptable (Avatar is the story
anchor). The costly part is combat: enemies near a guest need (a) to be awake
(section 1), (b) to be allowed to move (section 2's superchunk gate), and
(c) their `Combat_schedule` distance checks widened to "any observer".

## 5. Door use

- A door is an ordinary `Ireg`/`Ifix` object whose shape has `is_door()`
  (`shapes/shapeinf.h:836`). Doors are indexed per chunk in
  `Chunk_cache::doors` (`objs/chunks.cc:193-198`), found via
  `Game_object::find_door` (`objs/objs.cc:855`), and classified by
  `is_closed_door()` (`objs/objs.cc:865-889`).
- **Player double-click:** `Game_window::double_clicked` ends in
  `obj->activate(1)` (`gamewin.cc:2735`). For a plain object this reaches
  `Game_object::activate`, which calls
  `ucmachine->call_usecode(get_usecode(), this, event)` (`objs/objs.cc:1008`,
  `Usecode_internal::call_usecode`, `usecode/ucinternal.cc:3006-3036`). So door
  opening is a **Usecode function** (the shape's function, which checks lock
  state/quality/keys and toggles the frame via `set_item_frame`,
  `usecode/intrinsics.cc:256-260`). It is not an engine routine. The new
  Friend Mode guest path blocks it at `gamewin.cc:2717-2723`
  ("cannot start Avatar-owned usecode").
- **Engine precedent for NPCs opening doors without story Usecode:**
  `Path_walking_actor_action::open_door` (`actions.cc:361-420`) *temporarily
  zeroes the door's quality* and calls `door->activate()`, then restores the
  quality, specifically "to avoid unwanted usecode" (`actions.cc:383-387`).
  It only does so for unlocked doors (`framenum % 4 < 2`, `actions.cc:334-336`;
  same test in `paths.cc:80-82`). Door frame `% 4 >= 2` is the locked state.
  The pathfinder also treats closed unlocked doors as passable at cost 1
  (`paths.cc:76-100`).

**What a host would need to run a door for a guest:**

1. Guest sends `door_use(object_id_or_tile+shape, intent)`. Object IDs are not
   stable across processes; use `(tx, ty, tz, shapenum)` and let the host call
   `Game_object::find_door`/`find_nearby` to resolve it.
2. Host validates: assigned actor within reach, `info.is_door()`, not locked
   (`framenum % 4 < 2`). Refuse anything else (locked doors, quality-keyed
   doors, shapes with non-door Usecode).
3. Host runs the same trick as `open_door`: save quality, set 0, call
   `door->activate()`, restore. No Avatar-owned dialogue or UI gets triggered
   because it is the door shape's function. (Verify per game: BG/SI door
   functions can play sounds or occasionally call Usecode for special doors
   such as portcullises. Whitelist by shape.)
4. Replicate the result. The cleanest hook is `Game_object::change_frame`/
   `set_frame` (`objs/objs.cc:572`) for objects where
   `get_info().is_door()`: broadcast `{tx,ty,tz,shapenum,frame}`. Guest finds
   the door at that tile and shape and calls `change_frame` locally (with
   sound off). Because NPC AI (`actions.cc:380`) and Usecode scripts also open
   doors, hooking the frame change catches every opener, not just guests'.
   Doors in unread superchunks on the guest must be applied lazily on read
   (keep a small host-state map keyed by tile).
5. Chunk cache consequence: `Chunk_cache::update_object` indexes doors by
   shape not frame, so the guest's local collision cache must be refreshed
   after the frame change. `change_frame` -> `set_frame` already handles the
   blocking-cache update locally as it does for single-player.

Door state is the first instance of a general need: **object state sync**
(containers, levers, torches...). Design it generically ("frame/position/
existence delta for object at tile") rather than door-specific.

## 6. Sound/music and other view-tied things

- Positional audio uses the **view centre** and the **camera actor's lift**:
  `Audio::get_2d_position_for_tile` (`audio/Audio.cc:758-775`) and
  `Get_sfx_out_of_range` (`objs/animate.cc:51-56`). The guest's camera already
  makes this correct for guests; it is local and needs no network work. (SFX
  triggered by *other* players' actions need to be sent and played, or are
  inaudible.)
- Music: combat music is only started when the camera actor is the Avatar
  (`combat.cc:198`), and victory/over music uses `get_nearby_npcs`
  (`combat.cc:224-245`). Music-changing eggs (`jukebox`) are party-near eggs
  (`egg.cc:891`) and the host's, not guest-local.
- Weather/effects: `remove_weather_effects(120)` in `emulate_cache` and
  per-egg weather.
- Lighting/time-of-day palette, roof hiding (`set_above_main_actor`,
  `gamewin.cc:1120`) and dungeon flags are evaluated for the **camera actor**
  and are therefore already per-process local state; they work for free.
- Paint-driven side effects: anything that runs inside `paint()` rather than
  the time queue (NPC wake-up, nearby list population) only happens for the
  local view.
- Time stop, party gumps and barges are global and host-driven.

## Feasibility verdicts

### (A) Host simulates multiple active regions

**Feasible but invasive; recommended as the long-term target.**

Required changes (in dependency order):

1. **Observer registry** on the host: `{Avatar} + {guest companions}`, each
   with a position. Wrap with `Game_window::for_each_observer`.
2. **Residency**: on every observer chunk change, `ensure_chunk_read` for the
   3x3 superchunks around it; change `cache_out` (`gamemap.cc:1752`) to
   protect the union of all observers' 3x3 sets. Also change `emulate_cache`
   to be called per observer or remove the single-pair assumption
   (`gamewin.cc:3255`).
3. **Activation**: replace the camera/view tests with "near any observer":
   `Npc_actor::step` dormancy (`actors.cc:5266-5299`),
   `Monster_actor::step` (`monsters.cc:419`), `Combat_schedule::now_what`
   (`combat.cc:1559`), `emulate_is_move_allowed` (`gamewin.cc:3351`),
   `Npc_proximity_handler::handle_event` (`npcnear.cc:97`), missile egg checks
   (`egg.cc:100-107`). Replace the paint-driven wake in `Npc_actor::paint`
   with a periodic per-observer scan (e.g. 1 Hz) that walks NPCs and clears
   `dormant` for those within a radius.
4. **Eggs**: call `activate_eggs`/`unhatch_eggs` when applying guest moves
   (`exult.cc:1978`); decide per criterion whether guest companions count.
5. **Schedules**: `teleport_offscreen_to_schedule` already tests "chunk read";
   once residency covers observers, NPCs near guests will walk their schedules
   instead of teleporting. No change needed beyond pinning.
6. **Snapshots**: send NPCs near *each* observer (not only
   `get_nearby_npcs`), since `npc_prox` is paint-fed. Also send hourly
   schedule changes of numbered NPCs near anyone, and object deltas
   (section 5).
7. **Usecode purge** (`Usecode_script::purge`) must not cancel scripts within
   N chunks of any observer.

Effort: ~1 week for 1-2 (residency + observers), ~1-2 weeks for 3-4
(activation + eggs), ~1-2 weeks for 5-7 (snapshots, object deltas, purge),
plus 1-2 weeks of stabilization/testing. **~4-8 weeks** with one developer
who is comfortable in the Exult sources.

Main risks:
- Hidden coupling to paint, especially anything that only happens when an
  object is drawn. Only a full playthrough finds these.
- Cache-out/restore races for Usecode scripts and eggs (save/load also needs
  to be coherent).
- CPU: several regions of NPC pathfinding at once. Exult handles an entire
  screen of NPCs; 2-4 regions is within the same order, but monster-heavy
  areas (SI) deserve a profile pass.
- Story Usecode implicitly assumes the Avatar is near the action. Out-of-view
  guests can trigger half a quest cutscene with no spectators. Mitigate with
  Avatar-only eggs and the interaction policy.
- Save/load must write the extra resident superchunks consistently
  (they already are: saving writes every read superchunk).
- Network bandwidth: more NPC snapshots; the 100 ms/all-NPC format is fine for
  tens of NPCs, needs delta/priority beyond that.

### (B) Guests simulate their own regions and report to the host

**Not recommended.** It breaks the stated architecture (host authoritative for
world simulation and Usecode). You would have two writers per NPC, per object
and per quest flag. Quest/global flags, schedule hours, party state and
Usecode state are all single-writer in Exult and the Usecode VM is not
designed to be merged. Reconciliation (who owns an NPC when both observers see
it) is the classic hard problem and the guest copy is already a stale world.
Only genuinely local concerns (camera, audio, roofs, lighting) should be
client-side, and they already are. Effort: very large (months), with
correctness risk across the entire game.

### (C) Keep guests inside the host's single active region

**Feasible, cheap and robust. Take it as the baseline.**

Implementation: a leash with a "soft edge" so guests cannot request moves
beyond a radius R of the Avatar/host camera (host and guest both enforce; the
host rejection path at `exult.cc:1981` already exists). R should be small
enough that everything inside is resident and awake: host view rect plus
margin, e.g. R ~ 30-40 tiles (the view is roughly 40x25 tiles at default
scale; `c_screen_tile_size` is the engine's own "a screenful").

What is still needed in this option (and is needed by A too): door/object
replication (section 5), calling `activate_eggs` for guest moves, snapshotting
every NPC in the host view not just `get_nearby_npcs`, and a fix for
guest-vs-host cache-out within the leash. UX: how the guest is told they are at
the edge (the camera is host-independent today, so the host's view can scroll
away from the guest). A "party tether" like most co-op action games (pull the
guest back, or stop the camera) is acceptable.

Effort: ~1 week including door sync. Main risks: user frustration with the
leash; the host's view rect and the leash have to agree or NPCs near the
leash edge will be asleep on the host (they are only awake near the camera).

## Recommended path

1. **Now (days):** guard the current exposure. Reject guest moves into
   superchunks the host has not read, or call `ensure_chunk_read` on them;
   protect the guest's superchunk from the Avatar-keyed `cache_out`. Add the
   leash (option C) as an interim rule.
2. **Next (1-2 weeks):** generic object-state replication starting with doors
   (frame-change hook + guest-initiated `door_use` via the quality-zero trick,
   `actions.cc:383-387`), eggs on guest moves.
3. **Then (A), in stages, each independently shippable:**
   observer registry + residency (guests can leave the leash but only to
   already-resident superchunks), then activation widening for NPCs and
   monsters, then combat/music, then lifting the leash.
4. Keep story/Avatar-only Usecode and eggs on the host Avatar permanently; use
   the interaction policy (`interaction-policy.md`) to expose only the safe
   subset to guests.
5. Do not build (B).

## Open questions / things I did not verify

- Whether runtime frame changes on ifix objects (most doors) survive
  `cache_out_schunk` + re-read (`gamemap.cc:1793-1865`). Needs a quick test:
  open a door, walk 3 superchunks away, return.
- Exact set of door Usecode functions in BG vs SI that do something other than
  toggle frame (portcullis, trapped, key-locked variants). Needs a Usecode
  dump (ucxt) to build the shape whitelist.
- Whether `Npc_actor::step` set-dormant logic is reached for NPCs controlled via
  a time-queue action at all when they are on a guest's screen but not the
  host's. Reading says yes (they go dormant because `add_dirty` fails and they
  are far from the camera); a live test with the guest watching an NPC across
  town will confirm.
- Performance with several awake regions was not measured.
