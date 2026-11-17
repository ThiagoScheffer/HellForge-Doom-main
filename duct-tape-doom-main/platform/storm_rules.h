/* HELLFORGE storm event scheduling. Pure cosmetic, no engine RNG or allocations.
   Each visual event queues at most one delayed thunder sound. Simulation-tic
   clock: no events progress during pause, and each level gets a stable seed. */
#ifndef HELLFORGE_STORM_RULES_H
#define HELLFORGE_STORM_RULES_H
#include <algorithm>
#include <cstdint>
#include <cmath>
namespace doom_storm {
// Thunder distances: 1 close/strong, 2 medium, 3 distant.
enum class Kind : unsigned char { None=0, Close=1, Medium=2, Distant=3 };
inline std::uint32_t mix(std::uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
inline float unit(std::uint32_t x) { return (mix(x)&0xffffffu)/16777216.f; }
struct State {
    std::uint32_t seed=0;
    int nextFlash=-1,flashTic=-1,thunderTic=-1,lastTic=-1,mode=0;
    unsigned event=0;
    Kind kind=Kind::None;
    float boltAzimuth=0.f,boltSeed=0.f;
    bool hasBolt=false;
};
struct Frame { float flash=0,storm=0,bolt=0,azimuth=0,boltSeed=0; Kind thunder=Kind::None; };
inline void reset(State &s) {s=State{};}
inline int cadence(int mode,int rate,std::uint32_t sample) {
    // Subtle 25..50 s; normal 13..29; dramatic 8..19.
    int low=mode==1?25:mode==2?13:8,high=mode==1?50:mode==2?29:19;
    return (low+int(unit(sample)*float(high-low+1)))*std::max(1,rate);
}
inline int thunderDelay(Kind kind,int rate,std::uint32_t sample) {
    float u=unit(sample);
    float sec=kind==Kind::Close?.4f+.5f*u:kind==Kind::Medium?1.2f+1.0f*u:2.8f+2.2f*u;
    return std::max(1,int(std::lround(sec*std::max(1,rate))));
}
inline Kind choose(std::uint32_t sample) {
    float u=unit(sample);
    return u<.05f?Kind::Close:u<.35f?Kind::Medium:Kind::Distant;
}
inline float pulse(int age) {
    // Two asymmetrical bright pulses and a very faint afterflash, <= 10 tics.
    switch(age) {
    case 0:return .26f;
    case 1:return 1.f;
    case 2:return .38f;
    case 3:return .07f;
    case 4:return .04f;
    case 5:return .72f;
    case 6:return .34f;
    case 7:return .06f;
    case 8:return .13f;
    default:return 0;
    }
}
inline Frame advance(State &s,int tic,int rate,int mode,bool rainActive,bool strongFlashes,
                     std::uint32_t seed,bool visibleBolts=true) {
    Frame f{};
    if(mode<=0||!rainActive||tic<0) {reset(s);return f;}
    mode=std::clamp(mode,1,3);
    if(s.seed!=seed||s.mode!=mode||tic<s.lastTic||s.nextFlash<0) {
        reset(s);s.seed=seed;s.mode=mode;
        s.nextFlash=tic+cadence(mode,rate,seed^0x183f29adu)/3;
    }
    s.lastTic=tic;
    // Baseline for future atmosphere layers. Never drives the gameplay rain.
    f.storm=mode==1?.38f:mode==2?.65f:.95f;
    if(s.thunderTic>=0 && tic>=s.thunderTic) {
        f.thunder=s.kind;s.thunderTic=-1;
    }
    if(tic>=s.nextFlash) {
        // At most one event is scheduled per call, even after long time skips.
        std::uint32_t h=mix(seed^((s.event+1u)*0x9e3779b9u));
        s.kind=choose(h^0xa30b47u);
        // A single fixed world azimuth and branch seed per electrical event;
        // never use Doom's gameplay RNG.  Bolt visibility never changes audio.
        s.boltAzimuth=-3.14159265358979323846f+
            6.2831853071795864769f*unit(h^0x84a2c771u);
        s.boltSeed=unit(h^0x3d71f9b2u);
        float chance=s.kind==Kind::Close?.67f:s.kind==Kind::Medium?.44f:.25f;
        s.hasBolt=unit(h^0x7789eab1u)<chance;
        s.flashTic=tic;
        s.thunderTic=tic+thunderDelay(s.kind,rate,h^0x13727bu);
        ++s.event;
        s.nextFlash=tic+cadence(mode,rate,h^0x91adeu);
    }
    if(s.flashTic>=0) {
        int age=tic-s.flashTic;
        float strength=s.kind==Kind::Close?.70f:s.kind==Kind::Medium?.53f:.32f;
        if(!strongFlashes)strength=std::min(strength,.19f);
        f.flash=pulse(age)*strength;
        // A branch lives only during the visible pulses, with a controlled
        // gain. Restrict it to the first seven tics; weak afterglow has no bolt.
        f.bolt=visibleBolts&&s.hasBolt&&age>=0&&age<=6?f.flash:0.f;
        f.azimuth=s.boltAzimuth;
        f.boltSeed=s.boltSeed;
    }
    return f;
}
} // namespace doom_storm
#endif
