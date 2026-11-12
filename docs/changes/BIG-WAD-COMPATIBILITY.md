# Large WAD compatibility — 2026.11.10.2

```yaml
project: DOOM: HELLFORGE
build: 2026.11.12.1
updated_utc: 2026-11-11T17:52:02Z
status: active
tests: ../../build
index: ../README.md
```

This release removes loading limits encountered in the unmodified
`test-references/Sunder 2512.wad`. The fixes are selected by file formats and map
features, not by the WAD's filename. Monster-performance changes are deferred.

## Loading and memory

- Texture column lump numbers are signed 32-bit values; byte offsets are
  unsigned 32-bit values. Composite size arithmetic remains checked against
  the zone allocator's real capacity. Allocation and local pointer types match.
- Tall patches, negative patch origins, uncovered columns, non-power-of-two
  widths and more than 255 overlapping patches are handled. Software walls use
  the actual texture height. Masked composite walls have separate purgeable post
  streams, preserving transparent gaps and tall-post encoding.
- PNAMES, texture directories, patch indices and patch streams are validated
  before use. Malformed data receives an error instead of a wrapped offset.
- Classic Doom and uncompressed ZDBSP `XNOD` nodes are supported. Runtime BSP
  child references are 32-bit. Added vertices, packed eleven-byte segs,
  subsector ranges and child references are checked; cyclic BSP graphs are
  rejected. Compressed ZNOD and other extended node formats remain unsupported.
- Oversized or invalid blockmaps are rebuilt with checked allocation sizes.
  REJECT allocation uses wide arithmetic and short lumps are padded with zeros.
- The native 64-bit engine defaults to a **512 MiB zone**. Headless tools retain
  their 128 MiB default. `-heapsize 1024` requests a 1 GiB zone; valid arguments
  are whole MiB values from **16 through 2047**. This is an engine-zone budget,
  not a cap on total process or GPU memory. Live zone pointers are never moved.
- Allocation failures report capacity, used/free/cache bytes and the largest
  free block. The headless map check also reports zone accounting.
- CPU/GPU image caches are released at level transitions, alongside rebuilding
  level-derived lighting and geometry. Current-level artwork uploads lazily;
  software composite caches remain purgeable by the zone allocator.

## Map compatibility

The compatibility layer implements the action families observed in the supplied
Sunder file: generalized floors, non-transfer ceilings, doors, locked doors,
lifts and stairs; the specific extended floor/change/elevator actions; silent
thing/line teleports; generalized sector damage; scrollers and conveyors; fake
floor/ceiling and lighting transfers; translucent middle walls and sky textures.
Floor and ceiling movers have independent runtime ownership. Stair chains remain
locked until the complete chain finishes, including across saves.

The layer also reads packed `ANIMATED` and `SWITCHES` tables, classic MAPINFO
metadata (title, music, sky, next and secret-next), and bounded expanded DeHackEd
tables: 4096 states, 1024 actor definitions and the conventional extra sprite
names through SP99. This does **not** implement MBF/MBF21 actions or ZDoom scripts.
Sprite blocks using either S_START or SS_START merge in load order, preserving
later frame/rotation replacements and the dimensions of their actual artwork.

New internal interfaces are grouped in `p_boom.h`. `R_GetColumn` remains the
dense wall sampler; `R_GetMaskedColumn` supplies transparent post streams.
Existing native actor and mover save record layouts remain unchanged. Elevator
movers have an explicit archive class. A length-checked `BM01` save footer stores
fractional scrolling, displacement/acceleration state and stair locks. Footer
version 2 is written; version 1 and saves without this footer remain readable.
Software visplanes now carry texture offsets and a sky-transfer identifier;
the internal `R_FindPlane` callers and plane splitting preserve those fields.

## Verification

The test scripts use owned IWAD data and isolated output/profile directories.
No installed user profile or original WAD is modified.

Release validation: **31/31 CTest tests**, including **16 generated process
regressions and 407 Boom action assertions**; **23/23 Sunder maps** passed their
loading checks. MAP03 and MAP21 passed separate-process save/load. The existing
combat/wound integration retained **4,697 assertions in each collision mode**.
Settings validation retained **23 game-process runs and 18 launcher checks**.
The separate launcher ZIP-backup/UI regression suite also passed; its test
profile is isolated and its control lookup follows the current nested layout.

Full logs are `build/bigmap-final-build.log`, `build/bigmap-final-ctest.log`,
`build/bigmap-ctest-wad/results.json`, `build/bigmap-combat-ordinary/integration.log`,
`build/bigmap-combat-solid/integration.log` and
`build/bigmap-settings-persistence.log`.

