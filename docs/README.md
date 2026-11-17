# DOOM: HELLFORGE documentation

Current build: **2026.11.15.6**. See [DOOM2 startup crash fix](changes/DOOM2-STARTUP-LIGHT-CACHE-CRASH.md), [large platform protection](changes/LARGE-PLATFORM-SHIELD.md), [E1M8 exit and screen-edge fixes](changes/E1M8-AND-SPEEDRUN-FIXES.md), [Speedrun position/layout](changes/SPEEDRUN-LAYOUT.md), [HUD background opacity](changes/HUD-BACKGROUND-OPACITY.md) and [Elemental Damage 2.0 and Speedrun HUD](changes/ELEMENTAL-DAMAGE-2.md) for the latest update and the
[main integration notes](changes/MAIN-INTEGRATION-2026-11-14.md) for included
features, defaults, build identity and focused verification.

This index separates current fork behavior from historical release evidence.
The experimental reports below retain their original timestamps, claims and
coverage limits. Their source is now included in the main integration; old
statements that main was untouched describe their original investigation dates.

## Current feature and behavior documents

- [Survival escape, Arch-vile life hunting and Mancubus frenzy retaliation fix](changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)

- [Initial monster count/percentage budgets and explicit Compatibility/Performance staging](changes/INITIAL-MONSTER-POPULATION-BUDGET.md)

- [Original monster streaming: historical first-revision limits](changes/ORIGINAL-MONSTER-STREAMING.md)

- [F4: nested Encounter Director tabs and text-overlap fix](changes/F4-DIRECTOR-NESTED-TABS.md)

- [Encounter Director: independent randomization, bounded reinforcements and item pools](changes/ENCOUNTER-DIRECTOR.md)

- [Exact solid-world sight acceleration for large monster maps](changes/SOLID-SIGHT-MESH-CACHE.md)

- [Speedrun X/Y controls and column/row layouts](changes/SPEEDRUN-LAYOUT.md)

- [HUD and Speedrun background opacity](changes/HUD-BACKGROUND-OPACITY.md)

- [Elemental Damage 2.0, colored notifications and Speedrun HUD](changes/ELEMENTAL-DAMAGE-2.md)


- [Elemental combat, charred corpses and advanced injury reactions](changes/ELEMENTAL-COMBAT.md)

- [Eye injuries: blindness, corpse recovery and Spider rage](changes/EYE-INJURIES.md)

- [Load last save after death](changes/DEATH-SAVE-RECOVERY.md)

- [HELLFORGE launcher theme and repaint fix](changes/HELLFORGE-LAUNCHER-THEME.md)

- [Enhanced monster combat](changes/ENHANCED-MONSTER-COMBAT.md)
- [Monster feedback, bleeding, and survival AI](changes/MONSTER-SURVIVAL-AI.md)
- [Reload stability and persistent lighting cache](changes/RELOAD-LIGHTING-CACHE.md)
- [F4 settings persistence](changes/F4-SETTINGS-PERSISTENCE.md)
- [Large-WAD compatibility](changes/BIG-WAD-COMPATIBILITY.md)
- [Critical-parts implementation](changes/CRITICAL-PARTS.md)

## Build and verification reports

- [Ambush sight reuse: persisted F4 Performance switch](changes/AMBUSH-SIGHT-F4.md)

- [Ambush sight reuse: actual wake transitions and MAP02 equivalence](changes/AMBUSH-SIGHT-REUSE.md)

- [SF2012 MAP02: one-shot wake behavior and measured CPU costs](changes/SF2012-MAP02-SOUND-WAKE-AUDIT.md)

- [SF2012 sight crash and restored experimental wall destruction](changes/SF2012-SIGHT-AND-DESTRUCTION-FIX.md)

- [Phase 3A sprite frustum filtering: implementation and measured results](changes/SPRITE-FRUSTUM-FILTERING.md)

- [Abandon MAP01 platform switches: conveyor footprint fix](changes/ABANDON-PLATFORM-FIX.md)

- [oku2v31 extreme stress: measured rendering bottlenecks and streaming limits](changes/OKU-EXTREME-STRESS.md)

- [MAP01 switch/conveyor monster-release fix](changes/SWARM-ACTIVATION-FIX.md)

- [Experimental swarm optimization: idle-search reuse and targeted results](changes/SWARM-OPTIMIZATION-STEP1.md)

- [Phase 2 isolated monster profiling revision](changes/MONSTER-PROFILING-PHASE2.md)

Reports retain the build number and test conditions used when they were written.
New active documents begin with a metadata block containing the HELLFORGE build,
UTC update time, status, and related test/index links.

## Historical changes

The remaining files in [changes/](changes) are dated release notes, experiments,
design notes, and compatibility investigations. Their original product names,
dates, and claims are intentionally preserved for traceability.

## Project references

- [Root README](../README.md)
- [Windows build instructions](../BUILDING.md)
- [Third-party notices](../licenses/NOTICE.txt)
