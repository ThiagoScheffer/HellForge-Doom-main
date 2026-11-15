#include "map_world.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace lab;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static MapRegion room(int id,float left,float right){
 MapRegion r;r.sector=id;r.floor=0;r.ceiling=128;
 Vec p[]={{left,-256,0},{right,-256,0},{right,256,0},{left,256,0}};
 for(int i=0;i<4;++i)r.edges.push_back({p[i],p[(i+1)%4],i});return r;
}
static MapSource source(){MapSource s;s.reset({room(0,-256,56),room(1,72,320)});return s;}
static bool same(Vec a,Vec b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static bool compare(MapWorld& ordinary,MapWorld& cached,Vec start,Vec direction,float range){
 Hit a{},b{};a.distance=b.distance=-42;
 bool hit=ordinary.ray(start,direction,range,a);
 check(hit==cached.ray(start,direction,range,b,true),"Cached ray changed hit/miss");
 check(a.distance==b.distance&&same(a.point,b.point)&&same(a.normal,b.normal),"Cached ray changed exact hit data");
 return hit;
}
static void sweep(MapWorld& ordinary,MapWorld& cached){
 for(int i=0;i<80;++i){
  float y=float((i*73)%600)-300,z=float((i*37)%180)-20;
  compare(ordinary,cached,{-200,y,z},{1,float(i%7-3)*.11f,float(i%5-2)*.09f},600);
  compare(ordinary,cached,{280,y,z},{-1,float(i%5-2)*.13f,float(i%7-3)*.07f},600);
 }
 for(float z:{-.001f,0.f,.001f,64.f,127.999f,128.f,128.001f})
  compare(ordinary,cached,{0,0,z},{1,0,0},300);
}
int main(){try{
 // Non-axis-aligned tetrahedral surfaces and large world coordinates exercise
 // the indexed bounds against the original triangle loop, including normals.
 for(Vec origin:{Vec{-16,-16,-16},Vec{14000,-12000,-2000}}){
  Volume v;v.create(9,9,9,4,origin);
  for(size_t i=0;i<v.density.size();++i)v.base[i]=v.density[i]=std::sin(float(i)*1.73f)*7;
  v.rebuild();RayMesh mesh;check(mesh.build(v),"Indexed fixture failed to build");
  for(int i=0;i<512;++i){
   Vec start=origin+Vec{-8,float((i*31)%60)-14,float((i*17)%60)-14};
   Vec dir{1,float(i%19-9)*.07f,float(i%23-11)*.06f};Hit a{},b{};
   bool hit=v.ray(start,dir,100,a);check(hit==mesh.ray(start,dir,100,b),"Indexed bounds changed a sloped hit");
   check(a.distance==b.distance&&same(a.point,b.point)&&same(a.normal,b.normal),"Indexed bounds changed sloped hit bits");
  }
 }
 MapWorld ordinary,cached;ordinary.reset(source());cached.reset(source());
 sweep(ordinary,cached);
 auto initial=cached.rayMeshStats();sweep(ordinary,cached);auto warm=cached.rayMeshStats();
 check(warm.hits>initial.hits&&warm.misses==initial.misses,"Warm sight queries regenerated meshes");
 // Ordinary collision-cache turnover must not invalidate unchanged sight data.
 for(int i=0;i<2050;++i)cached.tile({i+10,100,100});
 auto beforeReuse=cached.rayMeshStats();sweep(ordinary,cached);
 check(cached.rayMeshStats().misses==beforeReuse.misses,"Collision eviction invalidated unchanged sight meshes");
 // Moving observer/target positions must still use current geometry.
 for(int i=0;i<40;++i)compare(ordinary,cached,{-100.f+i,float(i-20),64},{1,0,0},200);
 check(ordinary.blast({64,0,64},24)&&cached.blast({64,0,64},24),"Destruction fixture failed");
 check(!compare(ordinary,cached,{0,0,64},{1,0,0},200),"Destroyed wall retained a stale sight blocker");
 sweep(ordinary,cached);
 // Close/reopen the destination sector, testing all vertical target rays.
 compare(ordinary,cached,{-200,0,64},{1,0,0},1);
 auto remote=cached.rayMeshStats();
 check(ordinary.heights(1,0,24)&&cached.heights(1,0,24),"Door fixture failed");
 compare(ordinary,cached,{-200,0,64},{1,0,0},1);
 check(cached.rayMeshStats().misses==remote.misses,"Door motion discarded distant sight meshes");
 check(compare(ordinary,cached,{0,0,64},{1,0,0},200),"Closing sector reused a stale opening");
 sweep(ordinary,cached);
 ordinary.heights(1,0,128);cached.heights(1,0,128);sweep(ordinary,cached);
 std::ostringstream save(std::ios::binary);check(ordinary.save(save),"Save failed");
 ordinary.reset(source());cached.reset(source());sweep(ordinary,cached);
 check(compare(ordinary,cached,{0,0,64},{1,0,0},200),"Map reset retained an old breach");
 std::istringstream a(save.str(),std::ios::binary),b(save.str(),std::ios::binary);
 check(ordinary.load(a)&&cached.load(b),"Saved destruction failed to load");
 check(!compare(ordinary,cached,{0,0,64},{1,0,0},200),"Load retained intact cached geometry");
 sweep(ordinary,cached);
 ordinary.crumble(4);cached.crumble(4);sweep(ordinary,cached);
 {
  MapWorld moving,reference;moving.reset(source());reference.reset(source());
  check(moving.blast({-100,0,-2},12,0,0,0)&&reference.blast({-100,0,-2},12,0,0,0),"Anchored cut fixture failed");
  compare(reference,moving,{-100,0,32},{0,0,-1},128);
  moving.heights(0,-32,128);reference.heights(0,-32,128);
  compare(reference,moving,{-100,0,32},{0,0,-1},128);
  moving.heights(0,0,128);reference.heights(0,0,128);
  compare(reference,moving,{-100,0,32},{0,0,-1},128);
 }
 auto stats=cached.rayMeshStats();
 check(stats.bytes<=64*1024*1024&&stats.tiles<=16384,"Sight mesh budget exceeded");
 // Exercise the real LRU capacity with cheap empty-space tiles, then revisit an
 // evicted tile. The uncached and cached paths must still agree.
 MapWorld empty;MapSource noRooms;empty.reset(noRooms);
 for(int i=0;i<16390;++i){Hit hit{};check(!empty.ray({float(i)*64+32,32,32},{1,0,0},1,hit,true),"Empty tile unexpectedly hit");}
 stats=empty.rayMeshStats();check(stats.evictions>0&&stats.tiles==16384,"Sight mesh LRU did not evict");
 Hit miss{};check(!empty.ray({32,32,32},{1,0,0},1,miss,true),"Evicted empty tile changed ray result");
 std::cout<<"PASS exact sight meshes: moving rays, warm reuse, destruction, sector motion, reset, save/load, crumble and bounded storage\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
