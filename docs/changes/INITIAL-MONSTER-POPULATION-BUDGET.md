---
build: "2026.11.15.6 (native rebuild; launcher and distribution unchanged)"
updated_utc: "2026-11-16T06:19:10.154764+00:00"
status: "implemented; focused native/serialization/menu checks and 12 WAD audits passed; no FPS/GPU benchmark"
baseline_commit: "6bb254900d2799e4a7b136c057a5e39d90bf0349 + preserved streaming working changes"
parent: "ENCOUNTER-DIRECTOR.md"
previous: "ORIGINAL-MONSTER-STREAMING.md"
tests: "../../duct-tape-doom-main/tests/encounter_director.cpp"
audit: "../../duct-tape-doom-main/tests/population_budget_audit.py"
index: "../README.md"
---

# Initial original-monster budget and pack staging

**F4 -> Encounter Director -> ED Population** now has an actual map-start
original-monster budget. Enable the director and streaming, select **Performance**,
choose **Count / 500**, apply and restart the map to start with 500 originals on
the supplied fixture. Hard-protected originals can exceed a smaller target.
This is independent of extra reinforcements and the post-start soft target.

The module/master remain **off by default**. First-use safety mode is
**Compatibility**, which deliberately does not promise an exact population.

## Two distinct policies

**Compatibility** treats the budget as a soft target. No conservative portal
occlusion proof is currently implemented, so ordinary originals use an eager
fallback at map construction. On 4000 Monsters.wad this means **4,317 live**.
No visible original is deliberately withheld; later camera turns, doors,
destructible walls, mirrors, autoaim and teleports cannot reveal an omitted
original because all originals are already present. This is an honest fallback,
not a claim that visibility-based streaming now provides useful savings.

**Performance** chooses the nearest authored locations after accounting for
hard-protected originals. The rest remain plain descriptors without actors,
thinkers or BLOCKMAP entries. Camera/facing checks, mirrors, hitscan and rendering
**do not** materialize them. **Temporary visible absence/pop-in and altered
encounter timing are intentional possibilities**. There is no previous-frame
hold, simulation pause, player relocation, living-actor despawn, AI throttling,
speed change or damage reduction.

Initial construction occurs during map setup. All subsequent streaming activation
is simulation-tick driven. The former render-driven urgent creation path was
removed; its diagnostic entry point is retained as a no-op for test callers.

## Budget and controls

Count is clamped to the filtered original population. Percent uses
`ceil(filtered * percent / 100)`, not THINGS count or the WAD filename.
Hard-protected originals consume the initial capacity first. Selection of the
remaining actors uses deterministic player-start proximity and original THINGS
identity to break ties. It does not scatter the initial actors randomly.

| Key | Default | Meaning |
|---|---:|---|
| `encounter_stream_initial_budget_mode` | 0 | Count=0, Percent=1; next map |
| `encounter_stream_initial_count` | 500 | Positive count; menu steps of 100; config preserves precise integers |
| `encounter_stream_initial_percent` | 25 | 1–100 in 0.5% steps |
| `encounter_stream_population_policy` | 0 | Compatibility=0, Performance=1; next map |
| `encounter_stream_policy` | 0 | Proximity/Progress=0, Timed=1, Hybrid=2; next map |
| `encounter_stream_pack_size` | 8 | Successful originals per ordinary pack |
| `encounter_stream_pack_interval` | 2 | Seconds between timed credits; no accumulated catch-up waves |
| `encounter_stream_max_spawns_per_tic` | 8 | Attempt/work ceiling per tic, including master-off drain |
| `encounter_stream_activate_distance` | 1024 | Near candidates receive queue priority; does not bypass Performance packs |
| `encounter_stream_prefetch_distance` | 1536 | Predictive spatial query radius |
| `encounter_stream_soft_active_limit` | 128 | Post-start pressure; effective threshold is at least actual initial live |
| `encounter_stream_safe_stasis` | 0 | Unavailable and forced off |
| `encounter_stream_stasis_distance` | 2048 | Reserved/unavailable |

