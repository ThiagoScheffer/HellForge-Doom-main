---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T19:10:05Z
status: f4-performance-control-targeted-checks-passed
index: ../README.md
tests: ../../duct-tape-doom-main/tests/settings_persistence.inc
---

# Ambush sight reuse: F4 Performance control

Added **F4 -> Performance -> Monster visibility -> Ambush sight reuse**.
Default off; persisted graphics key `ambush_sight_reuse` accepts 0/1.
Esc, F4 and Apply/close retain the existing apply/save workflow. F10 discards.
The applied choice reaches the simulation immediately; saved choices are also
applied during startup, including software rendering. Existing profiles are
preserved and missing keys use the off default.

The original stack-local reuse algorithm, demo/network exclusions and wall
destruction remain unchanged. Legacy CLI flags still work as explicit startup
overrides; they are unnecessary when using F4. No save conversion is required.
All changes and artifacts remain in the experimental worktree; main is untouched.

## Targeted verification

Six isolated native settings processes passed 14 checks: default off, real
Performance-row selection and F4 apply, separate-process restoration, F10
discard without a file change, immediate disabling through F4 and saved-off
restoration. The tests found and corrected startup application that otherwise
waited for renderer settings. The tests used dummy video and isolated configs;
no user profile was changed. The prior gameplay/performance matrix was not rerun
because the visibility algorithm did not change.

Engine and settings harness builds passed. Twelve experimental packaging checks
passed for embedded engine identity, storage isolation, defaults, destruction
mode and SF2012 routing. `git diff --check` passed. Existing conversion/compiler
warnings and the launcher LAN async warning remain.

Raw results: `build/ambush-sight/f4/settings-results.json`; logs and isolated
profile beside it. Existing [MAP02 equivalence and timing evidence](AMBUSH-SIGHT-REUSE.md)
remains the basis for the underlying optimization.

## Delivery

`build/experimental-release/DOOM-Hellforge-Experimental-Sight-F4.exe` is the
self-contained launcher. Run it normally and use F4; no additional flags.
Matching native engine and PDB are under `sight-f4-engine/` beside the launcher.
The package identity and hashes are in `manifest-sight-f4.json`.
