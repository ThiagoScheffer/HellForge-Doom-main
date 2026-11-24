---
build: "2026.11.15.6 (launcher-only rebuild)"
updated_utc: "2026-11-17T09:06:11Z"
status: "implemented; focused checks and launcher captures passed"
index: "../README.md"
tests: "../../launcher/LauncherUpdateTests.cs"
---

# Launcher icon, quick tools and settings tabs

The launcher now shows an original ember/charcoal forge icon at six sizes,
four build-label highlights (destruction, weak points, survival AI and Encounter
Director), and an animated Arch-vile with fire beneath the build version.

The mascot reads a few Arch-vile/fire patches and PLAYPAL from the selected
Doom II-compatible IWAD, including mirrored combined sprite names. No commercial
sprites are redistributed. An IWAD without those sprites displays a placeholder.
Animation stops and decoded images are released while the game runs; minimized
windows do not animate or repaint. It is cosmetic and creates no game actors.

## Quick tools

- **Open Last Log** opens `%LOCALAPPDATA%/DOOM-HellforgeLauncher/last-game.log`.
- **Open Graphics.cfg** opens the active profile's graphics configuration.
- **Game Settings / Tabs** edits graphics preferences before launching.
- **Clear All / Fresh Start** asks for confirmation, creates a recovery ZIP under
  the launcher `backups` folder, removes only `graphics.cfg`, `doom.cfg` and
  `heretic.cfg`, clears selected add-ons and restores Low-FX starting defaults.
  IWAD selections, save files, screenshots, logs and backups are retained.
  A failed backup prevents reset; a partial filesystem failure is reported.
  This action leaves the lighting cache intact; its existing clear button is separate.

These controls are disabled during launch/game execution. Missing log/config
shortcuts explain that the file has not yet been created.

## Engine arguments

Type a flag such as `-goredebug` into **Engine Arguments** before choosing
**Play Game** or **LAN / Co-op**. To prefill the field, start the launcher from
PowerShell with `./DOOM-Hellforge.exe -goredebug`. Quote any argument value
that contains spaces. The field lasts for the current launcher session, and
**Open Last Log** opens the engine output from the most recent run.

## Settings editor and limits

The checked-in catalogue exposes **355 F4 and balance controls**, grouped into
category tabs. On/Off and enumerated values have dropdowns; numeric fields show
known limits. Existing unknown keys appear under Advanced / tuning. The native
engine remains responsible for hardware limits, snapping, dependencies, valid
bindings, and demo/network/voxel restrictions. Unavailable stasis remains
unimplemented even if its stored preference is edited.

Blank fields remove an override rather than guessing an engine default; old
legacy keys can still influence the effective value. Existing `doom.cfg` and
`heretic.cfg` get tabs opening their native text format in Notepad, including
advanced controls/bindings. This is not a live preview or a full recreation of
F4 dependency handling; changes apply on the next launch.

Apply creates a settings backup, preserves unrelated graphics keys/comments,
replaces duplicate edited keys consistently, rejects nonfinite numbers and
known invalid choices/ranges, and publishes through a same-directory temporary
file. External config changes detected since the dialog opened prevent saving.
Cancel writes nothing. Existing F4 apply/discard and import/export paths remain.

The catalogue is generated from native declarations by
`python tools/Generate-LauncherSettings.py`. Normal builds use the checked-in
XML and need no Python; packaging warns when the F4 source hash is stale.
`powershell -ExecutionPolicy Bypass -File tools/Generate-LauncherIcon.ps1`
regenerates the original vector-style icon and preview with System.Drawing.

## Focused verification

- Launcher-only rebuild succeeded; native engine and four-part version unchanged.
- **13** config/backup/reset/catalogue/artwork checks passed in an isolated profile.
- Existing settings-backup checks and **54** button repaint checks passed.
- **19** release packaging checks passed, including embedded engine equality.
- Captures under `build/launcher-update-check` cover the full launcher at 1,150
  and 940 client widths, settings tabs and decoded mascot artwork. Minimum-width
  layout was visually inspected. These are WinForms captures, not a full physical
  high-DPI/multi-monitor acceptance test.
- No engine build, broad gameplay suite or browser tests were run. The existing
  unrelated `LanMenu.cs` CS4014 warning remains.

Rebuild with `powershell -ExecutionPolicy Bypass -File launcher/build.ps1 -LauncherOnly`.
The output is `dist/DOOM-Hellforge.exe`; the release number remains 2026.11.15.6
with a new UTC launcher timestamp. No game format or gameplay behavior changes.
