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

struct WallFixture {
    bool markable=true,validAnchor=true,validSurface=true,collide=true,slanted=false;
    int traces=0,impacts=0,resolutions=0;
    Impact last{};
};

static bool wallTrace(void *context,Vec3 a,Vec3,float radius,TraceHit &hit) {
    auto &f=*static_cast<WallFixture*>(context);
    ++f.traces;
    if(!f.collide)return false;
    Vec3 normal=f.slanted?Vec3{-.8f,0,.6f}:Vec3{-1,0,0};
    hit={a,{a.x+radius,a.y,a.z},normal,0,{Surface::Wall,1,0,0},f.markable};
    return true;
}
static bool wallAttach(void *context,const TraceHit &hit,WallAnchor &anchor) {
    const auto &f=*static_cast<WallFixture*>(context);
    anchor={hit.surface,16.f,hit.contact.z,7};
    if(!f.validAnchor)anchor.surface.line=-1;
    return true;
}
static bool wallResolve(void *context,const WallAnchor &anchor,float sag,Vec3 &contact,Vec3 &normal) {
    auto &f=*static_cast<WallFixture*>(context);
    ++f.resolutions;
    if(!f.validSurface)return false;
    contact={32.f,anchor.along,anchor.offsetZ-sag};
    normal={-1,0,0};
    return true;
}
static void wallImpact(void *context,const Impact &hit) {
    auto &f=*static_cast<WallFixture*>(context);
    ++f.impacts;f.last=hit;
}

