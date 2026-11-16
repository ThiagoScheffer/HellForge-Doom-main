---
build: "2026.11.15.6 (native source experiment; launcher unchanged)"
updated_utc: "2026-11-16T05:27:02.923818+00:00"
status: "historical first streaming revision; superseded by INITIAL-MONSTER-POPULATION-BUDGET.md"
baseline_commit: "6bb254900d2799e4a7b136c057a5e39d90bf0349"
tests: "../../duct-tape-doom-main/tests/encounter_director.cpp"
settings_tests: "../../duct-tape-doom-main/tests/encounter_settings.cpp"
menu_tests: "../../duct-tape-doom-main/tests/settings_navigation.cpp"
index: "../README.md"
---

# Original monster streaming (experimental)

> **Historical baseline:** this report preserves the first revision and its failed
> performance acceptance. Current controls, two policies, V3 saves and the new
> 500/1,000/2,000 actual-population audit are documented in
> [Initial population budget and pack staging](INITIAL-MONSTER-POPULATION-BUDGET.md).
> The render-driven activation and unbounded disable described below were replaced.

F4 -> **Encounter Director -> ED Population** adds a fourth independent module,
**Original Monster Streaming**, default off. Enable the master and this module,
apply with Esc/F4, then restart the map. Reinforcement caps continue to control
additional monsters only. No launcher or distribution package was rebuilt.

**This revision is not yet a demonstrated solution for 4000 Monsters.wad.**
Correctness takes priority over a low live population. Its conservative safety
path activates through walls, so most of this particular map becomes eager when
the player faces into it. No FPS improvement is claimed.

## Implemented ownership and activation

- Intercept eligible filtered original THINGS before `P_SpawnMobj`; dormant
  entries are plain descriptors, with no placeholder actor/thinker/BLOCKMAP entry.
- Preserve original THINGS index, mapthing, sector, selected class, ambush/angle,
  native lastlook and initial-tic rolls. Reserve the normal initialization RNG
  draws in original map order; later materialization consumes no gameplay RNG.
- Include dormant counted monsters in `totalkills` immediately. Activation does
  not increment it again. Death, removal, corpse resurrection and Nightmare
  respawn retain native actor handling; respawn transfers descriptor ownership.
- Resolve enabled class randomization once using the existing director pools/RNG;
  retain the original class if the selected replacement cannot fit.
- Keep bosses/minibosses, stock boss-death maps, floating/emissive monsters,
  tagged/special sectors, action-bearing line boundaries, uncertain metadata,
  missing artwork and initially obstructed placements eager. Unknown actors
  follow the native path. DEH-patched definitions bypass streaming entirely.
- Build balanced spatial bounds with 512-unit leaves once. Queue nearby entries
  on 128-unit player-cell changes or once per second, not once per actor/tic.
  The priority queue is reserved, bounded by original descriptor count and uses
  stable descriptor-order tie breaking. Failed occupancy attempts are retried;
  originals are never relocated, discarded or telefragged by the streamer.
- Normal budget defaults to eight attempts per simulation tic. Near originals
  bypass pack delays. Timed/Hybrid packs only operate on nearby relevant entries.
- Before parallel sprite collection, conservatively intersect actual spawn-frame
  artwork/offset bounds plus the renderer's maximum floor-shadow reach against
  camera side planes. Possible visibility bypasses the pack budget immediately.
  There is no negative LOS cache and no assumption that walls hide an actor.
- Active mirrors/reflections, flashlight silhouette shadows, voxel presentation
  and secondary cameras use an all-originals safety fallback. Classic software
  rendering also drains originals. No previous-frame hold or teleport freeze.
- Autoaim/hitscan and player/missile position checks activate candidates before
  traversal, covering input turns or contacts between rendered frames. Teleport
  occupancy receives the same local safety step; tagged staging remains eager.
- Dormant locations reserve occupancy against additional director reinforcements.
  Master-off drains pending originals instead of deleting them. Streaming and
  policy changes apply on the next map/restart; pack tuning is live.

This intentionally changes remote encounter timing: absent originals cannot hear,
move, fight, take damage or join infighting until instantiated. Existing live AI,
physics, speed and damage are not throttled. Multiplayer, demo recording/playback
and arcade modes retain the existing eager initialization path.

## Configuration

| Key | Default | Behavior |
|---|---:|---|
| `encounter_stream_original_monsters` | 0 | Fourth independent module; next map |
| `encounter_stream_policy` | 0 | 0 proximity/progress, 1 timed packs, 2 hybrid |
| `encounter_stream_activate_distance` | 1024 | Urgent nearby activation |
| `encounter_stream_prefetch_distance` | 1536 | Predictive queue radius |
| `encounter_stream_max_spawns_per_tic` | 8 | Routine attempt budget; urgent work can exceed it |
| `encounter_stream_pack_size` | 8 | Routine timed/hybrid pack |
| `encounter_stream_pack_interval` | 2 | Seconds; half-second steps |
| `encounter_stream_soft_active_limit` | 128 | Diagnostic target only in this experiment |
| `encounter_stream_safe_stasis` | 0 | Unavailable; forced off |
| `encounter_stream_stasis_distance` | 2048 | Reserved; unavailable |

