# DOOM: HELLFORGE by Thiago Scheffer

**Current build: 2026.11.15.6** | [DOOM2 startup crash fix](docs/changes/DOOM2-STARTUP-LIGHT-CACHE-CRASH.md) · [Large platform protection](docs/changes/LARGE-PLATFORM-SHIELD.md) · [E1M8 exit and screen-edge fixes](docs/changes/E1M8-AND-SPEEDRUN-FIXES.md) · [Speedrun position and layout](docs/changes/SPEEDRUN-LAYOUT.md) · [HUD background opacity](docs/changes/HUD-BACKGROUND-OPACITY.md) · [Elemental Damage 2.0 and Speedrun HUD](docs/changes/ELEMENTAL-DAMAGE-2.md) · [Integration notes](docs/changes/MAIN-INTEGRATION-2026-11-14.md)

DOOM: HELLFORGE is a personal Windows fork of the classic DOOM source, built
from the DERP/Duct-Tape renderer and the Tuin-Doom-Enhanced source fork. It
extends that foundation with sprite-aware combat, monster survival behavior,
larger-map support, atmospheric sky rendering, cosmetic monster damage,
corpse ambience, persistent settings, and faster recovery after death.
The renderer, destruction systems, and bundled-mod integrations are described
below with their upstream credits.

Why? cos we love Doom. I wanted something personal as my taste.

**Windows x64 · C / C++ / C# · SDL3 · Vulkan**

[Build instructions](BUILDING.md) · [Documentation](docs/README.md) · [Change history](docs/changes)

![HELLFORGE launcher artwork](launcher/hero.png)

## What's new in HELLFORGE

From pixel-level hit detection to atmospheric rendering and corpse ambience,
these are the gameplay and engine improvements highlighted in this fork. Existing
feature reports are linked below; newer source-patch additions are described in
the sections that follow.

