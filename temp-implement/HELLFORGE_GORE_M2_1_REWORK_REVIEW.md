# HellForge — Gore M2.1 Visual & Behavioral Rework

Status: **Design / Codex implementation specification**. Not implemented, compiled, or visually tested. The latest M2 implementation source tree was not provided for this review; decisions are based on the user's M2 report, the previously uploaded M1 source baseline, and direct inspection of `droplets_1.3.1.zip`.

## Why change M2

M2's permanent death-frame anatomical cuts increase complexity without consistently improving presentation. Put the major visual payoff on (1) critical injuries to **living** durable monsters, and (2) **directional visceral/bone effects** with meaningful surface collisions. Preserve M1's accepted human head explosions. Never place torso/belly cavities on Zombieman or Shotgun Guy.

## Seven-monster policy

| Monster (Doom actor type) | Nonlethal, low-health/severe hit | Qualifying fatal shotgun/SSG discharge | Death art and anatomical masks |
|---|---|---|---|
| Zombieman (`MT_POSSESSED`) | Existing ordinary wounds/blood only | Preserve M1 head burst if qualified; otherwise enhanced red flesh and wall/floor splatter | No chest/belly/arm/leg cut; M1 headless cut only where stock sequence verified |
| Shotgun Guy (`MT_SHOTGUY`) | Existing ordinary wounds/blood only | Same as Zombieman | Same |
| Imp (`MT_TROOP`) | One bounded chest **or** belly cavity on severe shot leaving it alive and below health threshold, plus short visceral ejection | Head burst on validated art or red visceral burst without corpse torso cut | Clear live cavity at death; render ordinary death frames except validated M2 head burst |
| Revenant (`MT_UNDEAD`) | Bone-shard reaction on substantial confirmed head/rib impact, if appropriate | Skull/head burst only on verified art; otherwise pale bone shards, larger fragments falling onto floors | Never make unvalidated rib/body cut; no ghost skull fragments from mismatched sprite sequence |
| Mancubus (`MT_FATSO`) | One sizeable anatomically bounded chest **or** belly cavity after severe hit when still alive and below health threshold, with bleeding and tissue spill | Strong heavy tissue/blood burst, no additional death-frame cut | Live wound ends on death; stock corpse art intact |
| Pinky (`MT_SERGEANT`) | One visceral ejection at low HP following substantial torso hit | Stronger red viscera burst on severe close lethal hit | No holes or detached parts |
| Cacodemon (`MT_HEAD`) | **Noticeable blue** gut/lobe burst on near-death nonlethal hit | Stronger blue viscera burst on severe fatal hit | No holes, head removal, or corpse masks |

**Spectre** (`MT_SHADOW`) excluded, even if it uses a matching sprite family. Other monsters unchanged. Dismemberment remains lethal-only; the live chest/belly cavities are *injuries*, not detached anatomy. Arm/leg dismemberment stays unimplemented until independently validated full death-frame art is available.

## Proposed tuning defaults (not proven optimal)

All trigger parameters are **initial experimental values**, not empirical findings. Keep centralized as named constants, adjust from `-goredebug` logs. Eligible damage means **observed actual HP lost**, not nominal pellet damage; use shot-end aggregate. Always require an admitted, actual opaque hit for anatomical qualification. Use `healthAfter > 0` for living-only wound effects. Compare `healthAfter` with **actual maximum HP** if available; otherwise a documented spawn-health reference. Guard zero/invalid maxHP. Do not force body cuts from screen-space approximations if sprite classification fails.

| Monster | Initial eligibility suggestion |
|---|---|
| Humans | Fatal, <=128 Doom units, preserve M1 head-majority rule. Heavy splatter can occur for a validated close fatal hit without head majority. |
| Imp | Nonlethal, <=128 units, post-hit HP <=40% max, observed shot damage >=15, confirmed chest/belly contact. One cavity/event per actor. |
| Revenant | Fatal skull hit for verified head cut; confirmed severe head/rib impacts can emit pale bone pieces. Initial aggregate >=30 damage for rib-like shard reaction; no corpse cut otherwise. |
| Mancubus | Nonlethal, <=128 units, post-hit HP <=45% max, observed shot damage >=35, at least two confirmed torso-region contacts. One cavity per actor. |
| Pinky | Nonlethal <=128 units, post-hit HP <=45% max and damage >=20 with confirmed torso contact. Fatal burst requires a meaningful confirmed close hit. |
| Cacodemon | Nonlethal <=128 units, post-hit HP <=50% max, observed shot damage >=20, **>=2 opaque contacts OR >=1 opaque contact with >=30 observed damage**. Fatal severe burst uses a separate permissive threshold, not M2's old conjunction of >=4 contacts AND >=70 damage AND <=96 units. |

For Pinky/Caco, use a one-time `visceraReleased` visual-state flag per actor; allow an additional **single fatal accent** without repeating the living burst. For Imp/Mancubus, use a once-only region wound flag; clear visual masks on death, resurrection, actor removal, shutdown, and level change. Do not repeatedly re-trigger on every tic beneath the health threshold.

## Visual approach

