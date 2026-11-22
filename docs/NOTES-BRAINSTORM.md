add doc:

I'll retrieve only the files needed for each subsystem.
No GitHub connection, authorization or commits. We'll work locally with copies of source files and produce a ZIP for you to apply manually.


Inspect their complete relevant implementations and dependencies.



Develop changes against the actual HELLFORGE source.



Prepare a ZIP containing modified/new files and application instructions.



Include test results and anything requiring your Windows runtime verification.

We will be working on engine tweak and some ideas - to brainstorn and build codex prompt later.
this main build is a separated and modifed version of the https://github.com/tuin-boop/Tuin-Doom-Enhanced - and https://github.com/robongrui/derp
so if need to understand the base source can be used as.

use DOOM: HELLFORGE https://github.com/ThiagoScheffer/HellForge-Doom-main as the primary project and Tuin-Doom-Enhanced as the upstream reference whenever we need to investigate inherited engine behavior, rendering systems, or original implementations. DERP is the more updated modern version, can be used if needed.

It establishes the following working context:
- Current build: 2026.11.15.6
- Platform: Windows x64, C/C++/C#, SDL3, Vulkan.
- Engine foundation: DERP renderer → Tuin-Doom-Enhanced → your independently modified HELLFORGE fork.
- Existing extensions: sprite-based combat, monster weak points, survival AI, elemental damage, destruction, large-WAD compatibility, lighting caches, weapon systems, and additional HUD functionality.
- Compatibility priorities: classic DOOM/Boom WADs, save/load integrity, performance, and preserving established engine behavior.
How we'll approach the work
We'll separate the process into three stages:
1. Brainstorm and evaluate: Discuss each idea, its gameplay or technical benefits, implementation feasibility, likely performance costs, and potential conflicts with existing systems.
2. Design the implementation: Identify the relevant engine subsystems, dependencies, edge cases, compatibility constraints, and the best technical approach.
3. Build a Codex prompt: Turn the agreed design into a precise engineering task with implementation requirements, guardrails, testing procedures, and acceptance criteria.
An important distinction: The upstream repository is reference material, not necessarily an accurate representation of HELLFORGE's current source. We should avoid assuming its implementation is unchanged in your fork.

For each idea, I'll also distinguish between what is documented, what we can verify from source code, and what remains a technical hypothesis.

We don't need to produce Codex prompts immediately. We can explore ideas, challenge assumptions, compare alternatives, and refine the design before committing to an implementation.

Ready for the first engine tweak or idea.



--------------------------------------------------------

Understood. I'll use this as the working technical baseline for all upcoming DOOM: HELLFORGE engine discussions.

I've also received and reviewed the uploaded `README.md`. It establishes the documented feature set and build baseline for **HELLFORGE 2026.11.15.6**.&#x20;

README(10).md



## **Project hierarchy and source authority**

We'll follow this order:

PRIMARY — source of truth

**DOOM: HELLFORGE**

Your independently modified project. All implementation decisions, compatibility requirements and existing behavior must be evaluated against this codebase.

