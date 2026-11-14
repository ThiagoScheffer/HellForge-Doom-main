---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T19:10:05Z
status: opt-in-experiment-map02-equivalence-passed
index: ../README.md
tests: ../../duct-tape-doom-main/tests/ambush_sight.py
---

# Ambush sight reuse and actual wake-transition audit

## Scope and controls

Implemented only in the experimental worktree. Normal hearing, wake timing,
player selection, facing tests, monster states and RNG consumption are preserved.
No actor layout, save format, acoustic distance limit or cross-tic
visibility cache was introduced. Main and existing release artifacts are untouched.

**F4 -> Performance -> Monster visibility -> Ambush sight reuse** is now the
primary control, default **off**. Apply with Esc, F4 or Apply/close; F10 discards
the draft. The persisted graphics key is `ambush_sight_reuse` (0/1). Changes
reach the simulation immediately, and saved choices load at startup in both
accelerated and software rendering. No launcher argument is required.

The native `-ambushsightreuse` and launcher `--ambush-sight-reuse` options remain
optional startup overrides. Omit them when relying on your saved F4 preference.
Normal experimental launches retain `-solidmaps` for destruction. Multiplayer,
demo recording and playback still bypass this optimization.

See [F4 integration checks](AMBUSH-SIGHT-F4.md) for the current test build.

## Exact change

When an ambush monster's initial sound-target sight test returns false, `A_Look`
can pass that result on its stack to the original player-search loop. The loop
reuses false only for the same actor and player with identical position, height
and subsector. No gameplay action or world update occurs between these calls.
The result expires when that `A_Look` invocation returns.

The initial ambush test still ignores facing, including visible players behind
the monster. The player-search loop still advances `lastlook`, applies its
original selection/facing/range checks and returns exactly as before. Positive
results are not reused. Ordinary idle searches and unrelated sight calls keep
their existing path. Traversal scratch counters may advance less often; gameplay
state and RNG comparisons do not normalize gameplay differences away.

## Corrected wake accounting

The requested external patch was not available, so its specified CSV fields
were implemented directly. `wake.csv` adds `sound_reached_monsters`,
`sound_reached_ambush`, `sound_reached_nonambush`, `targeted_ambush` and
`targeted_nonambush`. Sound reach requires a currently valid shootable sound
target accepted by `P_Unseen`. The existing `moving` column remains a momentum
or movement-count proxy, not an awake count.

A benchmark-only hook immediately before `P_SetMobjState(...seestate)` records
actual `A_Look` transitions. `wake-events.csv` contains first/repeated entry,
runtime spawn-generation ID, thinker address, level generation, event tic,
type, ambush flag, sector, XYZ and sound/sight reason. IDs are observational,
not serialized; a load creates new runtime actor identities. Raw thinker
addresses are excluded from cross-process event comparisons, while stable IDs
and every other event field must match.

MAP02 has **457** initially living eligible monsters. After the first second:

| Observation | Count |
|---|---:|
| Monsters reached by sound / target-bearing | 436 |
| Ambush monsters reached / target-bearing | 271 |
| Non-ambush monsters reached / target-bearing | 165 |
| First idle-to-see transitions over the ordinary run | 165 |
| First transitions by sight before the shot | 30 |
| Additional first transitions caused by the sound branch | 135 |

Thus 436 targets do **not** mean 436 monsters entered chase. The 271 ambush
actors can retain targets while still failing visibility and remaining idle.
This refines the [earlier target-bearing audit](SF2012-MAP02-SOUND-WAKE-AUDIT.md).

## Sight diagnostics

`wake-sight.csv` records logical calls, actual queries, inclusive elapsed
nanoseconds, unchanged-pair duplicates within one `A_Look`, false duplicates
and reused false results. Sites are initial ambush sound sight, ordinary player
search, vanilla missile range, vanilla melee range, and solid geometry.
Solid geometry is nested inside sight calls: **do not sum these inclusive times**.
Enhanced-AI-specific call sites were not individually tagged or exercised.

Windows are before shooting, first eight shot tics, tics 8–34, and later tics.
Cost counters reset after map construction and benchmark setup, before the
35-tic pre-shot window. Actor transition events also retain initial pre-driver
entries. Population scans and state hashes run outside timed `G_Ticker` and
render calls; event hooks collect bounded in-memory records and flush afterward.
The audit is absent from the playable build. Records/actor identities are capped
at 65,536 with overflow reporting; all completed checks report zero drops.

