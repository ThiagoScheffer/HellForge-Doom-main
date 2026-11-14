#include "../platform/hud_screen_position.h"
#include "../../map_world.h"
const lab::MapWorld *SV_MapWorld();
#include <chrono>
extern "C" {void P_Ticker(void);extern int debuggod,debugfly;void P_PlayerInSpecialSector(player_t*);void A_FireShotgun(player_t*,pspdef_t*);}
static void e1m8Frame(){
    using Clock=std::chrono::steady_clock;static auto previous=Clock::now();auto now=Clock::now();double frameMs=std::chrono::duration<double,std::milli>(now-previous).count();previous=now;
    static int stage=0,start=0,moving=0,shots=0;static fixed_t initialFloor=0;static std::vector<double> baseline,lowering;static FILE *csv=nullptr;
    auto *p=&players[consoleplayer];auto *actor=p->mo;
    if(stage==0){
        for(int anchor:{0,1,2,3})for(float x:{-100.f,100.f})for(float y:{-100.f,100.f}){
            auto panel=hudScreenPosition(960,540,720,115,70,anchor,x,y,false);
            check(std::abs((x<0?panel.x:panel.x+panel.w)-(x<0?0.f:960.f))<.01f&&std::abs((y<0?panel.y:panel.y+panel.h)-(y<0?0.f:540.f))<.01f,"Speedrun can reach all physical widescreen corners without stretching text");
        }
        check(gameepisode==1&&gamemap==8,"diagnostic is restricted to original E1M8");
        const auto *world=SV_MapWorld();check(world&&world->floorShielded(30),"original E1M8 tag-666 platform must be shielded before activation");
        check(!world->floorShielded(28)&&!world->floorShielded(29),"small lift and normal courtyard remain damageable");csv=std::fopen((output+"/frames.csv").c_str(),"w");std::fprintf(csv,"tic,phase,frame_ms,floor_height\n");start=leveltime;initialFloor=sectors[30].floorheight;
        if(!e1m8Exit){settings.speedrunHUD=1;settings.speedrunLayout=0;settings.speedrunAnchor=3;settings.speedrunX=settings.speedrunY=100;}
        p->cheats|=CF_GODMODE;P_TeleportMove(actor,416*FRACUNIT,2550*FRACUNIT);actor->z=actor->floorz;actor->angle=ANG90;
        if(!e1m8Exit&&SDL_getenv("HELLFORGE_E1M8_COMBAT"))for(int i=0;i<numlines;++i)if(lines[i].special==2&&lines[i].tag==5){P_CrossSpecialLine(i,0,actor);break;}
        stage=1;return;
    }
    p->cmd={};
    if(e1m8Exit){
        if(leveltime-start<4)return;
        p->cheats=0;p->powers[pw_invulnerability]=p->powers[pw_ironfeet]=0;p->health=actor->health=100;p->playerstate=PST_LIVE;
        check(P_TeleportMove(actor,608*FRACUNIT,6464*FRACUNIT),"enter original E1M8 death-exit sector");actor->z=actor->floorz;actor->momx=actor->momy=actor->momz=0;
        std::printf("E1M8_EXIT_ENTER sector %d special %d z %d floor %d sector_floor %d debug %d/%d\n",(int)(actor->subsector->sector-sectors),actor->subsector->sector->special,actor->z,actor->floorz,actor->subsector->sector->floorheight,debuggod,debugfly);
        {auto *sector=actor->subsector->sector;p->health=actor->health=5;actor->z=actor->floorz=sector->floorheight+FRACUNIT;actor->momz=0;
            P_PlayerInSpecialSector(p);check(gameaction!=ga_completed,"raised support must not count as touching the exit floor");
            actor->floorz=sector->floorheight;actor->momz=-FRACUNIT;P_PlayerInSpecialSector(p);check(gameaction!=ga_completed,"falling above the exit floor must not complete the level");
            p->health=actor->health=100;actor->z=actor->floorz=sector->floorheight;actor->momz=0;}
        int ticks=0;for(;ticks<224&&gameaction!=ga_completed;++ticks){if(e1m8Snap)actor->z=actor->subsector->sector->floorheight;P_Ticker();}
        bool exited=gameaction==ga_completed;
        std::printf("E1M8_EXIT_RESULT exited %d hp %d ticks %d z %d floor %d sector_floor %d\n",exited,actor->health,ticks,actor->z,actor->floorz,actor->subsector->sector->floorheight);std::fflush(stdout);std::fclose(csv);
        check(exited,"E1M8 death-exit must complete the level instead of sticking at one HP");I_Quit();return;
    }
    std::fprintf(csv,"%d,%d,%.3f,%d\n",leveltime,stage,frameMs,sectors[30].floorheight);
    if(stage==1){
        if(leveltime-start==3){I_Render3DScreenshotTo((output+"/speedrun-bottom-right.png").c_str());}
        if(leveltime-start>=4)settings.speedrunHUD=0;
        if(leveltime-start>4)baseline.push_back(frameMs);
        if(leveltime-start<20)return;
        stage=2;start=leveltime;return;
    }
    if(stage==2&&SDL_getenv("HELLFORGE_E1M8_COMBAT")){
        mobj_t *target=nullptr;for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*m=(mobj_t*)t;if(m->type==MT_BRUISER&&m->health>0){target=m;break;}}
        if(target){
            if((leveltime-start)%35==0){
                check(P_TeleportMove(actor,target->x,target->y-48*FRACUNIT),"position shotgun shooter beside original Baron");actor->z=actor->floorz;
                actor->angle=R_PointToAngle2(actor->x,actor->y,target->x,target->y);tourpitch=std::atan2(units(target->z+target->height/2-actor->z-41*FRACUNIT),48.);
                p->readyweapon=wp_shotgun;p->ammo[am_shell]=1000;
                std::printf("E1M8_SIGHT %d distance %.2f\n",P_CheckSight(actor,target),units(P_AproxDistance(actor->x-target->x,actor->y-target->y)));int hp=target->health;A_FireShotgun(p,&p->psprites[ps_weapon]);++shots;
                std::printf("E1M8_SHOT %d health %d -> %d player_z %.2f target_z %.2f\n",shots,hp,target->health,units(actor->z),units(target->z));std::fflush(stdout);
                check(shots<80,"shotgun must kill the original Barons within bounded test time");
            }
            return;
        }
        std::printf("E1M8_SHOTGUN_KILLS shots %d\n",shots);P_TeleportMove(actor,416*FRACUNIT,(SDL_getenv("HELLFORGE_E1M8_COURTYARD")?3400:2550)*FRACUNIT);actor->z=actor->floorz;actor->angle=ANG90;tourpitch=0;stage=4;start=leveltime;
    }else if(stage==2&&!shots){
        int bosses=0;for(auto*t=thinkercap.next,*next=t->next;t!=&thinkercap;t=next,next=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*m=(mobj_t*)t;if(m->type==MT_BRUISER&&m->health>0){P_DamageMobj(m,actor,actor,10000);++bosses;}}
        check(bosses==2,"kill exactly both original E1M8 Barons");shots=1;
    }
    if(stage==2||stage==4){if(sectors[30].floorheight==initialFloor){check(leveltime-start<160,"Baron deaths must activate tagged floor");return;}stage=3;}
    lowering.push_back(frameMs);++moving;
    if(moving==64)I_Render3DScreenshotTo((output+"/lowering.png").c_str());
    if(moving%64==0){std::fprintf(stderr,"E1M8_FLOOR_PROGRESS samples %d floor %.2f\n",moving,units(sectors[30].floorheight));}
    if(!sectors[30].floordata||moving>=384){
        std::sort(baseline.begin(),baseline.end());std::sort(lowering.begin(),lowering.end());double a=baseline[baseline.size()/2],b=lowering[lowering.size()/2];
        std::printf("E1M8_FLOOR baseline_median_ms %.3f lowering_median_ms %.3f lowering_p95_ms %.3f samples %d\n",a,b,lowering[(lowering.size()*95)/100],moving);std::fflush(stdout);std::fclose(csv);
        check(lowering[(lowering.size()*95)/100]<std::max(80.,a*3),"lowering E1M8 floor must not cause a huge sustained frame-time spike");I_Quit();
    }
}

