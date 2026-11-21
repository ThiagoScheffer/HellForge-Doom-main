# Selected Texture Immersion v1 design

The production expansion supersedes the initial settings and profile list below. The approved spine motion now runs through the normal master switch. The expansion removes the cavity control and its contrast samples. Stock relief still provides the base material shading.

| Stock surface | Production motion |
| --- | --- |
| `SKSPINE1`, `SKSPINE2`, `SKINFACE` | Offset contraction waves across the original ribs, bone, and flesh, with red seepage and wet highlights. |
| `SKINMET1`, `SKINMET2`, `SKINSCAB`, `SKINSYMB` | Pressure from hand-like shapes rises behind the dry leather. The iron bands remain fixed. |
| `SKINCUT` | Pressure in the leather and downward seepage confined to the red tears. |
| `SP_FACE1` | Localized eye movement and changes to mouth shape on five face landmarks. |
| `MARBFACE`, `MARBFAC2`, `MARBFAC3` | Small changes inside the eyes based on the viewer's angle to the wall. |
| `SP_FACE2` | Seepage across the red parts of the skull mass. |

The effects use WAD pixels and the current texture filter. The two stock IWADs have no `W100_1` or `W100_2` patches. Exact-name support covers those names if an add-on supplies them. The feature uses the same GPU bindings and allows one additional artwork sample for animated pixels. No replacement textures are needed.

## Initial v1 design

The renderer adds cavity depth, subtle motion and material highlights to six named pieces of WAD artwork. The effect is opt-in and uses the existing indexed textures, relief, and world lighting.

| Selected artwork | Stock surface | Profile |
| --- | --- | --- |
| `HELL5_1` | Left patch of `SKINFACE` | Fused flesh with a soft wet finish and a restrained red pulse |
| `SPINE3_1` | Left patch of `SKSPINE2` | Stronger bone relief and small contractions in the red tissue |
| `BODIES` | Patch in Doom II `SP_FACE2` and `AASHITTY` | Deeper skull cavities and uneven movement |
| `SFLR6_4` | A flat used for floors and ceilings | Directional contraction and the strongest wet response |
| `W98_1` | Left patch of `SKINMET1` | Matte, creased skin with faint shading movement |
| `WALL25_1` | Patch in `SP_FACE1` | Facial contour depth and subtle tension from animated relief |

The image loader compares the six exact names after normalizing the eight-character WAD names to uppercase. It checks wall texture names and their actual patch constituents. Where the wall contains one selected patch, it keeps that patch's clipped rectangle and checks for overlaps before enabling the profile. The stock Doom and Doom II IWADs resolve `SFLR6_4` as a flat. A custom wall definition with that exact name can also use its profile. Image metadata is assigned during loading and reused on every draw.

Two nearby indexed samples estimate local contrast. A pixel darkens when nearby pixels are brighter and the relief indicates a crease. Coefficients bound the amount of darkening. The existing relief gradients also increase the apparent depth of ridges under the scene's lights.

The motion mask combines midrange luminance, chroma, existing relief slope, and local roughness. Bright, high-contrast contours get less movement, which helps keep bone and framing stable. The dry skin and faces use small changes in shading and normals with static image coordinates. The wet profiles can sample the artwork at a sub-texel offset under smooth or sharp bilinear filtering. They blend RGB using the tissue mask and the moved sample's coverage, while the main sample continues to determine opacity and depth. The displacement components peak below 0.63 texel at High. A texture-space wave with whole cycles makes the tentacle motion continuous across repeating tiles. World position and the existing level clock give neighboring regions different phases.

At the selected patch's partial edges, the effect fades inward to protect adjoining artwork from both motion and neighborhood sampling. The shader also fades out with distance and texel footprint. Fully disabled or unselected surfaces skip the extra reads and trigonometry. A nearby selected pixel uses one relief fetch, up to two cavity samples, and at most one morph sample, with the existing filter determining the cost of each artwork sample. It uses the existing world and reflection passes.

Region masks are heuristic. Pale faces, bone, flesh, and trim can share palette ramps. Fine anatomical details are preserved by conservative motion amplitudes, with no feature tracking. The gloss depends on scene lighting. Ambiguous overlapping or multi-selection composites, translucent wall variants, liquid-average surfaces, and crossfades between different regions retain their original shading. The classic software renderer uses its original artwork path.
