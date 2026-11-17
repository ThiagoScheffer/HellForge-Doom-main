// Standalone conservative body-region color filters for stock DOOM sprites.
// No changes to health, AI, actor state, original alpha, or original artwork.
#ifndef HELLFORGE_MONSTER_SKIN_REGIONS_H
#define HELLFORGE_MONSTER_SKIN_REGIONS_H
#include <cstdint>
namespace doom_skin_regions {
enum class Kind : unsigned char { None, Mancubus, Archvile, Cyberdemon, Cacodemon, HellKnight, Zombieman };
inline bool permit(Kind k,int x,int y,int w,int h,uint8_t index,int r,int g,int b,bool rear=false) {
    if(k==Kind::None||w<=0||h<=0||x<0||y<0||x>=w||y>=h)return false;
    const float u=(x+0.5f)/w,v=(y+0.5f)/h;
    // Exclude common desaturated/metallic palette bands; these are only
    // used with fingerprint-confirmed stock artwork.
    const bool greyRamp=index>=80&&index<112;
    const bool fleshRamp=(index>=16&&index<80)||(index>=128&&index<148);
    const bool warm=r>=30&&r*100>=g*93&&r*100>=b*107;
    const bool redFlesh=r>=32&&r*100>=g*119&&r*100>=b*118;
    switch(k) {
    case Kind::Mancubus:
        // Belly and fleshy arms, not upper face or grey cannon barrels.
        return v>=0.34f&&v<=0.85f&&
               u>=(v<0.60f?0.26f:0.18f)&&u<=(v<0.60f?0.74f:0.82f)&&
               !greyRamp&&warm&&r>=42&& (fleshRamp||r*100>=g*111);
    case Kind::Archvile:
        // Pale torso and limbs, excluding the head and bright attack flames.
        return v>=0.39f&&v<=0.89f&&u>=0.19f&&u<=0.81f&&
               !greyRamp&&warm&&g*100>=b*75&&
               !(r>210&&r*100>g*150);
    case Kind::Cyberdemon:
        // Central exposed organic patches; hard rejection of grey metal.
        return v>=0.35f&&v<=0.83f&&u>=0.28f&&u<=0.70f&&
               !greyRamp&&fleshRamp&&redFlesh;
    case Kind::Cacodemon:
        // A Caco is effectively a single large head: mark the orb's outer
        // flanks only, keeping the central eye, face, mouth and top horns clean.
        // Authored A5/B5/... frames show the rear: central orb is safe there.
        return v>=0.28f&&v<=0.82f&&u>=0.12f&&u<=0.88f&&
               (rear||(u<=0.32f||u>=0.68f))&&!greyRamp&&redFlesh;
    case Kind::HellKnight:
        // Beige/tan body rather than Baron-red skin. Horns/head are excluded
        // by both anatomical WP_BODY and a conservative vertical cutoff.
        return v>=0.33f&&v<=0.88f&&u>=0.13f&&u<=0.87f&&
               !greyRamp&&warm&&(fleshRamp||r*100>=g*105);
    case Kind::Zombieman:
        // Torso/uniform and exposed arms; no head, face or muzzle. This is
        // exclusively applied after verification of the original POSS art.
        return v>=0.44f&&v<=0.88f&&u>=0.14f&&u<=0.86f&&
               !(v<0.73f&&u>0.40f&&u<0.60f)&&!greyRamp&&
               r>=25&&g>=18&&b<r*1.22f&&
               !(r>190&&r*100>g*165);
    default:return false;
    }
}
} // namespace doom_skin_regions
#endif
