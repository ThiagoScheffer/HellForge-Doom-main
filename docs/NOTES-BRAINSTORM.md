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
