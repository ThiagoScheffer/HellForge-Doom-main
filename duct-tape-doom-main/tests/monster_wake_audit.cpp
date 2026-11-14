// Bounded, benchmark-only observations. No gameplay data is modified.
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
extern "C" {
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "m_argv.h"
#include "r_state.h"
}
#include "../platform/monster_wake_audit.h"
namespace {
constexpr size_t cap=65536;
bool enabled=false;unsigned nextId=0,level=0;size_t dropped=0;
struct Identity {unsigned id;bool seen=false;};
std::unordered_map<mobj_t*,Identity> identities;
struct Pair {mobj_t *a,*b;fixed_t ax,ay,az,ah,bx,by,bz,bh;subsector_t *as,*bs;
    bool operator==(const Pair &p)const{return a==p.a&&b==p.b&&ax==p.ax&&ay==p.ay&&az==p.az&&ah==p.ah&&bx==p.bx&&by==p.by&&bz==p.bz&&bh==p.bh&&as==p.as&&bs==p.bs;}};
Pair pair(mobj_t*a,mobj_t*b){return {a,b,a->x,a->y,a->z,a->height,b->x,b->y,b->z,b->height,a->subsector,b->subsector};}
struct Look {Pair previous={};bool has=false,visible=false;};
std::vector<Look> looks;
struct Cost {uint64_t calls=0,rays=0,ns=0,duplicates=0,falseDuplicates=0,reused=0;};
std::array<std::array<Cost,WA_SITES>,4> costs={};
int shotTic=-1,measureTic=-1;unsigned overflowDepth=0;
struct Event {unsigned level,id;uintptr_t thinker;int tic,type,sector;fixed_t x,y,z;bool ambush,first;int reason,line;};
std::vector<Event> events;
Identity *identity(mobj_t *a){auto it=identities.find(a);if(it!=identities.end())return &it->second;
    if(identities.size()>=cap){++dropped;return nullptr;}return &identities.emplace(a,Identity{++nextId,false}).first->second;}
int window(){return shotTic<0?0:gametic-shotTic<8?1:gametic-shotTic<35?2:3;}
void event(mobj_t *a,int reason,int line=-1){auto *id=identity(a);if(!id)return;
    bool first=reason<2&&!id->seen;if(reason<2)id->seen=true;
    if(events.size()>=cap){++dropped;return;}
    events.push_back({level,id->id,(uintptr_t)&a->thinker,gametic,(int)a->type,(int)(a->subsector->sector-sectors),a->x,a->y,a->z,(a->flags&MF_AMBUSH)!=0,first,reason,line});}
}
extern "C" void WA_Init(){enabled=M_CheckParm((char*)"-bench-wake-audit")!=0;if(enabled){identities.reserve(cap);events.reserve(cap);looks.reserve(16);}}
extern "C" void WA_MeasureBegin(){if(enabled){costs={};measureTic=gametic;}}
extern "C" void WA_Map(){if(enabled){++level;identities.clear();looks.clear();overflowDepth=0;}}
extern "C" void WA_Spawn(mobj_t*a){if(enabled)identity(a);}
extern "C" void WA_Remove(mobj_t*a){if(enabled)identities.erase(a);}
extern "C" void WA_LookBegin(mobj_t*){if(enabled){if(looks.size()<16&&!overflowDepth)looks.push_back({});else{++overflowDepth;++dropped;}}}
extern "C" void WA_LookEnd(){if(enabled){if(overflowDepth)--overflowDepth;else if(!looks.empty())looks.pop_back();}}
extern "C" void WA_Transition(mobj_t*a,int sound){if(enabled&&!a->player&&a->health>0&&(a->flags&MF_SHOOTABLE)&&((a->flags&MF_COUNTKILL)||a->type==MT_SKULL))event(a,sound?0:1);}
extern "C" int WA_Sight(mobj_t*a,mobj_t*b,int site,int reused){
    if(!enabled)return reused?0:P_CheckSight(a,b);
    auto &c=costs[window()][site];++c.calls;Pair current=pair(a,b);
    if(!overflowDepth&&!looks.empty()&&looks.back().has&&current==looks.back().previous){++c.duplicates;if(!looks.back().visible)++c.falseDuplicates;}
    auto start=std::chrono::steady_clock::now();int visible=reused?0:P_CheckSight(a,b);
    c.ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    c.reused+=reused!=0;c.rays+=reused==0;
    if(!overflowDepth&&!looks.empty()){looks.back().previous=current;looks.back().visible=visible!=0;looks.back().has=true;}
    return visible;
}
extern "C" void WA_SolidTime(unsigned long long ns){if(enabled){auto &c=costs[window()][WA_SOLID];++c.calls;++c.rays;c.ns+=ns;}}
extern "C" unsigned long long WA_SolidStart(){return enabled?std::chrono::steady_clock::now().time_since_epoch().count():0;}
extern "C" void WA_SolidEnd(unsigned long long start){if(start)WA_SolidTime(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()-std::chrono::steady_clock::duration(start)).count());}
extern "C" void WA_Teleport(mobj_t*a,line_t*l,int success){if(enabled&&l->special==126)event(a,success?3:2,(int)(l-lines));}
extern "C" void WA_Shot(void){if(enabled&&shotTic<0)shotTic=gametic;}
extern "C" void WA_Flush(const char *folder){if(!enabled)return;
    auto path=std::filesystem::u8path(folder);std::ofstream e(path/"wake-events.csv"),s(path/"wake-sight.csv");
    e<<"level,spawn_id,thinker_address,tic,type,sector,x,y,z,ambush,first_idle_to_see,reason,line\n";
    const char *reasons[]={"sound","sight","teleport126_attempt","teleport126_success"};
    for(auto &v:events)e<<v.level<<','<<v.id<<','<<v.thinker<<','<<v.tic<<','<<v.type<<','<<v.sector<<','<<v.x/(double)FRACUNIT<<','<<v.y/(double)FRACUNIT<<','<<v.z/(double)FRACUNIT<<','<<v.ambush<<','<<v.first<<','<<reasons[v.reason]<<','<<v.line<<'\n';
    s<<"window,site,calls,actual_queries,inclusive_ns,duplicate_unchanged_pairs,false_duplicates,reused_false\n";
    const char *sites[]={"ambush_sound","player_search","missile_range","melee_range","solid_geometry"};
    const char *windows[]={"before_shot","first_8","tics_8_34","later"};
    for(int w=0;w<4;++w)for(int i=0;i<WA_SITES;++i){auto &c=costs[w][i];s<<windows[w]<<','<<sites[i]<<','<<c.calls<<','<<c.rays<<','<<c.ns<<','<<c.duplicates<<','<<c.falseDuplicates<<','<<c.reused<<'\n';}
    std::ofstream meta(path/"wake-audit.txt");meta<<"measure_begin_gametic "<<measureTic<<"\nshot_gametic "<<shotTic<<"\ndropped_records "<<dropped<<"\nids runtime spawn generation; load creates new identities; thinker addresses diagnostic only\nsolid_geometry is nested in sight sites; do not sum inclusive columns\n";
}
