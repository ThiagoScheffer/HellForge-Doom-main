# Enhanced monster combat validation

Validated 2026-11-09 on Windows x64, Release build **2026.11.9.1**, engine fingerprint `c8542b505ada`.

## Results

- Engine, headless engine, launcher and test targets build successfully.
- **29/29 registered CTest tests pass**, including the new combat-policy suite.
- **149 policy/discharge/injury/persistence assertions pass** inside that suite.
- **3,252 actual-engine assertions pass in normal mode**, and the same **3,252 pass in solid-map mode**. The solid fixture disables terrain excavation to keep repeated samples comparable.
- Existing human coverage remains **121 detectable head views and seven safe fallbacks**.
- **960 live frame/rotation combinations** across thirteen new profiles are sampled, including fullbright attacks and Arch-vile heals. **191** have no reliable critical region and intentionally receive ordinary localized damage.
- The bounded GPU run checks damage and attached wounds for all thirteen profiles and saves 26 before/after captures.
- Actual native save/load preserves active injuries, cooldowns, projectile owner identity and in-flight provenance. A previous-format fixture without the extension loads with empty new state.
- Repeated projectile lifecycle tests return runtime record count to zero after 1,000 spawn/remove cycles; this is bounded ownership evidence, not a long-duration heap certification.

The actual-engine checks cover transparency/occlusion, mirrored artwork, per-pellet damage, close shotgun execution gates, immediate spawn collisions, moving plasma, direct rocket eye/leg localization, BFG direct/tracer eligibility, boss splash immunity, real movement/aim changes, normal death handling, switch persistence and replacement-art rejection.

## Stock anatomy coverage

Counts are frame/rotation combinations; shared artwork can occur in multiple rotations. Mechanical brain and leg counts overlap because one view may expose both.

| Monster | Views | Head | Face | Eye | Brain | Legs | Normal only |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Imp | 64 | 62 | 0 | 0 | 0 | 0 | 2 |
| Revenant | 96 | 90 | 0 | 0 | 0 | 0 | 6 |
| Arch-vile | 160 | 114 | 0 | 0 | 0 | 0 | 46 |
| Hell Knight | 64 | 64 | 0 | 0 | 0 | 0 | 0 |
| Baron of Hell | 64 | 64 | 0 | 0 | 0 | 0 | 0 |
| Mancubus | 80 | 44 | 0 | 0 | 0 | 0 | 36 |
| Cyberdemon | 56 | 56 | 0 | 0 | 0 | 0 | 0 |
| Pinky | 64 | 0 | 40 | 0 | 0 | 0 | 24 |
| Spectre | 64 | 0 | 40 | 0 | 0 | 0 | 24 |
| Cacodemon | 48 | 0 | 0 | 18 | 0 | 0 | 30 |
| Pain Elemental | 56 | 0 | 0 | 35 | 0 | 0 | 21 |
| Arachnotron | 72 | 0 | 0 | 0 | 70 | 45 | 2 |
| Spider Mastermind | 72 | 0 | 0 | 0 | 72 | 47 | 0 |

The region recipes deliberately exclude obscured Arch-vile spell heads, hidden floating eyes, unrecognized non-humanoid replacements and ambiguous mechanical areas. Exterior mechanical foot/leg coverage is conservative. These limitations do not disable location-independent weapon vulnerabilities on confirmed actor contacts.

## Bounded performance comparison

The cached query comparison uses the same unposed sprite artwork and geometry for the original human path versus expanded anatomy, with 100 warmups and 10,000 timed queries per path. It excludes image decoding and cache construction.

| Query path | Mean microseconds |
| --- | ---: |
| original_human | 0.175 |
| expanded_anatomy | 0.178 |

The GPU comparison uses a stationary 640x400 scene, four warmup frames and eight sampled frames per phase. It measures idle frame build/submission and GPU completion separately from the hit-query test.

| Rules | Mean CPU ms | Mean GPU ms |
| --- | ---: | ---: |
| original | 0.838 | 1.663 |
| enhanced | 0.795 | 1.528 |

No major regression was observed in these bounded fixtures. Differences between these small samples are not evidence of a general rendering speed improvement. Large-map combat, long sessions and GPU stress were not certified.

## Isolated damage and TTK comparison

These samples use one fixed valid damage roll, native stock health, guaranteed hits on the listed part and unchanged fire cadence. They exclude misses, AI, travel time, splash and shotgun discharge executions. Hit counts provide an ideal time-to-kill comparison at the same cadence; they are not measured real-level combat times.

| Monster | Attack/region | Fixed base roll | Previous damage/hits | New damage/hits |
| --- | --- | ---: | --- | --- |
| Imp | Bullet / head | 10 | 10 / 6 | 20 / 3 |
| Revenant | Bullet / head | 10 | 10 / 30 | 20 / 15 |
| Arch-vile | Bullet / head | 10 | 10 / 70 | 20 / 35 |
| Hell Knight | Bullet / head | 10 | 10 / 50 | 20 / 25 |
| Baron of Hell | Bullet / head | 10 | 10 / 100 | 20 / 50 |
| Mancubus | Plasma / body | 20 | 20 / 30 | 35 / 18 |
| Cyberdemon | Plasma / body | 20 | 20 / 200 | 30 / 134 |
| Pinky | Pellet / face | 10 | 10 / 15 | 15 / 10 |
| Spectre | Pellet / face | 10 | 10 / 15 | 15 / 10 |
| Cacodemon | Plasma / body | 20 | 20 / 20 | 40 / 10 |
| Pain Elemental | Plasma / body | 20 | 20 / 20 | 40 / 10 |
| Lost Soul | Plasma / body | 20 | 20 / 5 | 100 / 1 |
| Arachnotron | Plasma / body | 20 | 20 / 25 | 60 / 9 |
| Spider Mastermind | Plasma / body | 20 | 20 / 150 | 60 / 50 |

No approved balance default was changed. Recommended real-level playtesting: Cyberdemon/plasma durability, mechanical plasma dominance, Pinky execution frequency under realistic spread, and whether injury cooldowns remain readable in crowded fights.

## Artifacts and remaining limits

- [Anatomy overlays](../../build/critical-smoke/region-overlays.png)
- [GPU before/after contact sheet](../../build/critical-smoke/enhanced-captures.png)
- [Normal engine log](../../build/critical-smoke/integration.log), [solid-map engine log](../../build/critical-smoke-solid/integration.log)
- [Regression log](../../build/weakpoints-regression.log)
- CSV coverage, hit-query timing, frame timing and isolated damage/TTK data are in `build/critical-smoke/`.

The mechanics remain single-player and sprite-only. They do not implement voxel anatomy, LAN/demo gameplay changes, arbitrary replacement-art semantics or true 3D hitboxes. Projectile localization is a finite sprite/contact-volume reconstruction; uncertain contacts keep ordinary localized damage. Full playthrough balancing, physical LAN tests and long-duration memory/performance certification remain outside this evidence.