1. Separate **event admission**, **effect presentation**, and **art modification**. Events are shot-scoped; anatomical cuts are actor-scoped visual state.
2. Add fragment archetypes: small spray, mid-size tissue/gut, heavy lobed viscera, pale bone. Every archetype must be visibly distinct from ordinary blood specks. Cacodemon fragments MUST use HellForge's blue blood tint (`MT_HEAD`) for both airborne pieces and deposited marks.
3. Launch along the incoming blast direction with a controlled spread: center/forward lobe plus a few smaller satellite fragments. Gravity produces falling arcs; hit-tested walls get splashes/smears and floors get landing marks. No fake wall splat if no valid wall was intersected.
4. Use HellForge's existing `spray.cpp` ballistic routine (`throwDrop`/equivalent), `firstSolidWall`, `bloodWallSplat`, `bloodFloorSplat`, and decals. Extend the fragment visual rather than adding game actors or a second renderer.
5. Give Caco the most noticeable silhouette: a compact blue core and trailing small lobes; extend the visual life/trail (within caps). Existing M1 1–3 px clusters are too small to convey organ-like pieces on a large Caco.
6. Apply surface marks at valid collision time, not instantly upon spawn. Existing blood object budgets must be shared with new effects.
7. Live cavities: reuse original wound mask/renderer but bind the region to living frame anatomy. On death, **turn off only the M2 torso cavity overlay** so the stock death animation plays with no new mismatched cut. Normal ordinary wounds continue according to pre-existing settings.
8. Skull/head cut for Revenant only where a verified matching complete death sequence supports it. If no such art, render bone-shard explosion only and record fallback in log.

## Performance and behavior invariants

- Require `totalkills < 200` (map kill population), accelerated renderer, gore Enhanced, blood enabled, supported weapon/player context; preserve game-mode restrictions already present in M2.
- No gameplay health, damage, RNG, hit-tracing, AI, knockback, drop actors or save format modifications. Resolve one shot at `P_WeakpointEndShot` per unique victim/discharge; ignore synthetic postmortem hits.
- Keep **64 concurrent visible gore fragments** in shared **768 airborne drops** and pre-existing blood pools/decals. Up to **3 extra wall marks** per event, not an unbounded recursion of splashes.
- Scale *appearance*, not unbounded count, for larger monsters. Example first-pass counts: human 3–6 fragments; Imp 4–6; Revenant 3–6 bone; Mancubus 5–8; Pinky 4–7; Caco 6–10, subject to shared global headroom.
- F4 reuse existing Gore level, head, fragments, wall blood, body damage & viscera; keep limb toggle unavailable until a cutting implementation actually exists. Classic/Blood Off must not activate gore.

## Droplets 1.3.1 archive audit (directly inspected)

- ZIP CRC integrity passes, **113 entries**, 74 `.png` image files, including `Sprites/GIBLA0.png` through `GIBLZ0.png` (small 2D giblets, animated/floor variants), `PLUD*`, `WBLD*`, `Graphics/BSPLAT*`, `BSMEAR*`, `BRUNY*`, and an MD3 floor plane model plus floor textures.
- ZScript includes `Gibs.txt` (gib spin, bounce, drips, landing frame), `Droplets.txt` (droplet flight and surface interaction), `Wall.txt` (running wall blood), `FloorSplat.txt` (growth of landed marks), `Ceiling.txt` (drips) and an event handler. These are **GZDoom-specific**, not native HellForge C/C++ and not directly loadable as functional mechanics.
- **Eight grayscale Graphics PNGs** (`BRUNY1–3`, `BSMEAR1–2`, `BSPOT1–3`) **fail strict Pillow decoding** because of a malformed `alPh` ancillary chunk CRC; removing that extraneous chunk allows decoding. Any art selected for import must be normalized, alpha-masked, converted to renderer-compatible representation, and verified visually. The ZIP itself passes integrity checks; PNG compatibility is a separate issue.
- `DECALDEF.txt` refers to `BSPOT4` and `BSPOT5` not present in this archive. Do not copy its definitions as-is.
- ZIP contains **no license file**. The upstream [JRHard771/droplets repository](https://github.com/JRHard771/droplets) publishes a `license.md` allowing modification/reuse with credit to Josh771 and inclusion of the license text intact on redistribution. Verify asset provenance/version before copying binaries. `DECALDEF.txt` attributes some Doom 3 blood decals to Ghastly_Dragon; third-party permissions require separate verification. Favor using Droplets as a behavior reference and HellForge's own procedural blood visuals first.
- The `Sprites/GIBL*` images are generally only about **8–14 pixels across**, and do not by themselves supply a convincing large Cacodemon or Mancubus organ model. A weighted multi-lobe/procedural cluster is still needed.
- Do not bundle the entire ZIP or try to execute ZScript/DECORATE/DECALDEF/MODELDEF/MD3 in HellForge. Do not add GZDoom runtime dependencies.

## Acceptance criteria

- Humans: no new chest/belly holes, only M1 validated head explosions and directional body splatter; normal corpse silhouettes otherwise.
- Imp/Mancubus: clear, convincing living wound at health threshold, no cavity leaking into death/lying art, no repeated wound spam.
- Caco: visibly blue **large** pieces launch **before death on qualifying wounded shots**, and larger burst on close severe death. Pieces travel, land, and deposit tint-correct wall/floor marks in maps with actual nearby surfaces. Effect occurs in practical repeated tests (log eligibility failures).
- Pinky: visible visceral response while low HP, stronger fatal effect, no cut body sprite.
- Revenant: pale bone pieces on severe impacts; skull destruction only with verified art; no inappropriate flesh fragments.
- Classic and Blood Off disable added effect, no new health/damage changes, no infinite particles, no duplicate per-pellet bursts, no stale actor pointers.
- Check different death animations, ragdoll ON/OFF, custom sprite replacements, Spectres, save/load, resurrection and 199/200 population boundary.
- Before changing code, Codex must inspect the **actual current M2 tree**; the above is not a claim that those sources were supplied to this planning session.
