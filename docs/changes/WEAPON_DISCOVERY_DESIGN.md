# Weapon Discovery v1 design

**F4 → Gameplay → Balance → Monster Weapon Drops** applies to future deaths. Classic is the default. In multiplayer, demo recording and playback, and Arcade, newly killed monsters use Classic drops.

| Monster | Classic | Ammo Instead | Scarce Ammo |
| --- | --- | --- | --- |
| Shotgun Guy | Shotgun | 4 shells | 2 shells |
| Chaingunner | Chaingun | 10 bullets | 5 bullets |
| Zombieman or SS | Dropped clip | Dropped clip | Dropped clip |

The death selector spawns one existing pickup actor and marks it `MF_DROPPED`. A private tag records the quantity tier before save or pickup. The tag fits in the native actor record's alignment padding. A compile-time check preserves the tracer offset. Newly spawned actors and ordinary native saves from earlier versions have zero there. The pickup checks the tag, drop flag and actor type together.

Replacement pickups use exact base amounts, native skill multipliers and the current ammo capacity. Ammo collection leaves weapon ownership and the pending weapon alone. Normal pickups still use the native ammo and weapon routines. Selecting another mode affects future drops. Existing items retain the quantity tier they spawned with.

Strict modes can make PWADs substantially harder if their weapon progression depends on loot. The mode leaves weapon discovery to map placements, existing grants or normal start rules.