| Feature | What players get | Engineering highlights |
| :--- | :--- | :--- |
| **Immersive Sky Environment** | Moving layered clouds and distant rocky, icy, industrial, or infernal horizons, selectable through F4. | Three parallax horizon layers; WAD-sky-tinted pseudo-volumetric clouds; continuous wind; Off/Auto/manual profiles without changing map geometry or sunlight bakes. |
| **Soft Sun Shafts 2.0** | Smoother, less pixelated beams with gradual edges and depth transitions. | Removes 32-level alpha quantization and ordered dithering in the sunbeam shader; replaces stepped noise motion with continuous drift. |
| **HUD bloom ghosting fix** | HUD text and graphics no longer cast misplaced dark copies across bright emissive surfaces in the tested configuration. | Corrected bloom-mask sampling for the three-layer HUD texture atlas while retaining emissive textures. |
| **Progressive Monster Skin Damage** | Injuries, scratches, and blood increase as supported living monsters lose health. | Three health-driven artwork tiers; stock-sprite identity checks; reusable cached variants; independent of Wound Holes and gameplay bleeding. |
| **Corpse & Decoration Immersion** | Delayed flies, ground worms, and corpse-specific scavengers bring small encounters and biological decorations to life. | Reusable ambience profiles, deterministic selection, existing particle renderer, resurrection cleanup, and a hard cutoff at 200 spawned kill-count monsters. |
| 🎯 **[Sprite-based headshots](docs/changes/CRITICAL-PARTS.md)** | 2× human headshots, per-pellet hits, and shots through transparent sprite gaps. | Uses the displayed frame, rotation, mirroring, and silhouette; shares impact coordinates with attached wounds. |
| 👁️ **[Monster weak points](docs/changes/ENHANCED-MONSTER-COMBAT.md)** | Heads, eyes, faces, brains, and mechanical legs have distinct combat rules. Weapon choice matters. | Monster profiles, conservative region classification, projectile provenance, and damage multipliers that do not stack. |
| **[Elemental Damage 2.0, combat notifications & Speedrun HUD](docs/changes/ELEMENTAL-DAMAGE-2.md)** | Optional Imp/Mancubus fire damage, blue plasma burns, charred Cacodemons, colored combat events and seven independent session metrics. | One authoritative DoT stream per actor, v6 status saves, bounded event queue and event-based session history that survives checkpoint reloads. |
| **[Elemental combat & injury reactions](docs/changes/ELEMENTAL-COMBAT.md)** | Cosmetic fire, blue plasma burns, charred corpses, Mancubus stagger, wounded Baron aggression and Arch-vile health siphoning. | Confirmed health-loss provenance, bounded local donor queries, immutable corpse artwork variants and versioned actor status saves. |
| 👁️ **[Eye injuries](docs/changes/EYE-INJURIES.md)** | Hitscan eye hits can blind Cacodemons/Pain Elementals until they eat a corpse, or provoke ten seconds of Spider Mastermind rage. | Fingerprint-validated visible-eye masks, one chance per discharge, normal projectiles, bounded feeding and versioned save state. |
| 💥 **[Impact feedback & frenzy](docs/changes/MONSTER-FEEDBACK-AND-STATUSES.md)** | Metal-leg sparks, headshot confirmation, and close super-shotgun hits that can provoke infighting. | Confirmed-impact events, per-discharge bookkeeping, bounded particles, and preserved attacker attribution. |
| **[Survival escape & frenzy retaliation](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)** | Wounded Cacodemons/Pinkies retreat to feed; Arch-viles hunt living prey; same-species frenzy opponents can retaliate after frenzy ends. | Bounded searches, earned healing, timed combat fallback and opponent-specific missile provenance. |
| 🩸 **[Progressive bleeding & survival AI](docs/changes/MONSTER-SURVIVAL-AI.md)** | Three bleeding tiers; wounded Pinkies and Cacodemons can seek food and recover health. | Timed actor statuses, bounded food searches, collision-aware approaches, lifecycle cleanup, and versioned save extensions. |
| 🗺️ **[Large-WAD compatibility](docs/changes/BIG-WAD-COMPATIBILITY.md)** | Larger textures and maps can load beyond original engine limits. | 32-bit texture offsets, uncompressed XNOD support, checked allocations, blockmap rebuilding, and configurable zone memory. |
| ⚙️ **[Settings that stay saved](docs/changes/F4-SETTINGS-PERSISTENCE.md)** | Applied F4 choices survive quitting, Low-FX launches, and settings export/import. | A comment-tolerant config reader and same-directory temporary writes that preserve the previous file on failure. |
| ↩️ **[Use-to-load after death](docs/changes/DEATH-SAVE-RECOVERY.md)** | Press Use after dying to return to the last save written or loaded in the current session. | An optional death-flow hook into the normal loader, with ordinary map restart when no usable save is available. |
| **[Launcher tools & settings tabs](docs/changes/LAUNCHER-TOOLS-AND-SETTINGS.md)** | New forge icon, animated Arch-vile/fire, log/config shortcuts, backed-up fresh start and tabbed preferences. | 355 F4/balance controls, config-preserving edits and IWAD-sourced animation; saves remain intact. |
| **[Encounter Director](docs/changes/ENCOUNTER-DIRECTOR.md)** | Independent monster randomization, hidden reinforcements, category-safe item randomization and original-population staging. | Protected progression actors, native placement checks, separate budgets, single-player eligibility and nested F4 tabs. |
| **[Exact solid-world sight acceleration](docs/changes/SOLID-SIGHT-MESH-CACHE.md)** | Reduce repeated visibility geometry work on large monster maps while retaining destruction and activation. | Bounded, invalidated geometry cache and conservative triangle filtering; fresh visibility queries rather than cached sight answers. |
| **[HUD opacity](docs/changes/HUD-BACKGROUND-OPACITY.md) & [Speedrun layout](docs/changes/SPEEDRUN-LAYOUT.md)** | Transparent panel backgrounds, screen-edge X/Y positioning and row/column statistics. | Independent persisted opacity controls, widescreen bounds and text-preserving layout. |
| **[Reload stability & lighting cache](docs/changes/RELOAD-LIGHTING-CACHE.md)** | Safer restarts and reuse of static lighting across sessions. | Render-resource lifecycle cleanup and versioned, validated disk entries; live effects remain outside the cache. |
| **[Large moving-platform protection](docs/changes/LARGE-PLATFORM-SHIELD.md)** | Keep breakable surfaces enabled while protecting large floor movers from excavation damage. | Rejects protected cuts before remeshing; small platforms and ordinary surfaces retain destruction. Manual lag confirmation remains pending in the linked report. |
| **[Monster visibility performance](docs/changes/AMBUSH-SIGHT-F4.md)** | Toggle ambush sight reuse from F4 without changing hearing or encounter activation. | Reuses only an unchanged false sight result within one action; no cross-tic LOS cache. |
| **[Conservative sprite filtering](docs/changes/SPRITE-FRUSTUM-FILTERING.md)** | Reduce off-camera monster rendering work when safely outside view. | Artwork and shadow bounds, with fallbacks for mirrors, wounds, voxels and uncertain poses. |
| **[Boom conveyor and switch fixes](docs/changes/ABANDON-PLATFORM-FIX.md)** | Restore affected voodoo conveyor releases and platform switch chains. | Preserves voodoo inertia and carries actors while their footprint still touches the conveyor. |
| **[Safer WAD sight checks](docs/changes/SF2012-SIGHT-AND-DESTRUCTION-FIX.md)** | Avoid the identified SF2012 sight crash; keep wall destruction available. | Handles missing sectors on incorrectly flagged two-sided walls; normal launcher retains solid geometry. |

