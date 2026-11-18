/* HELLFORGE Storm Milestone C: deterministic outdoor weather envelope.
   Pure cosmetic math, no gameplay RNG, allocations, actors or graphics state.
   This changes rain/wind only while an enabled storm is already active.
   When disabled, the existing Milestone A/B rain path is bit-for-bit unchanged. */
#ifndef HELLFORGE_STORM_ATMOSPHERE_H
#define HELLFORGE_STORM_ATMOSPHERE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace doom_storm {
struct Atmosphere {
    float front=0.f;       // 0..1: slowly building/fading storm
    float gust=0.f;        // 0..1: faster, continuously varying wind
    float rainScale=1.f;   // applied only to existing rain intensity
    float windScale=1.f;   // applied only to existing rain velocity
    float curtain=0.f;     // 0..1: bounded distant rain curtains
    float haze=0.f;        // added to existing outdoor haze intensity
};
inline Atmosphere atmosphere(int tic,int rate,int mode,bool rainy,bool enabled,std::uint32_t seed) {
    Atmosphere a{};
    if(!enabled||!rainy||mode<=0||tic<0)return a;
    const float t=float(tic)/float(std::max(1,rate));
    const std::uint32_t h=(seed^(seed>>16))*0x7feb352du;
    const float offset=float(h&0xffffu)/65536.f;
    // One broad front over ~80..140 seconds; cosine has zero slope at
    // both extrema, avoiding sudden intensity jumps between game tics.
    const float period=80.f+60.f*float((h>>16)&255u)/255.f;
    const float angle=6.283185307179586f*(t/period+offset);
    a.front=0.5f-0.5f*std::cos(angle);
    // Small gusts modulate the broader weather front. No signed wind
    // reversals: the authored/per-map rain heading remains authoritative.
    a.gust=std::clamp(0.50f+0.33f*std::sin(t*0.67f+offset*6.2831853f)
                 +0.17f*std::sin(t*1.27f+offset*11.9f),0.f,1.f);
    float weight=mode==1?0.40f:mode==2?0.69f:1.f;
    float intensity=weight*(0.20f+0.80f*a.front);
    a.rainScale=std::clamp(0.80f+0.50f*intensity,0.75f,1.30f);
    a.windScale=1.f+intensity*(0.12f+0.60f*a.gust);
    a.curtain=std::clamp(intensity*0.85f,0.f,1.f);
    a.haze=std::clamp(intensity*0.22f,0.f,0.22f);
    return a;
}
} // namespace doom_storm
#endif
