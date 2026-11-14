# E1M8 exit contact fix and Speedrun screen-edge positioning

```yaml
project: DOOM: HELLFORGE
build: 2026.11.15.4
updated_utc: 2026-11-14T15:18:18Z
status: implemented-exit-and-hud; live-floor-lag-confirmed-unfixed
tests: ../../duct-tape-doom-main/tests/e1m8_regression.inc
index: ../README.md
```

## Delivered changes

Speedrun X/Y adjustments now use a dedicated HUD atlas layer and full-width quad. Both axes can reach the screen edges from every anchor, without stretching glyphs. Column backdrops fit the longest visible label. Existing controls, saved values, rows/columns and opacity remain supported. See [controls](SPEEDRUN-LAYOUT.md).

`P_PlayerInSpecialSector` previously required an exact fixed-point match between the player's Z and the sector floor. Solid-world support on original E1M8 settles **66 fixed units (about 0.001 map units)** below that plane, so special 11 stopped running and the player remained at 1 HP. Grounded solid-world contact within **1/64 map unit** of the native plane now counts. Non-solid behavior retains its original exact check. Raised supports and airborne actors remain excluded. The native special-11 exit threshold and damage protection are unchanged: original E1M8 completes at low health; it does not require a literal player death.

## Small, map-specific verification

Only the supplied original **DOOM.WAD / E1M8** was used, with an isolated copy of the supplied graphics settings. No user file was edited. Native tests use the launcher-standard solid world, UZ movement, FluidMonsters and bundled weapon/sound integrations, with no external add-ons. Music/sound output are disabled for unattended testing. Fullscreen output was 3072×1920 at the supplied 50% rendering scale (1536×960).

- Exit baseline **failed**: 224 tics, 1 HP, no completion, 66-fixed-unit contact mismatch.
- Diagnostic exact-plane snapping **passed**, identifying the contact guard.
- Production fix **passed**: completion in 179 tics, including negative controls for raised support and falling above the plane. No snapping is used by the fixed production path.
- **16 screen-corner assertions passed** across four anchors and extreme X/Y offsets. A native GPU capture confirms Speedrun text at the physical bottom-right edge with the modern HUD still docked independently. Overlap is possible when the user deliberately positions panels together.
- Native Release engine and diagnostic target built successfully. **19 release-package assertions passed**, including embedded-engine identity, product/version metadata and normal profile defaults. `git diff --check` passed.

## Lowering lag remains unconfirmed

The second reported issue has **not been fixed or dismissed**. Full 345-step floor descents completed in these focused measurements:

| Scenario | Lowering median | Lowering p95 | Samples |
| --- | ---: | ---: | ---: |
| Isolated Baron-death control | 8.330 ms | 10.939 ms | 345 |
| Shotgun combat, approach view | 8.372 ms | 9.853 ms | 345 |
| Shotgun combat, courtyard view | 8.300 ms | 10.097 ms | 345 |

These are local frame intervals, not an FPS improvement claim or a cross-machine benchmark. Test setup uses god mode, teleports beside targets, fires the actual standard-shotgun action every 35 tics, activates the original tag-5 entry door trigger, then observes the lowering sector. The two shotgun runs required 17 scripted shots; enabled injury/AI effects remain active according to the supplied settings. Scripted positioning and firing differ from a complete manual playthrough, so prior movement, destruction and combat history are not reproduced. Earlier blocked-shot and incomplete one-sample attempts are excluded from this table.

No speculative platform optimization was applied. The next useful input is a save immediately before the second Baron's death, plus `last-game.log` from an affected run on build 2026.11.15.4. This preserves the actual world/effect history and viewpoint needed to reproduce the lag.

## Follow-up: live lag capture confirmed

The user-provided `test-references/20261114-151315-823-UTC.zip` passes ZIP CRC validation and contains gameplay/renderer logs, starting graphics settings and a complete run manifest. It identifies build **2026.11.15.4**, original DOOM.WAD with normal bundled integrations, both diagnostic switches and normal exit code 0. This **does capture the reported severe slowdown**, unlike the earlier scripted scene.

The worst logged frame at tic **3050** is **213.007 ms**, approximately **4.7 FPS** instantaneous equivalent. Its gameplay-loop sample is **141 ms**; the frame reports **142.752 ms outside rendering**, **62.614 ms preparation**, **6.959 ms upload**, **0.536 ms draw-command encoding**, and **0.098 ms submission**. Geometry diagnostics at the same tic attribute **42.951 ms** to CPU wall work. Geometry time is nested inside preparation; gameplay is nested inside outside-render time. Do not sum nested timings or interpret command encoding as measured GPU execution.

A slow non-mobj thinker is logged for **344 tics, 3040?3383**, taking up to **35 ms per invocation**. This is consistent with a moving-sector operation but the generic `type -1/state -1` marker does not identify its exact callback. Multiple catch-up simulation tics between frames amplify the frame stall. The leading investigation targets are moving-sector/world updates and associated CPU wall preparation. The trace alone does not prove the exact underlying function or a fix.

Only initial level construction/baking is logged; no mid-run lighting rebake or reload appears. No error/exception is recorded. The captured settings differ from the earlier supplied file only in `fps 1` and **`sun_shadows 0`**; both must be retained for the next focused reproduction rather than assuming identical settings. Existing spike-only logs are unsuitable for computing overall median FPS. No engine change or additional gameplay tests were made during capture validation.

## Reproduction and artifacts

Build the existing `doom-critical-smoke` target, then from repository root:

```powershell
build/Release/doom-critical-smoke.exe test-references/DOOM.WAD test-references/graphics.cfg <existing-output-directory> --e1m8-exit
$env:HELLFORGE_E1M8_COMBAT = '1'
build/Release/doom-critical-smoke.exe test-references/DOOM.WAD test-references/graphics.cfg <existing-output-directory> --e1m8-floor
$env:HELLFORGE_E1M8_COURTYARD = '1' # optional second view
```

Without `HELLFORGE_E1M8_COMBAT`, the floor diagnostic uses direct Baron damage to isolate the moving-floor event. These hooks exist only in the diagnostic executable. The normal launcher/engine contains no E1M8 test setup. Logs, frame CSVs and capture: `build/e1m8-fixes/`; selected evidence ships in `dist/verification/2026.11.15.4/`. Matching engine symbols are in `dist/symbols/2026.11.15.4/`.

Supplied input hashes (not packaged):

- DOOM.WAD SHA256: `ff2c301b8719465a6e386a512bfa319931b7f64ea517d337c5a47afe03951902`
- graphics.cfg SHA256: `1d1e2eaf19bc192d26adbf9647056a2fed4746cf27926da8bd8ead67c97ff1f1`

No broad WAD, combat, network or save-format test matrix was run. General death-trigger maps and other render backends were not independently verified by this focused check.
