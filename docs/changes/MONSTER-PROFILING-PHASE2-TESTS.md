---
build: "2026.11.12.1"
updated_utc: "2026-11-13T11:01:29Z"
status: "concluded at user request; completed evidence and limitations in parent report"
baseline_commit: "b59ad46c1d5dcc92874074b3a2805a9b5af4b521"
feature: "MONSTER-PROFILING-PHASE2.md"
index: "../README.md"
---

# Phase 2 profiling verification

Testing was concluded at the user's request. Do not interpret the reproduction
commands below as a request to restart the long matrix. Completed results and
explicit limitations are recorded in [the parent report](MONSTER-PROFILING-PHASE2.md).

All paths below are relative to the isolated `HELLFORGE-profile-phase2`
worktree. Commercial WADs remain in the original checkout and are read only.
Generated profiles, saves, graphics settings, logs and lighting caches are
confined to `build/`. No Sunder test is part of this revision.

## Reproduce the sustained measurements

Configure Release builds of `duct-tape-doom-main` with the same generator,
architecture and compiler flags. Use the existing native dependency overrides
when dependencies are already available locally.

| Build directory | Profiling | Benchmark | Renderer | Supplemental argument probes |
|---|---|---|---|---|
| build/profile-off | OFF | ON | OFF | OFF |
| build/profile-on | ON | ON | OFF | OFF |
| build/profile-args | ON | ON | OFF | ON |
| build/native-off | OFF | ON | ON | OFF |
| build/native-on | ON | ON | ON | OFF |
| build/engine-profile | ON | OFF | ON | OFF |

The CMake switches are `DOOM_MONSTER_PROFILING`, `DOOM_MONSTER_BENCHMARK`,
`DOOM_RENDER3D`, `DOOM_BUILD_SDL` and `DOOM_MONSTER_PROFILE_ARGUMENTS`.
Simulation builds use SDL OFF; native builds use SDL ON. Build `doom-headless`,
`doom-monster-bench`, or `doom` respectively.

```powershell
python duct-tape-doom-main/tests/monster_profile_matrix.py `
  --root . `
  --iwad F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/DOOM2.WAD `
  --wad F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/dmnsns2k-monsters.wad
```

Do not run another benchmark, build or heavy test concurrently with this queue.
It runs correctness passes first, then serial performance passes. Simulation
controls are pinned to logical CPU 0; native workers retain normal affinity.
Windows benchmark-only fixed zone placement controls actor-address decisions.
It fails explicitly if the requested address is unavailable.

Performance runs warm up for 350 tics and measure 2,100 tics, five paired
repetitions with alternating execution order. `--verify` runs are separate:
their timings are invalid for performance conclusions. A run's `manifest.json`
contains commands/settings, binary/resource/source hashes and build-cache
hashes. `complete.json` records execution time and exact invocation.
`measured-sources.zip` is checked against that manifest before creation.
Raw `tics.csv` and enabled `probes/*.csv` are retained.

The matrix script deliberately starts fresh. For an interrupted individual
dataset, invoke `monster_profile_runs.py` with its original arguments and
`--resume`; it rejects changed binaries/resources. Never resume across a
source revision merely to combine unlike samples.

## Correctness comparisons

The driver hashes scalar state after each tic, outside the simulation timer.
Actor references are stable thinker-order indices. Read-only benchmark hooks
also observe private tactics, recovery/arena, FluidMonsters and combat records.
There are no observer calls in ordinary builds with benchmarking OFF.

Required comparisons are repeated compiled-out runs followed by compiled-in
disabled and enabled runs using the same initial conditions, inputs and fixed
zone. Compare every tic's state hash, gameplay RNG call count, rolling RNG
result trace and RNG index. A final wrapping index alone is insufficient.
`comparisons.json` reports the first differing measured tic without masking it.
The lifecycle fixture exercises actual save/load, same-map reconstruction,
spawn, damage/death, removal and death/Use recovery. Resurrection in this
fixture is synthetic; the existing combat suite also exercises native actions.

Unknown/light thinker internals are a remaining canonical-hash coverage limit.
The hashes are intended for this compiler/architecture, not a portable save
or a proof of equivalent behavior on all WADs.

## Other compatibility checks

```powershell
ctest --test-dir build/native-on -C Release --output-on-failure
python duct-tape-doom-main/tests/monster_profile_demos.py `
  --baseline build/profile-off/Release/doom-headless.exe `
  --instrumented build/profile-on/Release/doom-headless.exe `
  --iwad <DOOM2.WAD> --out build/demo-verify-final
python duct-tape-doom-main/tests/monster_profile_lan.py `
  --engine build/native-on/Release/doom-lan-smoke.exe `
  --iwad <DOOM2.WAD> --out build/lan-verify-final
```

The demo recorder uses the engine's existing exit-1 completed-recording path;
the test requires its completion message and valid output. Playback uses exit
0. The LAN script verifies only its two localhost UDP peers and terminates its
own children on failure. It does not use the headless networking stub, alter
firewalls, or establish Internet/multimachine compatibility.

`monster_profile_lobby.py` additionally exercises the existing in-game TCP
host/join path with a shared test content identity. Its basic mode is separate
from `--features`, which enables the existing aim/photo/wheel harness. The
latter failed its weapon-wheel assertion with profiling both enabled and
disabled; retain both logs rather than calling that UI check a pass. `--disabled`
provides that profiling-disabled control. The failed peer then causes the
other peer's final player-count assertion to fail as well.

Run existing `settings_persistence.py` against `doom-critical-smoke`, with an
isolated generated `ragdoll_arena.py` fixture. Run the combat integration
executable directly for wounds, damage, projectile, injury and survival tests.
Use `doom-reload-smoke` with the original DOOM2 IWAD for ten cycles each of
`restart`, `save`, and `autoload`, passing `-profilemonsters` after its four
required arguments. Keep each diagnostic folder isolated.

The profiler contract checks nested exclusive arithmetic, worker separation,
window-crossing jobs, bounded-buffer rotation, unavailable storage, population
eligibility/hysteresis, and a disabled scope. MSVC builds here do not provide
a ThreadSanitizer result.

## Phase 3 regression gate

Any separately authorized optimization must retain the full Phase 2 inputs
and pass tic-by-tic state and RNG comparisons before timing comparisons.
Repeat the normal stock and MAP02 controls, all five map/scenario cases, and
the feature preset affected by the patch. Include warm/cold cache behavior,
moving sectors, actor spawn/removal, save/load and map transitions where cache
state is involved. Exact LOS work must retain REJECT, BSP/solid-world results,
doors, moving platforms and partial-height occlusion. Collision work must
retain callback ordering, early exits, overlap escape, missiles and teleports.

Report paired absolute and relative costs with run variation; do not infer
FPS improvements from simulation-only measurements. GPU-fenced diagnostics
remain separate from ordinary presentation/frame-interval comparisons.
