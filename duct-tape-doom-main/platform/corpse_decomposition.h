/* HELLFORGE: cosmetic corpse drying, independent from gameplay RNG and IWAD art.
   Algorithms are standalone C++17 and operate on the original indexed pixels.
   No actor handles, WAD assets, or resource ownership live in this header. */
#ifndef HELLFORGE_CORPSE_DECOMPOSITION_H
#define HELLFORGE_CORPSE_DECOMPOSITION_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace doom_decay {
constexpr unsigned monsterLimit=200;
constexpr unsigned trackedLimit=199;
constexpr std::size_t textureBudget=8u*1024u*1024u; // estimated CPU+GPU image storage

inline int delayMinutes(int minutes) {
    return minutes==2||minutes==5||minutes==8||minutes==10?minutes:5;
}
// Stage 1 starts exactly after the chosen delay. Stages 2/3 follow at +45/+120 s.
inline int stage(int ageTics,int minutes,int ticrate) {
    if(ticrate<=0 || ageTics<0)return 0;
    const std::int64_t elapsed=ageTics;
    const std::int64_t start=std::int64_t(delayMinutes(minutes))*60*ticrate;
    if(elapsed<start)return 0;
    if(elapsed<start+std::int64_t(45)*ticrate)return 1;
    if(elapsed<start+std::int64_t(120)*ticrate)return 2;
    return 3;
}
inline std::uint32_t mix32(std::uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
    return x;
}
inline int closest(const unsigned char *palette,float red,float green,float blue) {
    int best=0;float error=1.e30f;
    for(int j=0;j<256;++j) {
        float r=palette[3*j]-red,g=palette[3*j+1]-green,b=palette[3*j+2]-blue;
        float d=2*r*r+4*g*g+3*b*b;
        if(d<error){error=d;best=j;}
    }
    return best;
}
// Palette families: 0 humanoids, 1 Imp, 2 Pinky, 3 heavy red flesh
// (Mancubus/Baron/Hell Knight), 4 Cacodemon blue-grey tissue.
// Only palette indices change. Alpha is copied byte-for-byte from the WAD.
inline bool paint(std::vector<unsigned char> &out,const std::vector<unsigned char> &original,
                  int width,int height,const unsigned char *palette,int tier,int family,std::uint32_t seed) {
    if(!palette||tier<1||tier>3||width<1||height<1||width>256||height>256||
       original.size()!=std::size_t(width)*std::size_t(height)*2)return false;
    // Stages must remain visibly distinguishable on low-saturation WAD sprites.
    // Preserve the original PLAYPAL indices/coverage pipeline and ink outlines.
    const float strength=tier==1?.46f:tier==2?.72f:.88f;
    const float darkening=tier==1?.90f:tier==2?.76f:.65f;
    // Two additional palette maps produce local dry crust and fissure accents.
    std::array<std::array<unsigned char,256>,3> lookup{};
    for(int i=0;i<256;++i) {
        const float r=palette[i*3],g=palette[i*3+1],b=palette[i*3+2];
        float l=0.299f*r+0.587f*g+0.114f*b;
        float tr=0,tg=0,tb=0;
        if(family==1){tr=l*.74f+31;tg=l*.62f+27;tb=l*.44f+23;} // ochre Imp
        else if(family==2){tr=l*.70f+30;tg=l*.63f+28;tb=l*.56f+27;} // faded Pinky flesh
        else if(family==3){tr=l*.71f+26;tg=l*.47f+20;tb=l*.43f+18;} // dried heavy flesh
        else if(family==4){tr=l*.45f+24;tg=l*.56f+30;tb=l*.69f+39;} // dried blue Cacodemon
        else {tr=l*.72f+30;tg=l*.70f+29;tb=l*.62f+28;} // dusty uniforms
        // Keep ink-black outlines recognizable and avoid crushing all sprite detail.
        const float s=l<22.f?strength*.24f:strength;
        const float rr=(r*(1-s)+tr*s)*darkening;
        const float gg=(g*(1-s)+tg*s)*darkening;
        const float bb=(b*(1-s)+tb*s)*darkening;
        for(int variation=0;variation<3;++variation) {
            float factor=variation==0?1.f:variation==1?.83f:.69f;
            lookup[variation][i]=(unsigned char)closest(palette,
                std::clamp(rr*factor,0.f,255.f),
                std::clamp(gg*factor,0.f,255.f),
                std::clamp(bb*factor,0.f,255.f));
        }
    }
    out=original;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const std::size_t pos=std::size_t(y)*width+x;
        if(!original[pos*2+1])continue;
        // Low-frequency deterministic mottling, with sparse fine dried cracks.
        const std::uint32_t patch=mix32(seed ^ std::uint32_t(x/5)*0x9e3779b9u ^
                                         std::uint32_t(y/4)*0x85ebca6bu);
        const std::uint32_t fine=mix32(seed ^ std::uint32_t(x)*0xc2b2ae35u ^
                                        std::uint32_t(y)*0x27d4eb2fu);
        int variation=(patch%100u < unsigned(9+tier*5))?1:0;
        if(tier>=2 && fine%100u<unsigned(1+tier) && (patch&7u)<5u)variation=2;
        out[pos*2]=lookup[variation][original[pos*2]];
    }
    return true;
}
} // namespace doom_decay
#endif
