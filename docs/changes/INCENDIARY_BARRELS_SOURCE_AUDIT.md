# Incendiary Barrels source audit

The starting revision is `75d5121e0a5b38bf3390ffe5959b9da6e8c8b764` on `master`, matching the local `origin/master` tracking ref. The working tree already contained a modified SDL Android Gradle lock file. The work is local.

- DoomEd 70 maps to `MT_MISC77`. Its stock definition has 1000 HP, `MF_SOLID`, a 16-unit radius and height, and three fullbright FCAN frames that loop every four tics. It has no pain, death, extreme death, or raise state. Native hitscan, missiles and radius damage require a shootable target. The stock definition provides no path to a safe `P_KillMobj` transition.
- DoomEd 2035 maps to `MT_BARREL`, which has its own BEXP states and runs the existing acid and breakable-environment hooks in `A_Explode`. The acid gameplay, renderer and AC01 extension have distinct entry points.
- DeHackEd can change actor definitions and FCAN states. A PWAD can replace artwork without changing actor definitions. The feature checks the underlying behavior before modifying an instance and accepts replacement art using the original stock actor and state definitions.
- The native radius pass uses shared traversal variables. Its single argument governs both blast reach and damage falloff. A rupture triggered during a radius, hitscan, or BFG traversal needs to defer its blast until the traversal finishes.
- The elemental subsystem distinguishes fire damage, optional status damage, and visual feedback. Oil uses the native damage boundary and its own floor-contact lifetime. It can yield to an already active elemental fire status.
- Native saves preserve actor health, flags, and indexed actor references. The trailer stack consumes optional tagged records from the end. Oil can add its own record without rearranging existing trailers or changing the actor record layout.
- The accelerated renderer already supports depth-tested soft effects and floor polygon clipping. Oil can use these paths with cached procedural textures and without changing GPU shader bindings.