```powershell
python duct-tape-doom-main/tests/bigmap_regressions.py --engine build/Release/doom-headless.exe --iwad D:/Tuin-Doom-Enhanced/DOOM2.WAD --out build/bigmap-regressions
python duct-tape-doom-main/tests/bigmap_smoke.py --engine build/Release/doom-headless.exe --iwad D:/Tuin-Doom-Enhanced/DOOM2.WAD --wad "test-references/Sunder 2512.wad" --save-load --out build/sunder-allmaps
```

To register these with CTest, configure `DOOM_COMPAT_TEST_IWAD` and optionally
`DOOM_COMPAT_TEST_WAD`. Generated tests exercise actual engine parsing, drawing,
movement and save code, including large offsets/lump numbers, masked roundtrips,
malformed textures, truncated/cyclic/out-of-range XNOD data, heap argument
validation, custom sprite namespaces, translucency and transferred skies. The
action fixture checks movement, trigger restrictions, keys, stair lockout,
teleport position/orientation/momentum, independent planes and save restoration.

The real-WAD check loads all 23 supplied maps, validates every spawned actor's
sprite, compares 216,888 composite columns with the independent indexed
compositor, draws a classic frame and simulates 35 tics per map. It covers
110,287 offsets above 64 KiB. Separate processes save/reload MAP03 and MAP21.
This is loading/smoke coverage, not a completed campaign playthrough.

Vulkan captures and reports are in `build/sunder-gpu/`. At selected starting
positions, with the isolated reduced-effects preset and a 960×540 window at
50% render scale, the initial measurements were:

| Map | Worst sampled frame cost | Load/preparation |
| --- | ---: | ---: |
| MAP01 | 3.3 ms | 1.3 s |
| MAP03 | 5.5 ms | 1.6 s |
| MAP15 | 53.1 ms | 5.5 s |
| MAP21 | 73.7 ms | 25.3 s |

Targeted GPU fixtures confirmed a visible object behind a translucent wall and
the transferred sky texture. A single GPU process also completed the two-level
fixture transition successfully, exercising image-cache teardown and rebuilding.

These are synchronized profiler samples, not a sustained gameplay FPS guarantee.
A second MAP21 view measured 92 ms (25.7 s preparation). The largest scenes still exceed a 16.7 ms rendering budget; whole-map geometry,
light masks and sprite lighting remain expensive. Monster AI and rendering
performance work is intentionally separate.

## Coverage limits

- Support is a tested compatibility subset, not complete Boom, MBF21 or ZDoom
  certification. Unsupported ordinary extended actions are reported in the log;
  generalized ceiling texture transfers produce an explicit unsupported error.
- Per-sector transferred sky **textures** render in both backends. The GPU sky
  currently uses its standard cylinder orientation: per-line sky angle/vertical
  offsets and mirroring are not reproduced there. Classic rendering applies
  those offsets and mirroring. Environmental lighting still follows the map sky.
- Translucent GPU walls use 50% blending, without depth writes, sorted by wall
  distance. Custom palette TRANMAP tables are used by the classic renderer;
  their exact palette mapping is not reproduced by GPU blending.
- Conveyor membership currently follows the actor's sector link, rather than
  Boom's list of every sector touched by its radius. Boundary-straddling actors
  can therefore differ from a reference port.
- MAPINFO script-like behavior and arbitrary new action functions are not
  interpreted. Debug movement controls are not constrained by MAPINFO directives.
- Existing network/demo protocols are unchanged. The smoke results do not certify
  Boom demo synchronization or multiplayer playback of these maps.
- GPU texture dimensions remain bounded at 8192×8192; the zone and fixed-point
  world retain explicit, checked limits. More RAM cannot implement an unsupported
  format or remove simulation/rendering costs.

Reference semantics were checked against the primary GPL Boom/PrBoom+ sources:
[generalized actions](https://github.com/coelckers/prboom-plus/blob/master/prboom2/src/p_genlin.c),
[map effects](https://github.com/coelckers/prboom-plus/blob/master/prboom2/src/p_spec.c),
[teleports](https://github.com/coelckers/prboom-plus/blob/master/prboom2/src/p_telept.c).
The new source remains under this project's GPL-2.0-or-later license.

## Packaged build

`dist/Tuin-Doom-Enhanced-big-wads.exe` reports version **2026.11.10.2**.
Its embedded engine was compared byte-for-byte by SHA-256 with the tested
`build/Release/doom.exe`:
`1224ae53591fc4babbcb528e8392e4bae886508fc7abe6463ce1c91a701b6dcb`.
The launcher contains the engine and existing bundled resources, not Sunder or
the commercial test IWAD. A machine-readable manifest is in
`build/bigmap-release-manifest.json`.
