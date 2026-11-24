#include "../platform/gore_giblets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace hellforge::giblets;

static void check(bool ok,const char *message) {
    if(!ok){std::fprintf(stderr,"%s\n",message);std::exit(1);}
}
struct Fixture {
    Surface surface=Surface::Floor;
    int impacts=0;
    bool immediate=false;
    bool invalid=false;
    Impact last{};
};
static bool trace(void *context,Vec3 a,Vec3 b,float radius,TraceHit &hit) {
    auto &f=*static_cast<Fixture*>(context);
    if(f.immediate) {
        hit={a,a,{0,0,-1},1,{f.surface,-1,-1,-1,0},true};
        if(f.invalid)hit.fraction=std::numeric_limits<float>::quiet_NaN();
        return true;
    }
    if(b.z>radius||b.z>=a.z)return false;
    const float t=std::clamp((a.z-radius)/(a.z-b.z),0.f,1.f);
    const Vec3 center={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,radius};
    hit={center,{center.x,center.y,0},{0,0,1},t,{f.surface,-1,-1,-1,4},true};
    return true;
}
static void impact(void *context,const Impact &hit) {
    auto &f=*static_cast<Fixture*>(context);
    ++f.impacts;f.last=hit;
}
struct Draw {
    unsigned count=0,settled=0;
    float sum=0;
};
static void draw(void *context,const Visual &v) {
    auto &result=*static_cast<Draw*>(context);
    ++result.count;
    result.sum+=v.center.x+v.center.y+v.center.z+v.rotation;
    if(v.resting) {
        ++result.settled;
        check(v.surface.type==Surface::Floor&&v.surface.sector==4&&v.contact.z==0&&v.normal.z>0,
              "B poses must retain the floor handle and contact point");
    }
}

int main() {
    Burst burst;
    burst.origin={0,0,30};burst.eligible=true;burst.eventId=73;
    burst.kind=Kind::Viscera;burst.tint=Tint::Blue;burst.pieces=8;
    System sim,other;
    Fixture a,b;
    const Host host={&a,trace,impact},otherHost={&b,trace,impact};
    check(sim.emit(burst)==8&&other.emit(burst)==8&&sim.emit(burst)==0,"Deterministic burst and event deduplication");
    sim.advance(0,host);
    sim.advance(std::numeric_limits<float>::quiet_NaN(),host);
    check(sim.stats().traces==0,"Paused and invalid time must not trace");
    for(int i=0;i<45;++i) {
        sim.advance(1.f/70.f,host);other.advance(1.f/70.f,otherHost);
        Draw one,two;sim.forEachVisual(draw,&one);other.forEachVisual(draw,&two);
        check(one.count==two.count&&one.sum==two.sum,"Cosmetic seed and fixed step are deterministic");
    }
    Draw floor;sim.forEachVisual(draw,&floor);
    check(floor.settled>0&&a.impacts>0&&a.last.eventId==73&&a.last.contact.z==0&&
          a.last.center.z>0&&a.last.tint==Tint::Blue,"Floor impact preserves event, tint, and separate swept center");
    sim.clear();check(!sim.activeCount()&&!sim.hasActiveEvent(73)&&!sim.stats().traces,"Clear resets history and metrics");
    check(sim.emit(burst)==8,"Map clear releases event identity");
    burst.mapKillCount=200;burst.eventId++;
    check(sim.emit(burst)==0,"Population gate");
    burst.mapKillCount=199;burst.pieces=255;
    check(sim.emit(burst)==12,"Burst ceiling");
    for(int i=0;i<8;++i){++burst.eventId;sim.emit(burst);}
    check(sim.activeCount()==64,"Shared texture simulation cap");
    sim.advance(5,host);
    check(sim.stats().traces<=64*8&&sim.stats().emittedImpacts<=12,"Bounded catch-up work and notifications");

    sim.clear();burst.pieces=1;
    Fixture ceiling;
    ceiling.surface=Surface::Ceiling;ceiling.immediate=true;
    const Host ceilingHost={&ceiling,trace,impact};
    sim.emit(burst);sim.advance(1.f/70.f,ceilingHost);
    Draw air;sim.forEachVisual(draw,&air);
    check(air.count==1&&!air.settled,"A ceiling contact cannot choose a floor pose");
    ceiling.invalid=true;sim.advance(1.f/70.f,ceilingHost);
    check(!sim.activeCount(),"Invalid fractions fail closed");
    for(auto surface:{Surface::Sky,Surface::Liquid,Surface::Void}) {
        sim.clear();ceiling.invalid=false;ceiling.surface=surface;ceiling.impacts=0;
        sim.emit(burst);sim.advance(1.f/70.f,ceilingHost);
        check(!sim.activeCount()&&!ceiling.impacts,"Sky, liquid and void suppress marks");
    }
    sim.clear();a={};burst.kind=Kind::Bone;
    sim.emit(burst);
    for(int i=0;i<70;++i)sim.advance(1.f/70.f,host);
    check(!a.impacts,"Bone never submits a blood impact");
    std::puts("Giblet fixed-step, contact, event, and budget tests passed");
}
