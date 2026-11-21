#include "../platform/episode_atmosphere.h"
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
 assert(ashBudget(0)==64&&ashBudget(1)==128&&ashBudget(2)==184);
 assert(emberBudget(0)==16&&emberBudget(1)==32&&emberBudget(2)==44);
 for(int i=0;i<3;++i) {
  for(int total:{ashBudget(i),emberBudget(i)}) {
   const auto cap=inferno::layerBudgets(total);
   assert(cap[0]>0&&cap[1]>0&&cap[2]>0&&cap[0]+cap[1]+cap[2]==total);
  }
 }
 const auto wind=inferno::wind(0x11443u);
 const auto repeat=inferno::wind(0x11443u);
 const auto other=inferno::wind(0x11444u);
 assert(wind.offset!=other.offset);
 const float gustStart=wind.period-wind.offset;
 const float quiet=wind.travel(gustStart-.01f)-wind.travel(gustStart-.02f);
 const float peak=wind.travel(gustStart+1.405f)-wind.travel(gustStart+1.395f);
 assert(std::abs(quiet)<.0001f&&peak>.0099f&&peak<.0101f);
 assert(std::abs(wind.travel(gustStart+.001f)-wind.travel(gustStart-.001f))<.001f);
 for(int i=0;i<3500;++i) {
  const float t=i/35.f;
  const float distance=wind.travel(t+.01f)-wind.travel(t);
  assert(wind.travel(t)==repeat.travel(t));
  assert(distance>=-1e-5f&&distance<.0101f);
 }
 using inferno::Point;
 auto sky=[](int x,int y,float a,float b) {
  return x>=0&&y==0&&x!=7&&std::min(a,b)>2.f&&std::max(a,b)<60.f;
 };
 assert(inferno::skyChord(Point{1,1,8},Point{29,1,12},0,0,5,sky));
 assert(!inferno::skyChord(Point{1,1,8},Point{79,1,12},0,0,5,sky));
 assert(!inferno::skyChord(Point{79,1,12},Point{1,1,8},0,0,5,sky));
 assert(!inferno::skyChord(Point{1,1,8},Point{29,1,65},0,0,5,sky));
 assert(!inferno::skyChord(Point{1,1,8},Point{-2,1,12},0,0,5,sky));
 for(int sec=0;sec<600;sec+=3) {
  auto b=deimosBlizzard(sec*35,35,0x11443u);
  assert(b.front>=0&&b.front<=1&&b.gust>=0&&b.gust<=1);
  assert(b.intensity>=.68f&&b.intensity<=1.f&&b.windScale>=1.2f&&b.windScale<=2.1f);
  auto r=deimosBlizzard(sec*35,35,0x11443u);
  assert(r.gust==b.gust&&r.front==b.front);
 }
 assert(deimosBlizzard(-1,35,0u).intensity==0.f);
}
