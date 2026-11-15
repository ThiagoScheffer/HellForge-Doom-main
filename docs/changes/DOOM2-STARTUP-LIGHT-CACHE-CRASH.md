# DOOM2 startup crash: light-blocker cache initialization

```yaml
project: DOOM: HELLFORGE
build: 2026.11.15.6
updated_utc: 2026-11-15T09:19:38Z
status: fixed; focused-startup-verification-passed
tests: ../../duct-tape-doom-main/tests/light_cache_startup.py
index: ../README.md
```

The supplied `test-references/crashlog/last-game.log` and minidump identify engine SHA256 `9ec75d28d04478c105201025c6a5028879facb83c5a4d584392c83c5adce6b24`. The local pre-fix executable matches exactly. Its matching PDB resolves exception RVA **0x125238** to `doom3d::appendLight`, called by `collectSurfaceLights`, `geometry`, `prepareFrame` and `I_Render3DPresent`. The exception is a read access violation at **0x38**, after baking/restoration completes and before the first rendered frame.

The new blocker cache had a `noteSectorChanges()` initializer that was never called. The first frame could store entries while the sector-change vector was empty; reusing an entry then accessed that uninitialized metadata. The missing call also prevented the intended per-frame reference reset, sector invalidation and level-change cache clearing.

**Fix:** `collectFlashes()` calls `noteSectorChanges()` before collecting any lights. All other current user edits, lighting features, rendering controls and shaders are retained. No gameplay RNG, actor-layout or save-format change. Existing persistent lighting caches can remain in place.

## Focused verification

- Pre-fix binary reproduces the access violation on original **DOOM2 MAP01** using an isolated copy of the current graphics profile and normal solid-map/Fluid integrations. The regression's cold pass exits **3221225477 / 0xC0000005**.
- Fixed Release engine and native fixture build successfully. The focused fixture renders through four frame callbacks, verifies the first submitted frame and quits through normal `I_Quit`, with **exit 0 in separate cold-cache and warm-cache processes**. No crash dump is produced.
- **19 launcher packaging checks pass**, including matching embedded-engine hash, product/version metadata and normal profile defaults. The existing LanMenu CS4014 warning remains.
- No broad WAD/combat suite was run. The older death/Use fixture rendered successfully after the fix but did not complete its restart under this profile within 90 seconds; that attempt is **inconclusive for restart behavior**. The dedicated startup mode verifies the reported load-and-quit failure only.

The engine change is one initializer call in a lighting file that already had unrelated user modifications. Existing changes are preserved; no commit, reset or merge was performed.

Run the focused regression from the repository root with a fresh output directory:

```powershell
python duct-tape-doom-main/tests/light_cache_startup.py --iwad test-references/DOOM2.WAD --graphics path/to/graphics.cfg --engine build/Release/doom-reload-smoke.exe --out build/startup-light-cache-check
```

The fixture uses isolated copies of the profile, cache and diagnostics. It never changes the supplied graphics file or the normal game profile. Evidence: `build/crash-investigation/`, including preserved pre-fix binaries/symbols, `symbolized-original.txt`, `regression-red/`, `startup-quit-fixed/` and build/package logs. The original user logs/dump remain untouched. Selected evidence and matching symbols are packaged with build 2026.11.15.6; user dumps, WADs, configs and saves are excluded.
