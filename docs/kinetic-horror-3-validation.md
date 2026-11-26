# Kinetic Horror 3.0 validation

## Build and tests

Use the configured Windows build tree. This change uses the existing engine and shader resources.

```powershell
cmake --build build --config Release --target doom doom-wounds-tests doom-giblet-foundation-tests doom-settings-navigation-tests
ctest --test-dir build -C Release -R 'wound-hit-placement|gore-giblet-foundation|gore-giblet-art|f4-director-navigation' --output-on-failure
python tools/pack_giblets.py --check
```

CMake registers the F4 test when `DOOM_COMPAT_TEST_IWAD` is configured. The wound test executable also accepts a Doom II IWAD path for its stock-art cases.

## Manual gameplay checks

Use the same IWAD, map, renderer settings, camera and encounter for paired runs. `-goredebug` reports per-discharge and per-explosion admission. `-ragdolllog` reports card reconstruction.

| Scenario | Check |
| --- | --- |
| Wounds Off, Classic and Deep | Check that Classic still exposes the background at the original openings, Deep keeps the same silhouette and shaded opaque interiors, and Off hides ordinary wounds. Compare mirrored and animated frames, procedural falls, shadows, flashlight, and an opaque sprite in front of a high-contrast wall. |
| Overlapping shots and saw wounds | Check that stains do not replace stronger marks, saw marks retain their appearance, and intentional openings in verified art stay transparent. |
| Zombieman, Shotgun Guy, Imp | Check featured boot, head and demon fragments in supported bursts. Compare flying, wall-attached and resting poses. |
| Revenant, Mancubus, Pinky, Cacodemon | Check skeletal, heavy and soft artwork on matching materials, including Cacodemon tint and unchanged attachment and impact behavior. |
| Verified Shotgun Guy death | Trigger an existing eligible head outcome in Horror. Toggle Fatal corpse appearance while inspecting the same corpse. The existing opening stays transparent and the shaded rim switches. Verify replacement death sprites fall back, and check resurrection and level reset. |
| Physical Bodies | Repeat with Legacy, Natural and Cinematic. Verified ordinary corpse masks should remain on the ordinary sprite path. Check support placement, mirrors and the limit of three ordinary plus two reserved urgent builds per tic. |
| Concurrent hits and explosions | Check one featured piece per admitted burst at most, existing shared explosion caps, and unchanged impact audio and wall-deposit counts. Compare Recognizable giblets on and off. |
| Settings and lifecycle | Switch Horror to Enhanced, Deep to Classic and Off, and toggle blood and both new options. Check actor removal, level changes, demo transitions and maps at the 200-monster protection threshold. |
| Performance | Record CPU and GPU frame times, live fragment count, stationary count and card builds on the same encounter. |

## Coverage

The featured silhouettes are original low-resolution artwork. The atlas grew from 256×114 to 256×129. The same three RG8 tint bands gain 23,040 bytes of pixel data, before driver allocation overhead. All 32 earlier frame records retain their indices and placement.

The new persistent corpse variant uses the existing fingerprinted Shotgun Guy death sequence. Existing intentional silhouette cuts retain their openings. Explosion victim snapshots supply fragment presentation, and persistent corpse variations require verified artwork on the discharge path. Compatible pain-frame relief masks and Lost Soul remains are deferred. Hell Knights, Barons and Arch-viles retain their existing cosmetic presentation.
