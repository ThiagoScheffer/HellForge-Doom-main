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
 assert(crystalBudget(0)==64&&crystalBudget(1)==112&&crystalBudget(2)==144);
 assert(frostDustBudget(0)==16&&frostDustBudget(1)==32&&frostDustBudget(2)==48);
 assert(ashBudget(1)==96&&emberBudget(1)==24);
}
