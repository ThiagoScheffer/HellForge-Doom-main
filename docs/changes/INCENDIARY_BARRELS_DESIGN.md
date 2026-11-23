# Incendiary Barrels v1 design

**F4 → Combat → Barrels → Incendiary Barrels** defaults to Off. The normal settings file and launcher catalogue use `incendiary_barrels`. Gameplay is single-player. The accelerated renderer displays the oil and fire effects.

| Mechanic | Value |
| --- | --- |
| Eligible FCAN barrel health | 30 HP |
| Initial blast | Native Doom radius argument 96, reduced by horizontal Chebyshev distance with victim radius subtracted |
| Burning contact | 3 HP every 20 tics after establishing contact |
| Residual exposure | 35 tics from last contact |
| Pool lifetime | 320 tics at the center, 290 to 314 for satellite pools |
| Spill radius | 40 units at the center, 18 for each of up to three satellites |
| Live resources | 32 pools and 1,024 exposed actors; at most 16 queued ruptures processed per tic |

A hit on an armed stock barrel goes through the common damage boundary before monster hit, pain, blood, or death handling. A nonlethal hit throws dark droplets from the struck side and leaves an inert floor spill. Later hits can refresh the spray and widen that same spill. Lethal hits mark the barrel once, record the attacker, and queue the blast. End-of-tic processing runs after the triggering traversal and defers any chain descendants to a later wave. The barrel displays the five stock BEXP frames over 24 tics and plays positional sound while its thinker stays in the action-free terminal state. A changed terminal state falls back to immediate safe removal.

Spills follow their owning sector floor. A satellite needs a same-height landing with an open floor-level path from the origin. Thirty-two radial samples constrain each irregular shape to the owning floor, and the accelerated renderer clips it again to nearby BSP floor polygons. Contact checks the actor’s sector, height, footprint, and floor-level path. A closed or blocking boundary stops propagation. Very high fliers can be hit by the initial blast and stay outside persistent floor contact thereafter.

One exposure record per actor controls cadence across overlapping pools. Native damage handles armor, skill scaling, god mode, invulnerability, monster deaths, and credit. Radius damage uses the native cyborg and Spider Mastermind concussion exception. The fire identity can request existing burn-death visuals when that option is active. Oil contact yields while an elemental fire DoT is already active. Radiation suits retain their acid protection and do not cancel oil damage. Oil itself never schedules geometry damage or spread from pool to pool.

The accelerated presentation uses cached indexed coverage masks and existing depth-tested effect batches. The oil is a low-gloss brown-black film with a small amber highlight. Low translucent flames, sparse smoke and compact sparks fade with the hazard. Its local glow is emissive, with no recurring lighting rebake or new shader bindings.

`OI01` stores pool locations and timers, one residual exposure record per affected actor, and the order of pending ruptures. Native actor records store the armed flags, health, attacker, and spent state. Source references use the native thinker order and are cleared when actors are removed. Disabling the option immediately clears the hazard and returns surviving decorations to stock health and flags. Already destroyed barrels stay destroyed. The existing `AC01` trailer and acid implementation remain independent.
