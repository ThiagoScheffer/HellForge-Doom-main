#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "episode_atmosphere.h"
namespace doom3d {
using byte=unsigned char;
struct Uniforms { float eye[4]={},effects[4]={}; };
struct sector_t {int ceilingpic=0;};
struct Settings {int episodeParticles=1;} settings;
int episodeAtmosphereNow=1;
int levelSerial=1,r_levelserial=1,skyflatnum=42;
unsigned weatherSeed=5937;
float rainWind[2]={148.f,-72.f};
float deimosBlizzardNow=.88f,deimosGustNow=.71f;
std::vector<int> seamCells(4,0),particleVertices;
std::vector<std::array<float,3>> particleColors;
sector_t outdoors{42},indoors{0};
bool forceIndoor=false;
float seconds=33.1f;
float gameSeconds(){return seconds;}
uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
float hashUnit(int x,int y,uint32_t s){return (hash32(uint32_t(x)*0x9e3779b9u^uint32_t(y)*0x85ebca6bu^s)&0xffffffu)/16777216.f;}
const sector_t *sectorAt(float x,float y){
 if(forceIndoor || (y>20.f&&y<21.f) || (x>20.f&&x<22.f))return &indoors;
 return &outdoors;
}
float floorZ(const sector_t*){return 0.f;}
float ceilZ(const sector_t*){return 200.f;}
float lighting(const sector_t*){return 0.7f;}
void pixelBlock(std::vector<int>&out,const Uniforms&,float,float,float,float,float,const std::array<float,3>&color,
                bool,const std::array<float,3>*,bool,void*,float){out.insert(out.end(),{1,2,3,4,5,6});particleColors.push_back(color);}
// D2.1 — Whiteout: Deimos has a near-field windblown layer and a thinner
// drifting powder layer. Uses deterministic world cells; no gameplay actors,
// persistent allocation or new shader. Every candidate and the swept motion
// path must remain under open sky. Ring order prioritizes near-camera flecks
// when the hard particle budget is reached, avoiding a far-side bias.
static void buildDeimosAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=28.f,reach=455.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int crystalLimit=doom_episode::crystalBudget(density);
    const int dustLimit=doom_episode::frostDustBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const float gust=std::clamp(deimosGustNow,0.f,1.f);
    const float storm=std::clamp(deimosBlizzardNow,0.f,1.f);
    const float wind=std::clamp(windLength,90.f,270.f);
    const uint32_t salt=weatherSeed^0x65ba19d3u;
    const int cx=(int)std::floor(camera.eye[0]/cell);
    const int cy=(int)std::floor(camera.eye[1]/cell);
    int crystals=0,dust=0;
    // Bounded 33x33 neighboring cells. The inner rings are processed first,
    // so foreground density does not disappear when the cap is reached.
    for(int ring=0;ring<=16&&(crystals<crystalLimit||dust<dustLimit);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(crystals<crystalLimit||dust<dustLimit);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(crystals<crystalLimit||dust<dustLimit);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x9f28a713u^
                                   uint32_t(gy)*0x6c8e9cf5u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool fine=kind!=0;
            if(fine?dust>=dustLimit:crystals>=crystalLimit)continue;
            const float selection=hashUnit(gx,gy,seed^(fine?0x293a16u:0xb1ce49u));
            if(selection>(fine?.75f+.10f*storm:.91f+.07f*storm))continue;
            const float life=fine?3.5f:2.8f;
            const float phase=hashUnit(gx,gy,seed^0x4c741fu);
            const float clock=now+phase*life;
            const int cycle=(int)std::floor(clock/life);
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.12f+.76f*hashUnit(gx,gy,own^0x14u))*cell;
            const float y0=(gy+.12f+.76f*hashUnit(gx,gy,own^0x25u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin),height=ceiling-floor;
            if(height<18.f)continue;
            const float start=floor+std::min(height-5.f,8.f+height*
                (.18f+.68f*hashUnit(gx,gy,own^0x41u)));
            const float vz=fine?-(3.f+7.f*hashUnit(gx,gy,own^0x63u)):
                -(11.f+15.f*hashUnit(gx,gy,own^0x54u));
            // Ice cuts across the air with sustained sideways motion. Powder
            // blows slower and broadens the nearby field between gusts.
            const float drift=wind*(fine?.35f:.61f)*(0.80f+.36f*gust);
            const float swirl=(fine?8.f:4.f)*std::sin(age*(fine?1.5f:2.2f)+phase*6.2831853f);
            const float dx=wx*drift*age-wy*swirl;
            const float dy=wy*drift*age+wx*swirl;
            const float x=x0+dx,y=y0+dy,z=start+vz*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            bool blocked=false;
            // Up to five conservative samples prevent wind transport through
            // indoor rooms (including a narrow roof along the swept path).
            for(int step=1;step<=5;++step) {
                const float u=float(step)/6.f;
                const sector_t *through=sectorAt(x0+dx*u,y0+dy*u);
                const float hz=start+vz*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   hz<=floorZ(through)+2.f||hz>=ceilZ(through)-2.f) {
                    blocked=true;break;
                }
            }
            if(blocked)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<12.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*2.7f,(life-age)*2.7f,
                       (reach-dist)/80.f,(dist-12.f)/18.f}),0.f,1.f);
            if(fade<.025f)continue;
            const float light=std::clamp(lighting(at),.22f,1.1f);
            const float hue=hashUnit(gx,gy,own^0x78u);
            const std::array<float,3> color=fine?
                std::array<float,3>{(.65f+.10f*hue)*(.75f+.25f*light),
                                    (.68f+.10f*hue)*(.75f+.25f*light),
                                    (.78f+.10f*hue)*(.75f+.25f*light)}:
                std::array<float,3>{(.72f+.13f*hue)*(.75f+.25f*light),
                                    (.83f+.10f*hue)*(.75f+.25f*light),
                                    (.98f+.02f*hue)*(.75f+.25f*light)};
            const float alpha=fade*(fine?.43f:.72f)*(0.82f+.18f*storm);
            // Separate bright head and dimmer trailing shard make a short,
            // oblique gust streak without adding any new renderer primitives.
            pixelBlock(particleVertices,camera,x,y,z,
                       fine?.75f:.77f,fine?.85f:1.80f,color,
                       false,nullptr,false,nullptr,alpha);
            if(!fine) {
                // Tail stays close to the validated head; the full path to
                // the current head was already conservatively checked.
                const float trail=2.3f+2.4f*gust;
                const float tx=x-wx*trail,ty=y-wy*trail,tz=z+.55f;
                const sector_t *tail=sectorAt(tx,ty);
                if(tail&&tail->ceilingpic==skyflatnum&&
                   tz>floorZ(tail)+2.f&&tz<ceilZ(tail)-2.f)
                    pixelBlock(particleVertices,camera,tx,ty,tz,
                               .48f,.90f,color,false,nullptr,false,nullptr,alpha*.35f);
                ++crystals;
            } else ++dust;
        }
    }
}

