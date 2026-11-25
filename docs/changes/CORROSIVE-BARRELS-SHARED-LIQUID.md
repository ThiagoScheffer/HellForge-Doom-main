# Corrosive Barrels 2.0: shared liquid effects

With Corrosive Barrels enabled, the accelerated renderer can reuse the blood liquid spray and procedural decals for yellow-green acid splashes, wall runoff, and temporary localized contamination on exposed sprite monsters. Detached droplets are cosmetic. Existing acid damage, radiation protection and hazardous pools remain authoritative and independent of the visual settings.

`acid_splatters 1` enables the additional liquid effects. `acid_monster_coating 1` enables temporary coating and droplets. Both default on but require the Corrosive Barrels master setting; disabling the master preserves their preferences. The new effects are independent of Blood and Blood Spray, and do not persist in saves.
