# CODEX TASK — HellForge Gore M2.1: Critical Wounds + Visceral Impact Rework

You are editing an **existing working HellForge Doom C/C++ project**, with Gore M1 approved in live playtesting and Gore M2 implemented but requiring visual changes. **Inspect the current working tree first.** Do not assume the old M1 code or the attached M2 design prompt matches your latest source; preserve the current shot-finalization and test infrastructure. Implement directly in the current workspace; no need to create a separate gore engine.

**Mandatory reference:** read `HELLFORGE_GORE_M2_1_REWORK_REVIEW.md` in the same folder. Its monster table and acceptance criteria are the authoritative scope. The document is a specification, not a claim that the latest M2 implementation was inspected by the author. Keep the source change focused and report deviations explicitly.

## Deliverable — only the seven approved monsters

1. **Zombieman (`MT_POSSESSED`) and Shotgun Guy (`MT_SHOTGUY`)**: remove M2 fatal chest/belly cavity outcomes; preserve approved M1 fatal head burst; all other qualifying close lethal shotgun/SSG hits create enhanced directional red flesh/blood/visceral splatter that interacts with walls/floors. No new arms/legs, no nonlethal gore outside the normal blood/wounds system. A stock non-head corpse retains its original shape.
2. **Imp (`MT_TROOP`)**: severe close shotgun/SSG hit that **leaves the Imp alive at low HP** may add ONE bounded chest or belly cavity and expel a few tissue fragments. The wound exists **only in living frames**; on death, clear this M2 cavity and allow normal death art. Preserve eligible verified M2 fatal Imp head burst; otherwise present fatal viscera without torso cut.
3. **Revenant (`MT_UNDEAD`)**: pale bone shards on substantial confirmed rib/skull impacts. For a lethal concentrated skull hit, use a verified skull-removal mask **only if the full death animation is validated**; otherwise never cut and emit pale bone pieces including a heavier fragment that can fall/land. Do not introduce rib holes or red organ clusters for bones.
4. **Mancubus (`MT_FATSO`)**: substantial, anatomically bounded chest **or** belly cavity on severe nonlethal hit while low HP, continued wound bleeding using existing budgeted mechanisms; heavy organic pieces on impact. On death, clear this cavity before drawing death frames and emit heavy viscera; keep ordinary death art.
5. **Pinky (`MT_SERGEANT`)**: once-only low-HP nonlethal torso visceral ejection, stronger visceral accent on severe fatal close shot. **Never mask/destroy its sprite silhouette.** Exclude `MT_SHADOW` Spectres.
6. **Cacodemon (`MT_HEAD`)**: prioritize fixing invisible/rare M2 gore. Add a large **blue** lobed core and satellites/trail on nonlethal near-death severe shotgun/SSG hit. Also a stronger severe fatal variant. No cosmetic hole and no death-art modifications. Fix both **event admission** and **effect visibility**; do not merely raise particle counts. Use existing blue tint consistently on geometry, wall marks, and floor marks.
7. Other monsters remain unchanged; limb dismemberment continues deferred (do not display a working limb option unless it actually functions).

## Working-tree inspection — before editing

Read and trace these **current** files (names may have changed in M2):
- `platform/scene3d/{gore_policy.h,gore_stock.h,wounds.cpp,spray.cpp,blood.cpp,internal.h,scene.cpp}`
- `linuxdoom-1.10/{p_map.c,p_pspr.c,p_enemy.c,p_mobj.c,r_main.c,r_state.h}`
- `platform/{scene3d.h,settings_panel.cpp,settings_layout.h}`
- `tests/{wounds.cpp,settings_navigation.cpp}` if present.

Locate: `P_WeakpointEndShot()` and shot finalization; the accumulation of actual HP lost; opaque sprite contacts; `goreCapture`/`goreAfterHit` or their M2 successors; `goreBurst`/M2 fragment emitter; `BloodSourceScope`, `monsterBloodTint`, `bloodWallSplat`, `bloodFloorSplat`, `firstSolidWall`, `throwDrop` (or successors); `woundMask` and live/death sprite switch; existing per-actor state lifecycle; the F4 switches. **Do not invent APIs when an existing hook is available.**

