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
 std::puts("PASS Encounter Director focused native integration");std::fflush(stdout);std::exit(0);
}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 char app[]="director-check",iwad[]="-iwad",file[]="-file",warp[]="-warp",one[]="1",checkmap[]="-checkbigmap",nomusic[]="-nomusic",nosound[]="-nosound",cfg[]="-config",cfgname[]="director-doom.cfg";
 char solid[]="-solidmaps";char*args[]={app,iwad,argv[1],file,argv[2],warp,one,checkmap,nomusic,nosound,cfg,cfgname,solid};myargc=13;myargv=args;R_BeforeTic=tests;D_DoomMain();return 0;
}
