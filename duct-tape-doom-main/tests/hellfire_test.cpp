#include "../platform/episode_atmosphere.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace doom_episode;
using namespace doom_episode::hellfire;
constexpr int rate=35;

static void check(bool condition,const char *message) {
    if(condition)return;
    std::fprintf(stderr,"FAIL: %s\n",message);
    std::exit(1);
}

static std::vector<int> simulate(std::uint32_t seed,bool outdoor) {
    State state;
    std::vector<int> attempts;
    for(int tic=0;tic<rate*750;++tic) {
        auto findSite=[&](std::uint32_t eventSeed) {
            attempts.push_back(tic);
            return outdoor?Site{160,64,0,192,12,0,0,eventSeed}:Site{};
        };
        const auto oldFall=state.fall;
        advance(state,tic,rate,1,seed,true,findSite,[](const Site &){return true;});
        if(!outdoor)check(state.fall.start<0&&state.fires[0].start<0,"no site skips the event");
        if(oldFall.start>=0&&tic==oldFall.end) {
            check(state.fall.start<0&&state.fires[0].start==tic,"impact starts ground fire");
            const int life=state.fires[0].end-tic;
            check(life>=6*rate&&life<=10*rate,"ground fire life");
            check(fireFade(state.fires[0],tic,rate)==1.f&&
                  fireFade(state.fires[0],state.fires[0].end-rate,rate)==.5f,"ground fire fade");
        }
        advance(state,tic,rate,1,seed,true,findSite,[](const Site &){return true;});
        check(state.fires[1].start<0,"natural cadence lets fires expire between events");
    }
    int previous=0;
    for(int tic:attempts) {
        check(tic-previous>=30*rate&&tic-previous<=70*rate,"attempt interval");
        previous=tic;
    }
    check(attempts.size()>=10,"attempts over several consecutive events");
    return attempts;
}

int main() {
    const auto attempts=simulate(12345,true);
    check(attempts==simulate(12345,true),"repeatable scheduling");
    check(attempts==simulate(12345,false),"site selection cannot affect cadence");
    check(attempts!=simulate(12346,true),"seed changes cadence");
    for(int p=0;p<=int(Profile::BloodHell);++p) {
        check(enabled(Profile(p),true)==(p==int(Profile::Inferno)),"Inferno only");
        check(!enabled(Profile(p),false),"software rendering excluded");
    }
    check(openColumn(true,true,128),"valid open sky column");
    check(!openColumn(false,true,128),"indoor column");
    check(!openColumn(true,true,64),"low sky ceiling");
    check(!openColumn(true,false,128),"unsupported floor");
    auto testDisk=[](float x,float y) {
        LandingDisk disk{x,y};
        disk.edge(0,0,160,0,true);
        disk.edge(160,0,160,160,true);
        disk.edge(160,160,0,160,true);
        disk.edge(0,160,0,0,true);
        disk.edge(64,70,96,70,false); // Internal wall.
        return disk;
    };
    check(testDisk(40,125).inside&&testDisk(40,125).clear,"clear landing footprint");
    check(!testDisk(220,80).inside,"outside map despite BSP sector assignment");
    check(!testDisk(15,125).clear,"ledge intersects footprint");
    check(!testDisk(80,85).clear,"wall intersects descent corridor");

    State state;
    auto site=[](std::uint32_t seed){return Site{160,64,0,192,12,0,0,seed};};
    auto valid=[](const Site &){return true;};
    auto step=[&](int tic,unsigned level=1,bool active=true) {
        advance(state,tic,rate,level,12345,active,site,valid);
    };
    step(0);
    state.next=1;
    step(1);
    state.next=2;
    step(2);
    check(state.fall.start==1,"active fall cannot be replaced");
    state.next=1+rate;
    step(1+rate);
    state.next=1+2*rate;
    step(1+2*rate);
    step(1+3*rate);
    check(state.fall.start<0&&state.fires[0].start==1+rate&&
          state.fires[1].start==1+2*rate,"third impact cannot evict either fire");
    step(state.fires[0].end);
    check(state.fires[0].start<0,"expired slot freed");
    state.next=state.last+1;
    step(state.next);
    check(state.fall.start>=0,"new event after expiration");
    advance(state,state.last+1,rate,1,12345,true,site,[](const Site &){return false;});
    check(state.fall.start<0&&state.fires[1].start<0,"changed support clears effects");
    state.next=state.last+1;
    step(state.next);
    step(state.last+1,2);
    check(state.fall.start<0&&state.fires[0].start<0&&state.event==0,
          "level serial change clears effects and reseeds cadence");
    state.next=state.last+1;
    step(state.next,2);
    step(state.last+1,2,false);
    check(state.fall.start<0&&state.next<0,"disabled profile clears state");
    step(100,2);
    state.next=101;
    step(101,2);
    step(0,2);
    check(state.fall.start<0&&state.next==interval(12345,rate),"simulation rewind resets state");
    std::printf("PASS: %zu deterministic attempts, landing rules and bounded lifecycle\n",attempts.size());
}
