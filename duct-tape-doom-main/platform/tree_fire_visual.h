#ifndef HELLFORGE_TREE_FIRE_VISUAL_H
#define HELLFORGE_TREE_FIRE_VISUAL_H

#include "lightning_interactions.h"

namespace doom_strike::tree_fire {
constexpr int outerLayers=6,innerLayers=4,coreLayers=2,smokeLayers=2;
constexpr int texturedQuads=outerLayers+innerLayers+coreLayers+smokeLayers;
static_assert(texturedQuads==flameStrands*2);
constexpr int masks=6,maskWidth=36,maskHeight=56;
constexpr float turn=6.2831853f;

inline float smooth(float x) {
    x=std::clamp(x,0.f,1.f);
    return x*x*(3.f-2.f*x);
}

// Original alpha silhouettes. Some profiles split into a shorter fork.
// Transparent border pixels also serve as padding between atlas tiles.
inline float maskAlpha(int tile,int x,int y) {
    if(x<=0||x>=maskWidth-1||y<=0||y>=maskHeight-1)return 0.f;
    const float u=2.f*x/(maskWidth-1)-1.f;
    const float v=1.f-float(y)/(maskHeight-1);
    const float phase=tile*1.73f;
    const float center=v*(.23f*std::sin(phase+v*7.4f)+.14f*std::sin(phase*.6f+v*16.f));
    const float width=(.74f*(1.f-v)+.085f)*
        (.87f+.09f*std::sin(v*23.f+phase)+.09f*std::sin(v*40.f-phase));
    const float edge=width-std::abs(u-center);
    const float base=smooth(v/.12f);
    const float body=smooth((edge+.048f)/.145f)*smooth((.99f-v)/.19f);
    const float forkHeight=.64f+.045f*tile;
    const float forkV=v/forkHeight;
    const float forkCenter=center+(tile%2?-1.f:1.f)*(.10f+.45f*forkV);
    const float fork=smooth((.21f*(1.f-forkV)-std::abs(u-forkCenter)+.04f)/.115f)*
        smooth((1.f-forkV)/.25f)*smooth((v-.16f)/.24f);
    return base*std::max(body,fork);
}

struct Tongue {
    float x,y,z,width,height,bendX,bendY,alpha;
    int mask;
};
// Every layer stays rooted in the same part of the tree. Independently
// phased growth and shear carry the bright shapes upward without a reset.
inline Tongue tongue(int layer,float seconds,float height,bool small,std::uint32_t seed) {
    const std::uint32_t key=hash(seed+std::uint32_t(layer)*0x9e3779b9u);
    const float phase=turn*unit(key);
    const float angle=layer*(turn/outerLayers)+turn*unit(seed^0x0ae36du)+
        .21f*std::sin(seconds*1.47f+phase);
    const float radius=(small?4.9f:6.7f)+(small?2.6f:4.6f)*unit(key^0x1834u);
    const float wave=seconds*(5.1f+4.7f*unit(key^0x1425u))+phase;
    const float pulse=.64f+.22f*std::sin(wave)+.14f*std::sin(wave*.63f+phase);
    const float branch=layer<3?.03f:(.20f+.16f*unit(key^0x1386u));
    const float length=(layer<3?.71f:.63f)*height*(.62f+.49f*pulse);
    return {std::cos(angle)*radius,std::sin(angle)*radius,1.f+height*branch,
            (small?6.8f:10.4f)*(0.77f+.46f*unit(key^0x1727u)),length,
            (2.f+height*.10f)*std::sin(wave*.48f+phase),
            (2.f+height*.09f)*std::cos(wave*.39f-phase),
            .30f+.44f*pulse,int(key%masks)};
}
} // namespace doom_strike::tree_fire

#endif
