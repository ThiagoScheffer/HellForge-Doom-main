# DOOM: HELLFORGE

**Current integration build: 2026.11.14.1** | [Release notes](docs/changes/MAIN-INTEGRATION-2026-11-14.md)

DOOM: HELLFORGE is a personal Windows fork of the classic DOOM source, built
from the DERP/Duct-Tape renderer and the Tuin-Doom-Enhanced source fork. It
extends that foundation with sprite-aware combat, monster survival behavior,
larger-map support, persistent settings, and faster recovery after death.
The renderer, destruction systems, and bundled-mod integrations are described
below with their upstream credits.

**Windows x64 · C / C++ / C# · SDL3 · Vulkan**

[Build instructions](BUILDING.md) · [Documentation](docs/README.md) · [Change history](docs/changes)

![HELLFORGE launcher artwork](launcher/hero.png)

## What's new in HELLFORGE

From pixel-level hit detection to save recovery: these are the gameplay and
engine improvements highlighted in this fork. Each feature links to its behavior,
configuration, and verification notes.

| Feature | What players get | Engineering highlights |
| :--- | :--- | :--- |
| 🎯 **[Sprite-based headshots](docs/changes/CRITICAL-PARTS.md)** | 2× human headshots, per-pellet hits, and shots through transparent sprite gaps. | Uses the displayed frame, rotation, mirroring, and silhouette; shares impact coordinates with attached wounds. |
| 👁️ **[Monster weak points](docs/changes/ENHANCED-MONSTER-COMBAT.md)** | Heads, eyes, faces, brains, and mechanical legs have distinct combat rules. Weapon choice matters. | Monster profiles, conservative region classification, projectile provenance, and damage multipliers that do not stack. |
| 💥 **[Impact feedback & frenzy](docs/changes/MONSTER-FEEDBACK-AND-STATUSES.md)** | Metal-leg sparks, headshot confirmation, and close super-shotgun hits that can provoke infighting. | Confirmed-impact events, per-discharge bookkeeping, bounded particles, and preserved attacker attribution. |
| 🩸 **[Progressive bleeding & survival AI](docs/changes/MONSTER-SURVIVAL-AI.md)** | Three bleeding tiers; wounded Pinkies and Cacodemons can seek food and recover health. | Timed actor statuses, bounded food searches, collision-aware approaches, lifecycle cleanup, and versioned save extensions. |
| 🗺️ **[Large-WAD compatibility](docs/changes/BIG-WAD-COMPATIBILITY.md)** | Larger textures and maps can load beyond original engine limits. | 32-bit texture offsets, uncompressed XNOD support, checked allocations, blockmap rebuilding, and configurable zone memory. |
| ⚙️ **[Settings that stay saved](docs/changes/F4-SETTINGS-PERSISTENCE.md)** | Applied F4 choices survive quitting, Low-FX launches, and settings export/import. | A comment-tolerant config reader and same-directory temporary writes that preserve the previous file on failure. |
| ↩️ **[Use-to-load after death](docs/changes/DEATH-SAVE-RECOVERY.md)** | Press Use after dying to return to the last save written or loaded in the current session. | An optional death-flow hook into the normal loader, with ordinary map restart when no usable save is available. |
| **[Monster visibility performance](docs/changes/AMBUSH-SIGHT-F4.md)** | Toggle ambush sight reuse from F4 without changing hearing or encounter activation. | Reuses only an unchanged false sight result within one action; no cross-tic LOS cache. |
| **[Conservative sprite filtering](docs/changes/SPRITE-FRUSTUM-FILTERING.md)** | Reduce off-camera monster rendering work when safely outside view. | Artwork and shadow bounds, with fallbacks for mirrors, wounds, voxels and uncertain poses. |
| **[Boom conveyor and switch fixes](docs/changes/ABANDON-PLATFORM-FIX.md)** | Restore affected voodoo conveyor releases and platform switch chains. | Preserves voodoo inertia and carries actors while their footprint still touches the conveyor. |
| **[Safer WAD sight checks](docs/changes/SF2012-SIGHT-AND-DESTRUCTION-FIX.md)** | Avoid the identified SF2012 sight crash; keep wall destruction available. | Handles missing sectors on incorrectly flagged two-sided walls; normal launcher retains solid geometry. |

**Start with F4.** Human critical parts, enhanced weak points, weapon
vulnerabilities, injuries, bleeding, feeding, frenzy, and death-save recovery
are optional and default off. New ambush sight reuse, sprite filtering and idle-search
reuse also default off in the normal build. Sprite combat additions are restricted to
single-player sprite rendering; multiplayer, demos, and voxel targets retain
their documented exclusions.

> **Current behavior:** the survival-AI update supersedes the original flat
> bleeding and amber world-impact effects. It adds progressive bleeding,
> blood-colored headshot bursts, headshot sound, and optional feeding.
> The earlier feedback report remains linked as implementation history.

### Engineering and verification

The feature reports document native gameplay tests, collision and wound
regressions, isolated settings profiles, and save/load checks. Examples include
**121 detectable human head views with seven safe fallbacks**, **23 large-WAD
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
4. Use **F4** for graphics, lighting, combat, bundled-mod, and accessibility settings.

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

See the [large-WAD compatibility report](docs/changes/BIG-WAD-COMPATIBILITY.md)
and [reload/cache report](docs/changes/RELOAD-LIGHTING-CACHE.md) for measured
limits and verification evidence.

## Building

Install Visual Studio 2022 C++ Build Tools, CMake, and the Windows SDK, then run:

```powershell
powershell -ExecutionPolicy Bypass -File launcher/build.ps1
```

The default output is `dist/DOOM-Hellforge.exe`. Native tests and optional GPU
checks are documented in [BUILDING.md](BUILDING.md). No commercial IWAD is
bundled with the source or launcher.

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

Build 2026.11.14.1 integrates the tested experimental source and F4 controls into
the main checkout. New performance controls remain optional. Idle-search reuse
is restricted to classic geometry; normal solid-map launches retain destruction.
Profiling and benchmark tools are opt-in build configurations and are compiled
out of the normal launcher engine. Historical reports retain their original
builds, dates and coverage limitations; see the current release notes for the
integration checks and outstanding manual coverage.
