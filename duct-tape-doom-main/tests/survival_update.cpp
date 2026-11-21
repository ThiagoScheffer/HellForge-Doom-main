// Focused real-core checks: same-species projectile collisions and survival AI.
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
extern "C" {
#include "doomstat.h"
#include "p_local.h"
#include "g_game.h"
#include "d_main.h"
#include "m_argv.h"
#include "p_weakpoints.h"
#include "r_main.h"
extern void G_DoSaveGame(void),G_DoLoadGame(void);
}
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL survival update: %s\n",text);std::exit(1);}std::printf("PASS survival update: %s\n",text);}
static mobj_t* playerBody;
static void clearActors(){
 for(auto* t=thinkercap.next;t!=&thinkercap;){auto* next=t->next;if(t->function.acp1==(actionf_p1)P_MobjThinker&&(mobj_t*)t!=playerBody)P_RemoveMobj((mobj_t*)t);t=next;}
 P_WeakpointClear();P_WeakpointInitFeedingHooks();
}
static void tests(){
 R_BeforeTic=nullptr;playerBody=players[0].mo;clearActors();
 P_WeakpointConfigure(1,0,0);P_WeakpointConfigureExtras(0,1);p_wp_balance.frenzyChance=100;
 check(P_TeleportMove(playerBody,0,-300*FRACUNIT)!=0,"player outside duel");
 auto* a=P_SpawnMobj(-120*FRACUNIT,0,ONFLOORZ,MT_FATSO);auto* b=P_SpawnMobj(120*FRACUNIT,0,ONFLOORZ,MT_FATSO);a->health=b->health=10000;
 P_WeakpointBeginShot(playerBody,wp_supershotgun);wp_damage_event_t hit{};hit.source=hit.inflictor=playerBody;hit.target=a;hit.kind=WP_SHOTGUN_PELLET;hit.baseDamage=1;hit.impact.region=WP_HEAD;hit.impact.confidence=100;hit.impact.distance=64;P_WeakpointApply(&hit);P_WeakpointEndShot();
 check(P_WeakpointFrenzied(a)&&a->target==b,"real frenzy chooses same-species victim");
 auto* missile=P_SpawnMissile(a,b,MT_FATSHOT);int before=b->health;P_TryMove(missile,b->x,b->y);check(b->health<before&&b->target==a,"first frenzy projectile damages and provokes retaliation");P_RemoveMobj(missile);
 leveltime+=p_wp_balance.frenzyDuration;P_WeakpointFrenzyTarget(a);check(!P_WeakpointFrenzied(a)&&a->target==b,"expiry retains an established infighting opponent");
 auto* other=P_SpawnMobj(0,240*FRACUNIT,ONFLOORZ,MT_FATSO);other->health=10000;
 missile=P_SpawnMissile(b,a,MT_FATSHOT);check(!P_WeakpointFriendlyMissile(missile)&&P_WeakpointCanHitSpecies(missile,a)&&!P_WeakpointCanHitSpecies(missile,other)&&!P_WeakpointCanHitSpecies(missile,b),"retaliation exemption is opponent-specific and preserves shooter immunity");
 before=a->health;P_TryMove(missile,a->x,a->y);check(a->health<before,"ordinary Mancubus retaliation really damages expired frenzy attacker");P_RemoveMobj(missile);
 missile=P_SpawnMissile(a,b,MT_FATSHOT);before=b->health;P_TryMove(missile,b->x,b->y);check(!P_WeakpointFriendlyMissile(missile)&&b->health<before,"former frenzy attacker can also damage its opponent with a normal missile");P_RemoveMobj(missile);
 missile=P_SpawnMissile(b,a,MT_FATSHOT);
 G_SaveGame(5,(char*)"SURVIVAL DUEL");G_DoSaveGame();gameaction=ga_nothing;G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;playerBody=players[0].mo;a=b=other=missile=nullptr;
 for(auto* t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto* m=(mobj_t*)t;if(m->type==MT_FATSO){if(m->x<0)a=m;else if(m->x>0)b=m;else other=m;}else if(m->type==MT_FATSHOT&&(m->flags&MF_MISSILE))missile=m;}
 check(a&&b&&other&&missile&&P_WeakpointCanHitSpecies(missile,a)&&!P_WeakpointCanHitSpecies(missile,other),"native V7 save restores duel and in-flight retaliation references");
 P_WeakpointConfigureExtras(0,0);P_WeakpointConfigure(0,0,0);before=a->health;P_TryMove(missile,a->x,a->y);check(a->health<before,"turning frenzy off does not make an existing duel invulnerable");P_RemoveMobj(missile);
 P_RemoveMobj(a);check(b->target!=a,"removal clears the hostile actor reference");
 clearActors();P_WeakpointConfigureFeeding(1);check(P_TeleportMove(playerBody,-200*FRACUNIT,-300*FRACUNIT)!=0,"player outside feeding threat radius");
 bool retreatFacesStep=true;
 for(auto type:{MT_HEAD,MT_SERGEANT,MT_VILE}){
  clearActors();auto* actor=P_SpawnMobj(0,0,ONFLOORZ,type);actor->health=actor->info->spawnhealth*30/100;actor->target=playerBody;
  int start=leveltime;P_WeakpointTick(actor);
  const int expectedSpeed=type==MT_VILE?100:120;
  check(P_WeakpointFeeding(actor)&&P_WeakpointWalkSpeed(actor,100*FRACUNIT)==expectedSpeed*FRACUNIT,"low-health predator searches at its configured speed");
  check(P_TeleportMove(playerBody,0,-64*FRACUNIT)!=0,"player approaches hungry predator");
  const fixed_t x=actor->x,y=actor->y;
  actor->angle=ANG270;
  P_WeakpointFeedingChase(actor);
  check(actor->y>y,"native movement retreats from nearby player threat");
  retreatFacesStep &= actor->angle==R_PointToAngle2(x,y,actor->x,actor->y);
  check(P_TeleportMove(playerBody,-200*FRACUNIT,-300*FRACUNIT)!=0,"player leaves survival search");
  leveltime=start+p_wp_balance.feedWindow-1;P_WeakpointTick(actor);check(P_WeakpointFeeding(actor),"search remains active before ten-second deadline");
  ++leveltime;P_WeakpointTick(actor);check(!P_WeakpointFeeding(actor)&&actor->target==playerBody&&(actor->flags&MF_JUSTHIT),"ten-second failed search resumes aggressive native attack");
 }
 bool pursuitFacesStep=true;
 for(auto type:{MT_HEAD,MT_VILE}){
  clearActors();
  auto* actor=P_SpawnMobj(0,0,ONFLOORZ,type);
  auto* prey=P_SpawnMobj(0,160*FRACUNIT,ONFLOORZ,MT_TROOP);
  actor->health=actor->info->spawnhealth*30/100;
  const fixed_t x=actor->x,y=actor->y;
  actor->angle=ANG270;
  check(P_WeakpointFeedingChase(actor)&&actor->target==prey&&actor->y>y,"native feeding chase approaches live prey");
  pursuitFacesStep &= actor->angle==R_PointToAngle2(x,y,actor->x,actor->y);
 }
 check(retreatFacesStep&&pursuitFacesStep,"retreat and feeding pursuit face their actual movement");
 // Existing corpse recovery still completes through native animation and movement.
 for(auto type:{MT_HEAD,MT_SERGEANT}){
  clearActors();auto* actor=P_SpawnMobj(0,0,ONFLOORZ,type);actor->health=actor->info->spawnhealth*30/100;int hp=actor->health;
  auto* body=P_SpawnMobj(0,80*FRACUNIT,ONFLOORZ,MT_TROOP);P_DamageMobj(body,playerBody,playerBody,1000);
  statenum_t end=(statenum_t)body->info->deathstate;for(int i=0;i<32&&states[end].nextstate!=S_NULL;++i)end=states[end].nextstate;P_SetMobjState(body,end);body->tics=-1;body->z=body->floorz;
  for(int i=0;i<120&&actor->health==hp;++i){++leveltime;P_MobjThinker(actor);}
  check(actor->health==hp+actor->info->spawnhealth/10&&body->thinker.function.acv==(actionf_v)(-1),"native hungry predator reaches and consumes one corpse for bounded recovery");
 }
 clearActors();auto* seeker=P_SpawnMobj(0,0,ONFLOORZ,MT_SERGEANT);seeker->health=45;seeker->target=playerBody;P_WeakpointTick(seeker);leveltime+=100;
 G_SaveGame(5,(char*)"SURVIVAL SEARCH");G_DoSaveGame();gameaction=ga_nothing;G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;playerBody=players[0].mo;seeker=nullptr;
 for(auto* t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker&&((mobj_t*)t)->type==MT_SERGEANT)seeker=(mobj_t*)t;
 check(seeker&&P_WeakpointFeeding(seeker),"targetless search survives native save/load");leveltime+=p_wp_balance.feedWindow-101;P_WeakpointTick(seeker);check(P_WeakpointFeeding(seeker),"save restores remaining search deadline without restarting ten seconds");++leveltime;P_WeakpointTick(seeker);check(!P_WeakpointFeeding(seeker)&&seeker->target==playerBody,"restored search expires at its original deadline");
 clearActors();auto* vile=P_SpawnMobj(0,0,ONFLOORZ,MT_VILE);auto* prey=P_SpawnMobj(0,64*FRACUNIT,ONFLOORZ,MT_TROOP);vile->health=vile->info->spawnhealth/2;prey->health=100;
 int health=vile->health;P_WeakpointTick(vile);for(int i=0;i<4&&vile->health==health;++i)P_WeakpointFeedingChase(vile);
 check(vile->health==health+24&&prey->health==76,"Arch-vile hunts and steals only actual 24-HP prey damage without requiring bleeding");
 P_WeakpointConfigureFeeding(0);check(!P_WeakpointFeeding(vile)&&P_WeakpointWalkSpeed(vile,100*FRACUNIT)==100*FRACUNIT,"feeding toggle removes survival overrides");
 std::puts("PASS focused native survival update");std::exit(0);
}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 char app[]="survival-update",iwad[]="-iwad",file[]="-file",warp[]="-warp",one[]="1",smoke[]="-checkbigmap",sound[]="-nosound",music[]="-nomusic",config[]="-config",cfg[]="survival-update.cfg";
 char* args[]={app,iwad,argv[1],file,argv[2],warp,one,smoke,sound,music,config,cfg};myargc=12;myargv=args;R_BeforeTic=tests;D_DoomMain();return 0;
}
