# Corrosive Barrels v1 — test report

This is the historical report for the partial source snapshot. See the [v2 Windows test report](CORROSIVE_BARRELS_TEST_REPORT.md) for current results.

## Automated/static checks

**Passed**
- 14 source-integration assertions: barrel damage hook, rupture hook ordering, thinker update, actor cleanup, map reset, save/load ordering, conditional save trailer, default-Off setting, F4 option, renderer placement, CMake source registration, shader mode isolation, acid/fire confirmation separation and weakpoint-module decoupling.
- `git apply --check CORROSIVE_BARRELS.patch` against the uploaded source snapshot.
- Applied the patch to a clean copy of the uploaded snapshot; every modified/new file matched the prepared source overlay byte-for-byte.
- No trailing whitespace on added patch lines.

## Build check

**Not completed.**

CMake configuration was attempted with the renderer and SDL app disabled to reduce dependencies. Configuration failed before compilation because the uploaded source subset does not contain required project files, including:

- `linuxdoom-1.10/m_fixed.c`
- `linuxdoom-1.10/am_map.c`
- `third_party/miniz/miniz.c`
- `platform/mod_archive.cpp`
- `heretic/am_map.c`
- `platform/i_headless.c`
- `platform/headless_main.c`

Therefore no C++ compilation, shader compilation, Windows build or runtime gameplay test is claimed.

## Windows gameplay test checklist

1. Start with **Corrosive Barrels Off**. Confirm the original barrel explosion and 128-unit radius damage remain unchanged.
2. Enable **F4 → Combat → Corrosive Barrels** and restart/reload the level if required by your build workflow.
3. Damage a barrel without destroying it. Confirm a small green pool and animated droplets appear beneath it.
4. Wait while the damaged barrel remains intact. Confirm the leak persists and gradually spreads.
5. Destroy the barrel. Confirm the original explosion remains and green pools spread around the barrel.
6. Test a ground monster and a flying monster near the rupture. Confirm both can receive acid exposure when visible to the barrel.
7. Stand in a pool and verify 5 HP damage every 17 tics. Leave the pool and verify residual exposure lasts approximately 2 seconds.
8. Equip a Radiation Suit and verify it prevents acid damage.
9. Test overlapping pools; damage should not stack into multiple simultaneous streams.
10. Save and reload while a barrel is leaking, while pools are active, and while an actor has residual exposure.
11. Confirm the feature is inactive in multiplayer and demo playback/recording.
12. Toggle the feature Off and verify existing barrels return to classic behavior.

## Result

The patch applies cleanly to the supplied source snapshot. Runtime/build acceptance remains pending a complete Windows x64 build and in-game testing.