Count/percent controls grey appropriately. Interval is unavailable for the
progress-only policy; pack size remains usable there. Existing Esc/F4 apply,
F10 discard, defaults, settings backup/import and persistence paths are reused.
`encounter_debug` aggregate logging is now available in Release as well.

The initial budget, safety mode and activation policy apply on map restart;
ordinary pack tuning remains live. Streaming-off applies next map. **Master-off
mid-map starts a bounded drain**, at the configured normal attempt budget; it
never constructs thousands in one tic. Occupied slots retry without relocation,
telefragging or losing their kill accounting. A requested drain remains in
progress if the master is turned back on before completion.

## Classification audit and region relevance

The new classifier distinguishes actual protection from conservative proxies.
Hard protection includes stock boss/miniboss actors, all monsters on stock
boss-death maps, singleton stock classes, unsupported/unknown actor definitions
and unsafe metadata. DEH-modified actor/state definitions retain the eager native
path. Non-monster special actors, map targets, lamps, decorations and pickups
continue through their normal spawn paths.

The supplied WAD has **six hard originals**: four Cyberdemons, one Baron and one
Arch-vile. The previous broad classifier's proxy union still audits as **2,339**,
explaining the original poor result. Soft reason counts are:

| Reason | Placements matching |
|---|---:|
| Tagged/special sector | 1,466 |
| Action-bearing boundary | 2,053 |
| Floating/emissive actor | 241 |
| Artwork/initial obstruction | 2 |

These counts overlap; do not sum them. A tag or nearby action line alone does
not prove an ordinary actor requires an eager pointer. Performance can stage
these ordinary actors. Static environmental lights are not removed; presentation
of a dormant monster is absent along with the monster itself.

Once-per-map sector adjacency and teleport destination/source links feed region
relevance. Entered/adjacent sectors and linked holding sectors can queue authored
originals even when a remote closet is outside the spatial radius. This is
predictive encounter staging, **not a wall-visibility proof**. Closed slots retry
until native placement validation succeeds. No sampled negative LOS heuristic or
persistent sight cache is introduced.

Ordinary queries use the existing balanced 512-unit-leaf index. Each tic visits
at most 128 BVH nodes and 256 descriptor candidates, shared with incremental
region queues. The reserved activation heap deduplicates entries and uses stable
ties. A large room is processed incrementally rather than scanned afresh per
monster or per frame. Master-off uses a bounded descriptor cursor.

## Pack and active-population semantics

- Proximity/Progress releases credits on player-cell/sector progress and original
  deaths. Timed releases credits at its simulation-tic deadline. Hybrid uses both.
- A pack retains credits across tics; successful activations consume credits.
  Attempts remain bounded by the per-tic work setting. Blocked attempts retry.
- Ordinary packs pause at `max(configured soft target, actual initial live)`.
  Deaths free slots. A progress-demanded pack may exceed that soft threshold,
  while still respecting pack size and attempts/tic. It cannot release unlimited
  extra packs merely because the queue is nonempty.
- Only nearby, visited, adjacent or linked encounter regions are relevant.
  Clock expiry does not select arbitrary remote positions across the map.
- Once materialized, originals retain normal simulation until native death or
  removal. No lifetime active-population cap or safe stasis is claimed.

## Ownership, RNG and saves

Original THINGS IDs, spawnpoint/angle/ambush flags, class choices and initialization
rolls remain owned by the original descriptor. Vanilla initialization rolls are
reserved in filtered THINGS order; later materialization consumes no gameplay
random values. Initial class randomization uses the separate director RNG once.
Selected replacements that cannot fit fall back to the original type. Native
map initialization retains authored-overlap semantics; subsequent materialization
uses occupancy checks. All original counted kills are reserved at map load, with
no second increment at activation.

An actor-to-original registry and living-original set provide constant-time
population pressure. Death/removal, replacement, Nightmare respawn and the
Arch-vile resurrection action update ownership/liveness; additional director
monsters remain in their independent budget.

**ED01 version 3** is necessary to store the budget/safety snapshot, initial/actual
counts, classification, eager actor IDs, bounded-drain cursor, pack credits,
progress override and region/query continuation alongside the V2 descriptors,
queue, selected classes, initialization rolls and retry/timer state. It retains
length/checksum/map-fingerprint validation and serialized actor indices, never
raw pointers. Spatial-index construction is deterministic across restoration.
Raw actor layout and the outer save/trailer boundaries are unchanged.

