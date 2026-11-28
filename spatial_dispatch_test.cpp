#include <cstdlib>
#include <array>
#include <map>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <cassert>
#include <cstring>
using fixed_t=int32_t;
constexpr int MF_MISSILE=1,MF_NOGRAVITY=2,MT_PUFF=3;
struct Info {int flags=0;};
struct mobj_t {fixed_t x=0,y=0,z=0,height=56,radius=20,floorz=12,ceilingz=128;int flags=0,type=0;Info* info=nullptr;};
struct Vec {float x,y,z;};
struct World {size_t cuts=0;bool near=false,indexLies=false;bool affected(Vec,float,float)const{return near;}bool mayBeAffected(Vec,float,float)const{return near&&!indexLies;}unsigned stampCalls=0;uint64_t rev=1;size_t cutCount()const{return cuts;}uint64_t supportStamp(Vec,float){++stampCalls;return rev;}} mapWorld;
static bool active=true,mapped=true,disabled=false,spatial=false,verify=false,stale=false;
static unsigned r_levelserial=1;static int gametic=0,calculations=0;
int M_CheckParm(char* name){return (disabled&&std::strcmp(name,"-nosolidsupportfast")==0)||(spatial&&std::strcmp(name,"-solidsupportspatial")==0)||(verify&&std::strcmp(name,"-verifysolidsupportspatial")==0);}
int SV_Active(){return active;}int SV_MapMode(){return mapped;}
float unit(fixed_t x){return float(x);}
#define MP_AUTO(x) ((void)0)
#define MP_COUNT(x,y) ((void)0)
#define MP_MAX(x,y) ((void)0)
int solidPositionUncached(mobj_t*,fixed_t,fixed_t,fixed_t* f,fixed_t* c){++calculations;if(mapped&&(!mapWorld.cuts||(!mapWorld.near&&!stale)))return -1;*f=24;*c=104;return 1;}
static uint64_t supportEmptyBypasses=0;
static uint64_t supportSpatialChecks=0,supportSpatialBypasses=0,supportSpatialVerified=0,supportSpatialMismatches=0;
static bool supportSpatialFailed=false;
static bool solidSupportSpatialEnabled(){
 static const bool requested=M_CheckParm((char*)"-solidsupportspatial")!=0;
 return requested&&!supportSpatialFailed;
}
static bool solidSupportSpatialVerify(){
 static const bool verify=M_CheckParm((char*)"-verifysolidsupportspatial")!=0;
 return verify;
}
static int finishSpatialSupportVerification(bool verify,int result){
 if(verify){
  ++supportSpatialVerified;
  if(result!=-1){
   ++supportSpatialMismatches;supportSpatialFailed=true;
   fprintf(stderr,"SOLID support spatial mismatch: tic %d legacy result %d; disabling spatial bypass\n",gametic,result);
  }
 }
 return result;
}
static bool solidSupportFastEnabled(){
 static const bool enabled=!M_CheckParm((char*)"-nosolidsupportfast");
 return enabled;
}
int SV_Position(mobj_t *m,fixed_t x,fixed_t y,fixed_t *floor,fixed_t *ceiling){
 MP_AUTO(MP_SUPPORT);
 if(!SV_Active())return -1;
 if(!SV_MapMode())return solidPositionUncached(m,x,y,floor,ceiling);
 // MapWorld::affected() is false with an empty cut journal. The original
 // uncached path then returns -1 without writing either height output.
 // Avoid actor-cache lookup, column stamps and cache churn in that case.
 // Test the live journal on every request: the first cut immediately
 // restores the ordinary support path, including for moving sectors.
 if(solidSupportFastEnabled()&&mapWorld.cutCount()==0){
  ++supportEmptyBypasses;return -1;
 }
 // Only bypass the exact existing MapWorld::affected gate. Near damage,
 // or if the index cannot prove rejection, keep all Stage 3 processing.
 bool verifySpatial=false;
 if(solidSupportFastEnabled()&&solidSupportSpatialEnabled()){
  ++supportSpatialChecks;
  if(!mapWorld.mayBeAffected({unit(x),unit(y),unit(m->z)},unit(m->radius),unit(m->height))){
   if(!solidSupportSpatialVerify()){++supportSpatialBypasses;return -1;}
   // In verification mode execute the original cached path, not just the
   // index oracle. Always return its result and preserve its output writes.
   verifySpatial=true;
   if(mapWorld.affected({unit(x),unit(y),unit(m->z)},unit(m->radius),unit(m->height))){
    ++supportSpatialMismatches;supportSpatialFailed=true;verifySpatial=false;
    fprintf(stderr,"SOLID support spatial predicate mismatch: tic %d; disabling spatial bypass\n",gametic);
   }
  }
 }
 struct Support {std::array<fixed_t,6> query{};uint64_t revision=~uint64_t(0);int result=-1;fixed_t floor=0,ceiling=0;};
 static std::map<mobj_t*,Support> supports;
 static unsigned level=~0u;
 if(level!=r_levelserial||supports.size()>4096){MP_COUNT(MP_CACHE_CLEAR,1);supports.clear();level=r_levelserial;}
 auto& support=supports[m];
 MP_MAX(MP_CACHE_PEAK,supports.size());
 std::array<fixed_t,6> query{x,y,m->z,m->height,m->radius,m->flags&MF_MISSILE};
 auto stamp=mapWorld.supportStamp({unit(x),unit(y),unit(m->z)},unit(m->radius));
 if(support.query==query&&support.revision==stamp){
  MP_COUNT(MP_CACHE_HIT,1);
  if(support.result==1){*floor=support.floor;*ceiling=support.ceiling;}
  return finishSpatialSupportVerification(verifySpatial,support.result);
 }
 MP_COUNT(MP_CACHE_MISS,1);MP_COUNT(MP_CACHE_INVALIDATE,support.revision!=~uint64_t(0)&&support.revision!=stamp);
 int result=solidPositionUncached(m,x,y,floor,ceiling);
 support.query=query;support.revision=mapWorld.supportStamp({unit(x),unit(y),unit(m->z)},unit(m->radius));support.result=result;
 if(result==1){support.floor=*floor;support.ceiling=*ceiling;}
 return finishSpatialSupportVerification(verifySpatial,result);
}
void SV_Refresh(mobj_t *m){
 if(!SV_Active()||m->type==MT_PUFF)return;
 // A spent, gravity-free projectile is only an explosion animation. Its
 // impact point may overlap the crater rim; trying to step its old collision
 // capsule out of that rim every tic is both unnecessary and expensive.
 if((m->info->flags&MF_MISSILE)&&!(m->flags&MF_MISSILE)
    &&(m->flags&MF_NOGRAVITY))return;
 static const bool profile=M_CheckParm((char*)"-profilesolid")!=0;
 // Keep the legacy path available for the A/B control, including its clock read.
 auto started=(profile||!solidSupportFastEnabled())?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
 fixed_t floor,ceiling;
 if(SV_Position(m,m->x,m->y,&floor,&ceiling)==1){m->floorz=floor;m->ceilingz=ceiling;}
 if(profile){double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();if(ms>.3)printf("SOLID support tic %d type %d flags %x %.3f ms\n",gametic,m->type,m->flags,ms);}
}
int main(int argc,char** argv){
 int mode=argc>1?std::atoi(argv[1]):0;spatial=mode!=1;verify=mode>=2;disabled=mode==4;
 Info info;mobj_t a;a.info=&info;fixed_t f=7,c=8;
 active=false;assert(SV_Position(&a,0,0,&f,&c)==-1&&f==7&&c==8);active=true;
 assert(SV_Position(&a,0,0,&f,&c)==-1&&f==7&&c==8);
 mapWorld.cuts=1;++mapWorld.rev;
 int before=calculations;unsigned stamps=mapWorld.stampCalls;
 for(int i=1;i<=1000;++i)assert(SV_Position(&a,i,0,&f,&c)==-1&&f==7&&c==8);
 if(mode==0)assert(calculations==before&&mapWorld.stampCalls==stamps&&supportSpatialBypasses==1000);
 else assert(calculations==before+1000&&supportSpatialBypasses==0);
 if(mode>=2&&!disabled)assert(supportSpatialVerified==1000);
 mapWorld.near=true;++mapWorld.rev;
 assert(SV_Position(&a,0,0,&f,&c)==1&&f==24&&c==104);
 if(mode==2){mapWorld.indexLies=true;++mapWorld.rev;assert(SV_Position(&a,1,0,&f,&c)==1&&supportSpatialFailed&&supportSpatialMismatches==1);}
 if(mode==3){mapWorld.near=false;stale=true;++mapWorld.rev;assert(SV_Position(&a,1,0,&f,&c)==1&&supportSpatialFailed&&supportSpatialMismatches==1);}
 mapped=false;assert(SV_Position(&a,0,0,&f,&c)==1);
 std::printf("PASS dispatch mode %d\n",mode);
}
