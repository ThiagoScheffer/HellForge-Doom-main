#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "episode_atmosphere.h"
namespace doom3d {
struct sector_t { int ceilingpic; float floor,ceiling; };
struct Uniforms {float eye[4]={},effects[4]={};};
struct Settings {int episodeParticles=1;} settings;
constexpr int skyflatnum=42;
const int levelSerial=1,r_levelserial=1;
unsigned weatherSeed=7749;
float rainWind[2]={155.f,25.f},starportDustNow=.93f;
float nowTime=41.f;
std::vector<int> particleVertices;
std::vector<int> seamCells(40,1);
sector_t exposed{42,0.f,300.f},inside{0,0.f,300.f};
bool indoors=false,partlyInside=false;
float gameSeconds() {return nowTime;}
std::uint32_t hash32(std::uint32_t x) {x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
float hashUnit(int x,int y,std::uint32_t salt){return float(hash32(std::uint32_t(x)*0x9e3779b9u^std::uint32_t(y)*0x85157af5u^salt)&0xffffffu)/16777216.f;}
const sector_t *sectorAt(float x,float y) {
 if(indoors)return &inside;
 if(std::abs(x)>20000||std::abs(y)>20000)return nullptr;
 if(partlyInside&&x>0)return &inside;
 return &exposed;
}
float floorZ(const sector_t *s){return s->floor;}
float ceilZ(const sector_t *s){return s->ceiling;}
float lighting(const sector_t *){return .74f;}
void pixelBlock(std::vector<int>&out,const Uniforms&,float,float,float,float size,float height,
                const std::array<float,3>&tint,bool,const void*,bool,const void*,float alpha) {
 if(!(size>0&&height>0&&alpha>0&&alpha<=1&&tint[0]>=0&&tint[1]>=0&&tint[2]>=0))std::abort();
 out.insert(out.end(),{1,2,3,4,5,6});
}
static void buildStarportAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=35.f,reach=480.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int fineCap=doom_episode::sandBudget(density);
    const int gritCap=doom_episode::sandGustBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const float speed=std::clamp(windLength,95.f,275.f);
    const float intensity=std::clamp(starportDustNow,0.f,1.f);
    const uint32_t salt=weatherSeed^0x45f6a12du;
    const int cx=(int)std::floor(camera.eye[0]/cell);
    const int cy=(int)std::floor(camera.eye[1]/cell);
    int fine=0,grit=0;
    // Nearest rings first: no distant-field bias if the hard cap is reached.
    for(int ring=0;ring<=15&&(fine<fineCap||grit<gritCap);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(fine<fineCap||grit<gritCap);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(fine<fineCap||grit<gritCap);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x769d5bbdu^
                                    uint32_t(gy)*0x2bb4e31bu^salt);
        for(int type=0;type<2;++type) {
            const bool heavy=type!=0;
            if(heavy?grit>=gritCap:fine>=fineCap)continue;
            if(hashUnit(gx,gy,seed^(heavy?0x91ca13u:0x0dc4e7u))>
                (heavy?.20f+.08f*intensity:.66f+.24f*intensity))continue;
            const float life=heavy?2.5f:3.6f;
            const float phase=hashUnit(gx,gy,seed^0x541cc5u);
            const float clock=now+phase*life;
            const int cycle=(int)std::floor(clock/life);
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.12f+.76f*hashUnit(gx,gy,own^0x1fu))*cell;
            const float y0=(gy+.12f+.76f*hashUnit(gx,gy,own^0x26u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float room=ceiling-floor;
            if(room<17.f)continue;
            const float start=floor+std::min(room-5.f,5.f+room*
                (.11f+.58f*hashUnit(gx,gy,own^0x45u)));
            const float velocity=heavy?-(4.f+7.f*hashUnit(gx,gy,own^0x35u)):
                -(1.5f+5.f*hashUnit(gx,gy,own^0x41u));
            const float travel=speed*(heavy?.65f:.42f)*age;
            const float eddy=(heavy?9.f:14.f)*
                (std::sin(age*(heavy?2.4f:1.9f)+phase*6.2831853f)-
                 std::sin(phase*6.2831853f));
            const float dx=wx*travel-wy*eddy;
            const float dy=wy*travel+wx*eddy;
            const float x=x0+dx,y=y0+dy,z=start+velocity*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            bool sheltered=false;
            for(int k=1;k<=5;++k) {
                const float u=float(k)/6.f;
                const sector_t *along=sectorAt(x0+dx*u,y0+dy*u);
                const float zz=start+velocity*age*u;
                if(!along||along->ceilingpic!=skyflatnum||
                   zz<=floorZ(along)+2.f||zz>=ceilZ(along)-2.f) {sheltered=true;break;}
            }
            if(sheltered)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<12.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*2.4f,(life-age)*2.4f,
                (reach-dist)/85.f,(dist-12.f)/20.f}),0.f,1.f);
            if(fade<.025f)continue;
            const float light=std::clamp(lighting(at),.26f,1.f);
            const float variation=hashUnit(gx,gy,own^0x73u);
            std::array<float,3> tint=heavy?
                std::array<float,3>{.51f+.13f*variation,.32f+.11f*variation,.18f+.07f*variation}:
                std::array<float,3>{.56f+.14f*variation,.39f+.12f*variation,.24f+.07f*variation};
            for(float &c:tint)c*=.68f+.32f*light;
            const float fog=1.f-std::exp(-std::max(dist-75.f,0.f)/430.f);
            const float alpha=fade*(heavy?.72f:.49f)*(.78f+.22f*intensity)*
                (1.f-.42f*fog);
            pixelBlock(particleVertices,camera,x,y,z,
                heavy?1.5f:.85f,heavy?1.4f:1.0f,tint,
                false,nullptr,false,nullptr,alpha);
            if(heavy)++grit;else ++fine;
        }
    }
}
}
int main(){
using namespace doom3d;
using doom_episode::Profile;
auto sel=doom_episode::selectSky;
assert(sel(0,1,1,0,1,1,1)==Profile::None);
assert(sel(1,1,1,1,2,0,0)==Profile::Deimos);
assert(sel(1,1,1,1,3,0,0)==Profile::Inferno);
assert(sel(1,1,1,0,1,1,1)==Profile::Starport);
assert(sel(1,1,1,0,1,1,2)==Profile::BurningCity);
assert(sel(1,1,1,0,1,1,3)==Profile::BloodHell);
assert(sel(1,0,1,0,1,1,1)==Profile::None);
assert(sel(1,1,0,0,1,1,1)==Profile::None);
assert(sel(4,0,0,1,1,0,1)==Profile::Starport);
assert(sel(4,0,0,0,2,1,3)==Profile::BloodHell);
assert(sel(4,0,0,0,2,1,0)==Profile::None);
assert(sel(5,0,0,0,0,0,0)==Profile::Starport);
assert(sel(2,0,0,0,0,1,0)==Profile::Inferno);
assert(sel(3,0,0,0,0,1,0)==Profile::Deimos);
for(int level=0;level<3;++level){
 assert(doom_episode::sandBudget(level)>=80&&doom_episode::sandBudget(level)<=160);
 assert(doom_episode::sandGustBudget(level)>=16&&doom_episode::sandGustBudget(level)<=32);
}
assert(doom_episode::starportStorm(-1,35,123).intensity==0);
for(int tic=0;tic<35*600;++tic){
 auto a=doom_episode::starportStorm(tic,35,123u);
 assert(a.front>=0&&a.front<=1&&a.gust>=0&&a.gust<=1);
 assert(a.intensity>=.51f&&a.intensity<=1.f&&a.windScale>=1.1f&&a.windScale<=1.95f);
 assert(a.front==doom_episode::starportStorm(tic,35,123u).front);
}
Uniforms camera{};camera.eye[0]=5;camera.eye[1]=5;camera.eye[2]=40;
for(int level=0;level<3;++level) {
 settings.episodeParticles=level;
 for(int t=0;t<30;++t){
   nowTime=float(t)*.74f+9.f;
   particleVertices.clear();buildStarportAtmosphere(camera);
   int blocks=int(particleVertices.size()/6);
   if(blocks>doom_episode::sandBudget(level)+doom_episode::sandGustBudget(level))std::abort();
   if(t==0 && blocks<=0)std::abort();
 }
}
indoors=true;particleVertices.clear();buildStarportAtmosphere(camera);assert(particleVertices.empty());indoors=false;
partlyInside=true;particleVertices.clear();buildStarportAtmosphere(camera);
assert((int)particleVertices.size()/6<=doom_episode::sandBudget(2)+doom_episode::sandGustBudget(2));
partlyInside=false;camera.effects[2]=1;particleVertices.clear();buildStarportAtmosphere(camera);assert(particleVertices.empty());
std::printf("PASS: E1 SKY selection, sandstorm envelopes, particle caps and indoor restrictions\n");
}
