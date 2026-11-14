# E1M8 lag capture

Close the game, then double-click **Trace-E1M8-Lag.cmd**. It uses
`dist/DOOM-Hellforge.exe` to select the matching engine, normal bundled resources
and your existing graphics settings. It starts original DOOM.WAD on E1M8; F3
can load your existing E1M8 save instead. No external add-ons are selected.

Reproduce the lowering-platform slowdown, watch it for roughly ten seconds,
then quit normally. Send the ZIP printed by the script under
`build/lag-capture/<UTC timestamp>-UTC.zip`.

The ZIP contains separate gameplay/sound and renderer/geometry timing logs,
the starting graphics settings and build/resource hashes. No WAD or save is
included. Existing saves remain in the normal game profile. Loading/saving
and applying F4 changes behave normally during this run.

The switches are `-profilespikes -profilegamesteps`; this uses existing
diagnostics without GPU fencing or gameplay changes. Logging adds overhead,
so use the capture to locate the stall, rather than as a benchmark. Normal
launcher launches do not enable these switches. The diagnostic log is separate
from the launcher's `last-game.log`.

Optional PowerShell usage:

```powershell
.\tools\Trace-E1M8-Lag.ps1 -Iwad 'C:\Games\DOOM.WAD'
.\tools\Trace-E1M8-Lag.ps1 -ValidateOnly
```

`-ValidateOnly` checks the launcher interface, IWAD and graphics profile without
starting the game, extracting resources or changing settings.
