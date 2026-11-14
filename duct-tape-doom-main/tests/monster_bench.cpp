// Isolated fixed-input runner. Never linked into ordinary builds.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_tick.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_random.h"
#include "p_weakpoints.h"
#include "p_fluid.h"
#include "p_swarm.h"
#include "p_boom.h"
extern int rndindex,prndindex;
extern void P_SetAdvancedTactics(int);
extern void P_NoiseAlert(mobj_t*,mobj_t*);
extern int g_death_load_last_save;
extern void G_DoSaveGame(void);
extern int p_monsterrecovery;
extern void A_Look(mobj_t*);
}
#include "../platform/monster_profile.h"
#include "../platform/monster_wake_audit.h"
#include "../platform/monster_bench_state.h"
#undef P_Random
#undef M_Random
extern "C" { void (*MP_BenchmarkRender)(double*)=nullptr; }
extern "C" { void (*MP_BenchmarkDone)(void)=nullptr; }
namespace {
uint64_t randomCalls=0,miscCalls=0,randomTrace=14695981039346656037ull;
uint64_t mapStarted=0,mapSetupNs=0;
uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
uint64_t bytes(uint64_t h,const void *data,size_t size){auto *p=(const unsigned char*)data;for(size_t i=0;i<size;++i)h=(h^p[i])*1099511628211ull;return h;}
template<class T> void field(uint64_t &h,T value){h=bytes(h,&value,sizeof(value));}
int arg(const char *name,int fallback){int at=M_CheckParm((char*)name);return at&&at+1<myargc?atoi(myargv[at+1]):fallback;}
const char *text(const char *name,const char *fallback){int at=M_CheckParm((char*)name);return at&&at+1<myargc?myargv[at+1]:fallback;}
uint64_t hashState() {
    uint64_t h=14695981039346656037ull;
    std::vector<mobj_t*> actors;std::unordered_map<mobj_t*,uint32_t> ids;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto *m=(mobj_t*)t;ids[m]=(uint32_t)actors.size()+1;actors.push_back(m);}
    field(h,leveltime);field(h,gamestate);field(h,gameaction);field(h,prndindex);field(h,randomCalls);field(h,randomTrace);
    for(auto *m:actors){
        for(int v:{(int)m->type,(int)(m->state-states),m->tics,m->x,m->y,m->z,m->momx,m->momy,m->momz,m->floorz,m->ceilingz,m->radius,m->height,m->health,m->flags,m->movedir,m->movecount,m->reactiontime,m->threshold,m->lastlook,m->frame})field(h,v);
        field(h,m->angle);field(h,ids[m->target]);field(h,ids[m->tracer]);
    }
    for(int i=0;i<MAXPLAYERS;++i){auto &p=players[i];field(h,playeringame[i]);field(h,p.playerstate);field(h,p.health);field(h,p.armorpoints);field(h,p.readyweapon);field(h,p.pendingweapon);field(h,p.killcount);field(h,p.cheats);
        for(int a:p.ammo)field(h,a);for(int a:p.powers)field(h,a);field(h,p.viewz);field(h,p.bob);
        for(auto &s:p.psprites){field(h,s.state?(int)(s.state-states):-1);field(h,s.tics);field(h,s.sx);field(h,s.sy);}
    }
    for(int i=0;i<numsectors;++i){auto &s=sectors[i];for(int v:{s.floorheight,s.ceilingheight,(int)s.floorpic,(int)s.ceilingpic,(int)s.lightlevel,(int)s.special,(int)s.tag})field(h,v);field(h,ids[s.soundtarget]);}
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next){
        if(t->function.acp1==(actionf_p1)P_MobjThinker)continue;
        if(t->function.acp1==(actionf_p1)T_VerticalDoor){auto *m=(vldoor_t*)t;field(h,1);field(h,m->sector-sectors);field(h,m->type);field(h,m->topheight);field(h,m->speed);field(h,m->direction);field(h,m->topwait);field(h,m->topcountdown);}
        else if(t->function.acp1==(actionf_p1)T_MoveFloor||t->function.acp1==(actionf_p1)T_BoomElevator){auto *m=(floormove_t*)t;field(h,2);field(h,m->sector-sectors);field(h,m->type);field(h,m->floordestheight);field(h,m->speed);field(h,m->direction);field(h,m->crush);}
        else if(t->function.acp1==(actionf_p1)T_MoveCeiling){auto *m=(ceiling_t*)t;field(h,3);field(h,m->sector-sectors);field(h,m->type);field(h,m->bottomheight);field(h,m->topheight);field(h,m->speed);field(h,m->direction);field(h,m->crush);}
        else if(t->function.acp1==(actionf_p1)T_PlatRaise){auto *m=(plat_t*)t;field(h,4);field(h,m->sector-sectors);field(h,m->speed);field(h,m->low);field(h,m->high);field(h,m->wait);field(h,m->count);field(h,m->status);field(h,m->oldstatus);}
        else field(h,100); // Unknown/light thinker internals are a documented coverage limit.
    }
    std::vector<unsigned char> boom(P_BoomSave(nullptr));P_BoomSave(boom.data());h=bytes(h,boom.data(),boom.size());
    // Existing trailer encodes status references as actor indices, not pointers.
    std::vector<unsigned char> status(P_WeakpointSave(nullptr));P_WeakpointSave(status.data());h=bytes(h,status.data(),status.size());
    struct Context{const std::vector<mobj_t*> &actors;const std::unordered_map<mobj_t*,uint32_t> &ids;} context{actors,ids};
    MP_StateHash sidecars={h,actors.size(),&context,
        [](void *opaque,size_t index)->const void*{return ((Context*)opaque)->actors[index];},
        [](void *opaque,const void *actor)->uint32_t{auto &table=((Context*)opaque)->ids;auto it=table.find((mobj_t*)actor);return it==table.end()?0:it->second;}};
    MP_StateTactics(&sidecars);MP_StateRecovery(&sidecars);MP_StateFluid(&sidecars);MP_StateCombat(&sidecars);h=sidecars.value;
    return h;
}
void dumpState(const std::filesystem::path &folder) {
    std::vector<mobj_t*> actors;std::unordered_map<mobj_t*,int> ids;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){ids[(mobj_t*)t]=(int)actors.size()+1;actors.push_back((mobj_t*)t);}
    std::ofstream dump(folder/"state.txt");
    for(auto *m:actors){dump<<ids[m];
        for(int v:{(int)m->type,(int)(m->state-states),m->tics,m->x,m->y,m->z,m->momx,m->momy,m->momz,m->floorz,m->ceilingz,m->radius,m->height,m->health,m->flags,m->movedir,m->movecount,m->reactiontime,m->threshold,m->lastlook,m->frame})dump<<' '<<v;
        dump<<' '<<m->angle<<' '<<ids[m->target]<<' '<<ids[m->tracer]<<'\n';
    }
    for(int i=0;i<numsectors;++i)dump<<"sector "<<i<<' '<<sectors[i].floorheight<<' '<<sectors[i].ceilingheight<<' '<<sectors[i].lightlevel<<' '<<ids[sectors[i].soundtarget]<<'\n';
    for(int i=0;i<MAXPLAYERS;++i)dump<<"player "<<i<<' '<<players[i].health<<' '<<players[i].viewz<<' '<<players[i].bob<<'\n';
}
}
// Benchmark-only call-site wrappers. The RNG implementation itself is unchanged.
extern "C" void MP_BenchmarkStage(const char *stage){
    if(!strcmp(stage,"map_setup_begin"))WA_Map();
    uint64_t stamp=now();if(!strcmp(stage,"map_setup_begin"))mapStarted=stamp;
    if(!strcmp(stage,"map_setup_end"))mapSetupNs=stamp-mapStarted;
    if(M_CheckParm((char*)"-bench-progress")){printf("BENCH_LOAD elapsed_ns %llu %s\n",(unsigned long long)(stamp-mapStarted),stage);fflush(stdout);}
}
extern "C" int MP_BenchRandom(void){int result=P_Random();++randomCalls;field(randomTrace,result);return result;}
extern "C" int MP_BenchMiscRandom(void){++miscCalls;return M_Random();}
extern "C" int mp_bench_demo_trace=0;
extern "C" int mp_bench_fixed_render=0;
extern "C" int mp_bench_light_clock=0;
extern "C" void MP_BenchmarkStartup(void){WA_Init();mp_bench_light_clock=M_CheckParm((char*)"-bench-fixed-light-clock")!=0;mp_bench_fixed_render=M_CheckParm((char*)"-monsterbench")!=0;mp_bench_demo_trace=M_CheckParm((char*)"-bench-demo-trace")!=0;if(mp_bench_demo_trace)singletics=true;}
extern "C" void MP_BenchmarkObserve(void){
    if(gamestate!=GS_LEVEL)return;
    static std::ofstream trace;
    static int count=0;
    if(!trace.is_open()){
        auto folder=std::filesystem::u8path(text("-bench-demo-trace","demo-trace"));std::filesystem::create_directories(folder);
        trace.open(folder/"demo.csv");trace<<"tic,state_hash,rng_calls,rng_trace,rng_index\n";
    }
    trace<<count++<<','<<hashState()<<','<<randomCalls<<','<<randomTrace<<','<<prndindex<<'\n';
    if(count>=arg("-bench-tics",2100)){
        trace.close();
        if(demorecording)G_CheckDemoStatus(); // Original engine reports completed recording through I_Error.
        exit(0);
    }
}
extern "C" void MP_BenchmarkRun(void) {
    const auto out=std::filesystem::u8path(text("-monsterbench","monster-bench"));std::filesystem::create_directories(out);
    int warm=std::clamp(arg("-bench-warmup",350),0,7000),length=std::clamp(arg("-bench-tics",2100),1,7000);
    const std::string scenario=text("-bench-scenario","chase"),preset=text("-bench-preset","classic");
    const bool setupTargets=scenario!="idle"&&scenario!="shotwake"&&scenario!="activation"&&scenario!="alert"&&scenario!="platform";
    const bool wakeAudit=M_CheckParm((char*)"-bench-wake-audit")!=0;
    bool verify=M_CheckParm((char*)"-bench-verify")!=0;
    const bool progress=M_CheckParm((char*)"-bench-progress")!=0;
    p_fluidmonsters=preset=="fluid";
    P_SwarmConfigure(preset=="hibernate",0);
    // Recovery is on by default and hashes actor addresses. Isolate it explicitly.
    p_monsterrecovery=preset=="recovery";
    P_SetAdvancedTactics(preset=="tactics");
    P_WeakpointConfigureFeeding(preset=="feeding");
    p_wp_enhanced=p_wp_frenzy=preset=="frenzy";
    auto &player=players[0];player.cheats|=CF_GODMODE;
    player.weaponowned[wp_plasma]=true;player.weaponowned[wp_supershotgun]=true;
    player.readyweapon=player.pendingweapon=wp_plasma;player.ammo[am_cell]=100000;
    if(wakeAudit||scenario=="shotwake") {
        player.readyweapon=wp_pistol;player.pendingweapon=wp_nochange;player.ammo[am_clip]=100;
    }
    fixed_t platformInitial=0;
    if(scenario=="platform"){
        if(gamemap!=1||numlines!=8026||numsectors<=358||lines[1339].special!=112||lines[1351].special!=112){
            fprintf(stderr,"Platform fixture requires supplied Abandon MAP01\n");exit(6);
        }
        platformInitial=sectors[115].floorheight;
    }
    if(scenario=="activation"){
        // MAP01's real start uses a shoot-once switch, feeding a voodoo conveyor.
        if(gamemap!=1||numlines<=5043||lines[82].special!=24708||lines[2501].special!=24706||lines[5043].special!=24792){
            fprintf(stderr,"Activation fixture requires the supplied dmnsns2k-monsters MAP01\n");exit(6);
        }
        player.readyweapon=player.pendingweapon=wp_pistol;player.ammo[am_clip]=1000;
        player.mo->angle=R_PointToAngle2(player.mo->x,player.mo->y,-2048*FRACUNIT,456*FRACUNIT);
    }
    int stock=0;for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto *m=(mobj_t*)t;
        if(!m->player&&m->health>0&&(m->flags&MF_SHOOTABLE)&&((m->flags&MF_COUNTKILL)||m->type==MT_SKULL)){
            ++stock;
            if(setupTargets){m->target=player.mo;P_SetMobjState(m,(statenum_t)m->info->seestate);}
            if(preset=="feeding"&&(m->type==MT_HEAD||m->type==MT_SERGEANT))m->health=std::max(1,m->info->spawnhealth/5);
        }
    }
    if(setupTargets)P_NoiseAlert(player.mo,player.mo);
    if(M_CheckParm((char*)"-bench-ambush-behind-test")) {
        mobj_t *subject=nullptr;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
            if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
            auto *m=(mobj_t*)t;
            if(m->player||m->health<=0||!(m->flags&MF_COUNTKILL)||!m->info->seestate)continue;
            if(P_AproxDistance(m->x-player.mo->x,m->y-player.mo->y)<=MELEERANGE)continue;
            if(P_CheckSight(m,player.mo)){subject=m;break;}
        }
        if(!subject){fprintf(stderr,"Ambush behind-player fixture found no visible monster\n");exit(8);}
        subject->flags|=MF_AMBUSH;subject->target=nullptr;
        subject->angle=R_PointToAngle2(subject->x,subject->y,player.mo->x,player.mo->y)+ANG180;
        subject->subsector->sector->soundtarget=player.mo;
        A_Look(subject);
        if(subject->target!=player.mo||subject->state!=&states[subject->info->seestate]){fprintf(stderr,"Ambush failed to react to visible player behind it\n");exit(8);}
        puts("BENCH_AMBUSH_BEHIND PASS initial ambush sight ignores facing");
    }
    std::ofstream metadata(out/"scenario.txt");metadata<<"map "<<gamemap<<"\npopulation "<<stock<<"\nscenario "<<scenario<<"\npreset "<<preset<<"\nwarmup "<<warm<<"\ntics "<<length<<"\nplayer_godmode 1\nsetup_targets "<<setupTargets<<"\nmonster_recovery "<<p_monsterrecovery<<"\nactor_bytes "<<sizeof(mobj_t)<<"\n";
    struct Row{int tic;uint64_t ns,hash,randoms,trace;int rng;uint64_t frameNs,frameInterval;double cpu,gpu,sprites;};std::vector<Row> rows;rows.reserve(length);
    metadata<<"map_setup_ns "<<mapSetupNs<<"\npending_monsters 0\n";
    std::vector<Row> warmRows;if(verify)warmRows.reserve(warm);
    uint64_t previousFrame=0;
    // Read-only, bounded population snapshots outside timed simulation/rendering.
    // No sight queries: those would mutate the global traversal validcount.
    std::ofstream wake;
    if(wakeAudit) {
        wake.open(out/"wake.csv");
        wake<<"tic,living,targeted,moving,sound_marked_sectors,targeted_0_512,targeted_512_1024,targeted_1024_2048,targeted_over_2048,sound_reached_monsters,sound_reached_ambush,sound_reached_nonambush,targeted_ambush,targeted_nonambush\n";
    }
    auto wakeSnapshot=[&](int tic) {
        if(!wakeAudit)return;
        int living=0,targeted=0,moving=0,marked=0,bins[4]={};
        int reached=0,reachedAmbush=0,targetedAmbush=0;
        for(int i=0;i<numsectors;++i)marked+=sectors[i].soundtarget!=nullptr;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
            if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
            auto *m=(mobj_t*)t;
            if(m->player||m->health<=0||!(m->flags&MF_SHOOTABLE)||(!(m->flags&MF_COUNTKILL)&&m->type!=MT_SKULL))continue;
            ++living;moving+=(m->momx||m->momy||m->movecount>0);
            auto *heard=m->subsector->sector->soundtarget;
            if(heard&&(heard->flags&MF_SHOOTABLE)&&!P_Unseen(heard)){++reached;reachedAmbush+=(m->flags&MF_AMBUSH)!=0;}
            if(!m->target)continue;
            ++targeted;
            targetedAmbush+=(m->flags&MF_AMBUSH)!=0;
            double dx=(double(m->x)-player.mo->x)/FRACUNIT,dy=(double(m->y)-player.mo->y)/FRACUNIT;
            double distance=std::hypot(dx,dy);
            ++bins[distance<=512?0:distance<=1024?1:distance<=2048?2:3];
        }
        wake<<tic<<','<<living<<','<<targeted<<','<<moving<<','<<marked;
        for(int bin:bins)wake<<','<<bin;
        wake<<','<<reached<<','<<reachedAmbush<<','<<reached-reachedAmbush<<','<<targetedAmbush<<','<<targeted-targetedAmbush<<'\n';
    };
    wakeSnapshot(-warm-1);
