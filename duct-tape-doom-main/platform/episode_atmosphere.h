/* HELLFORGE D1: pure episode-atmosphere profile and bounded geometry rules.
   No WAD sprite inference, allocation, actor state or gameplay RNG. */
#ifndef HELLFORGE_EPISODE_ATMOSPHERE_H
#define HELLFORGE_EPISODE_ATMOSPHERE_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace doom_episode {
// Keep the existing numeric values of Inferno and Deimos in graphics.cfg.
// E2/E3 use the profile IDs reserved by E1; retain legacy F4 values.
enum class Profile : int { None=0, Inferno=1, Deimos=2,
                           Starport=3, BurningCity=4, BloodHell=5 };
// skyKind describes the *current logical sky texture*, not a hardcoded MAP range.
// The renderer maps Doom II RSKY artwork through SKY1, SKY2 and SKY3.
inline Profile commercialSky(int skyKind) {
    return skyKind==1?Profile::Starport:
           skyKind==2?Profile::BurningCity:
           skyKind==3?Profile::BloodHell:Profile::None;
}
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
// Never auto-assign themed weather by sampled sky color. A modified WAD may
// deliberately replace SKY1 with unrelated artwork. Follow active sky is
// an explicit option; Authored episodes retains the stock authoring gate.
inline Profile selectSky(int setting,bool authoredEnabled,bool authoredSupported,
                         bool classicDoom,int episode,bool commercial,int skyKind) {
    // Preserve D1/D2 manual values even in a DOOM II/commercial game.
    if(setting==2)return Profile::Inferno;
    if(setting==3)return Profile::Deimos;
    if(setting==4) return commercialSky(skyKind); // Explicit custom-WAD opt-in.
    if(setting==5) return Profile::Starport;
    if(setting==6) return Profile::BurningCity;
    if(setting==7) return Profile::BloodHell;
    if(commercial) {
        return setting==1&&authoredEnabled&&authoredSupported?
               commercialSky(skyKind):Profile::None;
    }
    return select(setting,authoredEnabled,authoredSupported,classicDoom,episode);
}
// The authored gate uses the existing game-profile capability. Custom WADs
// can opt in explicitly through Follow active sky or the manual profile.
// E1: fixed per-frame dust budget; no persistent particles or actors.
inline int sandBudget(int density) {return density<=0?80:density==1?128:160;}
inline int sandGustBudget(int density) {return density<=0?16:density==1?24:32;}
struct Sandstorm {float front=0.f,gust=0.f,intensity=0.f,windScale=1.f;};
inline Sandstorm starportStorm(int tic,int rate,std::uint32_t seed) {
    Sandstorm out{};
    if(tic<0||rate<=0)return out;
    float t=float(tic)/float(rate);
    std::uint32_t h=(seed^(seed>>16))*0x7feb352du;
    float phase=float(h&65535u)/65536.f;
    float period=65.f+42.f*float((h>>16)&255u)/255.f;
    out.front=.5f-.5f*std::cos(6.283185307f*(t/period+phase));
    out.gust=std::clamp(.48f+.32f*std::sin(t*.67f+phase*6.283185307f)
                   +.20f*std::sin(t*1.31f+phase*8.7f),0.f,1.f);
    // Dust never disappears during a lull; no rain/wetness is generated.
    out.intensity=std::clamp(.51f+.32f*out.front+.17f*out.gust,.51f,1.f);
    out.windScale=1.10f+.85f*out.gust;
    return out;
}
inline float starportHaze(float intensity) {
    return std::clamp(.36f+.30f*intensity,.36f,.66f);
}
// E2 — Burning City: dark rising smoke, falling soot and rare orange embers.
// Smoke wisps, ash and embers each use one billboard. Fixed scene budgets.
inline int citySmokeBudget(int density) {return density<=0?16:density==1?28:40;}
inline int cityAshBudget(int density) {return density<=0?24:density==1?40:56;}
struct CityAir {float intensity=0.f,gust=0.f,windScale=1.f;};
inline CityAir burningCity(int tic,int rate,std::uint32_t seed) {
    CityAir a{};
    if(tic<0||rate<=0)return a;
    const float t=float(tic)/float(rate);
    const float phase=float(((seed^(seed>>15))*0x846ca68bu)&65535u)/65536.f;
    const float slow=.5f-.5f*std::cos(6.283185307f*(t/94.f+phase));
    a.gust=std::clamp(.48f+.34f*std::sin(t*.37f+phase*6.283185307f)
                      +.18f*std::sin(t*.91f+phase*9.1f),0.f,1.f);
    a.intensity=std::clamp(.70f+.17f*slow+.13f*a.gust,.70f,1.f);
    a.windScale=.78f+.44f*a.gust;
    return a;
}
inline float cityHaze(float intensity) {
    return std::clamp(.39f+.16f*intensity,.39f,.55f);
}
// E3 — Blood Hell: cold opaque crimson smog, heavy *cosmetic* blood drips
// and floating cinders. Never affects water rain, splashes or wet textures.
inline int bloodRainBudget(int density) {return density<=0?64:density==1?96:120;}
inline int bloodEmberBudget(int density) {return density<=0?12:density==1?24:32;}
struct BloodWeather {float intensity=0.f,gust=0.f,windScale=1.f;};
inline BloodWeather bloodHellWeather(int tic,int rate,std::uint32_t seed) {
    BloodWeather b{};
    if(tic<0||rate<=0)return b;
    const float t=float(tic)/float(rate);
    const float phase=float(((seed^(seed>>16))*0x7feb352du)&65535u)/65536.f;
    const float slow=.5f-.5f*std::cos(6.283185307f*(t/75.f+phase));
    b.gust=std::clamp(.50f+.35f*std::sin(t*.51f+phase*6.283185307f)
                     +.15f*std::sin(t*1.11f+phase*12.3f),0.f,1.f);
    b.intensity=std::clamp(.76f+.15f*slow+.09f*b.gust,.76f,1.f);
    b.windScale=.86f+.36f*b.gust;
    return b;
}
inline float bloodHellHaze(float intensity) {
    return std::clamp(.64f+.23f*intensity,.64f,.87f);
}
inline int ashBudget(int density) {return density<=0?64:density==1?128:184;}
inline int emberBudget(int density) {return density<=0?16:density==1?32:44;}

