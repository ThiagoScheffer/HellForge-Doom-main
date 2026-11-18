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
struct Uniforms { float eye[4]={},effects[4]={}; };
struct Settings {int episodeParticles=1;} settings;
constexpr int skyflatnum=42;
const int levelSerial=1,r_levelserial=1;
unsigned weatherSeed=7749;
float rainWind[2]={155.f,25.f},citySmokeNow=.95f,bloodHellMistNow=.94f;
float nowTime=41.f;
std::vector<int> particleVertices;
std::vector<int> seamCells(40,1);
sector_t exposed{42,0.f,300.f},inside{0,0.f,300.f};
bool indoors=false,partlyInside=false;
int smokePuffs=0,redDrops=0,fireBits=0;
float gameSeconds(){return nowTime;}
std::uint32_t hash32(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
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
 if(!(std::isfinite(size)&&std::isfinite(height)&&std::isfinite(alpha)&&size>0&&height>0&&alpha>0&&alpha<=1&&
       tint[0]>=0&&tint[1]>=0&&tint[2]>=0))std::abort();
 if(size==5.8f)++smokePuffs;
 if(size==.73f)++redDrops;
 if(size==.44f)++fireBits;
 out.insert(out.end(),{1,2,3,4,5,6});
}
static void buildBurningCityAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=38.f,reach=490.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int smokeCap=doom_episode::citySmokeBudget(density);
    const int ashCap=doom_episode::cityAshBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const float intensity=std::clamp(citySmokeNow,0.f,1.f);
    const uint32_t salt=weatherSeed^0x4bb29f7du;
    const int cx=int(std::floor(camera.eye[0]/cell));
    const int cy=int(std::floor(camera.eye[1]/cell));
    int smoke=0,ash=0;
    for(int ring=0;ring<=15&&(smoke<smokeCap||ash<ashCap);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(smoke<smokeCap||ash<ashCap);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(smoke<smokeCap||ash<ashCap);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x6e8feb86u^
                                    uint32_t(gy)*0x49b1ca15u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool soot=kind!=0;
            if(soot?ash>=ashCap:smoke>=smokeCap)continue;
            if(hashUnit(gx,gy,seed^(soot?0x71d13au:0xe54c3bu))>
               (soot?.48f:.76f+.12f*intensity))continue;
            const float life=soot?4.8f:6.3f;
            const float phase=hashUnit(gx,gy,seed^0x83a31du);
            const float clock=now+phase*life;
            const int cycle=int(std::floor(clock/life));
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.12f+.76f*hashUnit(gx,gy,own^0x31u))*cell;
            const float y0=(gy+.12f+.76f*hashUnit(gx,gy,own^0x42u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float height=ceiling-floor;
            if(height<24.f)continue;
            const float start=floor+std::min(height-6.f,8.f+height*
                (.07f+.48f*hashUnit(gx,gy,own^0x53u)));
            const bool ember=soot&&hashUnit(gx,gy,own^0x64u)<.28f;
            const float rise=soot?(ember?7.f:-4.f):8.f+6.f*hashUnit(gx,gy,own^0x75u);
            const float drift=(soot?13.f:19.f)*age;
            const float swirl=(soot?5.f:11.f)*
                (std::sin(age*(soot?1.7f:.84f)+phase*6.2831853f)-
                 std::sin(phase*6.2831853f));
            const float dx=wx*drift-wy*swirl,dy=wy*drift+wx*swirl;
            const float x=x0+dx,y=y0+dy,z=start+rise*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            bool blocked=false;
            for(int k=1;k<=5;++k) {
                const float u=float(k)/6.f;
                const sector_t *through=sectorAt(x0+dx*u,y0+dy*u);
                const float zz=start+rise*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   zz<=floorZ(through)+2.f||zz>=ceilZ(through)-2.f) {blocked=true;break;}
            }
            if(blocked)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<15.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*1.4f,(life-age)*1.4f,
                (reach-dist)/90.f,(dist-15.f)/25.f}),0.f,1.f);
            if(fade<.03f)continue;
            const float hue=hashUnit(gx,gy,own^0x86u);
            const float light=std::clamp(lighting(at),.20f,1.0f);
            if(!soot) {
                const std::array<float,3> gray={.16f+.10f*hue,.145f+.08f*hue,.13f+.07f*hue};
                const float alpha=fade*(.19f+.12f*intensity)*(.75f+.25f*light);
                pixelBlock(particleVertices,camera,x,y,z,5.8f,5.1f,gray,
                    false,nullptr,false,nullptr,alpha);
                pixelBlock(particleVertices,camera,x+wx*2.3f,y+wy*2.3f,z+2.7f,
                    4.6f,4.2f,gray,false,nullptr,false,nullptr,alpha*.63f);
                ++smoke;
            } else {
                const std::array<float,3> color=ember?
                    std::array<float,3>{.87f+.10f*hue,.28f+.17f*hue,.05f+.07f*hue}:
                    std::array<float,3>{.23f+.08f*hue,.20f+.07f*hue,.18f+.05f*hue};
                pixelBlock(particleVertices,camera,x,y,z,ember?.55f:.75f,
                    ember?.80f:1.1f,color,false,nullptr,false,nullptr,
                    fade*(ember?.72f:.54f));
                ++ash;
            }
        }
    }
}

