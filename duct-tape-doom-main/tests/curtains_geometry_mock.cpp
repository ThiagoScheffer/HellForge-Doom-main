#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstdio>
struct Uniforms { float eye[4]{}; };
struct sector_t { int ceilingpic=10; };
struct {int stormAtmosphere=1;} settings;
float stormCurtainNow=.95f;
float rainWind[2]={120,55};
uint32_t weatherSeed=0xabcdef;
int skyflatnum=10;
static sector_t sector;
const sector_t *sectorAt(float x,float y){return (std::abs(x)<800.f&&std::abs(y)<800.f)?&sector:nullptr;}
float floorZ(const sector_t*){return 0.f;}
float ceilZ(const sector_t*){return 176.f;}
float hashUnit(int x,int y,uint32_t salt){uint32_t n=uint32_t(x)*0x9e3779b9u+uint32_t(y)*0x7feb352du+salt;n^=n>>13;n*=0x85ebca6bu;return float(n&0xffffffu)/16777216.f;}
#include <array>
std::array<float,3> rainAlbedo(){return {0.5f,0.5f,0.6f};}
static int streakCalls=0;
void streak(const Uniforms&,float,float,float,float,float,float,float,int,const std::array<float,3>&,float){++streakCalls;}

static void buildStormCurtains(const Uniforms &camera,float now) {
    if(stormCurtainNow<0.10f||!settings.stormAtmosphere)return;
    constexpr float cell=160.f,reach=640.f;
    constexpr int maxCurtains=12,streaksPerCurtain=8;
    const int cx=int(std::floor(camera.eye[0]/cell));
    const int cy=int(std::floor(camera.eye[1]/cell));
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    if(windLength<1.f)return;
    const float wx=rainWind[0]/windLength,wy=rainWind[1]/windLength;
    const uint32_t salt=weatherSeed^0x7dbbe321u;
    int rendered=0;
    for(int gy=cy-4;gy<=cy+4&&rendered<maxCurtains;++gy)
    for(int gx=cx-4;gx<=cx+4&&rendered<maxCurtains;++gx) {
        if(hashUnit(gx,gy,salt)>.12f+.17f*stormCurtainNow)continue;
        const float centerX=(gx+.25f+.50f*hashUnit(gx,gy,salt^1u))*cell;
        const float centerY=(gy+.25f+.50f*hashUnit(gx,gy,salt^2u))*cell;
        const float dist=std::hypot(centerX-camera.eye[0],centerY-camera.eye[1]);
        if(dist<190.f||dist>reach)continue;
        const sector_t *sector=sectorAt(centerX,centerY);
        if(!sector||sector->ceilingpic!=skyflatnum)continue;
        const float bottom=floorZ(sector)+12.f;
        const float top=std::max(ceilZ(sector),camera.eye[2]+100.f)-14.f;
        if(top-bottom<36.f)continue;
        const float phase=hashUnit(gx,gy,salt^3u);
        // Horizontal sweep repeats per cell; fade the curtain near its wrap.
        const float motion=std::fmod(now*(28.f+20.f*stormCurtainNow)+phase*cell,cell)/cell;
        const float sweep=(motion-.5f)*cell*.42f;
        const float wrapFade=std::clamp(std::min(motion,1.f-motion)*8.f,0.f,1.f);
        const float distanceFade=std::clamp((reach-dist)/120.f,0.f,1.f);
        const float alpha=stormCurtainNow*wrapFade*distanceFade*.21f;
        if(alpha<.015f)continue;
        auto tint=rainAlbedo();
        for(float &component:tint)component=std::min(1.f,component*1.18f);
        for(int n=0;n<streaksPerCurtain;++n) {
            // A sparse narrow vertical sheet, perpendicular to wind.
            const float across=(float(n)-3.5f)*10.f;
            const float x=centerX-wy*across+wx*sweep;
            const float y=centerY+wx*across+wy*sweep;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum)continue;
            const float localFloor=floorZ(at)+10.f;
            const float localTop=std::max(ceilZ(at),camera.eye[2]+100.f)-14.f;
            if(localTop-localFloor<32.f)continue;
            const float speed=440.f+70.f*hashUnit(n,gx,salt^4u);
            const float fall=localTop-localFloor;
            const float z=localFloor+std::fmod(now*speed+fall*hashUnit(n,gy,salt^5u),fall);
            const float opacity=alpha*(.65f+.35f*hashUnit(n,gy,salt^6u));
            streak(camera,x,y,z,rainWind[0],rainWind[1],-speed,
                   12.f,1,tint,opacity);
        }
        ++rendered;
    }
}



int main(){ Uniforms camera{};camera.eye[2]=48.f;
int positive=0;int maxCalls=0;
for(int i=0;i<400;++i){streakCalls=0;buildStormCurtains(camera,float(i)*0.3f);assert(streakCalls<=96);positive+=streakCalls>0;maxCalls=std::max(maxCalls,streakCalls);}
assert(positive>0);printf("Active curtains in %d mock frames; max billboards %d\n",positive,maxCalls);
settings.stormAtmosphere=0;streakCalls=0;buildStormCurtains(camera,6.f);assert(streakCalls==0);
settings.stormAtmosphere=1;stormCurtainNow=0;buildStormCurtains(camera,6.f);assert(streakCalls==0);
puts("PASS bounded world-anchored rain curtains, off/active gating, native function compilation");
}