namespace inferno {
inline constexpr float reach=390.f;
inline int layer(float distance) {return distance<105.f?0:distance<235.f?1:2;}
inline std::array<int,3> layerBudgets(int total) {
    const int near=total*27/100,mid=total*40/100;
    return {near,mid,total-near-mid};
}
struct Wind {
    float period,offset;
    // Integral of a smooth 2.8-second gust. Taking the difference between
    // two times applies acceleration without moving older particles in jumps.
    float travel(float seconds) const {
        constexpr float half=1.4f;
        const float clock=seconds+offset;
        const float cycle=std::floor(clock/period);
        const float phase=clock-cycle*period;
        const auto ramp=[](float u) {return u*u*u*(1.f-.5f*u);};
        const float area=phase<half?half*ramp(phase/half):
            half*(1.f-ramp(std::max(0.f,2.f-phase/half)));
        return cycle*half+area;
    }
};
inline Wind wind(std::uint32_t seed) {
    const std::uint32_t h=(seed^(seed>>16))*0x7feb352du;
    const float period=8.5f+4.f*float(h&255u)/255.f;
    return {period,period*float((h>>8)&65535u)/65536.f};
}
struct Point {float x,y,z;};
// Visit every grid cell crossed by a chord, including the entry and exit
// heights in each cell. Fixed iteration bound also rejects unsupported paths.
template<class Open>
bool skyChord(Point a,Point b,float originX,float originY,float cell,Open open) {
    const float x=(a.x-originX)/cell,y=(a.y-originY)/cell;
    const float dx=(b.x-a.x)/cell,dy=(b.y-a.y)/cell,dz=b.z-a.z;
    int cx=int(std::floor(x)),cy=int(std::floor(y));
    const int sx=dx<0.f?-1:1,sy=dy<0.f?-1:1;
    const float stepX=dx==0.f?2.f:1.f/std::abs(dx),stepY=dy==0.f?2.f:1.f/std::abs(dy);
    float nextX=dx==0.f?2.f:(sx>0?cx+1-x:x-cx)*stepX;
    float nextY=dy==0.f?2.f:(sy>0?cy+1-y:y-cy)*stepY;
    float entry=0.f;
    for(int n=0;n<64;++n) {
        const float exit=std::min({nextX,nextY,1.f});
        if(!open(cx,cy,a.z+dz*entry,a.z+dz*exit))return false;
        if(exit>=1.f)return true;
        if(nextX==exit){cx+=sx;nextX+=stepX;}
        if(nextY==exit){cy+=sy;nextY+=stepY;}
        entry=exit;
    }
    return false;
}
} // namespace inferno