static void buildBloodHellAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=30.f,reach=455.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int dropCap=doom_episode::bloodRainBudget(density);
    const int emberCap=doom_episode::bloodEmberBudget(density);
    const float now=gameSeconds();
    const float speed=std::hypot(rainWind[0],rainWind[1]);
    const float wx=speed>1.f?rainWind[0]/speed:1.f;
    const float wy=speed>1.f?rainWind[1]/speed:0.f;
    const float intensity=std::clamp(bloodHellMistNow,0.f,1.f);
    const uint32_t salt=weatherSeed^0xbd19f8a3u;
    const int cx=int(std::floor(camera.eye[0]/cell));
    const int cy=int(std::floor(camera.eye[1]/cell));
    int drops=0,embers=0;
    for(int ring=0;ring<=16&&(drops<dropCap||embers<emberCap);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(drops<dropCap||embers<emberCap);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(drops<dropCap||embers<emberCap);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x17e9c1fbu^
                                    uint32_t(gy)*0x4cb29a37u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool ember=kind!=0;
            if(ember?embers>=emberCap:drops>=dropCap)continue;
            if(hashUnit(gx,gy,seed^(ember?0x154u:0x276u))>
               (ember?.18f:.66f+.18f*intensity))continue;
            const float life=ember?4.2f:2.1f;
            const float phase=hashUnit(gx,gy,seed^0x4d1u);
            const float clock=now+phase*life;
            const int cycle=int(std::floor(clock/life));
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.13f+.74f*hashUnit(gx,gy,own^0x386u))*cell;
            const float y0=(gy+.13f+.74f*hashUnit(gx,gy,own^0x497u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float height=ceiling-floor;
            if(height<25.f)continue;
            const float start=floor+std::min(height-5.f,10.f+height*
                (.30f+.56f*hashUnit(gx,gy,own^0x508u)));
            const float vz=ember?5.f+7.f*hashUnit(gx,gy,own^0x619u):
                -(100.f+70.f*hashUnit(gx,gy,own^0x72au));
            const float drift=(ember?15.f:17.f)*age;
            const float swirl=3.4f*(std::sin(age*1.7f+phase*6.2831853f)-
                                     std::sin(phase*6.2831853f));
            const float dx=wx*drift-wy*swirl,dy=wy*drift+wx*swirl;
            const float x=x0+dx,y=y0+dy,z=start+vz*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+1.f||
               z>=ceilZ(at)-2.f)continue;
            bool sheltered=false;
            for(int k=1;k<=5;++k) {
                const float u=float(k)/6.f;
                const sector_t *through=sectorAt(x0+dx*u,y0+dy*u);
                const float zz=start+vz*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   zz<=floorZ(through)+1.f||zz>=ceilZ(through)-2.f) {sheltered=true;break;}
            }
            if(sheltered)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<11.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*2.2f,(life-age)*2.2f,
                (reach-dist)/80.f,(dist-11.f)/19.f}),0.f,1.f);
            if(fade<.03f)continue;
            const float light=std::clamp(lighting(at),.23f,1.f);
            const float variation=hashUnit(gx,gy,own^0x83bu);
            if(ember) {
                const std::array<float,3> color={.83f+.15f*variation,
                    .18f+.13f*variation,.07f+.045f*variation};
                pixelBlock(particleVertices,camera,x,y,z,.44f,.72f,color,
                    false,nullptr,false,nullptr,fade*.70f);
                ++embers;
            } else {
                // Dark crimson drips, two short separated segments.
                const std::array<float,3> red={(.43f+.22f*variation)*light,
                    (.055f+.07f*variation)*light,
                    (.060f+.065f*variation)*light};
                pixelBlock(particleVertices,camera,x,y,z,.73f,2.9f,red,
                    false,nullptr,false,nullptr,fade*.86f);
                pixelBlock(particleVertices,camera,x-wx*.75f,y-wy*.75f,z+2.f,
                    .48f,1.8f,red,false,nullptr,false,nullptr,fade*.43f);
                // Tiny transient droplet break-up at ground level, not a decal.
                if(z-floorZ(at)<10.f)
                    pixelBlock(particleVertices,camera,x,y,floorZ(at)+1.1f,
                        1.25f,.55f,red,false,nullptr,false,nullptr,fade*.42f);
                ++drops;
            }
        }
    }
}
}
int main(){
using namespace doom3d;
using doom_episode::Profile;
auto sel=doom_episode::selectSky;
assert(sel(0,1,1,0,1,1,1)==Profile::None);
assert(sel(1,1,1,0,1,1,1)==Profile::Starport);
assert(sel(1,1,1,0,1,1,2)==Profile::BurningCity);
assert(sel(1,1,1,0,1,1,3)==Profile::BloodHell);
assert(sel(1,0,1,0,1,1,2)==Profile::None);
assert(sel(4,0,0,0,1,1,2)==Profile::BurningCity);
assert(sel(4,0,0,0,1,1,3)==Profile::BloodHell);
assert(sel(6,0,0,0,1,1,0)==Profile::BurningCity);
assert(sel(7,0,0,0,1,1,0)==Profile::BloodHell);
assert(sel(2,0,0,0,1,1,0)==Profile::Inferno);
assert(sel(3,0,0,0,1,1,0)==Profile::Deimos);
assert(doom_episode::bloodHellWeather(-1,35,23).intensity==0.f);
assert(doom_episode::burningCity(-1,35,23).intensity==0.f);
for(int t=0;t<35*300;++t){
 auto city=doom_episode::burningCity(t,35,2138);
 auto hell=doom_episode::bloodHellWeather(t,35,9485);
 assert(city.intensity>=.70f&&city.intensity<=1.f);
 assert(city.windScale>=.78f&&city.windScale<=1.22f);
 assert(city.gust>=0&&city.gust<=1);
 assert(hell.intensity>=.76f&&hell.intensity<=1.f);
 assert(hell.windScale>=.86f&&hell.windScale<=1.22f);
 assert(hell.gust>=0&&hell.gust<=1);
 assert(hell.intensity==doom_episode::bloodHellWeather(t,35,9485).intensity);
}
Uniforms camera{};camera.eye[0]=5;camera.eye[1]=5;camera.eye[2]=40;
for(int level=0;level<3;++level){
 settings.episodeParticles=level;
 for(int t=0;t<45;++t){
  nowTime=float(t)*.74f+9.f;
  particleVertices.clear();smokePuffs=0;
  buildBurningCityAtmosphere(camera);
  int blocks=int(particleVertices.size()/6);
  assert(blocks<=2*doom_episode::citySmokeBudget(level)+doom_episode::cityAshBudget(level));
  assert(smokePuffs<=doom_episode::citySmokeBudget(level));
  if(t==0){assert(blocks>0); assert(smokePuffs>0);}
  particleVertices.clear();redDrops=0;fireBits=0;
  buildBloodHellAtmosphere(camera);
  blocks=int(particleVertices.size()/6);
  assert(blocks<=3*doom_episode::bloodRainBudget(level)+doom_episode::bloodEmberBudget(level));
  assert(redDrops<=doom_episode::bloodRainBudget(level));
  assert(fireBits<=doom_episode::bloodEmberBudget(level));
  if(t==0){assert(blocks>0);assert(redDrops>0);}
 }
}
indoors=true;particleVertices.clear();buildBurningCityAtmosphere(camera);assert(particleVertices.empty());
particleVertices.clear();buildBloodHellAtmosphere(camera);assert(particleVertices.empty());indoors=false;
partlyInside=true;particleVertices.clear();buildBurningCityAtmosphere(camera);
assert(int(particleVertices.size()/6)<=2*doom_episode::citySmokeBudget(2)+doom_episode::cityAshBudget(2));
particleVertices.clear();buildBloodHellAtmosphere(camera);
assert(int(particleVertices.size()/6)<=3*doom_episode::bloodRainBudget(2)+doom_episode::bloodEmberBudget(2));
partlyInside=false;camera.effects[2]=1;
particleVertices.clear();buildBurningCityAtmosphere(camera);assert(particleVertices.empty());
particleVertices.clear();buildBloodHellAtmosphere(camera);assert(particleVertices.empty());
std::puts("PASS: E2 smoke/ash, E3 blood/embers, selectors, budgets, indoor and skip modes");
}
