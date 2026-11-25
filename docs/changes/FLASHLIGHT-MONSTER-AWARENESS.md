# Flashlight Monster Awareness

The optional **Flashlight Alerts Monsters** setting lets an idle monster detect the player when a direct flashlight beam illuminates it, including from behind. It uses the existing alert and pursuit behavior and respects normal sight and No Target. It defaults off and is available in supported single-player accelerated gameplay; multiplayer, demos, Arcade and detached cameras do not use it.

`flashlight_monster_awareness 0` disables the feature; set it to `1` to enable it. Classic and Modern profiles use the existing beam geometry. The simulation uses the player's position and look rather than visual-only corner peeking. In solid-map mode an obstructed shoulder origin is conservatively rejected.
