---
build: "2026.11.15.6 (local native source rebuild)"
updated_utc: "2026-11-15T20:10:39.382681+00:00"
status: "implemented; two focused native checks passed; single-player only"
tests: "../../duct-tape-doom-main/tests/encounter_director.cpp"
settings_tests: "../../duct-tape-doom-main/tests/encounter_settings.cpp"
index: "../README.md"
---

# Encounter Director

Three independent, default-off gameplay modules share a default-off master
switch. This is a clean native implementation from the supplied functional
specification, using the existing actor/collision/sight/settings/save systems.
No Joshua Hard AI-Director code or GZDoom/ZScript API was copied.

**Population boundary:** initial randomization preserves the original monster
count. Reinforcements add bounded monsters. This feature is not monster streaming,
AI throttling, or a reduction of the original population on large maps.

## Implemented modules

**Initial Monster Randomization** runs once before the first simulation tic,
after startup F4 preferences have loaded. It uses classes present on the map,
configurable threat costs, bounded threat similarity, compatible flying/ground
and kill-count flags, available sprite artwork and native placement validation.
An original remains intact until its replacement has passed validation. Its
THINGS position, angle, options/ambush/skill metadata and original spawn descriptor
are retained. Kill/item totals remain one-for-one. Zero strength leaves classes
unchanged. Enable before starting/restarting the map.

Strict boss protection is the default. Map-specific boss-death actors, Keen/brain
machinery, unclassified actors, singleton monster classes and unknown extended
spawn flags stay protected. Boss modes Off / Bosses / Strict affect initial
randomization; progression guards always apply. Strict additionally protects
Barons and Arch-viles as minibosses. No global actor definitions are rewritten.

**Dynamic Reinforcements** reuses eligible original monster positions, with at
most five small deterministic group offsets. It creates no spawnpoint actors.
Distance, direction, current floor/ceiling and sector hazards, native occupancy,
all living-player LOS and final native collision checks reject unsafe candidates.
No setting permits visible spawning. Native AI acquires targets normally.

The scheduler uses simulation tics. Its default interval is 45?90 seconds,
distance 1,024?2,048 units, group 1?3, living additional cap 8, lifetime additional
cap 24, and global living additional threat cap 16. Local pressure is bounded by
default 24, with 16 candidate attempts per event. A used point waits 180 seconds
before reuse. Failed events wait until the next normal interval. Pressure uses
existing BLOCKMAP iteration, not a full thinker scan. Budget checks precede
expensive placement/LOS work.

Adaptive pacing uses the weakest living player's bounded health + half armor.
Below 75 it doubles intervals, limits groups to one and halves additional threat
allowance; below 25 it suspends additions. Active nearby bosses suspend normal
events by default. Bosses, progression actors, Arch-viles and Pain Elementals are
not reinforcement candidates, avoiding new boss triggers, revivers and untracked
spawn chains. Ordinary Barons can reinforce outside progression maps.

**Item Randomization** performs one-for-one replacements at original pickup
positions. Ammo, armor, health, powerups, weapons and custom-inventory preferences
are independent. Ammo pools remain within ammo families. Weapon pools retain
ammo/progression tiers and at least one of each original weapon class. Keys,
backpacks, suits, invulnerability, berserk, unknown/objective items and singleton
powerup classes remain protected. Counted items are excluded by default; count
flags must still match when that protection is disabled. Coverage controls how
many eligible placements are considered, never the pickup population. Drops and
later gameplay spawns are not randomized.

## F4 controls and persistence

The existing flat-tab framework now provides **ENCOUNTER DIRECTOR**, **ED Monsters**,
**ED Reinforcements**, **ED Items**, and **ED Advanced** pages. They reuse normal
keyboard/controller controls, scrolling, draft/apply/discard behavior and the
existing graphics.cfg. Esc/F4/Apply applies; F10 discards. Initial replacements
require a new/restarted map; reinforcement controls work during play. Master off
stops future activity without deleting actors or clearing saved preferences.