## Implementation invariants

- Lethal **head/limb dismemberment** stays lethal-only. Live chest/belly wounds are an explicit *injury presentation*, not dismemberment.
- On each shot, calculate actual applied damage per pellet (including lethal overkill accounting as in M2) and finalize once for each victim/discharge. `preHP`, `postHP`, `maxHP`, damage, range, and confirmed opaque impacts are not interchangeable. One major outcome per event. No duplicate particle events for the same shot.
- Low-health injuries are driven by **post-hit health ratio plus severity**, and fire only if `postHP > 0`. Mark each living actor wounded/viscera-released only after the event qualifies. Prevent every-pellet/tic retriggering. Keep one allowed single fatal accent even if low-HP predeath viscera already happened.
- Initial tunable rules: Imp <=40% HP and >=15 actual damage with chest/belly contact; Mancubus <=45% and >=35 with >=2 opaque region contacts; Pinky <=45% and >=20 with torso contact; Caco <=50% and >=20 with >=2 opaque contacts **or** >=30 with >=1 opaque contact. All within <=128 Doom units for prototype. These are **test defaults**, not fixed truths. Skull bone/shard reaction >=30 observed damage as a starting point; require visual-region evidence for an anatomical cut.
- Lethal Caco must **not** retain the old cumulative `4 opaque && >=70 damage && <=96 range` gate. Set a separate reachable qualifying close-range severe-shot threshold, verify with debug counters and repeated game tests. Do not make all damage events trigger gigantic particles.
- Only enable a persistent live mask when a validated, stable living sprite frame/rotation supports it. **Do not carry Imp/Mancubus torso masks into their death or corpse states**. Suppress M2 body overlay when the sprite changes to death; normal wound effects should remain consistent with their independent setting. Do not mutate WAD source art.
- Stock-art fingerprint checks still required for **any real sprite cut**. If validation fails, fall back to fragments/blood; fragments alone can be allowed for supported actor types without stock corpse cuts, as permitted by existing M2 policy.
- Preserve effects settings, limits, multiplayer/demo/voxel restrictions, `totalkills < 200`, and cosmetic RNG. No gameplay changes, no additional game actors, no new shader or GZDoom script engine. Reuse current renderer and budgets.

## Rendering — build a stronger but bounded effect

Prefer enhancing the current `spray.cpp` emitter to spawning a new particle system. Define a **small set of named styles**: `HumanFlesh`, `ImpTissue`, `RevenantBone`, `MancubusHeavy`, `PinkyViscera`, `CacodemonBlueViscera`. Styles control size/lobes, velocity/spread, gravity, impact mark scale, and tint. Keep a **single shared** ballistic and surface-impact path.

Recommended first-pass fragment counts per event (clamp to available headroom, global 64 living gore fragments / 768 total drops): human 3–6, Imp 4–6, Revenant 3–6, Mancubus 5–8, Pinky 4–7, Caco 6–10. Do not increase count for every ordinary blood event; only qualifying M2 events.

**Cacodemon visibility requirement:** ordinary tiny red-like specks are insufficient. Render a recognizable compact blue clump with 2–3 connected lobes and a trailing series of smaller blue dots/pieces **while traveling**, not merely a single frame burst at spawn. Large clusters can be 2–4× the apparent area of M1 flesh, but tune against actual pixel size, camera distance and frame rate. Heavy pieces follow the projectile vector initially, fall to the level, and leave **blue** floor stains or wall splashes at valid collision sites. Use HellForge's existing `MT_HEAD` blue tint; never hardcode red decals for blue blood.

On impacts, place splat/smear using **existing surface validation** (solid wall span, no sky/invalid texture, correct floor/ceiling, valid level serial). Nearby walls get larger directional splashes and optional downward drip using existing decal mechanism. Open rooms still get floor landing marks; do **not** create phantom wall marks merely to satisfy visual effect. Fragments should have independent sizes/flight times, not all travel as a flat cone.

