/* HELLFORGE D1: pure episode-atmosphere profile and bounded geometry rules.
   No WAD sprite inference, allocation, actor state or gameplay RNG. */
#ifndef HELLFORGE_EPISODE_ATMOSPHERE_H
#define HELLFORGE_EPISODE_ATMOSPHERE_H
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
inline int crystalBudget(int density) {return density<=0?64:density==1?112:144;}
inline int frostDustBudget(int density) {return density<=0?16:density==1?32:48;}
} // namespace doom_episode
#endif
