#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "doomstat.h"
#include "p_local.h"
#include "g_game.h"
#include "d_main.h"
#include "m_argv.h"
#include "p_encounter.h"
extern int prndindex;
extern void G_DoSaveGame(void),G_DoLoadGame(void);
}
static void check(bool okay,const char* text){if(!okay){std::fprintf(stderr,"FAIL director: %s\n",text);std::exit(1);}std::printf("PASS director: %s\n",text);}
static std::vector<mobj_t*> actors(){std::vector<mobj_t*> out;for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker)out.push_back((mobj_t*)t);return out;}
static bool monster(mobj_t* m){return !m->player&&((m->flags&MF_COUNTKILL)||m->type==MT_SKULL);}
static std::vector<std::array<int,5>> metadata(){std::vector<std::array<int,5>> out;for(auto*m:actors())if(!m->player)out.push_back({m->spawnpoint.x,m->spawnpoint.y,m->spawnpoint.type,m->spawnpoint.angle,m->spawnpoint.options});std::sort(out.begin(),out.end());return out;}
static std::vector<unsigned char> saved(){std::vector<unsigned char> b(ED_Save(nullptr));ED_Save(b.data());return b;}
static uint32_t get(const std::vector<unsigned char>& b,size_t n){return uint32_t(b[n])|(uint32_t(b[n+1])<<8)|(uint32_t(b[n+2])<<16)|(uint32_t(b[n+3])<<24);}
static std::vector<unsigned> populationClasses(const std::vector<unsigned char>& data){size_t at=48+get(data,40)*8+get(data,44)*4;std::vector<unsigned> result;for(unsigned i=0;i<get(data,at);++i)result.push_back(get(data,at+20+i*32+8));return result;}
static std::vector<unsigned char> legacyPopulation(std::vector<unsigned char> data){size_t at=48+get(data,40)*8+get(data,44)*4;data.resize(at+20+get(data,at)*32+get(data,at+16)*12);data[0]=2;auto put=[&](unsigned n){for(int b=0;b<4;++b)data.push_back((unsigned char)(n>>(b*8)));};unsigned h=2166136261u;for(auto b:data)h=(h^b)*16777619u;put(h);put(unsigned(data.size()+8));data.insert(data.end(),{'E','D','0','1'});return data;}
static void reset(encounter_config_t c){ED_Configure(&c);G_InitNew(sk_medium,1,1);}
static void tests(){
 R_BeforeTic=nullptr;
 auto c=ED_DefaultConfig();check(!c.enabled&&!c.monsters&&!c.reinforcements&&!c.items,"all mechanics default off");
 c.minDistance=9000;c.maxDistance=1;c.minGroup=8;c.maxGroup=1;ED_ValidateConfig(&c);check(c.minDistance<c.maxDistance&&c.minGroup<=c.maxGroup,"numeric relationships clamped");
 c=ED_DefaultConfig();reset(c);auto baseline=metadata();auto count=actors().size();int rng=prndindex;ED_Tick();check(ED_Save(nullptr)==0&&prndindex==rng&&metadata()==baseline,"off leaves actors and gameplay RNG untouched");
 c.enabled=c.monsters=1;c.strength=100;c.seed=12345;reset(c);int kills=totalkills,items=totalitems;rng=prndindex;ED_Tick();
 int changes=0;for(auto*m:actors())if(monster(m)&&mobjinfo[m->type].doomednum!=m->spawnpoint.type)++changes;
 for(auto*m:actors())if(m->spawnpoint.type==16||m->spawnpoint.type==66)check(mobjinfo[m->type].doomednum==m->spawnpoint.type,"boss and unique monsters protected");
 check(changes>0,"initial monster substitutions actually occur");check(actors().size()==count&&metadata()==baseline&&totalkills==kills&&totalitems==items,"one-for-one map metadata and counts preserved");check(prndindex==rng,"replacement decisions/spawns do not consume gameplay RNG");
 c.monsters=0;c.items=c.ammo=c.health=c.weapons=1;c.excludeCounted=0;reset(c);rng=prndindex;ED_Tick();int pickupChanges=0,keys=0;
 for(auto*m:actors()){if(!monster(m)&&!m->player&&mobjinfo[m->type].doomednum!=m->spawnpoint.type)++pickupChanges;if(m->spawnpoint.type==5){++keys;check(mobjinfo[m->type].doomednum==5,"key remains unchanged");}}
 check(pickupChanges>0&&keys==1&&actors().size()==count&&metadata()==baseline,"category item replacements preserve original count/metadata");check(prndindex==rng,"item decisions do not consume gameplay RNG");
 c.items=0;c.reinforcements=1;c.minInterval=c.maxInterval=5;c.maxActive=2;c.maxTotal=3;c.minGroup=c.maxGroup=3;c.adaptive=0;c.maxAttempts=64;
 // Visibility rejection in the same convex room, without a direction/range rejection masking it.
 c.minDistance=64;c.noBehind=0;reset(c);ED_Tick();check(P_TeleportMove(players[0].mo,1800*FRACUNIT,0),"visible-spawn fixture positioning");
 for(auto*m:actors())if(monster(m)){check(P_CheckSight(players[0].mo,m),"fixture spawn point is visible");P_RemoveMobj(m);}
 leveltime+=5*TICRATE;ED_Tick();check(get(saved(),28)==0,"visible reinforcement locations always rejected");
 c.minDistance=1024;c.noBehind=1;
 reset(c);ED_Tick();for(auto*m:actors())if(monster(m))P_RemoveMobj(m);
 rng=prndindex;leveltime+=5*TICRATE;ED_Tick();auto first=saved();check(get(first,28)==2,"reinforcement group obeys active cap");check(prndindex==rng,"reinforcement choices/spawns use separate RNG");
 for(auto*m:actors())if(monster(m)){check(!P_CheckSight(players[0].mo,m),"reinforcement outside player LOS");double dx=double(m->x-players[0].mo->x)/FRACUNIT,dy=double(m->y-players[0].mo->y)/FRACUNIT;check(dx>=0&&dx*dx+dy*dy>=1024.*1024.&&dx*dx+dy*dy<=2048.*2048.,"reinforcement distance and forward direction");}
 for(auto*m:actors())if(monster(m)){P_DamageMobj(m,nullptr,players[0].mo,100000);break;}
 leveltime+=5*TICRATE;ED_Tick();auto cap=saved();check(get(cap,28)==3,"death frees active slot, lifetime count retained");
 for(int i=0;i<3;++i){leveltime+=200*TICRATE;ED_Tick();}check(get(saved(),28)==3,"cumulative per-map cap prevents accumulation");
 auto before=saved();G_SaveGame(5,(char*)"ENCOUNTER TEST");G_DoSaveGame();gameaction=ga_nothing;G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;auto restored=saved();
 check(before.size()==restored.size()&&std::equal(before.begin(),before.begin()+48+get(before,40)*8,restored.begin()),"native save/load restores seed, timers, counts and spawnpoint cooldowns");
 auto n=actors().size();ED_Tick();check(actors().size()==n,"load never rerandomizes or duplicates actors");
 auto stable=saved();netgame=true;leveltime+=100*TICRATE;ED_Tick();check(actors().size()==n,"network mode bypass");netgame=false;demoplayback=true;ED_Tick();check(actors().size()==n,"demo playback bypass");demoplayback=false;demorecording=true;ED_Tick();check(actors().size()==n,"demo recording bypass");demorecording=false;
 c.enabled=0;ED_Configure(&c);rng=prndindex;ED_Tick();check(actors().size()==n&&prndindex==rng,"master off stops activity without deleting actors");
 auto bad=restored;bad[4]^=1;check(ED_Load(bad.data(),bad.size())==bad.size(),"corrupt trailer safely consumed");
 c=ED_DefaultConfig();c.enabled=c.monsters=c.reinforcements=c.items=c.ammo=c.health=1;c.seed=77;reset(c);ED_Tick();check(actors().size()==count&&metadata()==baseline,"all three modules enabled together preserve initial population");
 c.strength=c.coverage=0;reset(c);ED_Tick();int substitutions=0;for(auto*m:actors())if(!m->player&&mobjinfo[m->type].doomednum!=m->spawnpoint.type)++substitutions;check(!substitutions,"zero strength/coverage leave originals intact");
 c=ED_DefaultConfig();reset(c);ED_Tick();G_SaveGame(5,(char*)"NO DIRECTOR TRAILER");G_DoSaveGame();gameaction=ga_nothing;
 c.enabled=c.monsters=c.items=1;c.strength=c.coverage=100;ED_Configure(&c);G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;ED_Tick();int legacyChanges=0;for(auto*m:actors())if(!m->player&&mobjinfo[m->type].doomednum!=m->spawnpoint.type)++legacyChanges;check(!legacyChanges&&metadata()==baseline,"save without director trailer loads without rerandomization");
 // Initial budgets and staging use the real native spawn and save paths.
 c=ED_DefaultConfig();reset(c);int initialRng=prndindex,authoredKills=totalkills;
 c.enabled=c.stream=1;c.populationPolicy=1;c.initialMonsterCount=3;c.streamPolicy=1;c.spawnBudget=2;c.packSize=4;c.softLimit=3;c.activateDistance=128;c.prefetchDistance=2048;c.seed=77;reset(c);
 auto pop=ED_Population();check(pop.filtered==9&&pop.requested==3&&pop.requiredEager==3&&pop.initialLive==3&&pop.deferred==6&&actors().size()+6==count,"Performance count really limits initial originals; bosses and singleton eager");
 check(totalkills==authoredKills&&prndindex==initialRng,"budget retains total kills and original-order initialization RNG");
 ED_Tick();pop=ED_Population();check(pop.liveOriginals==3&&pop.queued==6,"initial quota is not silently expanded on first tic");
 leveltime+=2*TICRATE;ED_Tick();check(ED_Population().liveOriginals==3,"routine pack pauses at effective soft target");
 for(auto* m:actors())if(m->spawnpoint.type==16){P_DamageMobj(m,nullptr,players[0].mo,100000);break;}
 rng=prndindex;ED_Tick();pop=ED_Population();check(pop.liveOriginals==3&&pop.deferred==5&&pop.routine==1,"death frees one slot without an entire pack overrun");check(prndindex==rng&&totalkills==authoredKills,"pack creates neither RNG draws nor duplicate kill totals");
 G_SaveGame(5,(char*)"BUDGETED POPULATION");G_DoSaveGame();gameaction=ga_nothing;
 G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;
 pop=ED_Population();check(pop.deferred==5&&pop.queued==5&&pop.liveOriginals==3&&pop.initialLive==3&&pop.requested==3,"V3 native save restores ownership, initial budget and pending queue");
 double eye[3]={0,0,41},right[3]={0,-1,0},up[3]={0,0,1},forward[3]={1,0,0};
 rng=prndindex;auto nBefore=actors().size();ED_OriginalView(eye,right,up,forward,1,1,1);P_AimLineAttack(players[0].mo,0,2048*FRACUNIT);
 check(actors().size()==nBefore&&ED_Population().deferred==5&&prndindex==rng,"render/facing/autoaim cannot bypass Performance packs");
 ED_Tick();check(ED_Population().deferred==5,"restored timer/cap does not issue duplicate or premature actors");
 c.initialBudgetMode=1;c.initialMonsterPercent=50;reset(c);pop=ED_Population();check(pop.requested==5&&pop.initialLive==5&&pop.deferred==4,"percentage rounds from filtered originals and affects live population");
 c.initialBudgetMode=0;c.initialMonsterCount=3;c.populationPolicy=0;reset(c);pop=ED_Population();check(pop.initialLive==9&&!pop.deferred&&pop.overrun==6&&pop.reasons[9]==6,"Compatibility explicitly overruns when no occlusion proof exists");
 c.populationPolicy=1;c.initialMonsterCount=1;reset(c);pop=ED_Population();check(pop.requested==1&&pop.initialLive==3&&pop.overrun==2,"hard originals override Performance target with reported reason");
 c.initialMonsterCount=3;c.streamPolicy=0;reset(c);check(P_TeleportMove(players[0].mo,1700*FRACUNIT,300*FRACUNIT),"progression fixture positioning");
 ED_Tick();check(ED_Population().routine==2&&ED_Population().liveOriginals==5,"progress pack overrides soft target but respects tic budget");
 ED_Tick();check(ED_Population().routine==4&&ED_Population().liveOriginals==7,"one progress pack spans tics without exceeding pack size");
 ED_Tick();check(ED_Population().routine==4,"no automatic extra pack without another activation event");
 c.streamPolicy=1;
 c.initialMonsterCount=3;reset(c);rng=prndindex;c.enabled=0;ED_Configure(&c);ED_Tick();check(ED_Population().deferred==4,"master-off drains at two per tic, not an unbounded surge");
 ED_Tick();ED_Tick();check(!ED_Population().deferred&&actors().size()==count&&prndindex==rng,"bounded disable drain retains all original monsters and RNG");
 c.enabled=1;reset(c);auto* blocker=P_SpawnMobj(1400*FRACUNIT,-250*FRACUNIT,ONFLOORZ,MT_TROOP);int blockerHealth=blocker->health;
 c.enabled=0;ED_Configure(&c);ED_Tick();ED_Tick();check(ED_Population().blocked>0&&blocker->health==blockerHealth,"occupied original retries without telefragging blocker");
 P_RemoveMobj(blocker);leveltime+=TICRATE;for(int i=0;i<5;++i)ED_Tick();check(!ED_Population().deferred&&totalkills==authoredKills&&actors().size()==count,"freed slot drains safely with original kill total");
 c.enabled=c.monsters=c.items=c.ammo=c.health=1;c.strength=c.coverage=100;reset(c);auto firstClasses=populationClasses(saved());auto partial=ED_Population();
 auto liveMetadata=metadata();G_SaveGame(5,(char*)"ALL MODULES BUDGET");G_DoSaveGame();gameaction=ga_nothing;G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;
 check(ED_Population().deferred==partial.deferred&&metadata()==liveMetadata&&totalkills==authoredKills,"partial save with monster/item randomization retains population and metadata");
 auto v3=saved();auto v2=legacyPopulation(v3);check(ED_Load(v2.data(),v2.size())==v2.size()&&ED_Population().deferred==partial.deferred,"V2 trailer remains readable without replanning or duplicate actors");
 reset(c);check(populationClasses(saved())==firstClasses,"same director seed reproduces per-original resolved classes");
 c=ED_DefaultConfig();c.enabled=c.stream=1;c.populationPolicy=1;c.initialMonsterCount=1;ED_Configure(&c);G_InitNew(sk_medium,1,7);pop=ED_Population();check(!pop.deferred&&pop.requiredEager==pop.filtered&&pop.liveOriginals==pop.filtered&&pop.reasons[1]==pop.filtered,"stock Doom II MAP07 boss-death map remains entirely eager");
 c=ED_DefaultConfig();reset(c);check(!ED_Population().deferred&&actors().size()==count,"disabled module follows ordinary eager path");
 std::puts("PASS Encounter Director focused native integration");std::fflush(stdout);std::exit(0);
}
static void populationAudit(){
 R_BeforeTic=nullptr;auto before=ED_Population();int alive=0;for(auto* m:actors())if(monster(m)&&m->health>0)++alive;
 check(!before.filtered||unsigned(alive)==before.liveOriginals,"runtime living-mobj census matches original population ledger");
 std::printf("POPULATION initial native_live=%d filtered=%u requested=%u required_eager=%u live=%u deferred=%u overrun=%u effective_soft=%u kills=%d policy=%u\n",alive,before.filtered,before.requested,before.requiredEager,before.liveOriginals,before.deferred,before.overrun,before.effectiveSoftTarget,totalkills,before.populationPolicy);
 double eye[3]={double(players[0].mo->x)/FRACUNIT,double(players[0].mo->y)/FRACUNIT,41},right[3]={0,-1,0},up[3]={0,0,1},forward[3]={1,0,0};
 unsigned angle=players[0].mo->angle>>ANGLETOFINESHIFT;forward[0]=double(finecosine[angle])/FRACUNIT;forward[1]=double(finesine[angle])/FRACUNIT;right[0]=forward[1];right[1]=-forward[0];
 int rng=prndindex;ED_OriginalView(eye,right,up,forward,1,1,1);auto after=ED_Population();
 check(prndindex==rng&&after.liveOriginals==before.liveOriginals,"WAD facing has no spawn/RNG side effects");
 std::printf("POPULATION post-facing live=%u deferred=%u routine=%u blocked=%u activation_ms=%.3f setup_ms=%.3f tic_ms=%.3f\n",after.liveOriginals,after.deferred,after.routine,after.blocked,after.activationMs,after.setupMs,after.ticMs);
 for(int r=0;r<10;++r)std::printf("REASON %s %u\n",ED_PopulationReasonName(r),after.reasons[r]);
 std::fflush(stdout);std::exit(0);
}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 char app[]="director-check",iwad[]="-iwad",file[]="-file",warp[]="-warp",one[]="1",checkmap[]="-checkbigmap",nomusic[]="-nomusic",nosound[]="-nosound",cfg[]="-config",cfgname[]="director-doom.cfg";
 char solid[]="-solidmaps";char*args[]={app,iwad,argv[1],file,argv[2],warp,one,checkmap,nomusic,nosound,cfg,cfgname,solid};myargc=13;myargv=args;
 if(const char* audit=std::getenv("HELLFORGE_POPULATION_AUDIT")){auto c=ED_DefaultConfig();c.enabled=c.stream=std::atoi(audit)!=0;c.seed=77;
 if(const char* v=std::getenv("HELLFORGE_POPULATION_POLICY"))c.populationPolicy=std::atoi(v);
 if(const char* v=std::getenv("HELLFORGE_INITIAL_MODE"))c.initialBudgetMode=std::atoi(v);
 if(const char* v=std::getenv("HELLFORGE_INITIAL_COUNT"))c.initialMonsterCount=float(std::atof(v));
 if(const char* v=std::getenv("HELLFORGE_INITIAL_PERCENT"))c.initialMonsterPercent=float(std::atof(v));
 ED_Configure(&c);R_BeforeTic=populationAudit;}else R_BeforeTic=tests;D_DoomMain();return 0;
}
