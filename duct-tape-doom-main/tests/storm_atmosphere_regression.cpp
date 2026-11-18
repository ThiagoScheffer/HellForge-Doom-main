#include "platform/storm_rules.h"
#include "platform/storm_atmosphere.h"
#include "old_rules.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
static bool eq(float a,float b) {return std::fabs(a-b)<1e-6f;}
int main(){
 using namespace doom_storm;
 for(int mode=1;mode<=3;++mode){
   State current;doom_storm_a::State original;
   unsigned thunder=0,bolt=0,flashes=0;
   float oldFront=-1;
   for(int tic=0;tic<35*600;++tic){
     Frame c=advance(current,tic,35,mode,true,true,0x173da45u,true);
     auto b=doom_storm_a::advance(original,tic,35,mode,true,true,0x173da45u);
     assert(eq(c.flash,b.flash));
     assert(c.thunder==static_cast<Kind>(b.thunder));
     assert(eq(c.storm,b.storm));
     thunder+=c.thunder!=Kind::None;bolt+=c.bolt>0;flashes+=c.flash>0;
     Atmosphere a=atmosphere(tic,35,mode,true,true,0x3a22bbccu);
     auto repeat=atmosphere(tic,35,mode,true,true,0x3a22bbccu);
     assert(eq(a.front,repeat.front)&&eq(a.gust,repeat.gust));
     assert(a.front>=0 && a.front<=1.00001f);
     assert(a.gust>=0 && a.gust<=1.00001f);
     assert(a.rainScale>=0.75f&&a.rainScale<=1.30f);
     assert(a.windScale>=1 && a.windScale<=1.72f);
     assert(a.curtain>=0&&a.curtain<=1);
     assert(a.haze>=0&&a.haze<=0.22f);
     if(oldFront>=0)assert(std::abs(oldFront-a.front)<0.0012f);
     oldFront=a.front;
     Atmosphere disabled=atmosphere(tic,35,mode,true,false,0x3a22bbccu);
     assert(disabled.front==0&&disabled.curtain==0&&disabled.haze==0);
     assert(disabled.rainScale==1&&disabled.windScale==1);
     Atmosphere dry=atmosphere(tic,35,mode,false,true,0x3a22bbccu);
     assert(dry.curtain==0&&dry.haze==0);
   }
   assert(thunder>2&&bolt>0&&flashes>0);
   printf("mode %d passed: %u thunder, %u bolt frames, %u flash frames; audio/flash schedule unchanged\n",mode,thunder,bolt,flashes);
 }
 for(int i=0;i<1000;++i){
    auto a=atmosphere(i,35,0,true,true,42);
    assert(a.curtain==0&&a.haze==0&&a.rainScale==1&&a.windScale==1);
 }
 puts("Storm C atmosphere and bolt scheduler tests passed");
}