Readers still accept V1 and V2. V1/ordinary saves retain eager behavior. A V2
streaming save preserves its already-staged population rather than applying a new
initial quota on load; a diagnostic explains that a restart is needed to apply
new budget/safety choices. Subsequent saves use V3. Older executables cannot read
V3; use an older save or restart the map when reverting this source. The native
loader's temporary base-map reconstruction before unarchiving remains unchanged;
this revision does not promise reduced peak memory or load latency for saves.

## Focused verification and actual WAD census

Release targets built: `doom`, `doom-encounter-tests`,
`doom-encounter-settings-tests`, `doom-settings-navigation-tests`.
The three focused binaries passed. Tests include hard overruns, count/percent,
first-tic population, native RNG/kill totals, soft pauses/death slots, multi-tic
pack limits, bounded disable, occupied retries, partial native saves with monster
and item randomization, V2 reading, deterministic resolved classes, stock MAP07
boss protection, disabled behavior, config round-trip and five-page menu painting.
No launcher rebuild, distribution packaging or broad regression suite was run.

The audit uses the actual native medium-skill MAP01 spawn path, Doom II IWAD,
`4000 Monsters.wad`, fixed director seed 77 and `-solidmaps`. An independent living
mobj census matches the streaming ledger in every run. All 12 policy/budget
combinations passed; facing leaves live/dormant counts unchanged in both modes.
`totalkills` remains **4,317** throughout:

| Budget | Requested | Performance live / dormant | Compatibility live / overrun |
|---|---:|---:|---:|
| 500 monsters | 500 | 500 / 3817 | 4317 / 3817 |
| 1000 monsters | 1000 | 1000 / 3317 | 4317 / 3317 |
| 2000 monsters | 2000 | 2000 / 2317 | 4317 / 2317 |
| 12.5% | 540 | 540 / 3777 | 4317 / 3777 |
| 25% | 1080 | 1080 / 3237 | 4317 / 3237 |
| 50% | 2159 | 2159 / 2158 | 4317 / 2158 |


Raw logs, CSV and executable/IWAD/PWAD SHA-256 manifest:
`build/population-budget/wad-audit/`. These are generated verification artifacts.
Reproduce the audit with:

```powershell
python duct-tape-doom-main/tests/population_budget_audit.py `
  build/Release/doom-encounter-tests.exe test-references/DOOM2.WAD `
  'test-references/4000 Monsters.wad' build/population-budget/wad-audit
```

The recorded single-run population setup spans about 3.8–5.0 ms; enabled staging's
initial queue sample is about 0.003–0.004 ms. Those are narrowly scoped CPU
instrumentation, **not whole-engine startup/game-tic costs or a sustained
benchmark**. FPS, frame-time distributions, GPU cost and combat performance were
not measured in this revision. No FPS improvement is claimed.

Remaining playtest coverage: arbitrary custom trap progression, moving/destructible
geometry during staging, teleport waves, mirror/flashlight views, actual
resurrection/Nightmare cycles and long combat sessions. Compatibility avoids
visibility deferral entirely; Performance explicitly accepts absent/popping-in
ordinary originals. The earlier failed GPU capture is not treated as passing
visual evidence. Static lamp rendering was left untouched, not newly validated.

## Changed sources and rollback

Core: `p_population.inc`, `p_encounter.cpp/.h`, map-spawn hooks and the Arch-vile
resurrection notification in `p_enemy.c`. UI: `settings_panel.cpp` with existing
nested `settings_menu.cpp` integration. Focused tests and this audit runner are
under `duct-tape-doom-main/tests/`. Prior working changes were preserved; the
pre-addendum diff/snapshot is under ignored `build/population-budget/`.

For ordinary eager gameplay, leave master/streaming off and restart. To return to
Compatibility, select it and restart. No live actor conversion or profile
migration is required. Native executable/symbols are in `build/Release`; packaged
launchers retain their previous embedded engine until a separate rebuild.
