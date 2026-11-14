# Speedrun HUD positioning and layout

```yaml
project: DOOM: HELLFORGE
build: 2026.11.15.4
updated_utc: 2026-11-14T14:58:02Z
status: implemented
index: ../README.md
```

**F4 > SPEEDRUN** adds:

- **Info layout** (`speedrun_layout`): **Column** (default, original stacked information) or **Row** (compact side-by-side metrics).
- **X adjustment** (`speedrun_x`): -100 to +100 percent of the full screen width; negative moves left, positive moves right.
- **Y adjustment** (`speedrun_y`): -100 to +100 percent of the screen height; negative moves up, positive moves down.

Offsets default to zero and adjust the selected existing anchor. Both sliders use 1% steps. As of build **2026.11.15.4**, the complete backdrop/text panel clamps to the physical widescreen edges, matching the modern HUD's docked corners. A dedicated atlas layer and quad preserve the original font proportions; horizontal movement is no longer limited by the centered 4:3 HUD viewport. The column backdrop fits its longest visible label, leaving only the normal small text padding at an edge. Large row values still wrap within the logical canvas. Existing config keys, stored values, layout choices and opacity controls are preserved.

See [E1M8 and screen-edge verification](E1M8-AND-SPEEDRUN-FIXES.md) for this build's focused native/GPU checks.

Row mode displays the map and enabled metrics on one horizontal bar for ordinary values. Short labels are `ATT` (attempts), `DTH` (deaths), `KS` (killstreak), `TIME` (attempt time), `RUN` (session runtime), `DEALT` and `TAKEN` (session damage). Long values wrap to another row when necessary rather than leaving the screen. Column labels and metric semantics remain unchanged. Hidden metrics consume no layout cells.

Background opacity works in both modes. All choices persist through the existing F4 apply/save/export/import mechanism; Esc/F4 or Apply/close applies, F10 discards. No gameplay, session accounting or save format changes.

Focused verification uses `--settings-layout-write` / `--settings-layout-read` in the existing native settings fixture: default preservation, both layouts at extreme/zero offsets, whole-panel bounds, actual compact row painting, side-by-side cells, long-value wrapping, hidden metrics and cross-process X/Y/layout persistence. The existing two-process opacity cases also run as a directly related regression. Artifacts: `build/speedrun-layout/`. No broad combat or WAD test matrix is required for this UI-only change.

Historical 2026.11.15.3 results: **13 layout/position checks and 13 opacity regression checks passed**, across isolated native processes. **19 release-package checks passed**; normal engine and launcher rebuilt successfully. No GPU capture or broad gameplay suite was run for this small UI change.