**Optional population staging:** [Initial monster budgets](docs/changes/INITIAL-MONSTER-POPULATION-BUDGET.md) add count/percentage controls under F4 → Encounter Director → ED Population. Performance mode starts the supplied 4,317-monster WAD with 500 originals when requested, accepting visible absence/pop-in; Compatibility retains eager safety. No FPS benchmark is claimed.

**Start with F4.** Human critical parts, enhanced weak points, weapon
vulnerabilities, injuries, eye injuries, bleeding, feeding, frenzy, and death-save recovery
are optional and default off. Damaging elemental burns, colored notifications and the Speedrun HUD also default off. Immersive Sky, Monster Skin Damage and Decoration Immersion also default Off. New ambush sight reuse, sprite filtering and idle-search
reuse also default off in the normal build. Sprite combat additions are restricted to
single-player sprite rendering; multiplayer, demos, and voxel targets retain
their documented exclusions.

> **Current behavior:** the survival-AI update supersedes the original flat
> bleeding and amber world-impact effects. It adds progressive bleeding,
> blood-colored headshot bursts, headshot sound, and optional feeding.
> The [latest survival update](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)
> adds escape/search behavior, Arch-vile hunting and persistent hostile-pair
> retaliation. Earlier reports remain linked as implementation history.

### Latest survival and encounter updates

- **Monster Feeding** remains optional and default off under F4 → Combat.
  Cacodemons and Pinkies at **30% health or less** search/approach food with a
  nominal **20% walking boost**, avoid nearby visible danger and recover through
  actual meals, up to **60% health**. Ten seconds without a successful meal
  returns them to combat with a ten-second failed-search cooldown.
- **Arch-vile survival:** at **50% health or less**, the same feeding option
  enables hunting eligible living biological monsters. A close siphon removes
  **24 HP**; healing is capped by actual damage and the recovery ceiling.
  Players and protected prey are excluded; normal resurrection retains priority.
- **Frenzy retaliation fix:** a Mancubus and its confirmed same-species hostile
  opponent can damage each other after frenzy expires. The Knight/Baron family
  is covered too. This preserves shooter and unrelated same-species immunity.
- **Encounter Director** has one F4 category with **ED Monsters**, **ED
  Reinforcements**, **ED Items**, **ED Population** and **ED Advanced** tabs,
  avoiding the earlier overlapping controls. All four modules default off;
  initial randomization preserves population, reinforcements add bounded actors,
  and population staging controls deferred originals separately.

[Survival behavior, tuning and focused checks](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)
? [Director controls and progression guards](docs/changes/ENCOUNTER-DIRECTOR.md)

### Recent atmospheric rendering and cosmetic FX updates

The following incremental changes target the **2026.11.15.6 source baseline**.
They do not introduce new gameplay damage or modify classic WAD content.
The relevant user-run verification is distinguished below from source-only tests.

#### Immersive Sky Environment (Milestones A–C)

- **Milestone A:** introduced moving procedural cloud layers over the original WAD
  sky, using the existing sky pipeline and cloud texture.