#ifdef DOOM_MONSTER_PROFILING
    MP_ExternalTics();
    MP_Window(0);
#endif
    const int total=warm+length;
    mobj_t *spawned=nullptr;
    std::ofstream live;
    if(progress){live.open(out/"progress.csv");live<<"tic,tic_ns,frame_ns\n";live.flush();printf("BENCH_PROGRESS map_ready population %d pending 0\n",stock);fflush(stdout);}
    WA_MeasureBegin(); // Exclude map construction and initial pre-driver sight.
    for(int n=0;n<total;++n){
        if(n==warm)P_SwarmResetStats();
#ifdef DOOM_MONSTER_PROFILING
        if(n==warm)MP_Window(1);
#endif
        // Explicit synthetic status stress, identical across binaries and outside timed tics.
        if(preset=="frenzy"&&n>=warm&&(n-warm)%175==0){
#ifdef DOOM_MONSTER_PROFILING
            int wasEnabled=mp_enabled;MP_SetEnabled(0);
#endif
            int count=0;
            for(auto *t=thinkercap.next;t!=&thinkercap&&count<32;t=t->next){
                if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
                auto *m=(mobj_t*)t;
                if(m->health<=10||(m->type!=MT_FATSO&&m->type!=MT_KNIGHT&&m->type!=MT_BRUISER))continue;
                ++count;P_WeakpointBeginShot(player.mo,wp_supershotgun);
                wp_damage_event_t event={};event.source=player.mo;event.target=m;event.kind=WP_SHOTGUN_PELLET;event.weapon=wp_supershotgun;event.baseDamage=1;
                event.impact.region=WP_HEAD;event.impact.confidence=100;event.impact.distance=64;P_WeakpointApply(&event);P_WeakpointEndShot();
            }
#ifdef DOOM_MONSTER_PROFILING
            MP_SetEnabled(wasEnabled);
#endif
        }
        if(M_CheckParm((char*)"-bench-lifecycle")){
            int at=n-warm;
            if(at==30&&M_CheckParm((char*)"-bench-teleport-test")){
                mobj_t *subject=nullptr;
                for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)
                    if(t->function.acp1==(actionf_p1)P_MobjThinker){auto *m=(mobj_t*)t;if(!m->player&&m->health>0&&(m->flags&MF_COUNTKILL)){subject=m;break;}}
                int teleported=-1;
                if(subject)for(int i=0;i<numlines;++i)if(lines[i].special==126){
                    fixed_t x=subject->x,y=subject->y;P_CrossSpecialLine(i,0,subject);
                    if(x!=subject->x||y!=subject->y){teleported=i;break;}
                }
                if(teleported<0){fprintf(stderr,"BENCH_TELEPORT FAIL no unoccupied monster-only destination\n");exit(6);}
                printf("BENCH_TELEPORT PASS monster-only line %d; synthetic crossing dispatch, real teleport collision path\n",teleported);fflush(stdout);
            }
            if(at==35){G_SaveGame(6,(char*)"PHASE2 LIFECYCLE");G_DoSaveGame();}
            if(at==70)G_LoadGame((char*)"doomsav6.dsg");
            if(at==100)G_InitNew(gameskill,gameepisode,gamemap);
            if(at==140)spawned=P_SpawnMobj(players[0].mo->x+64*FRACUNIT,players[0].mo->y,ONFLOORZ,MT_TROOP);
            if(at==150&&spawned){P_DamageMobj(spawned,players[0].mo,players[0].mo,10000);if(spawned->health>0)exit(5);}
            if(at==190&&spawned){
                P_WeakpointForget(spawned);spawned->height=spawned->info->height;spawned->flags=spawned->info->flags;
                spawned->health=spawned->info->spawnhealth;P_SetMobjState(spawned,(statenum_t)spawned->info->raisestate);
            }
            if(at==220&&spawned){P_RemoveMobj(spawned);if(spawned->thinker.function.acv!=(actionf_v)(-1))exit(5);spawned=nullptr;}
            if(at==250){g_death_load_last_save=1;players[0].cheats&=~CF_GODMODE;P_DamageMobj(players[0].mo,nullptr,nullptr,10000);}
            if(at==255&&players[0].playerstate!=PST_LIVE){fprintf(stderr,"Death save recovery failed\n");exit(5);}
        }
        ticcmd_t command={};
        if(setupTargets){
            command.angleturn=(short)((n/175)%2?128:-128);
            command.forwardmove=(char)((n/350)%2?12:-12);
            command.sidemove=(char)((n/175)%2?10:-10);
        }
        if(scenario=="combat"&&n%105<70)command.buttons=BT_ATTACK;
        if(scenario=="shotwake"&&n==warm){command.buttons=BT_ATTACK;WA_Shot();}
        if(scenario=="alert"&&n>=35)command.buttons=BT_ATTACK;
        if(scenario=="platform"){
            if(n==45&&M_CheckParm((char*)"-bench-platform-save")){G_SaveGame(5,(char*)"ABANDON PLATFORM");G_DoSaveGame();}
            if(n==50&&M_CheckParm((char*)"-bench-platform-save"))G_LoadGame((char*)"doomsav5.dsg");
            if(n==59&&sectors[115].floorheight!=platformInitial){fprintf(stderr,"Platform released before both switches\n");exit(6);}
            if(n==20||n==60){
                bool left=(n==20)!=(M_CheckParm((char*)"-bench-platform-reverse")!=0);
                if(!P_TeleportMove(player.mo,(left?-16:656)*FRACUNIT,-1856*FRACUNIT)){fprintf(stderr,"Platform fixture positioning failed\n");exit(6);}
                player.mo->z=player.mo->floorz;player.mo->momx=player.mo->momy=player.mo->momz=0;player.mo->angle=left?ANG180:0;
            }
            if(n==21||n==61)command.buttons=BT_USE;
        }
        // Fire at the actual gun switch without forcing any monster targets.
        if(scenario=="activation"){
            if(n>=70&&n<130)command.buttons=BT_ATTACK;
            if(n==140){
                if(!P_TeleportMove(player.mo,2016*FRACUNIT,2400*FRACUNIT)){fprintf(stderr,"Activation fixture positioning failed\n");exit(6);}
                player.mo->z=player.mo->floorz;player.mo->momx=player.mo->momy=player.mo->momz=0;player.mo->angle=ANG90;
            }
            if(n==150)command.buttons=BT_USE;
            if(n>=200)command.buttons=BT_ATTACK;
        }
        if(M_CheckParm((char*)"-bench-lifecycle")&&n-warm==251)command.buttons=BT_USE;
        netcmds[0][(gametic/ticdup)%BACKUPTICS]=command;
        if(progress){printf("BENCH_PROGRESS tic_begin %d\n",n);fflush(stdout);}
        uint64_t start=now();G_Ticker();uint64_t elapsed=now()-start;++gametic;++maketic;
        uint64_t frameNs=0;double rendered[3]={};
        uint64_t frameInterval=0;
        if(MP_BenchmarkRender){
            if(progress){printf("BENCH_PROGRESS frame_begin %d tic_ns %llu\n",n,(unsigned long long)elapsed);fflush(stdout);}
            uint64_t begin=now();frameInterval=previousFrame?begin-previousFrame:0;previousFrame=begin;MP_BenchmarkRender(rendered);frameNs=now()-begin;
        }
        if(progress){live<<n<<','<<elapsed<<','<<frameNs<<'\n';live.flush();printf("BENCH_PROGRESS tic_end %d tic_ns %llu frame_ns %llu\n",n,(unsigned long long)elapsed,(unsigned long long)frameNs);fflush(stdout);}
        if(n==warm-1||n==warm||n==warm+1||n==warm+7||n%35==34||n==total-1)wakeSnapshot(n-warm);
