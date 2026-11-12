# Reload stability and persistent lighting cache ? HELLFORGE 2026.11.12.1

```yaml
project: DOOM: HELLFORGE
build: 2026.11.12.1
updated_utc: 2026-11-11T17:52:02Z
status: active
tests: ../../build/reload-release-verification/results.json
index: ../README.md
```

## Reload failure

The original DOOM2 MAP01 reproducer spawned blood, puffs, rocket and plasma explosions, killed the player and restarted through Use. Before this fix, baking completed, then `drawScene` threw `invalid unordered_map<K, T> key`. The previous release cleared images at map transitions while retaining empty decal/effect batch keys. Drawing those empty batches still looked up their reclaimed images. A diagnostic-only terminate handler recovered that exception and stack.

A shared `R_BeforeLevelReset` barrier joins/discards relighting at the start of game level setup, before zone allocations or sector data are freed. Renderer level transitions also join/discard outstanding relighting, clear image-dependent geometry and reference caches, then release images. Empty batches do not resolve images. A nonempty batch with a missing image raises a descriptive error, including batch category, image key and level serial. Wound classifications, glass images and bake masks are invalidated before image reclamation. Full renderer shutdown uses the same geometry cleanup.

Bake workers propagate exceptions after joining. Background relight errors propagate to the render thread; a discarded level job cannot publish into the next level. Native diagnostics log uncaught C++ exceptions, stack RVAs, engine version, loading stages and the first submitted frame. Windows crash dumps use `dbghelp.dll` from System32 when available. The launcher flushes output continuously and records decimal/hexadecimal exit codes. Release `doom.pdb` must remain paired with the exact engine SHA-256.

## Controls and storage

- **F4 ? Lighting ? Cache map lighting** (`map_light_cache 1`) defaults on. Apply/close persists the setting through the existing F4 workflow.
- **Clear lighting cache** in the launcher removes generated cache entries and interrupted temporary files. It is disabled during game launch/play and preserves unrelated files and linked directories.
- Launcher cache root: `%LOCALAPPDATA%/DOOM-HellforgeLauncher/Cache/Maps/`.
- Entries: `<MAP01-or-E1M1>-<resource-content-hash>/<stage>-<input-hash>.bake`.
- Native launches can set `HELLFORGE_MAP_CACHE` (or legacy `TUIN_MAP_CACHE`) to a separate root. Otherwise SDL's per-user `DOOM-HellforgeLauncher` preference folder is used. `-nobakecache` bypasses reads/writes without changing the persisted setting.
- Completed entries use least-recently-used eviction at **1 GiB**. A stage exceeding **64 MiB** of uncompressed numeric output bypasses caching to bound temporary memory. Oversized maps still bake normally.
- Launcher crash dumps are under `%LOCALAPPDATA%/DOOM-HellforgeLauncher/crashes/`. Settings backups exclude caches and dumps.

## What is reused

Each stage rebuilds live metadata and restores only its owned numeric channels: sunlight coverage; direct RGB and direction/group textures; static sprite-light samples; bounce RGB; packed sky/AO; and sector flow indices/weights. GPU textures are recreated and uploaded through the ordinary paths. Source lists, lamp groups, masks, pointer references, sunbeam metadata and static world geometry are reconstructed normally. Successful cache reads therefore skip ray tracing without pretending that an old scene is still valid.

Identity includes ordered loaded resource content, a SHA-256 fingerprint of renderer/shared helper/game-profile source code, format/layout lengths, relevant lighting options, sector heights/materials/light levels, side placement, texture translations, sky/mood inputs, static occluder coverage, source lights/areas/groups and broken source tiles. No actor pointers or GPU handles are stored. Static-grid serialization excludes structure padding.

Saved worlds with different static inputs get different entries. Moving doors/lifts and shot-out fixtures continue normal live relighting. This does not introduce new save-game persistence for cosmetic destruction: the existing engine resets shot-out light tiles when loading/restarting. The cache follows that reconstructed state.

Wetness is recomputed on upload. Players, monsters, corpses, particles, blood, live flicker and weather state are not cached. Caustic patterns, shoreline construction, map geometry and sunbeam reconstruction remain normal preparation work in this version. Cache hits improve load time; they do not make loading instantaneous.

## File safety

Format v1 has an identity/layout header, exact file and decoded lengths, a payload checksum and bounded run/literal decoding. All output validates in temporary storage before publication. Writes use a unique sibling temporary file, flush and close it, then atomically replace the destination through SDL. Corrupt, obsolete, truncated, unavailable or unwritable entries produce nonfatal messages and ordinary baking. No settings or save-game format changes are required.

