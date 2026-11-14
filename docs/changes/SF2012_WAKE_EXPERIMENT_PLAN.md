# SF2012 mass-wake investigation — offline WAD reproduction and engine test plan

**Scope:** read-only analysis of uploaded `SF2012_final.wad`, inspected uploaded `monster_bench.cpp`, and Doom's published AI source. **No executable engine source, Doom II IWAD, or benchmark binary was supplied.** Consequently no live simulation or performance regression was run for a modified engine. The numerical tests below are initial-geometry estimates, not measured post-patch game behavior.

## 1. Reproduced map-level observations

MAP02 uses standard binary map lumps: 443 sectors, 3,374 linedefs, 785 things. The reconstructed player start is **thing 572, (-288, 320), sector 34**. Thing 0 is an *earlier* player-1 start at (4896, 4000); the original Doom source spawns each encountered player start and replaces the active player's actor with the latest, potentially leaving a voodoo doll. Misusing the first start yields a false sound result.

Assuming Ultra-Violence, single player, original monster DoomEd numbers, initial floor/ceiling heights and original `P_RecursiveSound` semantics:

| Measure | Offline result |
|---|---:|
| Recognized stock monster things placed on Ultra-Violence | 457 |
| Initial sectors reached by sound from player start | 407 / 443 |
| Monsters whose sectors are reached | **436** |
| Reached monsters farther than 2,048 map units | **238** |
| Reached monsters flagged MF_AMBUSH | 271 |
| Reached non-ambush monsters | 165 |
| Linedefs carrying ML_SOUNDBLOCK | **0** |
| Linedefs of monster-only repeatable teleport type 126 | **107** |
| Reached monsters in sector 193 (also adjoining type-126 lines) | 106, all non-ambush and farther than 2,048 units |

The totals **436** and **238** match the two corresponding values in the supplied Codex benchmark. This is strong corroboration of the map-level sound mechanism but not an engine trace or proof that all 436 became actively mobile.

In original Doom, `A_Look` assigns `actor->target = sector->soundtarget` before checking the MF_AMBUSH line-of-sight condition. Thus an ambush monster can have a non-null `target` while staying idle. Furthermore, a failed ambush sight check can immediately be followed by `P_LookForPlayers`, leading to another sight check for the same actor/player pair in single-player mode. The actual frequency of duplicate checks in this engine must be measured.

The existing wake benchmark reports `moving` using nonzero momentum **or `movecount > 0`**. This is not an authoritative state-transition counter. The `targeted` field similarly is not an authoritative count of monsters entering `seestate`.

## 2. Offline policy counterfactuals (not gameplay pass results)

All values are numbers of sound-reached monsters whose *original spawn positions* satisfy the hypothetical filter. The current engine does **not** implement these filters.

| Hypothetical policy | Candidates retained | Of these, non-ambush |
|---|---:|---:|
| Vanilla, unrestricted | 436 | 165 |
| Euclidean <=1,024 units | 55 | 36 |
| Euclidean <=1,536 units | 120 | 46 |
| Euclidean <=2,048 units | 198 | 46 |
| Euclidean <=3,072 units | 252 | 46 |
| Euclidean <=4,096 units | 293 | 51 |
| <=2,048 units, exempt all monsters in sectors incident on monster-only teleport lines | 304 | 152 |
| <=4 two-sided portal crossings | 76 | 46 |
| <=5 portal crossings | 208 | 46 |
| <=6 portal crossings | 401 | 165 |

**Important:** the trap exemption above is a heuristic, not an assurance of encounter compatibility. Sector 193 alone contains 106 monsters with obvious links to monster-only teleport geometry; a hard radius cap would suppress all of them from *sound* detection. Activation by sight, damage and specials may still occur, but at different times. The portal-hop sensitivity (208 -> 401 for a one-hop change) also makes arbitrary hop caps risky.

## 3. Recommended engineering sequence

### A. Benchmark the actual wake transitions before modifying behavior

Use the included `monster_bench_wake_audit.patch` (uncompiled locally) to extend `wake.csv` with `sound_reached_monsters`, `sound_reached_ambush`, `sound_reached_nonambush`, `targeted_ambush`, and `targeted_nonambush`. Its existing `moving` column remains a proxy and should not be renamed to `awake`.

