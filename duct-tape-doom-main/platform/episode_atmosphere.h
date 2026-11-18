/* HELLFORGE D1: pure episode-atmosphere profile and bounded geometry rules.
   No WAD sprite inference, allocation, actor state or gameplay RNG. */
#ifndef HELLFORGE_EPISODE_ATMOSPHERE_H
#define HELLFORGE_EPISODE_ATMOSPHERE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace doom_episode {
enum class Profile : int { None=0, Inferno=1, Deimos=2 };
// `authoredSupported` uses the existing Game_HasEffect gate: modified WAD
// content is not automatically labeled E3 from its sky color or map slot.
inline Profile select(int setting,bool authoredEnabled,bool authoredSupported,
                      bool classicDoom,int episode) {
    // Preserve D1's manual Inferno value for existing graphics.cfg entries.
    if(setting==2)return Profile::Inferno;
    if(setting==3)return Profile::Deimos;
    if(setting==1&&authoredEnabled&&authoredSupported&&classicDoom) {
        if(episode==3)return Profile::Inferno;
        if(episode==2)return Profile::Deimos;
    }
    return Profile::None;
}
inline int ashBudget(int density) {return density<=0?48:density==1?96:128;}
inline int emberBudget(int density) {return density<=0?12:density==1?24:32;}
// Deimos: icy crystalline drift plus finer grey-white debris. All rendering
// ceilings are per frame, not persistent allocations or gameplay actors.
// D2.1 — Blizzard caps count individual crystals and powder flecks.
// Crystals can have one separate, short wind-swept tail billboard, so the
// maximum geometry budget is 2*crystalBudget + frostDustBudget.
inline int crystalBudget(int density) {return density<=0?180:density==1?320:480;}
inline int frostDustBudget(int density) {return density<=0?80:density==1?144:220;}

// Smooth, deterministic Deimos-only whiteout weather. Independent of the
// rain/storm mode and of Doom's gameplay random stream. Calculated per tic,
// never stores particle state or changes existing rain/wet-surface simulation.
struct Blizzard {float front=0.f,gust=0.f,intensity=0.f,windScale=1.f;};
inline Blizzard deimosBlizzard(int tic,int rate,std::uint32_t seed) {
    Blizzard b{};
    if(tic<0||rate<=0)return b;
    const float t=float(tic)/float(rate);
    const std::uint32_t h=(seed^(seed>>16))*0x7feb352du;
    const float offset=float(h&65535u)/65536.f;
    const float period=42.f+30.f*float((h>>16)&255u)/255.f;
    b.front=.5f-.5f*std::cos(6.2831853f*(t/period+offset));
    b.gust=std::clamp(.50f+.31f*std::sin(t*.49f+offset*6.2831853f)
                   +.19f*std::sin(t*1.13f+offset*12.1f),0.f,1.f);
    // Always hostile: even a lull remains a visibly blowing blizzard.
    b.intensity=std::clamp(.68f+.21f*b.front+.11f*b.gust,.68f,1.f);
    b.windScale=1.20f+.90f*b.gust;
    return b;
}
} // namespace doom_episode
#endif
