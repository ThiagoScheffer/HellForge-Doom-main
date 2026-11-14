#include "../platform/sprite_frustum.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace doom_sprite_frustum;
static void check(bool ok,const char *s){if(!ok){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}std::printf("PASS %s\n",s);}
int main(){
    Camera c{{0,0,32},{0,-1,0},{0,0,1},{1,0,0},1,1,1};
    check(!outside({{90,-10,0},{110,10,64}},c),"visible body retained");
    check(outside({{90,200,0},{110,220,64}},c),"wholly outside side plane rejected");
    check(outside({{-110,-10,0},{-90,10,64}},c),"wholly behind view rejected by side planes");
    check(!outside({{90,90,0},{110,110,64}},c),"crossing side edge retained");
    check(!outside({{90,110,0},{110,120,64}},c),"boundary tolerance retains touching artwork");
    check(!outside({{0,1000,0},{2,1100,64}},c),"near-plane crossing retained");
    check(!outside({{0,0,0},{0,0,std::numeric_limits<double>::quiet_NaN()}},c),"invalid bounds fail open");
    auto invalid=c;invalid.horizontal=std::numeric_limits<double>::infinity();check(!outside({{90,200,0},{110,220,64}},invalid),"invalid projection fails open");
    auto b=artwork(100,300,0,600,100,500,80,c.right,0,128,true);
    check(b.low.y==200&&b.high.y==800,"asymmetric artwork offsets retained");
    b=artwork(100,300,0,600,100,200,80,c.right,0,128,true);
    check(!outside(b,c),"replacement artwork beyond collision radius remains visible");
    b=artwork(100,0,0,40,100,20,80,c.right,32,200,true);check(b.low.z==32&&b.high.z==132,"floor fitting bounds match placement");
    b=artwork(100,0,180,40,100,20,80,c.right,32,200,true);check(b.low.z==100&&b.high.z==200,"ceiling fitting bounds match placement");
    Bounds hidden{{90,200,0},{110,220,64}};include(hidden,{{90,90,-1},{110,210,1}});check(!outside(hidden,c),"shadow-only intersection retains actor");
    auto up=c;up.forward={0,0,1};up.up={-1,0,0};check(!outside({{-10,-10,100},{10,10,120}},up),"vertical camera orientation supported");
    auto wide=c;wide.horizontal=.2;check(!outside({{90,200,0},{110,220,64}},wide),"wide FOV expands view");
    check(shadowReach()>=shadowLengthMax+shadowWidthMax*shadowScaleMax*.5f,"shadow envelope follows shared projection limits");
}
