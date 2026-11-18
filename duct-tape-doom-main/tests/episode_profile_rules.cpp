#include "episode_atmosphere.h"
#include <cassert>
int main(){using namespace doom_episode;
 assert(select(0,true,true,true,2)==Profile::None);
 assert(select(1,true,true,true,3)==Profile::Inferno);
 assert(select(1,true,true,true,2)==Profile::Deimos);
 assert(select(1,true,true,true,1)==Profile::None);
 assert(select(1,true,false,true,2)==Profile::None);
 assert(select(1,false,true,true,2)==Profile::None);
 assert(select(1,true,true,false,2)==Profile::None);
 assert(select(2,false,false,false,200)==Profile::Inferno);
 assert(select(3,false,false,false,200)==Profile::Deimos);
 for(int i=0;i<3;i++) {assert(crystalBudget(i)>0&&frostDustBudget(i)>0);}
 assert(crystalBudget(0)==180&&crystalBudget(1)==320&&crystalBudget(2)==480);
 assert(frostDustBudget(0)==80&&frostDustBudget(1)==144&&frostDustBudget(2)==220);
 assert(ashBudget(1)==96&&emberBudget(1)==24);
 for(int sec=0;sec<600;sec+=3) {
  auto b=deimosBlizzard(sec*35,35,0x11443u);
  assert(b.front>=0&&b.front<=1&&b.gust>=0&&b.gust<=1);
  assert(b.intensity>=.68f&&b.intensity<=1.f&&b.windScale>=1.2f&&b.windScale<=2.1f);
  auto r=deimosBlizzard(sec*35,35,0x11443u);
  assert(r.gust==b.gust&&r.front==b.front);
 }
 assert(deimosBlizzard(-1,35,0u).intensity==0.f);
}
