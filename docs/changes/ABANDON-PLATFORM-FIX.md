---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T14:56:15Z
status: targeted-regressions-passed-manual-playtest-pending
index: ../README.md
---

# Abandon MAP01: spawn switches fail to lower the platform

The supplied WAD is in the experimental worktree's `test-references/Abandon.wad`.
All changes and test artifacts remain on `experiment/monster-profiling-phase2`.
Main, previous release executables and the WAD are unchanged.

## Reproduction and cause

Spawn switches are lines 1339 and 1351, special 112. They correctly open hidden
door sectors 357/358, tagged 2/4. They do not directly lower the visible platform.
A conveyor-driven voodoo doll must then pass both gates and cross line 1537,
special 38, to lower the tag-5 platform sectors 115, 144 and 145.

The engine applied conveyor force only to actors whose centres were in the
conveyor sector. The doll's centre entered a small triangular gate sector while
its footprint still overlapped the conveyor. Force stopped, inertia ran out and
the doll stalled before the downstream trigger. The earlier responsive-friction
fix remains necessary, but was insufficient for this slower conveyor arrangement.

Before the fix, the real Use command path consumed both switches, both hidden
ceilings reached 124, but line 1537 remained special 38 and the platform floor
stayed at 128. The regression exited with code 6.

## Change

`linuxdoom-1.10/p_boom.c` now includes actor footprint contact with conveyor
sectors, rather than centre-sector membership alone. At map initialization it
builds unique candidate-sector lists from geometry bounds and the maximum
defined actor radius, including loaded actor definitions. Per-tic checks use
bounding boxes and line-side tests on those candidates. There is no whole-thinker
scan per conveyor, no new collision broad phase, and no per-tic allocation.

Each actor receives each conveyor's impulse once, including corner contacts.
Noclip actors remain excluded; above-floor and gravity/submersion eligibility is
checked against the conveyor's floor. Non-BLOCKMAP actors remain eligible through
sector lists. Centre-sector behavior remains supported. Maps without carry
scrollers skip candidate preparation, and zero-impulse scrollers skip actor work.

The cache contains sector indices/bounds, no actor pointers. It is reconstructed
on map/save initialization. Actor layout, save-extension version/length and
scroller persistence fields remain unchanged. No random calls were added to
the conveyor helper. The fix changes broken conveyor outcomes deliberately;
it does not claim state identity against the formerly stalled map.

## Targeted verification

- Abandon MAP01 passes in both switch orders. One switch alone must not release
  the platform. After both switches, line 1537 consumes and all three platform
  floors lower from 128 to 64.
- Hibernation off/on traces match canonical state and gameplay RNG calls, trace
  and index across 350 tics. Save/load between switches also completes the chain.
- Twelve conveyor tests pass: adjacent footprint and non-BLOCKMAP contact,
  separated footprint, noclip, gravity, elevated footing, corner deduplication,
  large actor radius, submerged carry, save round-trip/reconstruction and leaving
  contact.
- The prior dmnsns2k MAP01 switch/conveyor activation and off/on state/RNG
  regression passes. Its previous user-confirmed playtest remains recorded.
- A bounded 35-tic oku MAP01 smoke test completes with 23,216 monsters. This is
  only a startup/compatibility smoke check, not a new performance benchmark.
- Release headless/native builds, embedded-engine packaging checks and
  `git diff --check` pass. No broad historical test matrix was repeated.

Reproduction driver:

```powershell
python duct-tape-doom-main/tests/abandon_platform.py `
  --engine build/profile-off/Release/doom-headless.exe `
  --iwad F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/DOOM2.WAD `
  --wad test-references/Abandon.wad --out build/abandon-platform/recheck
```

The fixture positions the player in front of each switch, then uses normal tic
commands and the actual Use-line, door, conveyor, crossing and floor paths. No
doll is manually advanced and no trigger is forced. Logs, commands, hashes and
CSV traces are under `build/abandon-platform/`. Tests do not constitute a complete
Abandon playthrough or coverage of every Boom platform action, demo or net mode.

## Updated test executable

`build/experimental-release/DOOM-Hellforge-Experimental-Platforms.exe` embeds the
fixed engine and retains the general WAD-selection launcher. Select Doom II and
add Abandon for manual confirmation. Existing experimental F4 choices persist;
release profiles remain separate. Previous experimental executables are retained
so an open launcher or earlier test build is not overwritten.
