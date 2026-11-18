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
float rainWind[2]={35.f,-20.f};
std::vector<int> seamCells(4,0),particleVertices;
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
int ashRendered=0;
void pixelBlock(std::vector<int>&out,const Uniforms&,float,float,float,float,float,const std::array<float,3>&,
                bool,const std::array<float,3>*,bool,void*,float){out.insert(out.end(),{1,2,3,4,5,6});}
// D1: Inferno particles are ephemeral analytic billboards, not persistent actors.
// The original rain, dripping-window effects and wetness remain unchanged.
// One candidate per 48-unit world cell; hard caps bound all generated geometry.
// Each current position and its swept path must be under an actual sky ceiling.
void buildEpisodeAtmosphere(const Uniforms &camera) {
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
 Uniforms camera{};camera.eye[0]=0;camera.eye[1]=0;camera.eye[2]=30;
 settings.episodeParticles=0;buildEpisodeAtmosphere(camera);
 auto low=particleVertices.size()/6;
 if(low==0||low>60){printf("low error %zu\n",low);return 1;}
 particleVertices.clear();settings.episodeParticles=1;buildEpisodeAtmosphere(camera);
 auto normal=particleVertices.size()/6;
 if(normal<low||normal>120){printf("normal error %zu\n",normal);return 2;}
 particleVertices.clear();settings.episodeParticles=2;buildEpisodeAtmosphere(camera);
 auto high=particleVertices.size()/6;
 if(high<normal||high>160){printf("high error %zu\n",high);return 3;}
 particleVertices.clear();episodeAtmosphereNow=0;buildEpisodeAtmosphere(camera);
 if(!particleVertices.empty()){puts("off rendered");return 4;}
 episodeAtmosphereNow=1;forceIndoor=true;buildEpisodeAtmosphere(camera);
 if(!particleVertices.empty()){puts("indoor rendered");return 5;}
 printf("PASS particle geometry low=%zu normal=%zu high=%zu, indoor/off=0\n",low,normal,high);
 return 0;
}
