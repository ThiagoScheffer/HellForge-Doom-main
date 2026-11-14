# Large moving-platform destruction protection

```yaml
project: DOOM: HELLFORGE
build: 2026.11.15.5
updated_utc: 2026-11-14T15:45:51Z
status: implemented-test-build; manual-lag-confirmation-pending
tests: ../../map_source_tests.cpp
save_tests: ../../map_world_tests.cpp
native_tests: ../../duct-tape-doom-main/tests/e1m8_regression.inc
index: ../README.md
```

## Behavior

The user confirmed that disabling breakable surfaces eliminates the E1M8 lowering lag. This build adds targeted damage protection beside the existing keyed-door protection path. **Breakable surfaces can stay enabled.** Large floor movers cannot acquire excavation cuts on their tops or exposed moving sides. Small platforms, ordinary walls/floors and ceilings keep their existing destruction behavior. This is a geometry damage guard; it adds no key requirement, actor armor, shield timer or additional visual force-field effect.

A large mover has an XY **bounding-box area of at least 1,048,576 square map units** (1024×1024 equivalent) and a recognized floor action. The box estimates the workload of sector invalidation, not the exact polygon area. Long or disconnected sectors may qualify because their broad bounds are costly. A 512×512 platform stays destructible. The threshold is a documented initial code constant in `platform/large_platform_shield.h`; no extra F4 setting was added.

Classification runs when constructing the map's solid source, before firing or activation. It recognizes implemented classic floor/lift/stair/donut actions, supported Boom floor/elevator actions and generalized floor/lift/stair triggers, plus the existing E1M8/E4M8 and MAP07 boss-floor tags. Large already-active floor movers also qualify. Static tagged sectors used only for lights, transfers, doors or teleports do not qualify simply because they have a tag. Original LINEDEFS specials are used where available, so a consumed one-shot switch remains identifiable after loading a save.

The guard rejects a protected floor's own cut before allocating/remeshing tiles. Per-sample ownership checks additionally prevent neighboring blast brushes from excavating its solid body while preserving samples outside its polygon. Detection is based on original sector geometry, independent of camera position. Normal floor movement, collisions, trigger timing, player/monster damage and RNG rules are retained.

## Saves and compatibility

No actor layout or save-journal format changes. When loading existing SMW journals, validated cuts owned by a newly protected floor are dropped; other cuts are retained. Thus old excavation on protected platforms repairs when loaded. Save versions SMW1/2/3 retain their existing parsing boundaries. The focused save test uses a pre-protection SMW3 journal containing both protected-platform and ordinary-floor damage.

This policy operates in the launcher's single-player **solid-map destruction backend**. Demo/network exclusions and existing keyed-door settings are unchanged. It is not a general solution to remeshing performance for small movers, nor a claim that every custom mover is recognized. Untagged sectors reached indirectly by stair/donut propagation may lack pre-activation classification. Other WADs and actors saved inside deep repaired craters were not manually tested in this focused revision.

## Focused evidence

- Added test first failed: **Large platform top must reject damage**.
- Updated `map-source-tests` passes: footprint threshold, classic/Boom categories, protected top/sides, static-blast overlap, moving floor height, ordinary neighbors, ceilings and unprotected small geometry.
- Updated `map-world-tests` passes: rejected impacts allocate no cut, existing save repair drops only protected-floor cuts, ordinary excavation survives and the repaired floor moves normally. Existing geometry/journal checks in that target also pass.
- Native **original DOOM.WAD E1M8 only**, using an isolated copy of the supplied live-capture graphics config: sector 30/tag 666 is protected before activation; the small lift and ordinary courtyard are not protected. Both original Barons die through the scripted shotgun setup and all **345 lowering samples** complete: **8.298 ms median / 9.054 ms p95**. Baseline approach samples: 7.767 ms median.
- E1M8 exit regression still completes in **179 tics**, including grounded-contact negative controls.
- Release engine/launcher build successfully; **19 package checks pass**, including embedded-engine hash/version and normal defaults. Existing LanMenu CS4014 warning remains.

The scripted scene was already smooth before this patch. These results verify protection and successful movement, **not a measured 5-FPS-to-120-FPS improvement**. The user's manual replay with breakable surfaces enabled is the remaining confirmation. No broad WAD, combat or network suite was run.

Evidence is in `build/e1m8-fixes/` (`platform-red.log`, `map-source-tests.log`, `map-world-tests.log`, `floor-platform-shield/`, `exit-platform-shield/`, `platform-release-checks.log`). The original live trace remains `test-references/20261114-151315-823-UTC.zip`; it is not packaged. See [the original capture analysis](E1M8-AND-SPEEDRUN-FIXES.md).

Deliverables: `dist/DOOM-Hellforge.exe`, `dist/DOOM-Hellforge-2026.11.15.5.zip`, matching engine/PDB, documentation and selected evidence. User WADs/configs/saves are excluded from the package.
