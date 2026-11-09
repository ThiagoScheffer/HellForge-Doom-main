# Monster feedback/status verification

Validated 9 November 2026 on Windows, Release x64. Launcher version **2026.11.9.2** embeds native engine SHA-256 `321e9d9b96bb1adc53724dc46eb0e1886ea33522018cda75faba97ea03d55f9f`.

## Results

| Check | Result |
|---|---|
| Production SDL engine, headless engine, test targets | Built successfully |
| Registered CTest suite | **29/29 passed** |
| Actual combat module policy/status/save checks | **273 passed** (previous baseline: 149) |
| Native engine integration, ordinary map collision | **3,307 passed** |
| Native engine integration, solid-map collision | **3,307 passed** |
| GPU feedback fixture | Four representative captures saved and inspected |

The integration suites retain the original human coverage (121 detectable head views and seven safe fallbacks), expanded stock-anatomy coverage and existing wound/collision/damage rules. The additions contribute 124 policy checks and 55 native engine checks over the earlier baseline. All requested numerical defaults are unchanged.

New automated scenarios cover four bleeding species, six exact health ticks, ordinary/guaranteed triggers, 30% RNG boundary, non-ballistic/melee exclusions, refresh without stacking, independent switches, speed scaling, lethal cleanup and attribution. Frenzy checks cover the three species, 40% boundary, one roll per discharge, 128-unit range, standard-shotgun exclusion, confidence/visibility/search gates, same-species targeting, authoritative targeting, expiry, surviving projectiles and actual native twenty-pellet super-shotgun firing.

Native projectile checks exercise actual Mancubus same-species and Knight/Baron cross-family contacts. The native save/load fixture restores active bleeding, frenzy victim references, marked projectiles and original player kill attribution. Version-1 60-byte extension records, version-2 84-byte records and older saves without a trailer are covered. Feature cancellation, demo/network/voxel exclusions, no extra RNG while disabled, and duplicate notification/application prevention are covered.

Feedback checks verify all four selector modes, crosshair-off HS text, lethal headshots, normal damage while cosmetic sparks are enabled, independence from door sparks, and direct projectile contact localization. The HUD overlay uses vertex tint through the existing flat-color shader, preserving the default crosshair color and making HS amber.

## Captures and logs

- [Amber HS with crosshair off](../../build/feedback-visual/headshot-feedback.png)
- [Arachnotron metal-leg impact](../../build/feedback-visual/metal-leg-sparks.png)
- [Moving Imp bleeding trail](../../build/feedback-visual/bleeding-trail.png)
- [Knight attacking a Baron during frenzy](../../build/feedback-visual/frenzy-infighting.png)
- [GPU fixture log](../../build/feedback-visual/gpu.log)
- [Ordinary collision integration log](../../build/critical-smoke/feedback-integration.log)
- [Solid-map integration log](../../build/critical-smoke-solid/feedback-integration.log)
- [CTest log](../../build/feedback-regression.log)

These are bounded real-engine fixtures with owned stock Doom II artwork. The frenzy capture deliberately invokes an actual monster projectile attack after the confirmed close head pellet; target selection, damage and status eligibility remain in gameplay code. The ordinary integration suite additionally fires the native super-shotgun action. All fixture processes exited normally; no browser or Node tests were used.

## Performance and limitations

The existing warmed collision microbenchmark ran 10,000 same-art queries per path. Ordinary-map mean query times were 0.172600 ?s for the original human path and 0.177050 ?s for expanded anatomy. Solid-map results were 0.255620 ?s and 0.184380 ?s respectively. These isolated query timings are not an overall frame-time or crowded-fight certification.

Sparks intentionally retain the small, brief door-track particle style and can be subtle in still screenshots. Unknown or ambiguous anatomy/contact localization receives no localized cosmetic effect. Moving blood trails respect blood/spray visibility and particle budgets. No multiplayer/demo/voxel support, extended live-map balancing, long-duration heap certification or full-map performance claim is made. The existing MSVC conversion warnings and launcher LanMenu CS4014 warning remain; builds completed successfully.

## Reproduction

Build targets: `doom`, `doom-headless`, `doom-critical-smoke`, `doom-wounds-tests`, `doom-weakpoints-tests` (Release). Run `ctest --test-dir build -C Release --output-on-failure` and `build/Release/doom-weakpoints-tests.exe`.

Run the native harness from an isolated directory:

```text
doom-critical-smoke <DOOM2.WAD> <critical-arena.wad> <absolute-output-directory>
doom-critical-smoke <DOOM2.WAD> <critical-arena.wad> <absolute-output-directory> --solid
doom-critical-smoke <DOOM2.WAD> <critical-arena.wad> <absolute-output-directory> --visual-feedback
```

Use separate working/output directories for the ordinary, solid and visual fixtures to keep graphics/config/save files isolated. The visual fixture requires an accelerated GPU; the other integration modes use SDL dummy video/audio.