Distances are validated as activation < prefetch < stasis. Pack controls are
unavailable in proximity mode. The soft target is explicitly **not enforced**:
no actor is suppressed or removed to satisfy it. Safe stasis is deliberately not
implemented. Timed and Hybrid currently share the same conservative visibility
and proximity overrides; neither delays a potentially visible original.

With `encounter_debug 1`, aggregate population diagnostics are emitted at most
once per second: deferred, queued, materialized, living streamed actors, initially
eager assignments, stasis (zero), urgent activations, blocked attempts and
cumulative activation CPU milliseconds. `ED_Population()` exposes the same data.
Eager is an assignment count, not a live eager-monster census. Existing exact
solid-sight acceleration and sprite filtering remain independent and unchanged;
this revision adds no new optimization to live monster AI.

## Save/load and rollback

The existing final **ED01** trailer uses version 2 when original descriptors are
present; version 1 remains written/read for ordinary director saves. Length,
checksum and immutable map fingerprint checks precede restoration. Version 2
stores descriptor/class/state ownership, serialized actor indices (never
pointers), original spawn rolls, retry delays, pack timer and the priority queue.
Spatial bounds are rebuilt and proximity is refreshed after loading. Old saves
remain eager and are not re-deferred or re-randomized. Corrupt state is diagnosed
and suspended rather than spawning duplicate actors; recovery of absent originals
from a corrupt streaming save is not guaranteed. Keep known-good saves.

To disable for new maps, switch streaming off and restart. Disabling the master
while playing drains pending originals. A valid version-2 save still restores its
original descriptors, even if current preferences are off; the next tick drains
those originals with the master off. Older executables do not understand version
2: resume an older save or restart the map when reverting the source. Raw actor
layout and the other save trailers were not changed.

## Focused evidence

Built native `doom`, `doom-encounter-tests`, `doom-encounter-settings-tests` and
`doom-settings-navigation-tests` (Release). Focused integration covers original
and additional populations, RNG indices, kill totals, urgent/out-of-view handling,
pre-render autoaim/contact, mixed dormant/live native save/load including queued
entries, master-off draining, default-off behavior, and legacy director behavior.
Settings round-trip and the five-page F4 navigation/painter checks passed.
No full suite or extensive gameplay run was performed.

A single native GPU screenshot attempt did not complete and produced no log;
the test process was terminated. GPU output and visual no-pop-in acceptance are
therefore **inconclusive**, not passed. CPU menu captures are in
`build/population-streaming/director-*.ppm`. Diagnostic logs and builds remain
under `build/population-streaming/` (generated, not source deliverables).

The tiny disconnected-room fixture proves seven originals are absent from live
actors; a two-spawn routine budget leaves five queued; native save/load preserves
that mixed population; a facing change activates the remainder without new RNG
or duplicate kills. It does not prove full-map visibility safety on arbitrary WADs.

### Supplied WAD population audit (MAP01, Doom II, medium)

| Stage | Live recognized monsters | Deferred originals | Total kills |
|---|---:|---:|---:|
| Streaming disabled, first tic | 4317 | 0 | 4317 |
| Streaming enabled, first tic | 2401 | 1916 | 4317 |
| Spawn-facing 90-degree guard | 4221 | 96 | 4317 |

There are 2,339 eager assignments in this conservative classification. The view
check activated 1,882 streamed originals cumulatively, with two blocked attempts;
aggregate measured activation work was approximately 0.263 ms in that one audit.
These are **population/activation diagnostics, not an FPS benchmark**. Initial
savings mostly disappear after the guard: only about 2.2% remain deferred.

Reproduce the narrow audit from `build/population-streaming` in PowerShell:

```powershell
$env:HELLFORGE_POPULATION_AUDIT='1' # use 0 for eager control
..\Release\doom-encounter-tests.exe ..\..\test-references\DOOM2.WAD '..\..\test-references\4000 Monsters.wad'
Remove-Item Env:HELLFORGE_POPULATION_AUDIT
```

## Remaining acceptance limits

A wall-aware, provably conservative visibility region is still needed for useful
savings on this WAD. Do not replace the guard with a few negative sight rays:
destructible openings and sprite edges invalidate that shortcut. Later occupancy
can also keep an authored slot pending; no visual guarantee is claimed for such
blocked placements. Tagged-region protection is intentionally broad, not a proof
of every custom encounter. Full trap/teleport/boss playthroughs, turn/door/reflection
image comparisons, moving-geometry stress and GPU timings remain outstanding.
This revision must remain optional pending that evidence. No 90-FPS promise or
stable-build migration is justified by these results.
