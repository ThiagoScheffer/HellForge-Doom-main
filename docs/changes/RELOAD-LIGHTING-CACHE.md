# Reload stability and persistent lighting cache ? HELLFORGE 2026.11.12.1

```yaml
project: DOOM: HELLFORGE
build: 2026.11.14.1
updated_utc: 2026-11-13T19:16:57Z
status: active
evidence_build: 2026.11.12.1
tests: ../../build/reload-release-verification/results.json
index: ../README.md
```

## Smart Persistent Lighting Cache 2.0 — M1

The old loading screen entered its bake stages before looking up cache entries. Release/Vulkan Doom II MAP01 restored all six stages on an unchanged death restart while the display still said it was baking. Original Doom E1M1 reproduced a real miss on the same path. Both preparations calculated all six stages and wrote different keys with identical numerical checksums. The old key hashed the entire texture and flat translation tables, whose animated entries advance while playing, even when their changes cannot affect a bake. It also hashed unrelated lighting settings and the whole loaded resource directory for every stage. A broad source checksum caused unrelated code changes to invalidate all entries after a build.

The cache identity now serializes explicit, fixed-width fields in canonical order. It preserves floating-point bits, ray and emitter ordering, sequence sizes, and the format and field layout. It uses resolved materials, geometry and world state from the current preparation pass. Savegame world state is unarchived before the renderer initializes the new scene. Changed geometry receives its own key, and moving-sector relighting still runs before presentation. The renderer continues rebuilding lamps, sky data, sunshafts, wetness, geometry, and GPU resources.

| Stage ID | Numeric payload | Identity and dependencies |
| --- | --- | --- |
| `sun` | Floor sunlight coverage and wall alpha | Ray geometry, masks, static occluders, floor ownership, wall atlas, resolved sun direction. Sky color and strength are rebuilt independently. The coverage calculation does not consume baked emitters or sector brightness. |
| `lights_hf2` | Floor and ceiling RGB, wall RGB, wall and flat direction/group bytes | Ray and atlas inputs, ordered computed point and area emitters, window split, groups, effective model, and the traced stained-glass inputs including the palette and emission mask. The numerical loop consumes generated values instead of raw animation tables. |
| `grid_hf2` | CPU sprite sample occupancy, heights, color, directions, flicker group and shadow-source weights | Ray geometry and ordered point/area emitter inputs, group associations, effective model and grid layout. The grid computes point traces and does not read the surface texture atlases or stained-glass radiance. |
| `bounce` | Floor, ceiling, and wall bounce RGB | Ray geometry, atlas, resolved albedo, and enabled upstream numeric sun coverage and direct RGB. When used, includes the corresponding algorithm fingerprints, sun radiance and current sector light headroom. Validated upstream digests are shared only during the current preparation pass. |
| `ambient` | Packed sky visibility and ambient occlusion in flat and wall bounce-layer alpha | Ray geometry, masks, occluders, atlas and sky presence. Sector sky brightness is refreshed for gameplay. It is not an input to the packed openness and sky visibility calculation. |
| `flow` | Neighbor index in the seam map and weight in contact alpha | Ray geometry, floor sector ownership, resolved BSP partition/leaf fields and the window policy. Runtime sector brightness is applied when consuming those flow weights. |

The stages share geometry fingerprints only within a single preparation pass. CMake tracks explicit numerical source manifests per stage and a compiler, architecture and build-flags domain for native payloads. Shared implementation files can invalidate multiple dependent stages. The cache retains a conservative source fingerprint for stages without an audited manifest. The bake calculations run on the CPU, with a fresh upload through the active graphics backend.

**Storage.** Key schema 2 uses `<map>-m1/<stage>-<key>.bake`. The binary format remains v1. The 48-byte native-endian header contains six 64-bit words for magic, version, key, decoded length, compressed length, and FNV-1a payload checksum. It is followed by the existing bounded run/literal byte stream. After one cold preparation in the new namespace, compatible entries are available for restart, reload and fresh-process reuse. Old entries can age out of the existing 1 GiB budget. Entries above 64 MiB remain uncached.