The Advanced reset changes only director values in the current draft. Threat
costs are exposed there without changing monster damage, health, speed or AI.

| Configuration key | Default |
|---|---:|
| `encounter_director_enabled` | 0 |
| `encounter_randomize_monsters` | 0 |
| `encounter_reinforcements_enabled` | 0 |
| `encounter_randomize_items` | 0 |
| `encounter_randomization_strength` | 50 |
| `encounter_preserve_bosses` | 2 (Strict) |
| `encounter_threat_weighted` | 1 |
| `encounter_min_spawn_distance` / `encounter_max_spawn_distance` | 1024 / 2048 |
| `encounter_min_group_size` / `encounter_max_group_size` | 1 / 3 |
| `encounter_min_spawn_interval` / `encounter_max_spawn_interval` | 45 / 90 seconds |
| `encounter_max_active_additional` / `encounter_max_total_additional` | 8 / 24 |
| `encounter_max_threat` / `encounter_max_local_threat` | 16 / 24 |
| `encounter_max_spawn_attempts` | 16 |
| `encounter_no_spawns_behind` | 1 |
| `encounter_adaptive_pacing` / `encounter_pause_for_boss` | 1 / 1 |
| `encounter_randomize_ammo`, `_armor`, `_health`, `_powerups`, `_weapons`, `_custom_inventory` | all 0 |
| `encounter_exclude_counted_items` | 1 |
| `encounter_item_coverage` | 100 |
| `encounter_random_seed` | 0 (automatic map/session seed) |

Positive custom seeds support exact integers through 16,777,215. They apply next
map and are written with sufficient precision to round-trip. Distances enforce
minimum < maximum; interval/group pairs enforce minimum <= maximum. Numeric
ranges, finite values, budgets and seeds are clamped. Mandatory LOS, population
and progression guards cannot be disabled through a config key.

Threat keys have prefix `encounter_threat_` and these suffix/default pairs:
`zombieman=1`, `shotgun_guy=2`, `chaingunner=3`, `imp=1`, `pinky=1`, `spectre=1`,
`lost_soul=2`, `cacodemon=3`, `hell_knight=4`, `baron=6`, `mancubus=6`,
`arachnotron=7`, `revenant=4`, `archvile=8`, `pain_elemental=5`, `cyberdemon=20`,
`spider=18`. Each cost is adjustable from 1 to 128. Development builds additionally
expose `encounter_debug` event summaries. Release UI omits that debug switch.

## Native architecture, performance and compatibility

`EncounterDirector` owns plain map descriptors, fixed-size class counts, category
pools, an independent xorshift64* stream, scheduling and bounded additional-actor
records. Data is captured during the existing filtered original-map spawn pass.
A map-phase pass builds pools. Disabled mechanics do not consume gameplay RNG;
disabled runtime ticks do not scan the map or select candidates. Spawn descriptors
are bounded at one million eligible placements and initially reserve at most
16,384 slots. Runtime additional records are bounded by the 256-per-map setting.

Director-created actors use the dedicated stream for spawn initialization too;
normal `P_SpawnMobj` callers keep their existing RNG draw. New actors' ordinary
AI subsequently uses the normal gameplay RNG. Death/removal hooks update records;
Nightmare respawn transfers ownership, and a once-per-second bounded recount
observes resurrections. No full actor scan runs each tic. Save/load may scan actors
once to map references to their serialized thinker indices.

An additive **ED01** versioned, length-checked, checksummed save trailer stores
completion flags, seed/RNG state, remaining timer, issued count, original population,
spawnpoint fingerprint/use/cooldowns and additional actor IDs. It is appended
outside existing Boom/weakpoint/arena/solid trailers and stripped before those
loaders. Restore never reapplies original replacements. Saves without ED01 load
with empty director runtime state and completed initial phases. A mismatched or
corrupt director trailer suspends reinforcements without duplicating actors.
Preferences and threat tuning remain global F4 choices rather than being silently
overwritten by loading a save. Actor layouts and existing save records are unchanged.