// D1: Inferno particles are ephemeral analytic billboards, not persistent actors.
// The original rain, dripping-window effects and wetness remain unchanged.
// One candidate per 48-unit world cell; hard caps bound all generated geometry.
// Each current position and its swept path must be under an actual sky ceiling.
void buildEpisodeAtmosphere(const Uniforms &camera) {
    if(episodeAtmosphereNow==int(doom_episode::Profile::Deimos)) {
        buildDeimosAtmosphere(camera);return;
    }
    if(episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)||
       camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=48.f,reach=390.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int ashLimit=doom_episode::ashBudget(density);
    const int emberLimit=doom_episode::emberBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const uint32_t salt=weatherSeed^0x4a71f2e9u;
    const int cx=(int)std::floor(camera.eye[0]/cell);
    const int cy=(int)std::floor(camera.eye[1]/cell);
    int ash=0,embers=0;
    for(int gy=cy-9;gy<=cy+9&&(ash<ashLimit||embers<emberLimit);++gy)
    for(int gx=cx-9;gx<=cx+9&&(ash<ashLimit||embers<emberLimit);++gx) {
        const uint32_t seed=hash32(uint32_t(gx)*0x632be59bu^
                                   uint32_t(gy)*0x85157af5u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool ember=kind!=0;
            if(ember?embers>=emberLimit:ash>=ashLimit)continue;
            const float selection=hashUnit(gx,gy,seed^(ember?0x874c13u:0x96e72bu));
            if(selection>(ember?0.20f:0.65f))continue;
            const float life=ember?5.5f:7.0f;
            const float phase=hashUnit(gx,gy,seed^0x1c74d35bu);
            const float clock=now+phase*life;
            const int cycle=(int)std::floor(clock/life);
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+0.10f+0.80f*hashUnit(gx,gy,own^0x18u))*cell;
            const float y0=(gy+0.10f+0.80f*hashUnit(gx,gy,own^0x29u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float height=ceiling-floor;
            if(height<16.f)continue;
            const float start=floor+std::min(height-5.f,8.f+height*
                (0.10f+0.65f*hashUnit(gx,gy,own^0x4bu)));
            const float velocity=ember?(6.f+6.f*hashUnit(gx,gy,own^0x5cu)):
                -(5.f+6.f*hashUnit(gx,gy,own^0x6du));
            const float speed=ember?7.f:12.f;
            const float turbulence=4.f*std::sin(age*1.4f+phase*6.2831853f);
            const float deltaX=wx*(speed*age)-wy*turbulence;
            const float deltaY=wy*(speed*age)+wx*turbulence;
            const float x=x0+deltaX,y=y0+deltaY,z=start+velocity*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            // Short conservative sweep: no particles traveling through rooms.
            bool blocked=false;
            for(int step=1;step<4;++step) {
                const float u=float(step)/4.f;
                const sector_t *through=sectorAt(x0+deltaX*u,y0+deltaY*u);
                const float hz=start+velocity*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   hz<=floorZ(through)+2.f||hz>=ceilZ(through)-2.f) {
                    blocked=true;break;
                }
            }
            if(blocked)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<14.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*1.6f,(life-age)*1.6f,
                         (reach-dist)/70.f,(dist-14.f)/25.f}),0.f,1.f);
            if(fade<0.025f)continue;
            const float light=std::clamp(lighting(at),0.22f,1.1f);
            const float hue=hashUnit(gx,gy,own^0x7eu);
            std::array<float,3> color=ember?
                std::array<float,3>{.76f+.19f*hue,.19f+.21f*hue,.035f+.07f*hue}:
                std::array<float,3>{.19f+.11f*hue,.13f+.08f*hue,.12f+.06f*hue};
            for(float &c:color)c*=ember?(.74f+.26f*light):(.50f+.50f*light);
            pixelBlock(particleVertices,camera,x,y,z,
                       ember?.48f:.85f,ember?.75f:.93f,color,
                       false,nullptr,false,nullptr,fade*(ember?.83f:.57f));
            if(ember)++embers;else ++ash;
        }
    }
}


} // namespace doom3d
int main(){
 using namespace doom3d;
 Uniforms camera{};camera.eye[2]=30.f;
 for(int profile: {1,2}) {
     int previous=0;
     for(int density=0;density<3;++density){
         settings.episodeParticles=density;
         episodeAtmosphereNow=profile;
         particleVertices.clear();particleColors.clear();
         buildEpisodeAtmosphere(camera);
         int drawn=int(particleVertices.size()/6);
         int max=profile==1?doom_episode::ashBudget(density)+doom_episode::emberBudget(density):
             2*doom_episode::crystalBudget(density)+doom_episode::frostDustBudget(density);
         if(drawn<1||drawn>max||drawn<previous){printf("bad profile %d density %d drawn %d max %d\n",profile,density,drawn,max);return 1;}
         if(particleColors.size()!=size_t(drawn))return 2;
         if(profile==2){
            bool foundPale=false;
            for(auto c:particleColors)if(c[2]>c[0]&&c[2]>c[1]){foundPale=true;break;}
            if(!foundPale)return 3;
         }
         previous=drawn;
         printf("profile=%d density=%d blocks=%d cap=%d\n",profile,density,drawn,max);
     }
 }
 particleVertices.clear();particleColors.clear();episodeAtmosphereNow=0;buildEpisodeAtmosphere(camera);
 if(!particleVertices.empty())return 4;
 particleVertices.clear();episodeAtmosphereNow=2;forceIndoor=true;buildEpisodeAtmosphere(camera);
 if(!particleVertices.empty())return 5;
 forceIndoor=false;particleVertices.clear();camera.effects[2]=1;buildEpisodeAtmosphere(camera);
 if(!particleVertices.empty())return 6;
 // Deimos blizzard stress test across several simulation phases.
 camera.effects[2]=0;forceIndoor=false;episodeAtmosphereNow=2;settings.episodeParticles=2;
 for(int k=0;k<30;++k) { seconds=k*.87f;particleVertices.clear();particleColors.clear();buildEpisodeAtmosphere(camera);
  const int n=int(particleVertices.size()/6);
  if(n>2*doom_episode::crystalBudget(2)+doom_episode::frostDustBudget(2))return 7;
  if(n<60){printf("too sparse at %.2f, n=%d\n",seconds,n);return 8;}
 }
 puts("PASS episode atmosphere geometry");return 0;
}