## Focused measurements

Only **SF2012 MAP02** was loaded: Doom II, hard skill, embedded DeHackEd,
`-solidmaps -uzmovement`, fixed zone base, fixed player spawn, 960×540 FIFO,
sprite filtering on, optional enhanced AI and FluidMonsters off. One pistol
round was verified by ammunition 100->99. Each ordinary run has 35 pre-shot
tics and 350 after the shot, including 315 later tics.

Two alternating off/on pairs produced these **profiled later game-tic** costs:

| Pair | Reuse off median / p95 | Reuse on median / p95 |
|---|---:|---:|
| 1 | 7.5033 / 12.1227 ms | 4.6692 / 9.1490 ms |
| 2, reversed order | 7.7597 / 12.7241 ms | 4.5916 / 9.0389 ms |

Median simulation reductions were approximately 37.8% and 40.8% in this workload.
Frame-call medians stayed around 5.8–6.0 ms; this establishes no FPS gain.
Instrumentation overhead is included and was not independently calibrated for
the new audit hooks. The playable build omits those hooks, so these figures
are not a production-wide performance promise.

A separate off/on follow-up corrected the before-shot scope boundary without
repeating lifecycle tests. It also matched state/RNG/events/populations exactly.
Its actual solid sight query counts were:

| Window | Reuse off | Reuse on |
|---|---:|---:|
| Before shot, 35 tics | 2,935 | 2,935 |
| First 8 | 801 | 365 |
| Tics 8–34 | 2,373 | 953 |
| Later 315 | 27,841 | 10,963 |

The follow-up's later solid sight time was 1,748.03->886.99 ms. Across shot
windows, **18,734** actual queries were avoided. Uncovered ordinary-search
duplicates remain deliberately unchanged.

## Correctness checks and limitations

- Both original timing pairs matched every canonical state hash, RNG call
  count/trace/index, population snapshot and transition event across all
  385 observed tics. The corrected-scope pair matched too.
- One combined native off/on lifecycle pair matched all 335 observed tics,
  including real save/load, death-save recovery, level restart, spawning,
  death, resurrection and removal.
- That pair explicitly verified initial ambush reaction to a visible player
  behind it, and one successful type-126 teleport through MAP02 line 1312.
  Teleport crossing was synthetic setup; dispatch, destination collision and
  teleport execution used the real game paths. Ordinary timing runs did not
  force any targets or crossings.
- One actual 105-tic MAP02 demo was recorded using the preserved pre-change
  headless executable. Playback traces from that executable and the changed
  executable, reuse off/on, matched exactly. This is a short idle replay,
  not broad combat-demo coverage. A changed-engine recording comparison was
  not run. Demos retain the engine's existing classic geometry restrictions.
- Sixteen launcher packaging/argument checks and `git diff --check` passed.

Broader WADs, live LAN sessions, all DeHackEd action combinations and level
completion were not retested. Eligibility uses runtime actor flags/type for
Lost Souls rather than a stock map-thing number list. More exotic patched
actors lacking those flags remain outside the population count. No global
sound-policy changes, REJECT shortcut or persistent LOS cache were implemented.

## Reproduction and delivery

```powershell
cmake --build build/native-on --config Release --target doom-monster-bench --parallel 6
python duct-tape-doom-main/tests/ambush_sight.py
# Optional scope-window-only follow-up, without lifecycle repetition:
python duct-tape-doom-main/tests/ambush_sight.py --scope-only
```

Raw commands, logs, traces and CSVs: `build/ambush-sight/`. Original paired
results are `paired-results.json`; the corrected scope pair is `results.json`.
Replay commands/results are under `demos/`. The audit is compiled only with
`DOOM_MONSTER_BENCHMARK` and enabled by `-bench-wake-audit`.

Self-contained launcher: `build/experimental-release/DOOM-Hellforge-Experimental-Sight.exe`.
Matching native executable/PDB: `build/experimental-release/sight-engine/`.
Build identity and hashes: `manifest-sight.json` beside the launcher.
