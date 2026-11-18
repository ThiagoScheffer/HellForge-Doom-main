/* HELLFORGE D1: pure episode-atmosphere profile and bounded geometry rules.
   No WAD sprite inference, allocation, actor state or gameplay RNG. */
#ifndef HELLFORGE_EPISODE_ATMOSPHERE_H
#define HELLFORGE_EPISODE_ATMOSPHERE_H
namespace doom_episode {
enum class Profile : int { None=0, Inferno=1 };
// `authoredSupported` uses the existing Game_HasEffect gate: modified WAD
// content is not automatically labeled E3 from its sky color or map slot.
inline Profile select(int setting,bool authoredEnabled,bool authoredSupported,
                      bool classicDoom,int episode) {
    if(setting==2)return Profile::Inferno; // Explicit opt-in for custom maps.
    if(setting==1&&authoredEnabled&&authoredSupported&&classicDoom&&episode==3)
        return Profile::Inferno;
    return Profile::None;
}
inline int ashBudget(int density) {return density<=0?48:density==1?96:128;}
inline int emberBudget(int density) {return density<=0?12:density==1?24:32;}
} // namespace doom_episode
#endif