The stable Windows launcher and default direct engine launch resolve `%LOCALAPPDATA%/DOOM-HellforgeLauncher/Cache/Maps`. Explicit `HELLFORGE_MAP_CACHE` takes precedence over `TUIN_MAP_CACHE`. Relative overrides are resolved once against the starting working directory. Other platforms retain the SDL per-user preference root. Experimental launcher roots continue to work through the explicit override.

**Diagnostics.** `[LIGHT CACHE]` stage records distinguish `HIT`, `MISS_NO_FILE`, disabled caching, oversized entries, invalid header/format/key/size/checksum, decode failure and I/O failure. They record expected and stored bytes, calculation dispatches, key component fingerprints and key, lookup, read, calculation, write and upload-staging timings. A separate map summary reports reuse, dispatch totals, write failures, eviction, geometry and moving-light reconciliation. Lookup and read times include operating-system filesystem effects. Staging times cover CPU-side texture creation and transfer preparation. Geometry and dynamic updates remain required on warm loads.

Use `-verifybakecache` to recalculate validated hits and compare their numeric channels byte for byte. It reports the first differing channel and element, uses the fresh result and attempts to repair a mismatched file. Normal production hits skip that verification work. The stage report distinguishes verification dispatches from ordinary bake dispatches.

### M1 engineering evidence

Release/Vulkan tests ran on Windows with an Intel Core Ultra 5 225H, 14 logical processors, Intel Arc 130T, 960×540 at 100% scale, and a bake worker limit of 16. The main fixture used the original DOOM2.WAD with all six stages enabled. E1M1 used the original DOOM.WAD with monsters disabled. Sunder used the local 2308 PWAD with monsters disabled. Each cold run used a separate directory under `build/m1-verify`. The operating-system file cache was left in its normal state and CPU/GPU load varied across runs. The baseline uses the preserved pre-change Release driver and a separate diagnostics-only build for phase timings. The native renderer was built before behavior changed.

| Scenario | Baseline min / median / max (ms) | M1 min / median / max (ms) | Median improvement (ms) | Improvement |
| --- | ---: | ---: | ---: | ---: |
| MAP01 independent cold process | 1145.0 / 1183.0 / 1389.0 (n=5) | 1016.4 / 1124.6 / 1161.9 (n=5) | +58.4 | +4.9% |
| MAP01 fresh process, populated disk cache | 778.0 / 810.0 / 849.0 (n=5) | 734.6 / 777.9 / 802.0 (n=5) | +32.1 | +4.0% |
| MAP01 warm death and restart | 691.0 / 704.5 / 732.0 (n=10) | 755.6 / 771.3 / 852.2 (n=10) | -66.8 | -9.5% |
| MAP01 warm same-level save reload | 681.0 / 697.5 / 754.0 (n=10) | 728.7 / 756.5 / 805.6 (n=10) | -59.0 | -8.5% |
| E1M1 death and restart | 866.0 / 866.0 / 866.0 (n=1) | 566.4 / 566.4 / 566.4 (n=1) | +299.6 | +34.6% |
| Sunder MAP01 cold | 39924.0 / 39924.0 / 39924.0 (n=1) | 39297.2 / 39297.2 / 39297.2 (n=1) | +626.8 | +1.6% |
| Sunder MAP01 fresh-process warm | 1042.0 / 1042.0 / 1042.0 (n=1) | 863.1 / 863.1 / 863.1 (n=1) | +178.9 | +17.2% |

Both builds skipped compatible static calculations on MAP01. The paced loading screen uses at most one standalone presentation per bake stage. The warm restart and save rows use the final pacing rerun. In E1M1 M1 removed six calculations and 13 ordinary bake dispatches on restart. The E1M1 and Sunder timings are single observed pairs. Sample counts are too small for a useful P95. Dedicated VRAM was unavailable. Preparation times exclude process initialization and gameplay rendering.

| MAP01 phase | Diagnostics baseline cold / warm (ms) | M1 cold / warm median (ms) |
| --- | ---: | ---: |
| Key generation | 25.54 / 0.68 | 3.53 / 3.90 |
| Entry lookup | 0.56 / 0.78 | 0.57 / 0.75 |
| Reading, decoding and restoration | 0.07 / 105.38 | 0.08 / 102.77 |
| Numeric bake | 303.48 / 0.00 | 274.68 / 0.00 |
| Packing, compression and write | 159.20 / 0.00 | 162.34 / 0.00 |
| GPU upload staging | 55.17 / 42.61 | 55.91 / 47.07 |