## Reproducing verification

Use the original owned DOOM2.WAD; no Sunder tests are required or run for this fix:

```powershell
cmake --build build --config Release --target doom doom-reload-smoke --parallel 8
python duct-tape-doom-main/tests/reload_cache.py --iwad test-references/DOOM2.WAD --engine build/Release/doom-reload-smoke.exe --out build/reload-release-verification --cycles 10
ctest --test-dir build -C Release -E "^bigmap-" --output-on-failure
```

The native driver uses real death/Use and save/load paths, original MAP01, blood/puffs, rocket/plasma explosions and moving doors. Save tests hold a test-only background relight briefly so loading occurs with work still active. Only test builds provide that hook. Profiles, saves, images, logs, dumps and caches remain inside the chosen output folder. The Python runner checks cold/warm reuse across processes, output checksums, corruption recovery, settings invalidation, numeric storage contracts and an intentional diagnostic failure. Windows memory samples and loading-stage timings are retained alongside JSON results.

Screenshots are representative captures, not pixel-identical assertions: live effects can differ. Cached numeric stage checksums are compared directly with computed cold outputs. Other maps/WADs and full campaign completion are outside this DOOM2 MAP01 regression. Read-only/path failures and atomic preservation are tested; physical disk exhaustion and a power failure during replacement require separate fault-injection facilities.

## Final automated results

Verified using the original `test-references/DOOM2.WAD`, Release/Vulkan, 960?540. No Sunder or bigmap-WAD tests ran.

- **60 reload cycles passed:** ten restarts and ten save loads each with caching disabled, cold-cache initialization, and warm cross-process reuse. Each run submitted all eleven expected map frames.
- All thirty save-load cycles observed an active worker before requesting load; actual pending work was joined before level-owned game memory was released.
- **29/29 existing native tests**, rebuilt against the updated core, passed. **18 launcher persistence/backup checks** and **17 backup/UI checks** passed, including minimum-size layout and disabling cache clearing during launch.
- **22 cache storage checks** passed: strided channel preservation, settings/world identity, truncation/version/identity/checksum rejection, bounded decode, injected write failure before replacement preserving the previous entry, temporary cleanup, unavailable paths and LRU eviction.
- Corrupt direct-light entries were rejected and rebuilt; valid stages remained reusable. Fresh-process warm runs restored all six stage kinds with **66 hits and zero misses** per run. Restored numeric checksums matched cold-computed outputs.
- An intentional native exception logged its message and engine version, returned an abnormal exit code, and produced a Windows minidump. This failure is intentional and isolated to the diagnostic regression.

| Mode | Median map preparation | Peak private commit | Cached image count |
|---|---:|---:|---:|
| uncached-restart | 884 ms | 921.2 MiB | 91?97 |
| uncached-save | 853 ms | 917.7 MiB | 97?97 |
| cold-restart | 685 ms | 930.5 MiB | 88?97 |
| cold-save | 700 ms | 979.2 MiB | 94?97 |
| warm-restart | 697 ms | 928.3 MiB | 88?94 |
| warm-save | 696 ms | 972.3 MiB | 94?95 |

Warm restart preparation improved from 884 to 697 ms (about **21%**); save-load preparation improved from 853 to 696 ms (about **18%**). The first fully cold cache load took 1086 ms versus 901 ms without caching, reflecting cache population overhead. These are observed MAP01 timings, not guarantees for other maps or hardware. Startup shader preparation and the deliberately delayed test worker are outside these map-preparation timings.

Process memory was sampled every 100 ms. Image counts remained bounded rather than accumulating per reload. Private memory varied with GPU/CPU allocator reuse: warm-cache peaks were about 7?55 MiB above the corresponding uncached peaks; the largest cold-cache peak was about 62 MiB above the uncached save run. Cache buffers trade temporary memory for faster repeated loads. No unbounded per-reload growth was observed in these runs; dedicated GPU allocation bytes were not measured.

Evidence: `build/reload-release-verification/results.json`, `summary.json`, per-run `engine.log`, `memory.json`, `initial.png` and `reloaded.png`; existing-test log `build/reload-existing-tests-release.log`. Symbols and matching hashes: `dist/symbols/2026.11.12.1/` when packaged. Release hashes: `build/reload-release-manifest.json`.
