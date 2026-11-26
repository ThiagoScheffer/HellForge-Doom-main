# Advanced Gore 2.0 validation

The source and standalone regression cases are ready for validation. Build the engine and the existing `doom-giblet-foundation-tests` target, then run `ctest --test-dir <build-dir> -R "^gore-giblet-foundation$" --output-on-failure`. Rebuild the launcher to load the updated settings catalogue. The existing shaders and atlas work with the new vertex data.

| Check | Setup and expected result |
| --- | --- |
| 1. Enhanced baseline | Repeat a familiar encounter with Advanced Gore set to Enhanced. Compare its effects, ordinary wall bounces, and sounds with the previous build. |
| 2. Kinetic feedback | In Horror, enable Heavy impact reactions and disable Sticky wall fragments. Repeat the shotgun encounter. Check for a shorter onset and a stronger cosmetic recoil, with identical gameplay outcomes. |
| 3. Adhesion | Enable Flying flesh fragments and Sticky wall fragments. Watch a supported soft piece hit a solid wall. It should stay aligned to the wall, then fall once. |
| 4. Deformation | Toggle Soft fragment deformation and repeat the same encounter. Look for compression and downward sag on soft pieces. |
| 5. Materials | Compare soft pieces with bone. Bone should retain its bounce and produce no attached wall pose or blood deposit. |
| 6. Wall bounds | Try narrow walls, corners, windows, doorways, and upper and lower wall sections. Check clipping, texture orientation, the back side of the wall, and the view through openings. |
| 7. Moving and edited walls | Observe an attachment on a moving door or lift and near an excavation. It should follow its wall anchor while supported and disappear when support is lost. |
| 8. Wet deposits | In Horror, turn on Enhanced wall blood, Wet gore deposits, and Blood shine. Use the flashlight and dynamic lighting to observe fresh attachment residue dry. Check that disabling wetness leaves matte residue while unrelated surfaces retain their appearance. |
| 9. Audio | Compare Horror with Heavy impact sounds on and off for a grouped shotgun hit. Listen for the original weapon and monster sounds and one extra sound per qualifying nonfatal target, up to two per tic across all targets. Compare direct and reflected views. |
| 10. Palette | Inspect supported red and blue fragments and pale bone. Verify green tint through an existing green-blood path or the atlas test fixture where available. |
| 11. Performance | Repeat the same encounter at identical settings and view in Enhanced and Horror. Record CPU and GPU frame times separately. Use existing gore debug output to compare trace and impact counts. Attached pieces should incur anchor checks and no ballistic traces. |
| 12. Population | Compare maps with kill counts of 199 and 200. Advanced Gore should remain available below 200 and gated at 200. |
| 13. Settings | Check Classic, Enhanced, and Horror transitions, Blood and Flying flesh fragments off, and all new toggles. Verify F4 settings survive restart and Horror is accepted by the rebuilt launcher. |
| 14. Lifecycle | Pause, save and reload, switch levels, and resume. Check that paused attachments stay still, expired pieces detach once, and old attachments and audio timing do not carry into a new level. |
| 15. Atlas fallback | Exercise an existing atlas-unavailable test condition. The procedural fragment fallback should retain its normal behavior and should produce no attachment. |

Wall-aligned clipping, shader lighting, audible timing, and performance require runtime validation. The standalone fixture covers attachment state, geometry invalidation, deterministic sag, bounded callbacks, and shared budgets.