**Droplets 1.3.1 reference** (optional artistic/behavioral inspiration only): gib tumble/bounce/landing from `Gibs.txt`; spray/landing from `Droplets.txt`; drip/run from `Wall.txt`; growing floor marks from `FloorSplat.txt`. These are **GZDoom ZScript**, not compilable HellForge code. Do not load or paste its `ZScript`, `DECORATE`, `DECALDEF`, `MODELDEF` or MD3 into HellForge. The reviewed ZIP has no LICENSE and some graphics are malformed PNGs; **default to original procedural implementation**. Only copy a specific third-party asset after provenance, author attribution and license inclusion are resolved separately. Its tiny 8–14 px giblets do not substitute for a visually large Caco gut clump.

## F4 behavior

Do not add unnecessary menus. Reuse existing Gore Enhanced, Head Destruction, Body Damage & Viscera, Fragments and Wall Blood settings; preserve `graphics.cfg` compatibility. Keep limbs nonfunctional/disabled or explicitly label unavailable if it is not implemented. Blood OFF and Classic must disable M2 effects. The existing ordinary Wound Holes switch must remain independently functional.

## Implementation sequence / checkpoints

**A — Resolve logic:** centralize eligible monster styles; split `nonlethal critical wound` vs `fatal burst`; remove low-tier fatal torso cuts; reset on level changes/resurrection/removal. Add debug decision reason strings: `health_gate`, `damage_gate`, `opaque_gate`, `range_gate`, `feature_off`, `art_unverified`, `already_emitted`, `emitted`.

**B — Visible guts:** enlarge/restructure Caco blue core/lobes and Pinky/Mancubus tissue pieces; ensure floor and wall marks are created by actual ballistic collisions, not synthetic decals. Put a ceiling on per-event emitters and total active particles.

**C — Live wound masks:** Imp and Mancubus only; bounded anatomy, apply while alive, clear exactly when entering death presentation. Validate different living rotations, stock/replacement art and ragdoll fallback. Avoid touching the approved M1 human headless death frame path.

**D — Revenant bones:** pale fragments; head/skull cut only on verified complete death art; otherwise fragments and stock death.

**E — Tests and reports:** update `tests/wounds.cpp` and targeted policy tests without broad rewrites. `-goredebug` must show per-shot eligibility and emitted style, HP before/after, damage, contacts, distance, and fallback. Update user-facing implementation/testing MD to reflect revised behavior.

## Minimum acceptance matrix

1. Shotgun/SSG Zombieman and Shotgun Guy: head burst still correct; torso kill causes splatter but **never cuts chest/belly**. Normal corpse shape after torso kill.
2. Imp: nonlethal low-HP severe torso hit displays a cavity on **living** sprite; next killing hit yields ordinary death pose (or separately validated fatal head behavior), with no torso-hole overlay on corpse.
3. Revenant: bone shards land; skull cut only if matching artwork; fallback leaves no wrong holes.
4. Mancubus: obvious heavy chest/belly live wound and bleeding; death transition restores uncut **death** sprite while spray continues.
5. Pinky: pre-death viscera at low HP and fatal accent, no modified silhouette. Spectre stays unaffected.
6. Cacodemon: verify **at least one reproducible nonlethal low-HP blue gut ejection** and one severe fatal burst using local Doom II IWAD. Check center/trail visually for >=2 camera viewpoints; blue floor/wall stains; no phantom splashes.
7. All: fixed damage/AI and pellet outcomes under Classic/Enhanced, no duplicate effects, correct settings persistence, no stale state on resurrection/removal/map changes, and tests at 199 vs 200 map kill population. Test both proximity to walls and an open room.
8. If actual GPU rendering is unavailable, make no claims of visual completion; report precise manual checks needed.

## End-of-task output

- Modified files with concise purpose per file.
- Exact effective trigger thresholds and debug reasons, identified from real code.
- Summary of which art cuts are stock-verified and which use fallback.
- Remaining known limitations; performance/budget notes.
- A concise Windows playtest checklist, especially *why* Cacodemon predeath effects were previously absent and how the new event admission/visibility was verified.
- Do not spend tokens introducing new unrelated features or expanding monster roster. **Implement now; avoid further planning rounds unless blocked by a missing source file.**