- **Milestone B:** corrected wind motion that appeared to reverse direction and
  added distinct rocky, icy, industrial-city and infernal horizon treatments.
  The user reported the Milestone B test passed.
- **Milestone C:** added three apparent-depth/parallax skyline layers, sparse
  softly shaded cloud puffs, and episode/map-range automatic presets. Classic
  sky pixels remain the underlying artwork; cloud colors follow the loaded sky.
  This is a *pseudo-volumetric* effect, not raymarched volumetric clouds.
- **F4 → Atmosphere → Air → Immersive sky environment:** Off (default), Auto,
  Rocky mountains, Ice mountains, Inferno or Burning city. The F4 selector
  requires `Settings::immersiveSky`; a separate header repair was supplied for
  older headers missing this field. F4 choices use the normal `graphics.cfg`
  apply/persist mechanism.
- **Compatibility:** Auto assigns **DOOM E1 rocky / E2 icy / E3+ infernal**
  scenes and **DOOM II MAP01–11 rocky / MAP12–20 industrial city /
  MAP21–32 infernal** scenes. These are map-range conventions, **not** a
  fingerprint of replacement sky art. Custom WADs can select a
  manual profile or Off. These effects do not alter level geometry, actor
  collision or baked lighting; glass reflection probes can retain cached sky
  images until their normal refresh.

#### Soft Sun Shafts 2.0 and emissive HUD fix

- **Soft Sun Shafts 2.0:** updated `platform/shaders/mist.frag` to remove Bayer
  dithering and 32-step shaft-opacity quantization, smooth beam edges/endpoints,
  soften intersections against scene depth, and animate low-frequency density
  continuously. Existing mist, sun direction, sun bake and shaft mesh remain.
  **User runtime test: passed and approved.**
- **HUD bloom ghosting:** bright emissive surfaces could display dark, displaced
  copies of HUD text and graphics. The bloom suppression mask in
  `platform/shaders/present.frag` sampled the 320×600 HUD atlas as **two**
  vertical bands even though it contains **three**. Correcting the main and
  widescreen HUD coordinates removed the observed artifact while keeping
  emissive textures enabled. **User runtime test: bug solved.**
  The separate third Speedrun HUD atlas layer was not given a new bloom mask
  by this narrowly scoped repair.
- Both fixes change **shader source**: run `Recompile-Shaders.bat` before
  `Build-Hellforge.bat` so updated shader binaries are embedded in the engine.

#### Progressive Monster Skin Damage

- **F4 → Combat → Combat → Monster skin damage:** independent On/Off setting,
  **Off by default**. Supported living monsters accumulate body-only scratches,
  cuts and blood as their *current health* decreases; this has no gameplay,
  damage-over-time or AI effect.
- Health bands: **76–100%** original sprite, **51–75%** light scratches,
  **26–50%** moderate injuries, **1–25%** severe injuries.
- **Supported monsters (10):** Imp, Baron of Hell, Pinky, Spectre, Mancubus,
  Arch-vile, Cyberdemon, Cacodemon, Hell Knight and Zombieman.
  Anatomy-aware masks protect heads and important non-skin areas such as
  Cyberdemon machinery, Mancubus cannons and the Cacodemon eye and mouth.
- Uses cached, shared damaged **sprite-frame variants** rather than tracking
  individual hit positions or allocating unique textures per monster. Wound
  Holes continue to attach to the *original* artwork. Sprite replacements
  lacking recognized stock fingerprints, voxel rendering, charred variants
  and death artwork are not given these cosmetic injury textures.
- The six-monster expansion raised the **estimated combined CPU/GPU variant
  payload budget from 16 MiB to 32 MiB**. At its limit the renderer falls back
  to unmodified artwork. This is a cache policy, **not a benchmark**.
- Barons and Hell Knights use the existing green-blood treatment. Exact
  Zombieman stock-frame recognition can be generated from a user-owned DOOM
  IWAD; if no verified fingerprints are available, its replacement artwork
  is intentionally not modified.
- **User runtime tests:** the original Imp/Baron/Pinky/Spectre prototype and
  subsequent six-monster expansion were reported as passed and approved.
  These cosmetic health-stage variants do **not** persist onto death sprites.

#### Corpse & Decoration Immersion