In addition, instrument the **actual `A_Look` branch at `seeyou:` immediately before `P_SetMobjState(actor, actor->info->seestate)`**. Count first entry for each monster (including the actor's spawn ID/thinker identity) separately from the number of target-bearing monsters. Record the reason (sound or sight), monster sector, position and event tic, using a benchmark-only hook. This requires the real engine source and is not provided by the C++ patch alone.

Instrument calls and time spent in `P_CheckSight`, separately tagging the call sites: `A_Look` ambush sound test, `P_LookForPlayers`, `P_CheckMissileRange`, `P_CheckMeleeRange`, and solid-world geometry traversal. Distinguish duplicate, unchanged actor/player pairs within the same `A_Look` invocation.

### B. Optimize redundant sight checks without suppressing wake behavior

Experiment first with **reuse of an immediately preceding false sight result** when an MF_AMBUSH actor checks the same player a second time through `P_LookForPlayers`. Preserve `lastlook` progression, angle checks, player selection, reactions, RNG consumption and other semantics. Do not replace the initial ambush sight test with `P_LookForPlayers(false)`: the initial test reacts to visible players behind the monster, whereas `P_LookForPlayers(false)` applies a facing condition.

Only skip the known-redundant raycast; do not broadly cache negative visibility across ticks without geometry invalidation. Destructible walls, doors, actor movement, slopes and changing sight blockers make persistent caching hazardous. Use the benchmark's existing per-tic `state_hash`, RNG trace and index to check strict equivalence to the unmodified build in this mode; verify actual save/load and replays as well.

A separate low-level optimization of solid-world `P_CheckSight` may yield more benefit, but requires visibility profiling in the actual engine. The supplied '76% of simulation' figure identifies a high-value area; it does not establish what fraction is redundant.

### C. OPTIONAL map-scoped sound policy, disabled by default

For users who explicitly prefer reduced remote activation, add a per-map option such as `compat_sound_mode = vanilla | local | trap_aware`, defaulting to `vanilla`. Identify map profiles by a hash of the map's *actual geometry/gameplay lumps*, not simply by WAD filename. Do not apply a general limit to every WAD.

At a controlled sound-adoption decision in `A_Look`, a map-specific policy can prevent a monster from adopting the sector's sound target while still allowing **ordinary sight, damage and scripted activation**. For initial experiments, test a fixed 2,048-unit limit; a second prototype can exempt an explicitly reviewed list of critical trap sectors such as MAP02 sector 193. Do not assume that other remote monster boxes are dispensable. Preserve ordinary behavior when map is not matched, and take special care with multiplayer, voodoo dolls, non-player emitters, monsters that move after spawn, and monster wakes from other sources. Avoid a policy that silently depends on the *current position of the target player* when the sound was emitted by a different actor.

The apparent benefit of a `trap_aware` 2,048-unit policy is only **132 fewer target-eligible monsters**, and just **13 fewer non-ambush candidates**, compared with unrestricted sound. Therefore it may do relatively little to reduce actual chase work while it does change internal targeting. Treat it as a compatibility option, not the default performance fix.

A map-authored sound isolation fix (properly placed ML_SOUNDBLOCK linedefs, if the author wants isolation) is another alternative. Stock Doom allows sound across **one** sound-blocking linedef, so a single new ML_SOUNDBLOCK is generally insufficient to form a hard acoustic barrier: paths crossing two are required. Such changes also need explicit map/trap testing.

## 4. Live engine test matrix (still required)

Run baseline and each experimental build with matching IWAD, SF2012 PWAD, skill and DeHackEd. Record independent windows before shooting, first 8 tics after the shot, and at least a few hundred later tics. Repeat runs for noise and OS scheduling variance, compare matched map starts, and keep `-bench-wake-audit` instrumentation outside the timed `G_Ticker` window (as the current harness does).

Measure `living`, `sound_reached`, `targeted`, *actual idle->see transitions*, type-126 teleport activations, wall clock per simulation tic, P_CheckSight call counts, and solid-world sight cost. Compare RNG and `state_hash` for the behavior-preserving optimizations, for which exact matches should be a requirement. Intentionally behavior-changing acoustic policies are **not** expected to match the baseline state hash, but must be checked for broken fight progression, missed teleports, secrets, multiplayer, demo compatibility and level completion. Repeat with solid destruction enabled as the normal launcher uses; classic geometry alone is not representative.

Suggested maps: SF2012 MAP01 (quiet control), MAP02 (exact match), MAP03 (additional broad sound reach), MAP15/MAP21 (large maps with known malformed two-sided sidedefs). MAP27+ use extended BSP nodes that the attached offline parser does not implement. Check whether any DeHackEd-modified monsters are missing from the stock-number list in other maps.

## 5. Reproduction

```bash
python doom_sound_audit.py SF2012_final.wad --map MAP02 --skill 4 --out sf2012_map02_sound_audit.json
python test_sf2012_sound_audit.py
patch -p1 < monster_bench_wake_audit.patch   # from engine directory containing monster_bench.cpp
```

The five Python tests pass against this uploaded WAD. The source diff passes `patch --dry-run` against the uploaded C++ benchmark file. No C++ engine compilation or actual modified-game benchmark was possible from the provided files alone.

### Reference source

- Original id Software `p_enemy.c` (noise propagation, A_Look, P_LookForPlayers): https://github.com/id-Software/DOOM/blob/master/linuxdoom-1.10/p_enemy.c
- Original id Software `p_mobj.c` (player spawn order): https://github.com/id-Software/DOOM/blob/master/linuxdoom-1.10/p_mobj.c
- Boom reference for monster-only type-126 teleporter: https://doomlegacy.sourceforge.net/docs/boomref.html
