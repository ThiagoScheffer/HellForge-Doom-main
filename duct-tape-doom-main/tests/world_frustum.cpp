#include "../platform/world_frustum.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
using namespace doom_world_frustum;
static void check(bool pass,const char *message) {
    if(!pass){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
int main() {
    Camera c{{0,0,32},{0,-1,0},{0,0,1},{1,0,0},1,1,1};
    Frustum f(c);
    check(f.outside({90,200,110,220}),"outside horizontal view");
    check(f.outside({-110,-10,-90,10}),"behind camera");
    check(!f.outside({90,90,110,110}),"crossing frustum edge");
    check(!f.outside({90,110,110,120}),"touching edge retained");
    check(!f.outside({-1,-1,1,1}),"eye inside bounds");
    check(!f.outside({0,0,-1,1}),"invalid bounds fail open");
    auto invalid=c;invalid.horizontal=std::numeric_limits<double>::quiet_NaN();
    check(!Frustum(invalid).outside({90,200,110,220}),"invalid projection fail open");
    invalid=c;invalid.up.z=std::numeric_limits<double>::infinity();
    check(!Frustum(invalid).outside({90,200,110,220}),"invalid basis fail open");
    // A very wide projection must retain a point excluded by a narrower one.
    auto wide=c;wide.horizontal=.2;
    check(!Frustum(wide).outside({90,200,110,220}),"actual projection respected");
    auto vertical=c;vertical.forward={0,0,1};vertical.up={-1,0,0};
    check(!Frustum(vertical).outside({-1000,-1000,-900,-900}),"vertical view retains any height");
    // Differential against points inside the shader's actual homogeneous side
    // inequalities. Exercise pitch, yaw, roll, aspect/FOV, distant points and
    // cameras inside boxes. A box containing even one visible point MUST stay.
    std::mt19937 rng(8675309);std::uniform_real_distribution<double> unit(0,1);
    for(int i=0;i<100000;++i) {
        double yaw=unit(rng)*6.283185307,pitch=(unit(rng)-.5)*3.141592654,roll=unit(rng)*6.283185307;
        Vec right{std::sin(yaw),-std::cos(yaw),0};
        Vec up{-std::cos(yaw)*std::sin(pitch),-std::sin(yaw)*std::sin(pitch),std::cos(pitch)};
        Camera a{{(unit(rng)-.5)*60000,(unit(rng)-.5)*60000,(unit(rng)-.5)*10000},
            {right.x*std::cos(roll)+up.x*std::sin(roll),right.y*std::cos(roll)+up.y*std::sin(roll),right.z*std::cos(roll)+up.z*std::sin(roll)},
            {up.x*std::cos(roll)-right.x*std::sin(roll),up.y*std::cos(roll)-right.y*std::sin(roll),up.z*std::cos(roll)-right.z*std::sin(roll)},
            {std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)},.1+unit(rng)*5,.1+unit(rng)*5,1};
        double z=1+unit(rng)*32767,x=(unit(rng)*2-1)*z/a.horizontal,y=(unit(rng)*2-1)*z/a.vertical;
        double px=a.eye.x+a.forward.x*z+a.right.x*x+a.up.x*y;
        double py=a.eye.y+a.forward.y*z+a.right.y*x+a.up.y*y;
        float radius=float(unit(rng)*512);
        check(!Frustum(a).outside({float(px)-radius,float(py)-radius,float(px)+radius,float(py)+radius}),"visible shader point falsely rejected");
    }
    std::puts("PASS world frustum: boundaries, fail-open, vertical view, and 100000 shader-visible points with pitch/roll/FOV");
}