- **F4 → Atmosphere → Decoration Immersion:** independent On/Off switch
  (**Off by default**), **Corpse insects** (Off / Flies / Flies and worms), and
  **Decay delay** (Short / Normal / Long). The more unusual ground creatures
  require **Flies and worms** mode.
- The reusable **Decoration Ambience Manager** supports ordinary monster
  corpses, crushed/gibbed or pre-placed remains, impaled bodies and hanging
  biological decorations. Delayed flies can surround suitable bodies;
  crawling worms are restricted to eligible ground-level remains.
- **Ordinary worms:** 1–3 per selected corpse, with a black head, flesh/pink-beige
  segmented body and slow wriggle. A visibility repair increased their chance
  and rendered size; the later visual pass uses tapered, overlapping particle
  blocks for a rounder-looking outline (not curved 3D mesh geometry).
- **Mancubus and Baron:** specialized **dark maroon-red earthworms**, 1–3 per
  selected corpse, with six segments and approximately **1.6–1.7×** the normal
  worm footprint after the latest refinement. Flies remain possible.
- **Revenant:** small bone-colored spiders with eight dark legs; **no flies**.
- **Arch-vile:** crawling fiery red/orange/yellow sea-slug-like scavengers with
  animated frills; **no flies**.
- **Cacodemon:** blue jelly-like scavengers with animated tassels and subtle
  pulsing; the newest palette is darker blue. **No flies.** Their glow is a
  color effect, **not** an added emissive light, bloom pass or shader.
- Other corpses retain ordinary fly/worm behavior; hanging and impaled bodies
  retain their general fly behavior. Special species are selected from known
  monster types; custom sprites using those existing types are **not**
  independently fingerprint-validated by the ambience system.
- Selection is deterministic and does not consume gameplay RNG. Generic flies
  have a **40%** selection chance; the worm visibility fix raised the ordinary
  worm chance to **45%**; special-species ground creatures have a **65%**
  selection chance. Appearance is intentionally not guaranteed per body.
  On Normal delay, flies start after roughly **15–30 seconds** and ground
  creatures after roughly **30–60 seconds**. Short/Long adjust the delays.
- **Performance gate:** automatically disabled when Doom's spawned
  kill-count monster population (`totalkills`) reaches **200 or more**.
  The manager caps tracking at **256 nearby bodies**, **12 active emitters**,
  **48 flies**, **24 ground creatures**, **192 ground-creature render blocks**,
  and **512 DOOM units** for rendering, with nearby scans about once per second.
  There are no new gameplay actors, shaders or save-format changes.
- Revived or removed corpses stop displaying scavengers. A corpse's delay
  begins when it is first detected near the player rather than necessarily
  at its original death event; visually displaced ragdoll corpses may have
  effects near the underlying actor position.
- **User runtime tests:** corpse flies passed, the initial invisible-worm
  issue was corrected, and the black-headed worm visual update passed.
  Species-specific creatures have source-level mock integration coverage;
  the **latest smaller/rounder worm and darker palette refinement still needs
  an explicit final Windows/Vulkan visual acceptance report**.

### Engineering and verification

The linked feature reports document native gameplay tests, collision and
wound regressions, isolated settings profiles, and save/load checks. Examples include
**571 combat/rule/save checks and a focused real-core survival fixture** for
the latest survival update, **121 detectable human head views with seven safe fallbacks**, **23 large-WAD
maps passing historical loading checks**, and **three live death/Use cycles
restoring saved player state**. These results belong to the builds and test
conditions recorded in their linked reports; loading coverage does not imply
full campaign or performance certification.

