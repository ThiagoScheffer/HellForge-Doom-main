---
updated_utc: "2026-11-15T15:10:52.442172+00:00"
status: "reproduced; isolated to solid-world-enabled live path; no engine changes"
index: "../README.md"
---

# 4000 Monsters MAP01: quick lag diagnostic

The supplied launcher log is build 2026.11.15.6, engine 4658ea7d07f4.
It exits normally (code 0), with 4,570 mobjs and 4,317 dormant monsters at
initialization. The 2.7-second lighting bake is a loading cost, not a measurement
of live play. Fifteen unknown type-32000 placements are a separate compatibility
warning, not established as the cause of lag.

## Focused reproduction

Used the existing local `build/Release/doom-perf-smoke.exe` (no rebuild), supplied
Doom II IWAD and 4000 Monsters WAD, MAP01 spawn (160,-304), copied current
HELLFORGE graphics settings, fullscreen 3072x1920 at 50% scale (1536x960).
The local diagnostic binary is not asserted byte-identical to the attached
release. Existing source modification in m_menu.c was preserved.

| Check | Result |
|---|---|
| Paused spawn rendering, solid world | Worst confirmed view: CPU 9.5 ms, GPU 7.2 ms; sprite lighting 4.7 ms. Four sampled views, none over 16.7 ms. |
| Live run, `-solidmaps -uzmovement` | One measured interval of 7,986.21 ms; zero plasma cells fired. Exited 1 because firing validation failed. |
| Live comparison, `-nodigging -uzmovement` | 394 intervals; median 9.85 ms, p95 12.82, p99 13.88, max 14.58; 54 plasma cells fired; exit 0. Reported 4,311 target-bearing monsters at tic 175. |

Live scenarios request four seconds plus one second warmup, god mode and held
plasma fire through the existing harness. The solid run's long stall overruns
that window; its single sample is NOT a valid distribution or matched combat
benchmark. The classic run's wake count describes target-bearing actors, not
instrumented first idle-to-see transitions. Paused rendering measurements use
GPU fences; live measurements use normal rendering. Do not combine them into
a simulation-time estimate or claim a general FPS improvement.

## Finding and next narrow investigation

The severe symptom is reproduced on the live solid-world-enabled path. Classic
geometry completes this short active-monster check at playable intervals. This
supports investigating solid-world simulation queries before optimizing particle
FX, resolution or ordinary sprite rendering. It does not isolate one function:
`-nodigging` changes multiple solid-world behaviors.

First profile `P_CheckSight` -> `SV_Sight` (p_sight.c:333;
platform/solid_runtime.cpp:476), then solid position/support queries. The sight
function tries up to three solid traces before classic REJECT/BSP handling;
previous SF2012 measurements identified it as expensive, but no per-function
attribution was collected for this WAD. Preserve remote monster activation and
teleport triggers. Do not bypass sight, freeze distant monsters, or use static
REJECT results blindly for destructible geometry.

Temporary workaround: use classic geometry (`-nodigging`); this disables the
solid/destruction feature set for that launch. Merely switching off visual wall
effects is not equivalent. No engine fix, launcher rebuild, or broad suite was
performed during this diagnostic.

Raw commands, hashes, isolated graphics settings, logs, spawn screenshot and
renderer report are under `build/4000-monsters-diagnostic/`. No user settings
were edited. Results are short diagnostic evidence, not an all-map playtest.
