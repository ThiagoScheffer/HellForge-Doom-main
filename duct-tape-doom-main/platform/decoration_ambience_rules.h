/* Pure cosmetic selection/limits for HELLFORGE corpse and decoration ambience.
   No gameplay random draws, gameplay state, or engine dependencies. */
#ifndef DOOM_DECORATION_AMBIENCE_RULES_H
#define DOOM_DECORATION_AMBIENCE_RULES_H
#include <cstdint>

namespace doom_decor {
enum class BodyKind : unsigned char { None, Corpse, Remains, Impaled, Hanging };
// Species-specific ambience is purely cosmetic. The engine maps stock mobj
// types to these profiles; unknown types and decorative remains stay Generic.
enum class CorpseProfile : unsigned char {
    Generic, LargeRedWorms, BoneSpiders, FireSlug, BlueJelly
};
inline bool allowsFlies(CorpseProfile profile) {
    // Only the three exotic species suppress ordinary flies.
    return profile==CorpseProfile::Generic || profile==CorpseProfile::LargeRedWorms;
}

constexpr unsigned monsterLimit=200; // The effect is disabled at 200 or more map monsters.
constexpr unsigned trackedLimit=256, emitterLimit=12, flyLimit=48, wormLimit=24;
constexpr float effectRadius=512.f, trackRadius=768.f;
inline std::uint32_t hash(std::uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
inline float unit(std::uint32_t x) {return (hash(x)&0x00ffffffu)/16777216.f;}
inline bool specialCreature(std::uint32_t seed,CorpseProfile profile) {
    if(profile==CorpseProfile::Generic)return false;
    // Consistent per corpse, independent from the gameplay random stream.
    return hash(seed^0x3d114cc7u)%100u<65u;
}
// Additional hard cap on sprite blocks for any combination of scavengers.
constexpr unsigned critterBlockLimit=192;
inline bool active(unsigned mapMonsters,bool switchOn,int insects) {
    return switchOn&&insects>0&&mapMonsters<monsterLimit;
}
inline bool flies(std::uint32_t seed) {return hash(seed^0x91ab3d11u)%100u<40u;}
inline bool worms(std::uint32_t seed) {return hash(seed^0x68271c47u)%100u<45u;}
inline float delayScale(int preset) {return preset<=0?0.5f:preset==1?1.0f:2.0f;}
inline int flyDelayTics(std::uint32_t seed,int preset,int ticrate) {
    return int((15+(hash(seed^0x124babbdu)%16))*ticrate*delayScale(preset));
}
inline int wormDelayTics(std::uint32_t seed,int preset,int ticrate) {
    return int((30+(hash(seed^0x3c648a29u)%31))*ticrate*delayScale(preset));
}
inline bool wormSurface(BodyKind kind) {
    return kind==BodyKind::Corpse||kind==BodyKind::Remains;
}
} // namespace doom_decor
#endif
