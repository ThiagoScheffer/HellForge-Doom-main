# Corrosive Barrels — implementation notes

These notes describe the original v1 design. The [v2 implementation report](CORROSIVE_BARRELS_FINAL_REPORT.md) records the current damage, save and rendering paths.

## Runtime behavior

- A non-lethal hit on an `MT_BARREL` creates or expands a leak pool below the barrel.
- While the damaged barrel remains alive, the leak pool is refreshed every 35 tics and gradually expands to a maximum radius of 30 map units.
- A barrel rupture preserves the original `A_Explode()` and 128-unit `P_RadiusAttack()` behavior. The acid subsystem converts an existing leak pool to a central spill or creates one, then creates up to three additional nearby spill pools.
- The rupture also marks nearby visible players and kill-count monsters as splashed. This covers flying monsters that cannot touch a floor pool.
- Floor-pool contact exposes players and kill-count monsters in the same sector when they are at floor level.
- One exposure stream per actor prevents overlapping pools from stacking damage.
- Exposure deals 5 HP every 17 tics. It persists for 70 tics after the last contact.
- A player with `pw_ironfeet` (Radiation Suit) is immune.
- Acid damage is kept separate from elemental fire confirmation and severe bloodspill bookkeeping. Acid deaths do not receive fire-charred corpse treatment.

## Limits and lifetime

| Item | Value |
|---|---:|
| Damage per application | 5 HP |
| Damage interval | 17 tics |
| Residual exposure | 70 tics |
| Leak-pool lifetime | 280 tics, refreshed while barrel remains damaged |
| Rupture-pool lifetime | 350 tics |
| Maximum active pools | 48 |
| Initial leak radius | 12–30 map units, based on lost barrel health |
| Central rupture radius | 48 map units |
| Additional rupture pools | Up to 3, radius 25 map units |

The pool cap bounds per-actor collision work. Each actor checks at most 48 pools per tic; rupture visibility checks happen only when a barrel explodes.

## Save/load

- Acid state is serialized in a versioned `AC01` trailer.
- Pool records preserve coordinates, sector, radius, remaining lifetime, age, kind, barrel owner and source actor.
- Exposure records preserve actor, remaining exposure time, next damage time and source actor.
- Actor references use the same thinker-list ordering strategy as the existing weakpoint extension.
- The trailer is omitted when the setting is Off or no acid state exists. This avoids changing the save layout for classic/default play.
- New code accepts older saves without `AC01`.

## Renderer

- `platform/scene3d/acid.cpp` creates a procedural indexed/coverage texture and clips acid decals to the owning sector's floor polygons.
- The world shader adds a green, animated slick treatment and a small green illumination contribution.
- The acid mode reuses the sprite-only `MODE_GLOWING_EYES` bit for non-sprite decals. The shader explicitly identifies acid only on non-sprite geometry, so it does not alter glowing-eye sprites.
- Acid geometry is appended after map-light baking and before light-mask assignment.
- Renderer code does not depend on the blood-spill setting or blood particle cap.

## Known first-version limitations

- Only `MT_BARREL` is recognized; custom WAD barrel actor types are not automatically included.
- The original barrel artwork is unchanged.
- Rupture visuals are floor pools; no wall-splash decals or separate airborne particle streaks are implemented.
- No dedicated acid sound bank or sizzle loop is added.
- Acid decals require the accelerated 3D renderer; gameplay damage can remain enabled when the renderer is not accelerated.
