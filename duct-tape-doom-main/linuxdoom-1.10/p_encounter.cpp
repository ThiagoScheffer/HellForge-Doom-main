// EncounterDirector: clean native implementation from the supplied functional
// specification. No GZDoom/ZScript code or external actor replication is used.
#include "p_encounter.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>
#include <map>
#include <unordered_set>
#include <queue>
#include "../platform/sprite_frustum.h"
extern "C" {
#include "doomstat.h"
#include "p_local.h"
#include "r_state.h"
#include "m_arcade.h"
#include "w_wad.h"
#include "r_data.h"
#include "m_random.h"
#include "p_swarm.h"
#include "z_zone.h"
extern fixed_t tmfloorz,tmceilingz;
}
namespace EncounterDirector {
constexpr size_t pointLimit=1000000;
struct Profile {int type,threat;bool boss,mini;};
static Profile profiles[]={
 {MT_POSSESSED,1,0,0},{MT_SHOTGUY,2,0,0},{MT_CHAINGUY,3,0,0},{MT_TROOP,1,0,0},
 {MT_SERGEANT,1,0,0},{MT_SHADOWS,1,0,0},{MT_SKULL,2,0,0},{MT_HEAD,3,0,0},
 {MT_KNIGHT,4,0,0},{MT_BRUISER,6,0,1},{MT_FATSO,6,0,0},{MT_BABY,7,0,0},
 {MT_UNDEAD,4,0,0},{MT_VILE,8,0,1},{MT_PAIN,5,0,0},{MT_CYBORG,20,1,1},{MT_SPIDER,18,1,1}};
struct Point {mapthing_t map;int type,sector;uint32_t used=0;int next=0;};
struct Placement {mobj_t* actor;int type;};
struct Additional {int threat;bool alive;};
static encounter_config_t config=ED_DefaultConfig();
static std::vector<Point> points;
static std::vector<Placement> placements;
static std::unordered_map<mobj_t*,size_t> pending;
static std::unordered_map<mobj_t*,Additional> additional;
static std::array<int,NUMMOBJTYPES> counts{},remaining{};
static std::vector<int> roster;
static std::array<std::vector<int>,6> itemPools;
static bool loading=false,ready=false,done=false,itemsDone=false,patched=false,spawning=false,poolsReady=false;
static uint64_t rng=0,seed=0;
static uint32_t issued=0,initialCount=0,captured=0;
static int nextEvent=0,lastRecount=-1,active=0,activeThreat=0;
static bool supported(){return !netgame&&!demoplayback&&!demorecording&&!arcade_active;}
static bool enabled(){return supported()&&config.enabled;}
static const Profile* profile(int type){for(const auto& p:profiles)if(p.type==type)return &p;return nullptr;}
static uint32_t random32(){rng^=rng>>12;rng^=rng<<25;rng^=rng>>27;return uint32_t((rng*UINT64_C(2685821657736338717))>>32);}
static int roll(int n){return n>1?int(random32()%uint32_t(n)):0;}
static void ensureSeed(){if(rng)return;seed=config.seed?uint64_t(config.seed):uint64_t(std::chrono::steady_clock::now().time_since_epoch().count())^uint64_t(gameepisode*257+gamemap);rng=seed?seed:UINT64_C(0x9e3779b97f4a7c15);}
static bool living(const mobj_t* m){return m&&m->health>0&&(m->flags&MF_SHOOTABLE)&&!(m->flags&MF_CORPSE);}
static bool progression(int type){
 if(type==MT_KEEN)return true;
 if(gamemode==commercial)return gamemap==7&&(type==MT_FATSO||type==MT_BABY);
 if(gamemap==8&&((gameepisode==1&&type==MT_BRUISER)||(gameepisode==2&&type==MT_CYBORG)||((gameepisode==3||gameepisode==4)&&type==MT_SPIDER)))return true;
 return gameepisode==4&&gamemap==6&&type==MT_CYBORG;
}
static bool protectedMonster(int type){auto p=profile(type);return !p||progression(type)||(config.bossProtection>=1&&p->boss)||(config.bossProtection>=2&&p->mini);}
// Classic THINGS has position, angle, type and options only. Unknown extended
// flags/DEH definitions are deliberately ineligible instead of losing metadata.
static bool metadataSafe(const mobj_t* m){return !patched&&!(uint16_t(m->spawnpoint.options)&~31u)&&!(m->flags&MF_DROPPED);}
static int category(int type){switch(mobjinfo[type].doomednum){
 case 2007:case 2048:case 2008:case 2049:case 2010:case 2046:case 2047:case 17:return 0;
 case 2018:case 2019:case 2015:return 1;
 case 2011:case 2012:case 2014:return 2;
 case 2013:case 83:case 2024:case 2045:return 3;
 case 2001:case 82:case 2002:case 2003:case 2004:case 2005:case 2006:return 4;
 default:return -1; // Keys, suits, invulnerability/berserk, objectives and unknown inventory stay intact.
}}
static int itemTier(int type){switch(mobjinfo[type].doomednum){
 case 2007:case 2048:return 1;case 2008:case 2049:return 2;
 case 2010:case 2046:return 3;case 2047:case 17:return 4;
 case 2001:case 82:return 5;
 default:return category(type)==4?1000+type:0;
}}
static bool categoryEnabled(int c){const int flags[]={config.ammo,config.armor,config.health,config.powerups,config.weapons,config.customInventory};return c>=0&&c<6&&flags[c];}
static bool artwork(int type){int state=mobjinfo[type].spawnstate;if(state<=S_NULL||state>=NUMSTATES)return false;int sprite=states[state].sprite;return sprite>=0&&sprite<numsprites&&sprites[sprite].numframes>0;}
static bool compatibleMonster(int original,int candidate,bool reinforcement){
 const auto *a=profile(original),*b=profile(candidate);if(!a||!b)return false;
 if((reinforcement?progression(candidate):protectedMonster(candidate))||b->threat*2<a->threat||b->threat>a->threat*2)return false;
 if((mobjinfo[original].flags&(MF_FLOAT|MF_COUNTKILL))!=(mobjinfo[candidate].flags&(MF_FLOAT|MF_COUNTKILL)))return false;
 int breadth=std::max(1,int(a->threat*config.strength/100));
 return std::abs(a->threat-b->threat)<=breadth;
}
static int selectMonster(int original,bool reinforcement){
 int selected=original,total=0;
 for(int candidate:roster){
  if(!compatibleMonster(original,candidate,reinforcement))continue;
  if(reinforcement&&(profile(candidate)->boss||candidate==MT_VILE||candidate==MT_PAIN||progression(candidate)))continue;
  int weight=config.weighted?std::max(1,32/(1+std::abs(profile(original)->threat-profile(candidate)->threat))):1;
  total+=weight;if(roll(total)<weight)selected=candidate;
 }
 if(reinforcement&&(profile(selected)->boss||selected==MT_VILE||selected==MT_PAIN||progression(selected)))return -1;
 return selected;
}
static void probe(mobj_t& m,int type,fixed_t x,fixed_t y){
 std::memset(&m,0,sizeof(m));m.type=(mobjtype_t)type;m.info=&mobjinfo[type];m.x=x;m.y=y;m.radius=m.info->radius;m.height=m.info->height;
 m.flags=m.info->flags&~(MF_SPECIAL|MF_PICKUP|MF_MISSILE|MF_SKULLFLY);m.health=m.info->spawnhealth;
 m.subsector=R_PointInSubsector(x,y);m.z=m.floorz=m.subsector->sector->floorheight;m.ceilingz=m.subsector->sector->ceilingheight;
}
static bool fits(mobj_t& m,mobj_t* ignore=nullptr){
 if(int64_t(m.ceilingz)-m.z<m.height)return false;
 int flags=ignore?ignore->flags:0;if(ignore)ignore->flags&=~MF_SOLID;
 bool okay=P_CheckPosition(&m,m.x,m.y)&&int64_t(tmceilingz)-tmfloorz>=m.height&&tmfloorz<=m.z&&tmceilingz>=int64_t(m.z)+m.height;
 if(ignore)ignore->flags=flags;return okay;
}
struct SpawnScope {SpawnScope(){spawning=true;}~SpawnScope(){spawning=false;}};
static mobj_t* spawn(int type,fixed_t x,fixed_t y,fixed_t z){SpawnScope scope;return P_SpawnMobj(x,y,z,(mobjtype_t)type);}
static bool replace(mobj_t* old,int type){
 if(type==old->type)return false;
 mobj_t test;probe(test,type,old->x,old->y);test.z=old->z;
 if(!fits(test,old))return false;
 auto *next=spawn(type,old->x,old->y,old->z);
 int flags=old->flags;old->flags&=~MF_SOLID;
 bool valid=P_CheckPosition(next,next->x,next->y);old->flags=flags;
 if(!valid){next->flags&=~MF_SPECIAL;P_RemoveMobj(next);return false;}
 next->spawnpoint=old->spawnpoint;next->angle=old->angle;next->flags|=old->flags&MF_AMBUSH;
 if(next->tics>0)next->tics=1+roll(next->tics);
 ED_Respawn(old,next);old->flags&=~MF_SPECIAL;P_RemoveMobj(old);return true;
}
static void resolveOriginals();
static bool reservedOriginal(const mobj_t&);
static void finalize(){
 if(!ready||done||loading)return;
 done=itemsDone=true;
 if(enabled()){
  ensureSeed();remaining=counts;
  for(int type=0;type<NUMMOBJTYPES;++type){if(counts[type]&&artwork(type)){
   if(counts[type]>1&&profile(type)&&!progression(type))roster.push_back(type);
   int c=category(type);if(c>=0&&std::find(itemPools[c].begin(),itemPools[c].end(),type)==itemPools[c].end())itemPools[c].push_back(type);
  }}
  std::sort(roster.begin(),roster.end());roster.erase(std::unique(roster.begin(),roster.end()),roster.end());poolsReady=true;
  // Copy descriptors: removing an original actor invalidates its pending entry.
  for(size_t i=0;i<placements.size();++i){auto entry=placements[i];auto* m=entry.actor;if(!m||!metadataSafe(m))continue;
   if(profile(entry.type)){
    if(!config.monsters||protectedMonster(entry.type)||counts[entry.type]<=1||roll(100)>=config.strength)continue;
    replace(m,selectMonster(entry.type,false));
   }else{
    int c=category(entry.type);if(!config.items||!categoryEnabled(c)||(c==3&&counts[entry.type]<=1)||roll(100)>=config.coverage||(config.excludeCounted&&(m->flags&MF_COUNTITEM)))continue;
    std::vector<int>& pool=itemPools[c];int selected=entry.type,total=0;
    for(int candidate:pool){
     if(itemTier(candidate)!=itemTier(entry.type)||(mobjinfo[candidate].flags&MF_COUNTITEM)!=(m->flags&MF_COUNTITEM))continue;
     if(c==4&&(remaining[entry.type]<=1||counts[candidate]<=0))continue;
     if(roll(++total)==0)selected=candidate;
    }
    if(replace(m,selected)){--remaining[entry.type];++remaining[selected];}
   }
  }
 }
 resolveOriginals();
 for(auto& p:placements)p.actor=nullptr;placements.clear();pending.clear();
}
static void recount(){active=activeThreat=0;for(auto& entry:additional){entry.second.alive=living(entry.first);if(entry.second.alive){++active;activeThreat+=entry.second.threat;}}}
static double distance2(fixed_t x,fixed_t y,const mobj_t* m){double dx=(double(x)-m->x)/FRACUNIT,dy=(double(y)-m->y)/FRACUNIT;return dx*dx+dy*dy;}
static bool playerAlive(int i){return playeringame[i]&&players[i].playerstate==PST_LIVE&&players[i].health>0&&living(players[i].mo);}
static int survival(){int value=200;bool found=false;for(int i=0;i<MAXPLAYERS;++i)if(playerAlive(i)){found=true;value=std::min(value,std::clamp(players[i].health,0,200)+std::clamp(players[i].armorpoints,0,200)/2);}return found?std::clamp(value,0,200):0;}
static int interval(){int lo=int(config.minInterval),hi=int(config.maxInterval);int seconds=lo+roll(hi-lo+1);if(config.adaptive&&survival()<75)seconds*=2;return seconds*TICRATE;}
static int pressure=0;static bool bossEncounter=false;static mobj_t* pressurePlayer=nullptr;
static boolean pressureThing(mobj_t* m){
 if(!living(m)||m->player||(!profile(m->type)&&!(m->flags&MF_COUNTKILL)))return true;
 if(distance2(m->x,m->y,pressurePlayer)>double(config.maxDistance)*config.maxDistance)return true;
 auto p=profile(m->type);pressure+=p?p->threat:16;
 if(p&&(p->boss||progression(m->type))&&m->target)bossEncounter=true;
 return pressure<=config.maxLocalThreat&&!(config.pauseBoss&&bossEncounter);
}
static bool crowded(mobj_t* player){
 pressure=0;bossEncounter=false;pressurePlayer=player;
 if(bmapwidth<=0||bmapheight<=0)return true;
 int64_t r=int64_t(config.maxDistance)*FRACUNIT;
 int x0=int(std::clamp<int64_t>((int64_t(player->x)-r-bmaporgx)>>MAPBLOCKSHIFT,0,bmapwidth-1));
 int x1=int(std::clamp<int64_t>((int64_t(player->x)+r-bmaporgx)>>MAPBLOCKSHIFT,0,bmapwidth-1));
 int y0=int(std::clamp<int64_t>((int64_t(player->y)-r-bmaporgy)>>MAPBLOCKSHIFT,0,bmapheight-1));
 int y1=int(std::clamp<int64_t>((int64_t(player->y)+r-bmaporgy)>>MAPBLOCKSHIFT,0,bmapheight-1));
 for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)if(!P_BlockThingsIterator(x,y,pressureThing))return pressure>config.maxLocalThreat||(config.pauseBoss&&bossEncounter);
 return pressure>config.maxLocalThreat||(config.pauseBoss&&bossEncounter);
}
static bool safePoint(mobj_t& m){
 auto* sector=m.subsector->sector;
 if(sector->special!=0||int64_t(m.ceilingz)-m.floorz<m.height)return false;
 int flat=firstflat+sector->floorpic;if(flat<0||flat>=numlumps)return false;
 const char* name=lumpinfo[flat].name;
 for(auto prefix:{"NUKAGE","SLIME","LAVA","BLOOD","FWATER"})if(!std::strncmp(name,prefix,std::strlen(prefix)))return false;
 for(int i=0;i<MAXPLAYERS;++i)if(playerAlive(i)){
  auto *player=players[i].mo;double d=distance2(m.x,m.y,player);
  if(d<double(config.minDistance)*config.minDistance||d>double(config.maxDistance)*config.maxDistance)return false;
  if(config.noBehind){
   double dx=player->momx,dy=player->momy;
   if(dx*dx+dy*dy<double(FRACUNIT)*FRACUNIT){dx=finecosine[player->angle>>ANGLETOFINESHIFT];dy=finesine[player->angle>>ANGLETOFINESHIFT];}
   if((double(m.x)-player->x)*dx+(double(m.y)-player->y)*dy<0)return false;
  }
 }
 // Native occupancy/world validation is side-effect-free for this non-player
 // probe. Reject it before paying for up to four player visibility queries.
 if(reservedOriginal(m)||!fits(m))return false;
 for(int i=0;i<MAXPLAYERS;++i)if(playerAlive(i)&&P_CheckSight(players[i].mo,&m))return false;
 return true; // The committed actor receives the final native collision check.
}
static void event(){
 recount();int health=survival();if(!health||nomonsters||points.empty()||roster.empty())return;
 if(active>=config.maxActive||issued>=config.maxTotal||activeThreat>=config.maxThreat)return;
 mobj_t* player=nullptr;for(int i=0;i<MAXPLAYERS;++i)if(playerAlive(i)){player=players[i].mo;break;}
 if(!player||crowded(player)||(config.adaptive&&health<25))return;
 int budget=int(config.maxThreat);int group=int(config.minGroup)+roll(int(config.maxGroup-config.minGroup)+1);
 if(config.adaptive&&health<75){group=1;budget=std::max(1,budget/2);}
 int spawned=0;
 static const int offsets[][2]={{0,0},{1,0},{0,1},{-1,0},{0,-1}};
 for(int attempt=0;attempt<int(config.maxAttempts)&&spawned<group;++attempt){
  if(active>=config.maxActive||issued>=config.maxTotal)break;
  auto& point=points[size_t(roll(int(points.size())))];if(point.next>leveltime||counts[point.type]<=1||progression(point.type)||profile(point.type)->boss)continue;
  int type=selectMonster(point.type,true);if(type<0)continue;
  int threat=profile(type)->threat;if(activeThreat+threat>budget||pressure+threat>config.maxLocalThreat)continue;
  int offset=spawned%5;int spacing=std::max(32,2*(mobjinfo[type].radius/FRACUNIT)+8);
  int64_t x=int64_t(point.map.x)*FRACUNIT+int64_t(offsets[offset][0])*spacing*FRACUNIT;
  int64_t y=int64_t(point.map.y)*FRACUNIT+int64_t(offsets[offset][1])*spacing*FRACUNIT;
  if(x<INT32_MIN||x>INT32_MAX||y<INT32_MIN||y>INT32_MAX)continue;
  mobj_t test;probe(test,type,fixed_t(x),fixed_t(y));if(!safePoint(test))continue;
  auto* m=spawn(type,test.x,test.y,test.z);
  if(!P_CheckPosition(m,m->x,m->y)){P_RemoveMobj(m);continue;}
  if(m->tics>0)m->tics=1+roll(m->tics);
  m->spawnpoint=point.map;m->angle=ANG45*(point.map.angle/45);if(point.map.options&MTF_AMBUSH)m->flags|=MF_AMBUSH;
  additional.emplace(m,Additional{threat,true});++active;activeThreat+=threat;++issued;++spawned;pressure+=threat;
  if(m->flags&MF_COUNTKILL)++totalkills;
  ++point.used;point.next=leveltime+180*TICRATE;
 }
 if(config.debug)std::fprintf(stderr,"EncounterDirector tic %d: group %d active %d issued %u threat %d points %zu\n",leveltime,spawned,active,issued,activeThreat,points.size());
}
static void put(std::vector<unsigned char>& data,uint32_t n){for(int i=0;i<4;++i)data.push_back((unsigned char)(n>>(i*8)));}
static uint32_t get(const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
static uint32_t checksum(const unsigned char* data,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;++i)h=(h^data[i])*16777619u;return h;}
static uint32_t fingerprint(){uint32_t h=2166136261u;for(const auto& p:points)for(int n:std::array<int,6>{p.map.x,p.map.y,p.map.angle,p.map.options,p.type,p.sector})h=(h^uint32_t(n))*16777619u;return h;}
#include "p_population.inc"
}
using namespace EncounterDirector;
extern "C" {

void (*ED_StartupSettings)(void)=nullptr;
void ED_MapThingIndex(int id){mapThingId=id;}
void ED_OriginalRevived(mobj_t* m){reviveOriginal(m);}
int ED_DeferOriginal(const void* raw,int type){return deferOriginal(*(const mapthing_t*)raw,type);}
encounter_population_t ED_Population(){return populationStats();}
const char* ED_PopulationReasonName(int reason){return reason>=0&&reason<10?populationReasons[reason]:"unknown";}
// Kept for diagnostic callers. Rendering is never allowed to construct actors.
void ED_OriginalView(const double*,const double*,const double*,const double*,double,double,int){}
void ED_ValidateConfig(encounter_config_t* c){
 if(!c)return;auto d=ED_DefaultConfig();
 int* toggles[]={&c->enabled,&c->monsters,&c->reinforcements,&c->items,&c->weighted,&c->noBehind,&c->adaptive,&c->pauseBoss,&c->ammo,&c->armor,&c->health,&c->powerups,&c->weapons,&c->customInventory,&c->excludeCounted,&c->debug};for(auto p:toggles)*p=*p!=0;c->bossProtection=std::clamp(c->bossProtection,0,2);
 auto bound=[](float& v,float lo,float hi,float fallback){v=std::isfinite(v)?std::clamp(std::round(v),lo,hi):fallback;};
 c->stream=!!c->stream;c->streamPolicy=std::clamp(c->streamPolicy,0,2);c->safeStasis=0;c->initialBudgetMode=std::clamp(c->initialBudgetMode,0,1);c->populationPolicy=std::clamp(c->populationPolicy,0,1);
 bound(c->initialMonsterCount,1,1000000,500);
 c->initialMonsterPercent=std::isfinite(c->initialMonsterPercent)?std::clamp(std::round(c->initialMonsterPercent*2)/2,1.f,100.f):25;
 bound(c->activateDistance,128,8190,d.activateDistance);bound(c->prefetchDistance,129,8191,d.prefetchDistance);c->prefetchDistance=std::max(c->prefetchDistance,c->activateDistance+1);
 bound(c->stasisDistance,130,8192,d.stasisDistance);c->stasisDistance=std::max(c->stasisDistance,c->prefetchDistance+1);
 bound(c->spawnBudget,1,64,8);bound(c->packSize,1,64,8);bound(c->softLimit,1,8192,128);
 c->packInterval=std::isfinite(c->packInterval)?std::clamp(std::round(c->packInterval*2)/2,.5f,60.f):2;
 bound(c->strength,0,100,d.strength);bound(c->minDistance,64,8191,d.minDistance);bound(c->maxDistance,65,8192,d.maxDistance);c->maxDistance=std::max(c->maxDistance,c->minDistance+1);
 bound(c->minGroup,1,16,d.minGroup);bound(c->maxGroup,1,16,d.maxGroup);c->maxGroup=std::max(c->maxGroup,c->minGroup);
 bound(c->minInterval,5,600,d.minInterval);bound(c->maxInterval,5,600,d.maxInterval);c->maxInterval=std::max(c->minInterval,c->maxInterval);
 bound(c->maxActive,1,64,d.maxActive);bound(c->maxTotal,1,256,d.maxTotal);bound(c->maxThreat,1,256,d.maxThreat);bound(c->maxLocalThreat,1,512,d.maxLocalThreat);bound(c->maxAttempts,1,64,d.maxAttempts);bound(c->coverage,0,100,d.coverage);bound(c->seed,0,16777215,0);for(int i=0;i<17;++i)bound(c->threatCosts[i],1,128,d.threatCosts[i]);
}
void ED_Configure(const encounter_config_t* c){
 if(!c||!std::memcmp(c,&config,sizeof(config)))return;
 int previousBoss=config.bossProtection;config=*c;ED_ValidateConfig(&config);
 for(size_t i=0;i<std::size(profiles);++i)profiles[i].threat=int(config.threatCosts[i]);
 for(auto& entry:additional)if(auto p=profile(entry.first->type))entry.second.threat=p->threat;
 recount();
 if(previousBoss!=config.bossProtection){roster.clear();poolsReady=false;}
 if(!enabled()||!config.reinforcements)nextEvent=0;
}

void ED_Clear(){clearOriginals();points.clear();placements.clear();pending.clear();additional.clear();roster.clear();for(auto& p:itemPools)p.clear();counts.fill(0);remaining.fill(0);ready=done=itemsDone=poolsReady=false;rng=seed=0;issued=initialCount=captured=0;nextEvent=0;lastRecount=-1;active=activeThreat=0;}
void ED_BeginMap(int capacity){if(ED_StartupSettings)ED_StartupSettings();beginOriginals();patched=P_SwarmDefinitionsPatched()!=0;if(!supported())return;size_t n=std::min(size_t(16384),size_t(std::max(0,capacity)));points.reserve(n);placements.reserve(n);pending.reserve(n);additional.reserve(256);roster.reserve(NUMMOBJTYPES);if(streaming){originals.reserve(std::min(size_t(std::max(0,capacity)),pointLimit));originalActors.reserve(n);liveOriginalActors.reserve(n);}}
void ED_Capture(mobj_t* m){
 if(ready||!supported()||!m)return;
 captureEagerOriginal(m);
 if(captured>=pointLimit||!metadataSafe(m))return;
 bool monster=profile(m->type)!=nullptr;int c=category(m->type);if(!monster&&c<0)return;
 ++captured;++counts[m->type];
 if(monster){++initialCount;points.push_back({m->spawnpoint,int(m->type),int(m->subsector->sector-sectors)});}
 if(!loading){pending.emplace(m,placements.size());placements.push_back({m,int(m->type)});}
}
void ED_MapReady(){ready=true;if(populationRequested){finalize();planInitialOriginals();}}
void ED_Tick(){
 if(!supported()||loading)return;finalize();tickOriginals();reportOriginals();if(!enabled()||!config.reinforcements)return;
 ensureSeed();if(!poolsReady){roster.clear();for(int type=0;type<NUMMOBJTYPES;++type)if(counts[type]>1&&profile(type)&&!progression(type)&&artwork(type))roster.push_back(type);poolsReady=true;}if(lastRecount<0||leveltime-lastRecount>=TICRATE){recount();lastRecount=leveltime;}
 if(!nextEvent){nextEvent=leveltime+interval();return;}if(leveltime<nextEvent)return;
 event();nextEvent=leveltime+interval();
}
void ED_Forget(mobj_t* m){forgetOriginal(m);auto p=pending.find(m);if(p!=pending.end()){placements[p->second].actor=nullptr;pending.erase(p);}auto a=additional.find(m);if(a!=additional.end()){if(a->second.alive){--active;activeThreat-=a->second.threat;}additional.erase(a);}}
void ED_Respawn(mobj_t* old,mobj_t* next){respawnOriginal(old,next);auto a=additional.find(old);if(a==additional.end())return;auto record=a->second;additional.erase(a);if(!record.alive){++active;activeThreat+=record.threat;}record.alive=true;additional.emplace(next,record);}
void ED_Died(mobj_t* m){diedOriginal(m);auto a=additional.find(m);if(a!=additional.end()&&a->second.alive){a->second.alive=false;--active;activeThreat-=a->second.threat;}}
void ED_SetLoading(int value){loading=value!=0;}
int ED_SpawnRandom(){if(originalRandom>=0)return originalRandom;return spawning?roll(MAXPLAYERS):P_Random();}
size_t ED_Save(unsigned char* destination){
 if(!supported()||(!rng&&!issued&&!config.enabled&&originals.empty()))return 0;
 std::vector<unsigned char> out;put(out,streaming?3:1);put(out,(done?1u:0)|(itemsDone?2u:0));put(out,uint32_t(seed));put(out,uint32_t(seed>>32));put(out,uint32_t(rng));put(out,uint32_t(rng>>32));put(out,uint32_t(std::max(0,nextEvent-leveltime)));put(out,issued);put(out,initialCount);put(out,fingerprint());put(out,uint32_t(points.size()));put(out,0);
 for(const auto& p:points){put(out,p.used);put(out,uint32_t(std::max(0,p.next-leveltime)));}
 std::unordered_map<mobj_t*,uint32_t> ids;uint32_t id=0;for(auto* t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker)ids.emplace((mobj_t*)t,++id);
 uint32_t count=0;for(const auto& entry:additional){auto p=ids.find(entry.first);if(p!=ids.end()){put(out,p->second);++count;}}
 for(int i=0;i<4;++i)out[44+i]=uint8_t(count>>(i*8));
 saveOriginals(out,ids);
 put(out,checksum(out.data(),out.size()));put(out,uint32_t(out.size()+8));out.insert(out.end(),{'E','D','0','1'});
 if(destination)std::memcpy(destination,out.data(),out.size());return out.size();
}
size_t ED_Load(const unsigned char* data,size_t length){
 done=itemsDone=true;placements.clear();pending.clear();additional.clear();nextEvent=0;active=activeThreat=0;rng=seed=0;issued=0;
 clearOriginals();
 if(length<60||std::memcmp(data+length-4,"ED01",4))return 0;
 size_t bytes=get(data+length-8);if(bytes<60||bytes>length)return 0;
 auto* p=data+length-bytes;uint32_t n=get(p+40),count=get(p+44);
 size_t base=48+size_t(n)*8+size_t(count)*4;
 bool valid=(get(p)>=1&&get(p)<=3)&&n<=pointLimit&&count<=256&&bytes>=base+12&&(get(p)!=1||bytes==base+12)&&get(p+bytes-12)==checksum(p,bytes-12)&&n==points.size()&&get(p+36)==fingerprint();
 if(valid&&get(p)>=2)valid=validateOriginalSave(p+base,bytes-base-12,get(p));
 if(!valid){clearOriginals();points.clear();roster.clear();std::fprintf(stderr,"EncounterDirector: incompatible/corrupt save state ignored; reinforcements suspended.\n");return bytes;}
 seed=uint64_t(get(p+8))|(uint64_t(get(p+12))<<32);rng=uint64_t(get(p+16))|(uint64_t(get(p+20))<<32);
 nextEvent=leveltime+int(std::min(get(p+24),42000u));issued=std::min(get(p+28),256u);initialCount=get(p+32);
 for(size_t i=0;i<n;++i){points[i].used=get(p+48+i*8);points[i].next=leveltime+int(std::min(get(p+52+i*8),42000u));}
 // Rebuild class pools from immutable map descriptors, never loaded actors.
 roster.clear();for(int type=0;type<NUMMOBJTYPES;++type)if(counts[type]>1&&profile(type)&&!progression(type)&&artwork(type))roster.push_back(type);
 std::vector<mobj_t*> list;for(auto* t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker)list.push_back((mobj_t*)t);
 for(uint32_t i=0;i<count;++i){uint32_t id=get(p+48+n*8+i*4);if(id&&id<=list.size()){auto* m=list[id-1];auto pr=profile(m->type);if(pr)additional.emplace(m,Additional{pr->threat,living(m)});}}
 if(get(p)>=2)loadOriginals(p+base,list,get(p));
 recount();lastRecount=leveltime;return bytes;
}
}
