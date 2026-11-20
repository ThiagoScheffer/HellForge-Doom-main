# Corrosive Barrels v2 implementation report

The default-off Combat setting gives existing Doom barrels progressive leaks and acid spills. When enabled, acid follows the stock barrel death action and vanilla explosion. It uses the normal engine health and armor path with a separate corrosion identity. In single player, airborne rupture can coat nearby visible flying monsters. Contact near the floor can refresh one residual exposure even when multiple puddles overlap.

| Parameter | Value |
| --- | --- |
| Damage and interval | 5 HP every 17 tics before normal protection and armor |
| Residual exposure | 70 tics after last contact |
| Leak | 12–30 map units, 280-tic lifetime refreshed every 35 tics while actively leaking |
| Rupture | Central radius 48, up to three visible satellite pools of radius 25 |
| Spill expiry and cap | 350 tics, 48 total active pools |
| Protection | Radiation Suit clears exposure. Normal god mode and invulnerability apply. |
| Victims | Living, shootable players, Lost Souls, kill-count monsters and actors with native chase and attack states; excludes the hanging Keen switch actor |

Changes are in `p_acid.cpp` and `p_acid.h` for gameplay and snapshots, `p_inter.c` for the damage entry point, and `g_game.c` for trailer bounds. The pool renderer lives in `platform/scene3d/acid.cpp`. The scene call uses existing effect batches; the world shader and shared mode definitions no longer contain an acid-specific mode. Vulkan and Metal shader sources are reflected in the regenerated checked-in header. Launcher settings category generation and the generated catalogue are included. `tests/acid_smoke.inc` extends the real engine fixture.

AC01 remains version 1. Both pool and exposure records retain previous fields and timing units. The writer omits an inactive trailer and serializes exposures in actor order. The reader skips malformed, well-framed payloads, ignores invalid references, and reads nothing for an absent trailer. It does not attempt to reconstruct a missing footer or repair damaged vanilla save bytes.

The renderer reuses one small coverage texture and the existing depth-tested effect pass. Pool clipping runs only for active pools, uses subsector bounds to skip distant floor polygons, and applies to the owner's sector. Airborne droplets are bounded per rupture and fade in under a second. Source changes and validation are summarized in the [audit](CORROSIVE_BARRELS_AUDIT.md) and [test report](CORROSIVE_BARRELS_TEST_REPORT.md).