Peak private process commit in uncached-restart was 913.9 MiB.

Peak private process commit in warm-restart was 923.7 MiB.

Peak private process commit in warm-save was 898.3 MiB.

Peak private process commit in modified-world-save was 929.5 MiB.

The main MAP01 suite used the isolated cache root `build/m1-verify/final-map01/Cache/Maps`. An empty MAP01 cache calculated 6 of 6 stages with 13 dispatches. Warm restart and warm save each restored 66 of 66 stage instances across their initial load and ten reloads. Five stages staged GPU resources and the sprite grid stayed on the CPU. The six files total 13,074,439 bytes with headers. Individual sizes are 54,636 bytes for sun, 7,786,420 for direct, 282,769 for the sprite grid, 3,486,639 for bounce, 892,597 for ambient and 571,378 for flow. Normal warm runs recorded zero write failures. E1M1 used 10,080,547 bytes. Sunder MAP01 used 63,046,106 bytes and restored all six stages. Sunder MAP02 direct lighting needed 70,256,550 uncompressed bytes. Its warm load restored five stages (83.3%) and ran two direct-light dispatches.

### Regression coverage

- **CACHE-01–06, 08–10, 19, 25, 27–30, 33–35.** Verified cold and warm loads, death/Use restart, actual save reload, same-process MAP01→MAP02→MAP01, different-working-directory direct launches, disabled/re-enabled disk caching, active door relighting, the MaterialGallery PWAD, Enhanced/Hybrid/Physical numerical verification, repeated restarts and concurrent publication. The changed-world save restored the edited ceiling and light level before presentation. The first reload calculated all six stages for that state and the second restored all six.
- **CACHE-11–16.** The storage suite passed 36 checks, including invalid and truncated headers, truncated payloads, size and checksum mismatch, decoder failure, checked arithmetic, concurrent same-key writers and previous-entry preservation after an injected write failure. An isolated unwritable root allowed normal gameplay. The oversized bypass also ran on Sunder MAP02.
- **CACHE-18, 31–32.** The window-light change retained the independent sun and ambient entries and changed flow. The isolated CMake fixture kept all six fingerprints after a HUD-only change and invalidated direct and grid after changing the direct implementation. Bounce consumes the direct fingerprint when direct lighting is enabled. The normal Release build reconfigured after bake dependencies were edited.
- Reference comparisons matched all six numeric payloads for E1M1, MAP01 and MaterialGallery. An altered sunlight payload with a recomputed checksum failed the reference comparison at channel 0, element 0, byte 0. The engine replaced it, then restored all six stages in a fresh process.

- **CACHE-17.** Changing the available pixels of the same explicit LITE3 emission mask invalidated direct lighting while sun, ambient and flow were retained.
- **CACHE-21 and partial CACHE-23.** Warm-cache lamp destruction and live flicker passed the stock renderer fixtures.
- **CACHE-26.** Reversing two local palette PWAD overrides changed the resolved input. The second order restored and verified all six outputs.

The launcher directory handoff was inspected in source. A packaged launcher run, lift-specific fixture, destructible-wall save, persisted shot-out-lamp save, and dedicated atmosphere transitions remain unverified. Existing level initialization resets cosmetic fixture destruction on reload. A dedicated animated emissive texture transition remains unverified. Windows/Vulkan was exercised. Metal, other hardware, demos, multiplayer, explicit calculation cancellation and an injected production GPU allocation failure remain unverified.

### Changed files and build commands

