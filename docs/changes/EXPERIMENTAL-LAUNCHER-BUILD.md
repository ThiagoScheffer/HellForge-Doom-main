# Experimental launcher for arbitrary WAD testing

The `experiment/monster-profiling-phase2` worktree can build a self-contained
`DOOM-Hellforge-Experimental.exe` embedding the tested native engine and existing
launcher resources. Main and release deliverables are unchanged.

```powershell
./launcher/build.ps1 -LauncherOnly -Experimental `
  -EnginePath ./build/engine-profile/Release/doom.exe
```

Output: `build/experimental-release/DOOM-Hellforge-Experimental.exe`.
Double-click it, select the appropriate owned IWAD, add the desired PWAD(s) in
the existing add-on list, then launch. No map or WAD is hardcoded. The usual
classic/Boom compatibility boundaries still apply.

This flavor uses separate `DOOM-HellforgeLauncher-Experimental` local storage
and `Local Games/DOOM Hellforge Experimental` roaming profiles. Its title/build
display identify it as experimental. New profiles enable idle hibernation;
existing experimental F4 choices persist. Launch arguments use `-nodigging`
instead of solid-map mode, matching the tested compatibility path. Default
release build behavior is unchanged. Hot-path profiling remains disabled unless
explicitly requested; this build does not implement deferred monster spawning.

Verification: `launcher/ExperimentalBuildTests.ps1` checks the embedded engine
hash, profile isolation, default setting and add-on argument routing for both
supplied monster WADs. This packaging task does not repeat engine benchmarks.

Known extreme-map costs remain documented in [the oku report](OKU-EXTREME-STRESS.md).