namespace hellfire {
inline bool enabled(Profile profile,bool accelerated) {
    return accelerated&&profile==Profile::Inferno;
}
inline std::uint32_t hash(std::uint32_t h) {
    h=(h^(h>>16))*0x7feb352du;
    h=(h^(h>>15))*0x846ca68bu;
    return h^(h>>16);
}
struct Site {
    float x=0,y=0,floor=0,ceiling=0,driftX=0,driftY=0;
    int sector=-1;
    std::uint32_t seed=0;
};
constexpr float clearance=28.f;
// BSP lookup alone can return a sector for a point outside the map.
// Count real boundary crossings and keep the entire effect inside them.
struct LandingDisk {
    float x,y;
    bool inside=false,clear=true;
    void edge(float ax,float ay,float bx,float by,bool boundary) {
        const float dx=bx-ax,dy=by-ay;
        const float lengthSq=dx*dx+dy*dy;
        if(lengthSq==0)return;
        const float t=std::clamp(((x-ax)*dx+(y-ay)*dy)/lengthSq,0.f,1.f);
        const float rx=x-(ax+t*dx),ry=y-(ay+t*dy);
        if(rx*rx+ry*ry<clearance*clearance)clear=false;
        if(boundary&&((ay>y)!=(by>y))&&x<ax+(y-ay)*dx/dy)inside=!inside;
    }
};
inline bool openColumn(bool sky,bool solidFloor,float height) {
    return sky&&solidFloor&&height>=88.f;
}
struct Effect {
    Site site;
    int start=-1,end=-1;
};
struct State {
    Effect fall;
    std::array<Effect,2> fires{};
    int next=-1,last=-1;
    unsigned event=0,level=0;
};
inline int interval(std::uint32_t seed,int rate) {
    return 30*rate+int(hash(seed)%(40*rate+1));
}
inline float fireFade(const Effect &fire,float tic,int rate) {
    return std::clamp((fire.end-tic)/(2.f*rate),0.f,1.f);
}
template<class FindSite,class StillValid>
void advance(State &state,int tic,int rate,unsigned level,std::uint32_t seed,
             bool active,FindSite findSite,StillValid stillValid) {
    if(!active||state.level!=level||tic<state.last) {
        state={};
        state.level=level;
    }
    if(!active||tic==state.last)return;
    state.last=tic;
    if(state.next<0)state.next=tic+interval(seed,rate);
    for(auto &fire:state.fires)
        if(fire.start>=0&&(tic>=fire.end||!stillValid(fire.site)))fire={};
    if(state.fall.start>=0) {
        if(!stillValid(state.fall.site))state.fall={};
        else if(tic>=state.fall.end) {
            for(auto &fire:state.fires)if(fire.start<0) {
                const Site &site=state.fall.site;
                const int life=6*rate+int(hash(site.seed^0x55d6cb39u)%(4*rate+1));
                if(tic-state.fall.end<life)fire={site,state.fall.end,state.fall.end+life};
                break;
            }
            state.fall={};
        }
    }
    if(tic<state.next)return;
    const std::uint32_t eventSeed=hash(seed+(++state.event)*0x9e3779b9u);
    state.next=tic+interval(eventSeed,rate);
    if(state.fall.start>=0)return;
    const Site site=findSite(eventSeed);
    if(site.sector>=0)state.fall={site,tic,tic+rate};
}
} // namespace hellfire
// Deimos: icy crystalline drift plus finer grey-white debris. All rendering
// ceilings are per frame, not persistent allocations or gameplay actors.
// D2.1 — Blizzard caps count individual crystals and powder flecks.
// Crystals can have one separate, short wind-swept tail billboard, so the
// maximum geometry budget is 2*crystalBudget + frostDustBudget.
inline int crystalBudget(int density) {return density<=0?180:density==1?320:480;}
inline int frostDustBudget(int density) {return density<=0?80:density==1?144:220;}

// D2.2: fog strength is independent of particle density and water rain.
// Levels are authored for a depth-based outdoor whiteout, not generic full-screen fog.
// 0 means no new blizzard fog; existing engine height fog is unaffected.
inline float deimosFogStrength(int visibility,float blizzard) {
    if(visibility<=0)return 0.f;
    const float storm=std::clamp(blizzard,0.f,1.f);
    const float base=visibility==1?.29f:visibility==2?.64f:.88f;
    const float modulation=visibility==1?.09f:visibility==2?.17f:.12f;
    return std::clamp(base+modulation*storm,0.f,1.f);
}
// Same extinction curve used in world.frag. Beyond the onset, haze builds
// exponentially and reaches near-opacity in Whiteout; the foreground stays readable.
inline float deimosFogOpacity(float distance,float strength) {
    if(strength<=0.f)return 0.f;
    strength=std::clamp(strength,0.f,1.f);
    const float onset=200.f-186.f*strength;
    const float reach=600.f-548.f*strength;
    return strength*(1.f-std::exp(-std::max(distance-onset,0.f)/reach));
}

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