**Explore:** [Combat matrix](docs/changes/ENHANCED-MONSTER-COMBAT.md#implemented-matrix)
· [Survival AI](docs/changes/MONSTER-SURVIVAL-AI.md)
· [Build and test instructions](BUILDING.md)
· [Documentation index](docs/README.md)

## Download and play

1. Build or obtain `DOOM-Hellforge.exe`.
2. Launch it and select a compatible Doom, Doom II, Final Doom, or Freedoom IWAD.
3. Add compatible WAD/PK3 add-ons in load order, then choose **PLAY GAME**.
4. Use **GAME SETTINGS / TABS** before launching, or **F4** in-game, for graphics, lighting, combat and other preferences.

Launcher quick tools open the last log and graphics config. **CLEAR ALL / FRESH
START** backs up and resets preferences/add-ons while keeping saves and IWADs.

Commercial IWADs are not included. Windows x64, a Vulkan-capable graphics
driver, and .NET Framework 4.5+ are required for the supplied launcher.

The launcher stores extracted resources, backups, crash logs, and its lighting
cache under `%LOCALAPPDATA%\DOOM-HellforgeLauncher`. Game settings and saves
use `%APPDATA%\Local Games\DOOM Hellforge`; screenshots are saved beside the
launcher in `Screenshots`.

## Renderer, bundled content, and additional features

HELLFORGE builds on the DERP renderer and the Tuin-Doom-Enhanced fork. This
broader feature set includes inherited systems and integrations as well as
the extensions highlighted above.

### Renderer, lighting, and world presentation

- Optional three-layer Immersive Sky Environment with sky-tinted cloud motion,
  episode/map-based automatic presets and manual F4 scene profiles.
- Soft Sun Shafts 2.0 (continuous alpha and density transitions) and the
  emissive HUD bloom-mask fix for dark duplicated HUD silhouettes.
- Native SDL3 accelerated 3D renderer with dynamic lights, colored materials,
  reflections, liquid surfaces, sky lighting, shadows, AO, caustics, and
  destruction geometry.
- Multithreaded lighting bake, spatial ray traversal, selective relighting,
  reusable geometry, and bounded light/shadow caches.
- Persistent map-lighting cache keyed by resource content, map inputs, world
  state, lighting settings, and bake format. F4 controls caching; the launcher
  can clear it. GPU handles, pointers, actors, blood, particles, and live
  effects are never serialized.
- Safe level-transition teardown, worker barriers, first-frame diagnostics,
  crash logs, retained PDB symbols, and Windows minidumps.
- Large-texture and large-WAD compatibility improvements, including expanded
  texture column offsets and configurable native zone memory.

### Weapons and bundled content

- Native Fluid Weapons presentation for fists, pistol, shotgun, super shotgun,
  chaingun, rocket launcher, plasma rifle, BFG, and chainsaw.
- Recoil, reload and support hands, shell/bullet catches, muzzle flashes,
  plasma/BFG effects, weapon fidgets, smooth display interpolation, widescreen
  framing, fist alternation, weapon wheel, and optional fast-pistol behavior.
- Bundled Fluid Weapons, Fluid Weapons SFX, and FluidMonsters packages can be
  enabled independently through F4. Their original notices and credits remain.

### Monsters and combat

- Optional health-driven **Monster Skin Damage**, using shared cached sprite
  variants for ten stock monster types and independent of gameplay bleeding.
- Optional **Corpse & Decoration Immersion**: delayed flies, pale worms,
  large dark-red worms, Revenant bone spiders, Arch-vile fire slugs and
  Cacodemon blue jelly creatures, bounded by a small-map performance gate.
- Optional sprite-pixel critical parts with transparent-pixel misses and
  human headshots.
- Extended monster weak points and weapon vulnerabilities for humanoids,
  floating monsters, Lost Souls, Pinky/Spectre faces, and mechanical brain/leg
  regions, with explicit single-player sprite-only exclusions.
- Headshot impact feedback, metal-leg sparks, progressive bleeding, temporary
  injuries, localized effects, and optional monster frenzy/infighting rules.
- Native FluidMonsters movement for supported ground monsters, overlap recovery,
  door/corner chase fixes, floor/ceiling fitting, and typed notarget controls.

### HUD, input, and game flow

- Classic, compact modern, and tactical HUD layouts with scale and placement
  controls, widescreen anchoring, kills/secrets/items counters, notices, Toasty
  presentation, photo mode, camera director, arcade scoring, and LAN/co-op.
- Independent HUD/Speedrun background opacity down to **0%**, plus Speedrun
  **Row/Column** layout and **X/Y offsets** that reach the widescreen edges.
- Local single-player movement options including autorun, jumping, mouse look,
  and limited air steering. Demo, multiplayer, voxel, and unsupported scripted
  content retain their documented restrictions.
- F4 settings persist only when applied through the normal close/apply flow;
  F10 discards unapplied drafts. Settings export/import creates recovery backups.

## Compatibility and limitations

The supported map target is classic DOOM/Boom-style WAD content. This fork does
not implement a general GZDoom ZScript/DECORATE/UDMF runtime, arbitrary custom
shaders, or full voxel-mod scripting. Native voxel rendering does not provide
sprite-specific anatomy or deformation. Multiplayer, demos, and several
renderer-only features intentionally bypass single-player sprite combat rules.

Performance depends on map complexity, hardware, resolution, lighting settings,
and accumulated destruction. The lighting cache reduces repeated bake work but
does not avoid map reconstruction, geometry preparation, or GPU uploads.
Decoration ambience is automatically disabled at 200 or more spawned
kill-count monsters, but this is not a measured FPS guarantee. Sprite skin
injuries are deliberately limited to recognized stock artwork; unusual PWAD
replacements may remain visually unmodified.

See the [large-WAD compatibility report](docs/changes/BIG-WAD-COMPATIBILITY.md)
and [reload/cache report](docs/changes/RELOAD-LIGHTING-CACHE.md) for measured
limits and verification evidence.

## Building

Install Visual Studio 2022 C++ Build Tools, CMake, and the Windows SDK, then
double-click **Build-Hellforge.bat** in the repository root. For shader changes
(including Immersive Sky, sun shafts and HUD bloom masking), run
**Recompile-Shaders.bat** first; required tools are listed in
[BUILDING.md](BUILDING.md). Command-line builds also work:

```powershell
powershell -ExecutionPolicy Bypass -File launcher/build.ps1
```

The default output is `dist/DOOM-Hellforge.exe`. Native tests and optional GPU
checks are documented in [BUILDING.md](BUILDING.md). No commercial IWAD is
bundled with the source or launcher.

**Forced rebuild:** double-click [Build-Hellforge-V2.bat](Build-Hellforge-V2.bat)
to clean/rebuild native code and dependencies, then package the launcher, even
when edited/copied source files have older timestamps than existing binaries.
Use `Build-Hellforge-V2.bat -RecompileShaders` to regenerate shaders too.
Source, settings and the CMake cache are preserved; this takes longer than the
normal incremental build and does not run game tests.

**Stale-binary troubleshooting:** Some engine files were updated without a
correspondingly newer executable being launched. After applying a source patch,
ensure the changed files are rebuilt, embedded shaders are regenerated when
needed, and the launcher runs the newly built executable. A newer source
file alone does not prove its changes are present in a running binary.

## Source and credits

- [DERP / original engine project](https://github.com/robongrui/derp)
- [Tuin-Doom-Enhanced source fork](https://github.com/tuin-boop/Tuin-Doom-Enhanced)
- DOOM source by id Software; original notices are retained.
- MR's Fluid Weapons, Fluid Weapons SFX, FluidMonsters, SDL3, Nuked OPL3,
  TinySoundFont, Toasty media, and other dependencies retain their own credits
  and licenses.

See [LICENSE](LICENSE), [licenses/NOTICE.txt](licenses/NOTICE.txt), and
[licenses/THIRD_PARTY.md](licenses/THIRD_PARTY.md).

## Project status

**README baseline: build 2026.11.15.6.** The sky, soft sun-shaft, HUD bloom,
monster skin-damage and decoration-ambience updates above were developed as
incremental source patches for this build family. The user's reported runtime
passes cover the specific changes identified above; the latest ambience visual
refinement still requires an explicit final runtime acceptance report. No new
release/build identifier is asserted solely because a patch package exists.

The latest survival/retaliation changes have a rebuilt native engine and focused
rule/real-core verification; their launcher package was **not** rebuilt in that
update. Rebuild the launcher to include them. Visual steering, crowded-map
performance and animation presentation still need in-game review; see the
[dated survival report](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md).

Build 2026.11.14.1 was an earlier integration milestone. New performance
controls remain optional. Idle-search reuse is restricted to classic geometry;
normal solid-map launches retain destruction. Profiling and benchmark tools
are opt-in build configurations and are compiled out of the normal launcher
engine. Historical reports retain their original builds, dates and coverage
limitations; see the current release notes for the integration checks and
outstanding manual coverage.
