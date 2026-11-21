# Weapon Discovery v1 test report

- **Windows x64 Release** passed for the Doom engine and headless test platform. No shader changes were required.
- **Native survival integration** passed against DOOM II and the encounter fixture. It verified all three modes, exact quantities, skill scaling, full ammo, backpack capacity, weapon ownership and switching, map shotgun acquisition alongside enemy ammo, mode changes, and save/load of both scarce quantities. It also verified one drop after repeated lethal calls, weakpoint, barrel, Arch-vile fire, acid, infighting and telefrag deaths, resurrection followed by a second death, map ammo respawn, level reset, and Classic fallback under demo, co-op and deathmatch flags.
- **F4 navigation and persistence** passed using the live menu handlers and the actual config reader and writer. Classic, Ammo Instead and Scarce Ammo are selectable. The applied value round-trips and an invalid setting is ignored.
- **Registered CTest suite** passed 38 of 39 in the build directory. Portability could not open the existing `portability.cfg` for writing there. Its rebuilt executable passed in a fresh writable directory, including its DeHackEd checks.

The session fallback checks used native engine flags. Live multiplayer, demo playback streams with this option selected, and Arcade were not separately exercised. Custom PWAD progression can be harder in strict modes. Replacement quantities survive saves in the updated build. Older builds read the same record layout and treat tagged items as ordinary pickups.