static void wallAttachmentTests() {
    constexpr float step=1.f/70.f;
    System sim,other;
    WallFixture fixture,twin;
    const Host host={&fixture,wallTrace,wallImpact,nullptr,wallAttach,wallResolve};
    const Host twinHost={&twin,wallTrace,wallImpact,nullptr,wallAttach,wallResolve};
    const auto visual=[](const System &system) {
        Visual result{};
        system.forEachVisual(+[](void *out,const Visual &v){*static_cast<Visual*>(out)=v;},&result);
        return result;
    };
    Burst burst;
    burst.origin={0,0,30};burst.eligible=true;burst.eventId=101;burst.pieces=1;
    burst.kind=Kind::Viscera;burst.brief=true;

    check(!burst.sticky&&sim.emit(burst)==1,"Sticky fragments are opt-in");
    sim.advance(step,host);
    check(sim.activeCount()==1&&!sim.attachedCount()&&fixture.impacts==1&&fixture.last.cling&&!fixture.last.attached,
          "Ordinary wall impacts bounce and retain the legacy cling suggestion");

    burst.sticky=true;burst.softDeformation=true;
    sim.clear();fixture={};sim.emit(burst);sim.advance(step,host);
    auto first=visual(sim);
    check(first.attachment==Attachment::Wall&&!first.resting&&first.squash>0&&fixture.last.attached,
          "A soft wall contact becomes a compressed wall attachment");
    check(fixture.impacts==1&&sim.stats().emittedImpacts==1,"Attachment emits only the original impact");
    sim.advance(0,host);
    check(visual(sim).attachedAge==first.attachedAge&&fixture.resolutions==0,"Pause freezes attachment age");
    other.emit(burst);other.advance(step,twinHost);
    for(int i=0;i<28;++i){sim.advance(step,host);other.advance(step,twinHost);}
    auto a=visual(sim),b=visual(other);
    check(a.attachment==b.attachment&&a.contact.z==b.contact.z&&a.sag==b.sag&&a.squash==b.squash&&a.sag>0,
          "Identical seeds and hosts produce identical attachment deformation");
    check(fixture.traces==1&&fixture.impacts==1,"Attached pieces resolve anchors without ballistic sweeps");
    for(int i=0;i<70&&sim.attachedCount();++i)sim.advance(step,host);
    check(sim.activeCount()==1&&fixture.traces==1&&fixture.impacts==1,
          "An expired attachment detaches without another impact");
    fixture.collide=false;sim.advance(step,host);
    check(fixture.traces==2&&visual(sim).attachment==Attachment::None,"Detached pieces resume flight");
    for(int i=0;i<6;++i)sim.advance(step,host);
    fixture.collide=true;fixture.slanted=true;sim.advance(step,host);
    check(!sim.attachedCount()&&fixture.impacts==2,"A detached piece may collide again but cannot reattach");

    sim.clear();fixture={};sim.emit(burst);sim.advance(step,host);
    fixture.validSurface=false;sim.advance(step,host);
    check(!sim.activeCount()&&!sim.attachedCount(),"Invalidated wall anchors discard the fragment");
    sim.clear();
    check(!sim.hasActiveEvent(burst.eventId)&&!sim.stats().traces,"Clearing releases attachments and event history");

    for(int invalid=0;invalid<4;++invalid) {
        sim.clear();fixture={};
        burst.kind=invalid==0?Kind::Bone:Kind::Viscera;
        fixture.markable=invalid!=1;fixture.validAnchor=invalid!=2;
        Host selected=host;
        if(invalid==3)selected.resolveWall=nullptr;
        sim.emit(burst);sim.advance(step,selected);
        check(sim.activeCount()==1&&!sim.attachedCount(),"Hard material, invalid walls and absent host callbacks cannot attach");
        if(invalid<2)check(!fixture.impacts,"Bone and unmarkable walls submit no impact");
    }

    sim.clear();fixture={};burst.kind=Kind::Viscera;burst.maxActive=1;
    sim.emit(burst);sim.advance(step,host);
    check(sim.attachedCount()==1,"Fixture must attach before slot reuse");
    burst.sticky=false;burst.softDeformation=false;++burst.eventId;
    check(sim.emit(burst)==1&&sim.activeCount()==1&&!sim.stationaryCount(),"A new burst may evict an attached piece");
    auto fresh=visual(sim);
    check(fresh.attachment==Attachment::None&&fresh.attachedAge==0&&fresh.squash==0&&fresh.sag==0&&fresh.anchor.texture==-1,
          "Reused slots reset every attachment and deformation field");

    sim.clear();fixture={};burst.sticky=true;burst.softDeformation=true;burst.maxActive=64;burst.pieces=12;
    Fixture floorFixture;
    const Host floorHost={&floorFixture,trace,impact};
    for(int i=0;i<4;++i){++burst.eventId;sim.emit(burst);}
    for(int i=0;i<70;++i)sim.advance(step,floorHost);
    check(sim.restingCount()==System::MaxResting,"Fill the shared stationary pool with floor pieces");
    const auto oldEvictions=sim.stats().evictedResting;
    for(int i=0;i<4;++i){++burst.eventId;sim.emit(burst);}
    sim.advance(step,host);
    check(sim.attachedCount()==System::MaxAttached&&sim.stationaryCount()==System::MaxResting&&
          sim.activeCount()<=System::MaxPieces&&sim.stats().evictedResting>=oldEvictions+System::MaxAttached,
          "Wall attachments share the existing stationary and active budgets");
    check(fixture.impacts<=System::MaxImpactEventsPerAdvance,"Wall contacts preserve the per-advance impact budget");
}

int main() {
    wallAttachmentTests();
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
    for(int i=0;i<15*70;++i)sim.advance(1.f/70.f,host);
    check(sim.restingCount()>0,"Settled viscera remain after fifteen game seconds");
    for(int i=0;i<13*70;++i)sim.advance(1.f/70.f,host);
    check(!sim.activeCount(),"Settled viscera expire within thirty seconds");
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
    for(int i=0;i<100;++i)sim.advance(1.f/70.f,host);
    check(sim.restingCount()==System::MaxResting&&sim.activeCount()==System::MaxResting&&sim.stats().evictedResting>0,
          "The resting subcap evicts old debris from the same pool");
    burst.maxActive=24;burst.pieces=12;burst.eventId++;
    const auto oldEvictions=sim.stats().evictedResting;
    check(sim.emit(burst)==12&&sim.activeCount()==24&&sim.restingCount()==12&&sim.stats().evictedResting==oldEvictions+12,
          "Fresh bursts replace old grounded pieces when the shared capacity has no spare slot");
    burst.maxActive=64;

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
