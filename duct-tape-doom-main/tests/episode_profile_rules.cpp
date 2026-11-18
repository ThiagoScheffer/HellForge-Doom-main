#include "episode_atmosphere.h"
#include <cassert>
int main(){using namespace doom_episode;
 assert(select(0,true,true,true,3)==Profile::None);
 assert(select(1,true,true,true,3)==Profile::Inferno);
 assert(select(1,true,false,true,3)==Profile::None);
 assert(select(1,false,true,true,3)==Profile::None);
 assert(select(1,true,true,false,3)==Profile::None);
 assert(select(1,true,true,true,2)==Profile::None);
 assert(select(2,false,false,false,100)==Profile::Inferno);
 assert(ashBudget(0)==48&&ashBudget(1)==96&&ashBudget(2)==128);
 assert(emberBudget(0)==12&&emberBudget(1)==24&&emberBudget(2)==32);
}