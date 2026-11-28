#include "map_world.h"
#include <iostream>
#include <chrono>
#include <random>
#include <cstring>
#include <sstream>
#include <stdexcept>
using namespace lab;
static uint64_t digest=1469598103934665603ULL;
static void add(uint32_t u){digest=(digest^u)*1099511628211ULL;}
static void scalar(float v){uint32_t u;std::memcpy(&u,&v,4);add(u);}
static void vec(Vec v){scalar(v.x);scalar(v.y);scalar(v.z);}
static MapRegion room(int id,float xmin,float xmax){MapRegion r;r.sector=id;r.floor=0;r.ceiling=128;Vec p[]={{xmin,-128,0},{xmax,-128,0},{xmax,128,0},{xmin,128,0}};for(int i=0;i<4;++i)r.edges.push_back({p[i],p[(i+1)%4],i});return r;}
static MapSource source(){auto door=room(2,56,72);door.ceiling=0;door.movingCeiling=true;MapSource s;s.reset({room(0,-128,56),room(1,72,256),door});return s;}
static void volume(const Volume& v){for(float f:v.density)scalar(f);for(const auto& chunk:v.chunks){add(chunk.triangles.size());for(auto t:chunk.triangles){vec(t.a);vec(t.b);vec(t.c);vec(t.normal);add(t.damaged);}}}
static void hit(MapWorld& world,Vec a,Vec b,float range,bool mesh){Hit h;bool ok=world.ray(a,b,range,h,mesh);add(ok);if(ok){scalar(h.distance);vec(h.point);vec(h.normal);}}
int main(){auto begin=std::chrono::steady_clock::now();auto s=source();MapWorld world;world.reset(s);std::mt19937 rng(413);std::uniform_real_distribution<float> xy(-120,120),z(4,124),direction(-1,1);
 // Identical workload and seed in both versions. Use original test geometry.
 for(int phase=0;phase<5;++phase){
  if(phase==1)world.heights(2,0,96);
  if(phase==2)world.blast({64,0,96},24,2,1,96);
  if(phase==3)world.heights(2,0,64);
  if(phase==4){std::ostringstream out(std::ios::binary);world.save(out);std::istringstream in(out.str(),std::ios::binary);if(!world.load(in))return 2;}
  for(int i=0;i<3000;++i){Vec a{xy(rng),xy(rng),z(rng)},d{direction(rng),direction(rng),direction(rng)};hit(world,a,d,300,true);hit(world,a,d,300,false);}
  for(int x=-2;x<4;++x)for(int y=-2;y<2;++y)for(int zz=-1;zz<3;++zz)volume(world.tile({x,y,zz}).volume);
 }
 // Force more than the original 2,048 XY cache entries, with hot tiles revisited.
 for(int i=0;i<2400;++i){auto v=s.tile(20+i%60,20+i/60,0,false);scalar(v.density[0]);if(i%20==0)volume(s.tile(0,0,0));}
 auto stats=world.rayMeshStats();std::cout<<"digest="<<std::hex<<digest<<std::dec<<" hits="<<stats.hits<<" misses="<<stats.misses<<" evictions="<<stats.evictions<<" elapsed_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<'\n';
}
