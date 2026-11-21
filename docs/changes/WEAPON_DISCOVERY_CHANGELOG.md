# Weapon Discovery v1 changelog

- `linuxdoom-1.10/p_drop_policy.h` defines the three modes. `p_inter.c` selects death drops and awards replacement ammo through the shared ammo limits and skill rules.
- `linuxdoom-1.10/p_mobj.h` and `p_saveg.c` store the tier in existing actor alignment padding and preserve the native save layout. Earlier builds can read the record layout but interpret replacement pickups as ordinary ammo.
- `platform/scene3d.h`, `platform/settings_panel.cpp`, and `platform/scene3d/scene.cpp` expose, persist and apply `monster_weapon_drops`. Missing settings use Classic.
- `tests/survival_update.cpp` and `tests/settings_navigation.cpp` cover native drops and the F4 setting.
