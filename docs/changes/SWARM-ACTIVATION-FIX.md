---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T12:32:15Z
status: targeted-regression-passed-manual-retest-pending
index: ../README.md
parent: SWARM-OPTIMIZATION-STEP1.md
---

# MAP01 monster-release activation fix

The user reported improved idle navigation but failed monster activation after
switches in the experimental MAP01 build. A focused reproduction found a failure
with idle hibernation both off and on. The idle-search cache was not the cause of
this reproduced release-chain failure.

Manual follow-up: the user confirmed the second switch worked, then confirmed
the `dmnsns2k-monsters.wad` MAP01 playtest passed before the oku stress task.

## Cause and fix

`P_XYMovement` applied responsive player friction to every actor with a `player_t`
pointer. Boom voodoo dolls share that pointer with the controlled player body.
The stronger braking stopped a conveyor-driven doll before the next walkover
trigger, interrupting MAP01's monster-release machinery.

The responsive-friction branch now also requires `player->mo == mo`. The
controlled player retains responsive movement; voodoo dolls retain vanilla
inertia. No hibernation, spawning, collision, damage, save-format or RNG changes
were needed. This narrow compatibility fix remains in the experimental worktree;
main and release artifacts were not updated.

## Regression evidence

The new benchmark-only `activation` scenario uses supplied
`dmnsns2k-monsters.wad` MAP01 with Doom II, hard skill:

1. Shoot the real starting gun switch, line 82, using ordinary pistol fire.
2. Position the player in front of the first encounter's Use switch, line 2501,
   identically in both runs. This positioning shortcut is test setup, not normal
   gameplay or an engine change. Press Use through the normal tic-command path.
3. Require both switches to consume, the conveyor doll to cross line 5043, its
   downstream sector 47 floor to open, and monsters to acquire targets through
   ordinary AI. No monster states or targets are forced.

Before the fix, both switches consumed but conveyor line 5043 remained active
and sector 47 stayed closed. After the fix, the release chain completes with
hibernation off and on, and 1,758 living monsters have targets at the end of the
short scenario. This count is not a claim that every staged encounter has fired.

Final paired runs matched canonical state and gameplay RNG calls, trace and index
after every tic: 70 warmup plus 350 measured tics. The enabled run recorded 506
reused idle searches and 1,752 sound wakes during the measurement window.
This was a correctness check, not a new performance benchmark.

Run only this affected regression:

```powershell
python duct-tape-doom-main/tests/swarm_compare.py --activation-only `
  --engine build/profile-off/Release/doom-headless.exe `
  --iwad F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/DOOM2.WAD `
  --wad F:/Programming/C/Tuin-Doom-Enhanced-main/test-references/dmnsns2k-monsters.wad `
  --out build/swarm-activation/recheck
```

Raw samples, resource/executable hashes and original failing logs are under
`build/swarm-activation/`; final evidence is in `final/` and `final.log`.
Release headless and native builds and `git diff --check` passed. Temporary
diagnostic probes were removed. The broad test matrix was not repeated.

## Manual test build and remaining scope

The rebuilt executable and matching symbols are in
`build/swarm-activation/engine/`. Double-click `Play-MAP01.cmd` there. Its test
configuration enables hibernation, diagnostics and classic geometry via
`-nodigging`; existing main settings are untouched.

The GPU environment limitation from the previous report remains. Later encounter
chains and full-map completion need manual confirmation. The optimization remains
experimental; this report does not approve migration to main.
