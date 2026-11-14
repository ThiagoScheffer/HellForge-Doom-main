# HUD background opacity

```yaml
project: DOOM: HELLFORGE
build: 2026.11.15.2
updated_utc: 2026-11-14T12:42:40Z
status: implemented
index: ../README.md
```

Two independent persisted sliders range from **0% to 100%**, in 5% steps:

| Location | Config key | Default | Controls |
| --- | --- | ---: | --- |
| F4 > Display > View > HUD background opacity | `hud_background_opacity` | 75% | Modern health/armor/ammo/face backdrops, empty meter tracks and corner-stat backdrops |
| F4 > SPEEDRUN > Background opacity | `speedrun_background_opacity` | 80% | Speedrun statistics panel backdrop |

At 0% the dark backdrops are fully transparent; text retains full opacity. At 100% backgrounds are opaque. Informational bars, portraits and icons retain their existing controls and appearance. Classic status-bar artwork and combat-notification backdrops are unchanged. Defaults approximate the previous panel opacity.

Settings use the existing F4 draft/apply/discard workflow, graphics-config parser and atomic writer. Apply with Esc/F4 or Apply/close. No gameplay, actor status, save trailer or combat rules changed.

Focused verification uses the existing native settings fixture: `--settings-opacity-write` checks composed panel/stat pixels at 0/25/100% and opaque text, then saves separate opacity values; `--settings-opacity-read` verifies them in a new process and checks range clamping. Evidence is under `build/hud-opacity/`. Engine and launcher are rebuilt as 2026.11.15.2; no broad combat/WAD test matrix is required for this UI-only change.

Results: **13 focused native pixel/persistence assertions passed** across two processes; **19 normal-release packaging checks passed**. Native Release build exit status was verified as zero. No GPU capture or broad combat/WAD suite was run for this small UI change.
