---
project: DOOM: HELLFORGE
build: 2026.11.14.1
updated_utc: 2026-11-13T19:26:29Z
status: active
tests: MAIN-INTEGRATION-2026-11-14.md
index: ../README.md
evidence_scope: existing-feature-evidence-and-focused-integration-checks
---

# Load last save after death

Updated: 12 November 2026.

Enable **F4 → Gameplay → Save recovery → Load last save on death** and
apply the settings. The persisted graphics key is death_load_last_save
(default 0). After death, pressing Use loads the most recent save successfully
written or loaded in the current engine session through the normal save loader.

This does not create automatic saves. After starting the engine, first load a
save or make a save. If that file is missing or its native version header is
incompatible, Use restarts the map normally. Multiplayer, demos, and Arcade
retain their normal death handling. Save files and their format are unchanged.

Verification: the original DOOM2 MAP01 live death/Use harness restored the
saved 77 armor points through three consecutive deaths. It also checked
disabled-option, missing-file, network and demo guards before each death.
The existing 29 registered non-bigmap CTest checks pass.
Evidence: build/death-autoload/autoload.log.
