# Medical Supply Recovery

The optional Gameplay / Balance setting preserves unused Medikit healing on the ground as a small recovery pack. Mode 2 also preserves Stimpack healing. Each pack holds its exact remaining health, can be collected in multiple visits, and survives save/load without creating extra healing. The default is Classic. Recovery is disabled in multiplayer, LAN, deathmatch, demos and Arcade; the preference remains saved.

In `graphics.cfg`, `medikit_overflow 0` selects Classic, `1` selects Medikits only, and `2` selects Medikits and Stimpacks. Modified medical actor definitions that cannot be safely transformed retain classic pickup behavior. Older executables may not understand recovery capacities in newer saves.
