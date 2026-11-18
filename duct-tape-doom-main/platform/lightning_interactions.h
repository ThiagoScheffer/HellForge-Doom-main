/* HELLFORGE D3 — optional cosmetic lightning on original burnt-tree sprites.
   Selection, pulse, and ember timing are independent of Doom gameplay RNG.
   This header intentionally has no Doom engine or rendering dependencies. */
#ifndef HELLFORGE_LIGHTNING_INTERACTIONS_H
#define HELLFORGE_LIGHTNING_INTERACTIONS_H
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace doom_strike {
constexpr float maxTargetDistance=800.f;
constexpr int boltSegments=28;
constexpr int branchSegments=7;
constexpr int emberCount=18;
constexpr int emberLifetimeTics=18;
inline std::uint32_t hash(std::uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
inline float unit(std::uint32_t x) {return float(hash(x)&0xffffffu)/16777216.f;}
// Vanilla Doom's big dead/burnt tree: MT_MISC76 / SPR_TRE2 (thing 54).
// Caller provides the actor's type and *actual displayed* sprite identity.
inline bool eligibleTree(int type,int sprite,int treeType,int treeSprite,
                         bool sky,float distance,float headroom) {
    return type==treeType&&sprite==treeSprite&&sky&&
           distance>=24.f&&distance<=maxTargetDistance&&headroom>=56.f;
}
inline bool selectEvent(std::uint32_t seed) {return unit(seed^0x66c2d31bu)<.72f;}
inline float impactFade(int age) {
    return age<0||age>=emberLifetimeTics?0.f:
        std::clamp(1.f-float(age)/float(emberLifetimeTics),0.f,1.f);
}
inline bool drawBolt(int age) {return age>=0&&age<=6;}
// Anchored at both ends, so the bolt meets the target and sky consistently.
inline float lateral(int segment,int count,std::uint32_t seed) {
    const float u=float(segment)/float(std::max(1,count));
    const float taper=std::min(1.f,4.f*std::min(u,1.f-u));
    return (unit(seed+std::uint32_t(segment)*0x9e3779b9u)-.5f)*12.f*taper;
}
} // namespace doom_strike
#endif