#ifdef DOOM_MONSTER_PROFILING
        if(n>=warm)MP_Tic(gametic);
#endif
        uint64_t state=verify?hashState():0;
        Row row{n-warm,elapsed,state,randomCalls,randomTrace,prndindex,frameNs,frameInterval,rendered[0],rendered[1],rendered[2]};
        if(n>=warm)rows.push_back(row);else if(verify)warmRows.push_back(row);
        if(n-warm==arg("-bench-dump-tic",-1))dumpState(out);
        if(gamestate!=GS_LEVEL){fprintf(stderr,"Benchmark exited level at tic %d\n",n);exit(4);}
    }
    if(MP_BenchmarkDone)MP_BenchmarkDone(); // Join renderer jobs before collecting their thread-local totals.
#ifdef DOOM_MONSTER_PROFILING
    MP_Window(0);MP_Flush();
#endif
    auto writeRows=[&](const char *name,const std::vector<Row> &table){
        std::ofstream csv(out/name);csv<<"tic,ns,state_hash,rng_calls,rng_trace,rng_index,frame_ns,frame_interval_ns,fenced_cpu_ms,fenced_gpu_ms,sprite_ms\n";
        for(const auto &r:table)csv<<r.tic<<','<<r.ns<<','<<r.hash<<','<<r.randoms<<','<<r.trace<<','<<r.rng<<','<<r.frameNs<<','<<r.frameInterval<<','<<r.cpu<<','<<r.gpu<<','<<r.sprites<<'\n';
    };
    writeRows("tics.csv",rows);if(verify)writeRows("warmup.csv",warmRows);
    WA_Flush(out.u8string().c_str());
    if(wakeAudit){
        metadata<<"pistol_ammo_start 100\npistol_ammo_end "<<player.ammo[am_clip]<<'\n';
        if(scenario=="shotwake"&&player.ammo[am_clip]!=99){fprintf(stderr,"One-shot fixture did not fire exactly one pistol round\n");exit(7);}
    }
    auto swarm=P_SwarmStats();std::ofstream swarmFile(out/"swarm.csv");
    swarmFile<<"idle_checks,reused_searches,negative_searches,sound_wakes,spawned\n"<<swarm.idle_checks<<','<<swarm.reused_searches<<','<<swarm.negative_searches<<','<<swarm.sound_wakes<<','<<swarm.spawned<<'\n';swarmFile.close();
    if(scenario=="activation"){
        int awake=0;for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)
            if(t->function.acp1==(actionf_p1)P_MobjThinker){auto *m=(mobj_t*)t;if(!m->player&&m->health>0&&(m->flags&MF_COUNTKILL)&&m->target)++awake;}
        int line=arg("-bench-activation-line",5043);
        if(line<0||line>=numlines||lines[82].special||lines[2501].special||lines[line].special||sectors[47].floorheight>=sectors[47].ceilingheight||awake==0){
            fprintf(stderr,"ACTIVATION FAIL gun-switch %d use-switch %d conveyor-line %d special %d awake %d player %d %d\n",lines[82].special,lines[2501].special,line,line>=0&&line<numlines?lines[line].special:-1,awake,player.mo->x/FRACUNIT,player.mo->y/FRACUNIT);exit(6);
        }
        printf("ACTIVATION PASS line %d consumed; %d monsters targeted through ordinary AI\n",line,awake);
    }
    if(scenario=="platform"){
        bool pass=!lines[1339].special&&!lines[1351].special&&!lines[1537].special&&
            sectors[115].floorheight==64*FRACUNIT&&sectors[144].floorheight==64*FRACUNIT&&sectors[145].floorheight==64*FRACUNIT;
        printf("PLATFORM %s switches %d %d gates %d %d release %d floor %d -> %d\n",pass?"PASS":"FAIL",lines[1339].special,lines[1351].special,sectors[357].ceilingheight/FRACUNIT,sectors[358].ceilingheight/FRACUNIT,lines[1537].special,platformInitial/FRACUNIT,sectors[115].floorheight/FRACUNIT);fflush(stdout);
        if(!pass)exit(6);
    }
    metadata.close();printf("MONSTER BENCH PASS map %d %s %s %d tics population %d\n",gamemap,scenario.c_str(),preset.c_str(),length,stock);fflush(stdout);exit(0);
}
