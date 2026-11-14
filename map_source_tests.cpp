#include "map_source.h"
#include "duct-tape-doom-main/platform/large_platform_shield.h"
#include <iostream>
#include <stdexcept>
using namespace lab;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void loop(MapRegion& r,std::initializer_list<Vec> points){
 std::vector<Vec> p(points);
 for(size_t i=0;i<p.size();++i)r.edges.push_back({p[i],p[(i+1)%p.size()],int(i)});
}
static MapRegion box(int id,float x0,float y0,float x1,float y1,float floor,float ceiling){
 MapRegion r;r.sector=id;r.floor=floor;r.ceiling=ceiling;
 loop(r,{{x0,y0,0},{x1,y0,0},{x1,y1,0},{x0,y1,0}});return r;
}
int main(){try{
 MapSource source;
 auto left=box(0,-128,-64,0,64,0,128);
 auto right=box(1,0,-64,128,64,0,128);
 source.reset({left,right});
 check(source.sample({0,0,40})<0,"Shared linedef created a solid seam");
 check(source.sample({32,0,-4})>0,"Floor missing");
 check(source.sample({32,0,132})>0,"Ceiling missing");
 auto a=source.tile(-1,0,0),b=source.tile(0,0,0);
 for(int z=0;z<a.nz;++z)for(int y=0;y<a.ny;++y)
  check(a.density[a.index(16,y,z)]==b.density[b.index(0,y,z)],"Tile edge mismatch");
 for(int z=0;z<a.nz;++z)for(int y=0;y<a.ny;++y)for(int x=0;x<a.nx;++x)
  check(a.density[a.index(x,y,z)]==source.sample(a.point(x,y,z)),"Tile and point sampling disagree");
 Hit hit;
 check(!a.ray({-32,20,40},{1,0,0},50,hit),"Internal tile edge became a wall");
 check(!b.ray({0,20,40},{1,0,0},40,hit),"Sector join blocks ray");
 check(source.heights(1,0,0),"Door sector missing");
 check(source.sample({32,0,40})>0,"Closed door sector is air");
 source.heights(1,0,128);
 check(source.sample({32,0,40})<0,"Raised door did not open");
 source.heights(1,24,128);
 check(source.sample({32,0,12})>0&&source.sample({32,0,40})<0,"Lift height ignored");
 // Reused XY columns must match fresh sampling through closure/reopening,
 // lift motion, negative Z tiles, and topology replacement.
 for(float floor:{0.f,16.f,64.f,-32.f})for(float ceiling:{0.f,48.f,128.f}){
  source.heights(1,floor,ceiling);
  for(int tz:{-1,0,1}){
   auto cached=source.tile(0,0,tz,false);
   MapSource fresh;fresh.reset(source.topology());auto expected=fresh.tile(0,0,tz,false);
   check(cached.density==expected.density,"Cached outline changed moving-sector density");
  }
 }
 auto ring=box(0,-64,-64,64,64,0,128);
 loop(ring,{{-16,-16,0},{16,-16,0},{16,16,0},{-16,16,0}});
 source.reset({ring});
 check(source.sample({0,0,40})>0&&source.sample({32,0,40})<0,"Sector hole incorrectly filled with air");
 auto resetTile=source.tile(0,0,0,false);
 for(int z=0;z<17;++z)for(int y=0;y<17;++y)for(int x=0;x<17;++x)
  check(resetTile.density[resetTile.index(x,y,z)]==source.sample(resetTile.point(x,y,z)),"Outline cache survived topology reset");
 MapRegion concave;concave.sector=2;concave.floor=-32;concave.ceiling=100;
 loop(concave,{{-64,-64,0},{64,-64,0},{64,0,0},{0,0,0},{0,64,0},{-64,64,0}});
 source.reset({concave});
 check(source.sample({-32,32,20})<0&&source.sample({32,32,20})>0,"Concave region misclassified");
 auto tile=source.tile(-1,-1,0);
 check(tile.nx==17&&tile.density.size()==4913,"Unbounded tile allocation");
 auto lava=box(3,-64,-64,64,64,32,128);lava.breakFloor=false;
 source.reset({lava});
 check(!source.damageAllowed({0,0,32})&&source.damageAllowed({0,0,8}),"Liquid protection made its supporting wall indestructible");
 check(floorMoverSpecial(62)&&floorMoverSpecial(0x6006)&&floorMoverSpecial(0x3406),"Floor actions must be recognized");
 check(!floorMoverSpecial(26)&&!floorMoverSpecial(242)&&!floorMoverSpecial(0x4000),"Doors, height transfers and ceilings are not floor movers");
 check(largePlatformFootprint(box(0,0,0,1024,1024,0,128)),"Large footprint threshold");
 check(!largePlatformFootprint(box(0,0,0,512,512,0,128)),"Small platform must not receive the shield");
 auto platform=box(10,-512,-512,512,512,64,192);platform.shieldFloor=true;
 auto neighbor=box(11,512,-512,640,512,0,192);
 source.reset({platform,neighbor});
 check(!source.damageAllowed({0,0,64},10,0),"Large platform top must reject damage");
 check(!source.damageAllowed({510,0,32},10,0),"Large platform moving side must reject damage");
 check(!source.damageAllowed({510,0,32},-1,0),"Static blast overlap must not cut a shielded platform");
 check(source.damageAllowed({0,0,196},10,1),"Platform protection must not shield its ceiling");
 check(source.damageAllowed({600,0,-4},11,0),"Nearby ordinary floor must remain damageable");
 source.heights(10,32,192);
 check(!source.damageAllowed({0,0,28},10,0),"Shield must follow the moving floor");
 platform.shieldFloor=false;source.reset({platform});
 check(source.damageAllowed({0,0,60},10,0),"Small or ordinary platforms retain damage");
 std::cout<<"PASS map source: shared linedefs, exact tile edges, negative coordinates, floor/ceiling, door/lift heights, holes, concave sectors, bounded allocation.\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