| Workspace-relative path | Change |
| --- | --- |
| `duct-tape-doom-main/platform/scene3d/bake_cache.cpp` | BakeCache I/O, stage keys and session reports, root resolution, upstream digests, verification and 36 storage checks. |
| `duct-tape-doom-main/platform/scene3d/bake_cache.h` | CacheField, BakeCache and map-session APIs. |
| `duct-tape-doom-main/platform/scene3d/bake_cache_key.h` | BakeKeyHash canonical fields, float bit patterns and explicit sequence lengths. |
| `duct-tape-doom-main/platform/scene3d/bake_sun.cpp` | bakeSun stages restored coverage and counts actual calculation. |
| `duct-tape-doom-main/platform/scene3d/bake_lights.cpp` | bakeLights instruments direct RGB and directional resource setup. |
| `duct-tape-doom-main/platform/scene3d/bake_indirect.cpp` | bakeBounce, bakeAmbient, bakeFlow and bakeSpread use stage-specific inputs and report dispatches and uploads. |
| `duct-tape-doom-main/platform/scene3d/thing_light.cpp` | bakeStaticGrid reports occupied samples, restoration and calculation without GPU staging. |
| `duct-tape-doom-main/platform/scene3d/glass_light.cpp` | hashGlassBakeInputs captures traced geometry, palette and resolved emission data. |
| `duct-tape-doom-main/platform/scene3d/loading.cpp` | Loading stage names, phase updates, capped presentation cadence and session end. |
| `duct-tape-doom-main/platform/scene3d/internal.h` | Loading phase label declaration. |
| `duct-tape-doom-main/platform/scene3d/scene.cpp` | prepareFrame opens sessions and records geometry and live reconciliation. |
| `duct-tape-doom-main/CMakeLists.txt` | doom_bake_fingerprint manifests, conservative fallback and CPU ABI definitions. |
| `duct-tape-doom-main/platform/settings_panel.cpp` | Clarifies the existing F4 option. |
| `duct-tape-doom-main/tests/reload_cache.py` | Structured session parsing, result assertions, independent samples and reference verification. |
| `duct-tape-doom-main/tests/reload_smoke.c` | Native map revisit and modified-world save coverage. |
| `BUILDING.md` | Reproduction commands and explicit verification switch. |
| `README.md` | Current cache and lighting model behavior. |
| `Log-Status.md` | M1 investigation and verification summary. |
| `docs/changes/RELOAD-LIGHTING-CACHE.md` | Dependency audit, timing evidence and storage contract. |

```powershell
cmake --build build --config Release --target doom doom-reload-smoke --parallel 8
ctest --test-dir build -C Release --output-on-failure
python duct-tape-doom-main/tests/reload_cache.py --engine build/Release/doom-reload-smoke.exe --iwad test-references/DOOM2.WAD --out build/m1-verify/final-map01 --cycles 10 --samples 5
```

The Release engine and smoke driver built. All 48 CTests passed after the portability test received permission to write its scratch configuration. Logs, timing samples, captures and intentional crash dumps are in `build/m1-verify`. The interrupted runner reused completed results through `build/m1-verify/resume-reload.py`. The final warm benchmarks and direct render check used the paced loading screen. The final direct render check reused all six stage entries. Shader binaries were untouched.

M1 supplies the diagnostics, compatibility checks and oversized-stage evidence needed for M2. Chunked storage, bounded per-chunk decode and reuse of large stages remain M2 work.

## Reload failure

The original DOOM2 MAP01 reproducer spawned blood, puffs, rocket and plasma explosions, killed the player and restarted through Use. Before this fix, baking completed, then `drawScene` threw `invalid unordered_map<K, T> key`. The previous release cleared images at map transitions while retaining empty decal/effect batch keys. Drawing those empty batches still looked up their reclaimed images. A diagnostic-only terminate handler recovered that exception and stack.

A shared `R_BeforeLevelReset` barrier joins/discards relighting at the start of game level setup, before zone allocations or sector data are freed. Renderer level transitions also join/discard outstanding relighting, clear image-dependent geometry and reference caches, then release images. Empty batches do not resolve images. A nonempty batch with a missing image raises a descriptive error, including batch category, image key and level serial. Wound classifications, glass images and bake masks are invalidated before image reclamation. Full renderer shutdown uses the same geometry cleanup.

Bake workers propagate exceptions after joining. Background relight errors propagate to the render thread; a discarded level job cannot publish into the next level. Native diagnostics log uncaught C++ exceptions, stack RVAs, engine version, loading stages and the first submitted frame. Windows crash dumps use `dbghelp.dll` from System32 when available. The launcher flushes output continuously and records decimal/hexadecimal exit codes. Release `doom.pdb` must remain paired with the exact engine SHA-256.

## Previous release controls and storage

The remaining sections record the earlier reload/cache release. M1 supersedes its key, directory and timing details.

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
