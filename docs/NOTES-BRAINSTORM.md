add doc:

We will be working on engine tweak and some ideas - to brainstorn and build codex prompt later.
this main build is a separated and modifed version of the https://github.com/tuin-boop/Tuin-Doom-Enhanced - and https://github.com/robongrui/derp
so if need to understand the base source can be used as.

use DOOM: HELLFORGE as the primary project and Tuin-Doom-Enhanced as the upstream reference whenever we need to investigate inherited engine behavior, rendering systems, or original implementations. DERP is the more updated modern version, can be used if needed.

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

FX FEATURES:

- alternative small imp fireball - new better fx - same as rocket smooth fire fx -> rocked fire effect = soft glow
- same to Cacodemon, we create a new soft glow mixed blue/red that will create a deep color energy small ball from it.
- mancubus also a new long fireball Fx that feels hot like plasma and fire. when it hit ground it looks like melted hot glass.
- arachnotron also a new plasma small energy ball green. 
-- all done--


first: we do have - Wound holes - that damage and make monsters with a permanent hole like.

- can we use the same idea, for add blood fx on monsters? the more they get hurt we can visually see blood and scratches on the body - ignore head part-&#x20;
- we can focus only on more monsters like: Barons,imps,mancubus,archville,cyberdemon,pinkdemons. - these have a visual body that can expose as skin.
- This will be a separated option that can display bloodfx on skin





BUG:
- modern hud background shadow not at same position of the current text or main headface - fixed

- 4000 monster map - not running anymore. - fixed