[HELLFORGE GitHub repository](https://github.com/ThiagoScheffer/HellForge-Doom-main?utm_source=chatgpt.com)

UPSTREAM REFERENCE

**Tuin-Doom-Enhanced**

Reference for inherited engine behavior, original implementations, rendering architecture and historical modifications.

[Tuin-Doom-Enhanced repository](https://github.com/tuin-boop/Tuin-Doom-Enhanced?utm_source=chatgpt.com)

SECONDARY REFERENCE

**DERP**

Reference for more modern renderer implementations, architectural improvements, optimizations and alternative technical approaches.

[DERP repository](https://github.com/robongrui/derp?utm_source=chatgpt.com)

**Important:** HELLFORGE is always authoritative. An upstream implementation is not evidence that the same implementation exists in your fork.

## **How we'll evaluate each idea**

I'll use the following engineering approach:

| Stage                      | Objective                                                                           |
| -------------------------- | ----------------------------------------------------------------------------------- |
| 1. Brainstorm              | Explore possibilities, benefits, limitations and alternatives.                      |
| 2. Technical investigation | Identify actual source files, functions, dependencies and existing behavior.        |
| 3. Architecture            | Design the implementation and account for compatibility, performance and lifecycle. |
| 4. Risk assessment         | Identify regressions, edge cases, save/load implications and rendering conflicts.   |
| 5. Codex prompt            | Produce a precise implementation task with guardrails and acceptance criteria.      |

## **Engineering principles**

These will be our standing priorities:

1. **Preserve classic DOOM/Boom compatibility.** Avoid breaking existing WADs and established game mechanics.
2. **Protect existing HELLFORGE features.** New functionality must coexist with destruction, lighting, sprite combat, survival AI, weapons and HUD systems.
3. **Performance matters.** Avoid unnecessary allocations, repeated expensive calculations, unbounded caches and excessive per-tick processing.
4. **Save/load integrity.** New persistent gameplay state must have a deliberate serialization strategy.
5. **Rendering correctness.** Respect SDL3/Vulkan architecture, resource lifetimes, cache invalidation and shader dependencies.
6. **Minimal invasive modifications.** Prefer targeted changes over unnecessarily rewriting established engine systems.
7. **Verify the actual source.** Never assume that an upstream function, structure or subsystem is identical in HELLFORGE.

I'll also distinguish three evidence levels throughout our discussions:

- **Verified:** Confirmed directly in the HELLFORGE source.
- **Documented:** Described in project documentation but not yet independently verified in source.
- **Hypothesis:** A proposed technical explanation or implementation that still needs investigation.

**We're ready.**

We can explore ambitious engine changes, rendering improvements, monster behavior, AI, physics, environmental effects, optimization, gameplay systems or entirely new mechanics.

No need to rush into implementation. We'll challenge ideas, examine alternatives and refine the technical design before writing a Codex prompt.

**HELLFORGE is the project. The upstream repositories are references, not restrictions.**





w98_1  to  w101_2 + w103_1  w103_2 -- a skin leather wall with a moldure of iron - we can create a breathing effect on this one, to make the skins feels it is alive - same effect of a bed sheet with handes pressing it creating a depth and fear of inside alive.

w102_1 and w102_2 - these same skin leather walls but have some skin rupture and blood going out from it on the red parts of it.
 
 
 
wall25_1 - wall with many melted faces and torture souls morph effect and faces eyes moving and mouth open and closing.
 
 
 
 mwall4_1 mwall4_2 mwall5_1 - marble rock walls with demons simbols and engraved faces - we can give a soft moving effect on it, to make like it is looking at the player.
 
 
 hell5_1 hell5_2 - wall of alive flesh with skins and bone skelletons like, give  similiar spinewall effect to make it feels its moving and alive.
 
 bodies - many skulls glued together we can add a bleeding effect like its alive and driping blood from the red skulls.
 
 
 
 nex:
 body_1  body_2 - wall with a dead body half smashed, only upper part torax,arms and head are nailed to the wall 
 give a immersion effect as it still alived an with torax moving, and wall blood part still bleeding.

wall50_1 wall50_2 wall51_2 wall51_3 - wall marble with a bodies hanged or chained on it - give immersion as the bodies are convulsing and half alive moving trying to scape.



teleports - from gate1 to  gate4 textures - give a moving and pulsating effect as a energy and evil place  to improve the teleports immersion.


 from rp2_1 to  rp2_4 - wall textures looks like pipes flowing blood down, give its blood textures (the red parts) a realism immersion as it is bleeding and flowing down.
 
 wall58_1 wall58_2 wall58_3 - marble wall with a bleeding like monster face and its mouth flowing blood similar to the pile walls effect of blood.
 
 Skies rework: improve immersion and depth of the skies
 
 rsky1 - dark brown dawn sky - make the sky feeliz alive with moving clouds,windy effect and depth like a long and far horizon.
 
 rsky2 - burning city sky - give death to the layers as a real city background burning on fire, fire and smoke to give a detph and immersion of caos.
 
 rsky3 - montains ice cold and a red sky with fire and blood faces skelletons  similiar to the hell5_1 textures.
 give depth to the montains and the red sky to make it looks like parallax distant and alive.
 
 
 sw2_5 sw2_6 - Eye switch: make it looks alive and blinking and looking around the eye to creppy immersion
 
 
 
 w65b_1 w65b_2 - a fire and blue like plasma - create depth and immersion as it the wall is pulsing and hot the blue plasma is melting.
 
 from (w73a_1 to w74b_1 + also this  from wall22_1 to wall23_2) textures of a fire like wall - create a indepth feeling of a wall that is alive and burning, as a dense fire is consuming it infite fire effect.
 
 
 
 
 
 
 next improvements:
 Barrels - we had implemented the acid barrels new fx and damage
 now need to check how to improve and make the other type of barrels on fire - to be also explode and spread oil and fire around it.
 
 
 need fix : arch vile when bleeding or severe damaged it will run away or hunt to steal life from other monster - bug is the moviment direction is wrong - sometimes its even walking backwards. fix moviment direction to match where it want to go. check cacodemon also for direction bug.
 
 
 new options for balance game: avoid monster with common weapons to drop tis weapon, but make it drop ammo when it dies. idea is to make the level weapons worth discovery.
 
 
 full rework on Menu F4 - to better organize and recreate correct new function inside Tabs and well organized - idea to make easy to player find and change settings,similar to Encounter director we have sub tabs well organized categories.
 


need to fix the critical blood FX - still looks horrible like a blood jet. it should be some detail that make it easy to see its critical but not looks like a 3 jet spread.


tree on fire from Inferno: use the same fire effect but bigger as the falling fireball temporary ground fires - repalce current sprit look bad one with the ground fires  effect which is good to use.

well kill a revenants - a ghou soul effect like will leave the body


better Hud: improve the speedrun to allow add secrets,itens, health,armor,ammo - to allow a simple customized hud and clean screen.
user can create 2 dif columms that can be separated on screen sides.
so map related info can be adjusted on left side and player info can be on right, or use the already one row mode with player can add what he want to be showed.
option to disable all other huds, and use only new Speedrun hud. so it save memory and keep it clean.



----------
new reoworked Launcher 
now with a separated Tab for the Add-on loadout and Maps wads, so user can easy add its only addons, but can easily change differents maps.

Add a new compatible monster on the same place of archvile for normal Doom.wad add a Baron of hell. so its archville when the wad is doom2.wad and baron of hell when its original dooms wads. like doom or final doom.

----------



fix: Sever critical - should be only displayed when player hit - not by in monster fighting


improve map: refine details and give doors key colors lines so player can see where the doors is, add secrete sectors dif color. when found.
if user gets a Fullautomap - key icon locations shows on the maps ,as square color match keys so player can see where the keys are easly.



Inferno,Hell sky moon - change the sun on Hell skies maps like environment to a Huge Ecplise moon red - similiar to Berserker Bloodmoon - check reference for the correct idea to give  dark immersion on hell enviroment


Check if Foggy deimous like atmosphere can also hide the decorations to affect and make it more immersion - need img ref-



https://doom.fandom.com/wiki/Thing_types