**Multiplayer is disabled by the user's explicit follow-up decision.** The native
network exchanges player commands rather than replicated actor events; no host
replication protocol was added. All director F4 controls become read-only during
netgames. Recording and playback also bypass the director. These are tested bypass
guards, not a claim of synchronized multiplayer or demo support.

**Conservative content limits:** actor/state DeHackEd patches disable eligibility
for the entire map; text-only patches need not do so. Unknown classes and extended
spawn flags remain untouched. This native engine has no safe custom-inventory
class metadata registry, so the opt-in custom category currently has no eligible
classes and leaves unknown inventory unchanged. Known-class, classic Doom/Doom II
maps are the implemented target. Safety can intentionally leave maps/classes with
no eligible substitutions or reinforcements.

Living caps gate director-issued additions. Native resurrection/respawn can revive
previously issued actors; the next recount/event accounts for them and stops new
additions. Existing monster resurrection behavior is not suppressed to force a
cap. Broader mod combinations, Nightmare, full map/trap completion and F4 visual
layout/controller interaction remain manual coverage limits.

## Files and focused verification

New source/test files:

- `linuxdoom-1.10/p_encounter.h`, `p_encounter.cpp` ? native subsystem/configuration.
- `tests/encounter_director.cpp` ? focused real-core integration fixture.
- `tests/encounter_fixture.py` ? two-room generated PWAD using the existing arena builder.
- `tests/encounter_settings.cpp` ? option registry/configuration round-trip check.

All paths above are under `duct-tape-doom-main`. Existing files changed:
`CMakeLists.txt`; `g_game.c`; `p_setup.c`; `p_mobj.c`; `p_inter.c`; `p_tick.c`;
`p_swarm.c/.h` (read-only patched-definition query); `platform/scene3d.h`;
`platform/scene3d/scene.cpp`; `platform/settings_panel.cpp`; and
`platform/settings_menu.cpp`. Documentation index updated.

The affected native engine and two test targets compiled successfully. The small
headless fixture, with solid geometry enabled, passed independent and combined
modules, unchanged initial population/metadata/counts, protected bosses/unique
monsters/keys, real substitutions, separate RNG indices, visible-spawn rejection,
distance/forward rules, active/lifetime limits, native save/load, cooldown restore,
no duplicates, absent/corrupt trailers, network/demo bypasses, master off and zero
strength/coverage. The option check passed unique config keys, all director fields
round-tripping including a full-precision seed, disabled defaults, paired bounds
and read-only network controls. GPU capability queries are stubbed only in that
registry-only test; no visual/GPU behavior is asserted by it.

```powershell
python duct-tape-doom-main/tests/encounter_fixture.py build/encounter-director/fixture.wad
cmake --build build --config Release --target doom doom-encounter-tests doom-encounter-settings-tests
# Run from an isolated writable working directory:
build/Release/doom-encounter-tests.exe <DOOM2.WAD> <fixture.wad>
build/Release/doom-encounter-settings-tests.exe
```

CTest registers `encounter-settings-contract`; supplying `DOOM_COMPAT_TEST_IWAD`
also registers fixture generation and `encounter-native-contract` (Doom II required).
No full suite, launcher build, distribution packaging or extensive gameplay test
was performed. No performance improvement is claimed for this gameplay feature.
Existing solid-sight optimization is retained.

Local evidence: `build/encounter-director/{build.log,test.log,settings-test.log,manifest.json}`.
The native binary and matching symbols are `build/Release/doom.exe` and `doom.pdb`.
The build label remains 2026.11.15.6 for this local source revision. The packaged
launcher still embeds the older engine.

For a manual check, `build/encounter-director/Play-Encounter-Director.cmd` launches
the rebuilt native engine with isolated copied preferences. An optional PWAD path
argument selects another Doom II map set. All new modules start off; enable the
master and desired module in F4, then start/restart the map for initial changes.
The local test profile persists its choices and is not recopied on later runs.
