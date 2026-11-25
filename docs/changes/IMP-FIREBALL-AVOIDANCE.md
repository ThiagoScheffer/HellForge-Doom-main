# Imp Fireball Avoidance

The optional `imp_fireball_avoidance` setting defaults to `0` (off). Enable it
under Monsters / AI in F4 or set it to `1` in `graphics.cfg`. In supported
single-player sessions, an Imp avoids starting or releasing a fireball when a
living Imp is predicted to intercept it first. Melee, infighting, projectiles
already in flight, and the normal chase logic are unchanged. Network games,
demos, and Arcade temporarily disable the policy without changing the setting.

The read-only query models the spawn half-step and subsequent discrete movement,
using the projectile's configured dimensions. It examines at most 96 blockmap
cells and 256 actors; incomplete queries allow the attack. It also allows
attacks when prediction is uncertain, including invisible targets, modified
attack states or projectile movement, unusual aim offsets, species infighting,
and weakpoint frenzy or retaliation. Normal Imp attacks remain eligible with
Advanced Tactics, Aggressive AI, and Persistent Pursuit enabled.

This is a gameplay option; performance benefits have not been benchmarked.
