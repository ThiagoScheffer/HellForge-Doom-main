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
#include "p_drop_policy.h"
#include "p_health_recovery.h"
#include "p_acid.h"
extern void G_DoSaveGame(void),G_DoLoadGame(void);
void A_VileChase(mobj_t *);
void A_VileTarget(mobj_t *);
void A_VileAttack(mobj_t *);
void A_Explode(mobj_t *);
void P_SpawnMapThing(mapthing_t *);
}
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL survival update: %s\n",text);std::exit(1);}std::printf("PASS survival update: %s\n",text);}
static mobj_t* playerBody;
static void clearActors(){
 for(auto* t=thinkercap.next;t!=&thinkercap;){auto* next=t->next;if(t->function.acp1==(actionf_p1)P_MobjThinker&&(mobj_t*)t!=playerBody)P_RemoveMobj((mobj_t*)t);t=next;}
 P_WeakpointClear();P_WeakpointInitFeedingHooks();
}
static mobj_t* dropAt(int x,int y,mobjtype_t type) {
 mobj_t* match=nullptr;
 for(auto* t=thinkercap.next;t!=&thinkercap;t=t->next) {
  if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
  auto* thing=(mobj_t*)t;
  if(thing->type!=type||thing->x!=x||thing->y!=y||!(thing->flags&MF_SPECIAL))continue;
  check(!match,"one pickup at the drop location");
  match=thing;
 }
 return match;
}
static void monsterDrops(){
 clearActors();
 auto kill=[](mobjtype_t type,int x,int y,mobj_t* source=nullptr) {
  auto* enemy=P_SpawnMobj(x,y,ONFLOORZ,type);
  P_DamageMobj(enemy,nullptr,source,1000);
  P_DamageMobj(enemy,nullptr,source,1000);
 };
 auto pickup=[](mobj_t* item) {
  check(item&&P_TeleportMove(playerBody,item->x,item->y),"reach pickup");
  P_TouchSpecialThing(item,playerBody);
 };
 auto* player=playerBody->player;
 constexpr int y=128*FRACUNIT;
 P_SetMonsterWeaponDrops(MONSTER_DROPS_CLASSIC);
 kill(MT_SHOTGUY,0,y);kill(MT_CHAINGUY,64*FRACUNIT,y);
 check(dropAt(0,y,MT_SHOTGUN)&&dropAt(64*FRACUNIT,y,MT_CHAINGUN),"Classic gives both original weapons");
 pickup(dropAt(0,y,MT_SHOTGUN));
 check(player->weaponowned[wp_shotgun],"Classic pickup grants the dropped weapon");
 P_SetMonsterWeaponDrops(MONSTER_DROPS_AMMO);
 pickup(dropAt(64*FRACUNIT,y,MT_CHAINGUN));
 check(player->weaponowned[wp_chaingun],"mode change keeps existing weapon drops collectible");
 clearActors();
 mapthing_t placed{64,128,0,2001,7};
 P_SpawnMapThing(&placed);
 auto* placedShotgun=dropAt(64*FRACUNIT,y,MT_SHOTGUN);
 kill(MT_SHOTGUY,0,y);kill(MT_CHAINGUY,-64*FRACUNIT,y);
 kill(MT_POSSESSED,128*FRACUNIT,y);kill(MT_WOLFSS,192*FRACUNIT,y);
 check(!dropAt(0,y,MT_SHOTGUN)&&!dropAt(-64*FRACUNIT,y,MT_CHAINGUN)&&
       placedShotgun&&!(placedShotgun->flags&MF_DROPPED),"weapon replacements coexist with a map shotgun");
 player->weaponowned[wp_shotgun]=player->weaponowned[wp_chaingun]=0;
 player->readyweapon=wp_fist;player->pendingweapon=wp_nochange;
 player->ammo[am_shell]=player->ammo[am_clip]=0;
 auto* shells=dropAt(0,y,MT_MISC22);
 auto* bullets=dropAt(-64*FRACUNIT,y,MT_CLIP);
 check(shells&&bullets&&(shells->flags&MF_DROPPED)&&(bullets->flags&MF_DROPPED),"replacements retain dropped-item flags");
 player->ammo[am_shell]=player->maxammo[am_shell];pickup(shells);
 check(dropAt(0,y,MT_MISC22)==shells,"full ammo leaves the pickup available");
 player->ammo[am_shell]=0;pickup(shells);pickup(bullets);
 check(player->ammo[am_shell]==4&&player->ammo[am_clip]==10&&
       !player->weaponowned[wp_shotgun]&&!player->weaponowned[wp_chaingun]&&
       player->pendingweapon==wp_nochange,"normal replacements give exact ammo without ownership or switching");
 pickup(dropAt(128*FRACUNIT,y,MT_CLIP));pickup(dropAt(192*FRACUNIT,y,MT_CLIP));
 check(player->ammo[am_clip]==20,"Zombieman and SS both retain half-clip drops");
 P_SetMonsterWeaponDrops(MONSTER_DROPS_SCARCE);
 pickup(placedShotgun);
 check(player->weaponowned[wp_shotgun]&&player->pendingweapon==wp_shotgun,"map shotgun still grants weapon and switches normally after mode change");
 clearActors();
 mapthing_t backpack{0,128,0,8,7};P_SpawnMapThing(&backpack);
 pickup(dropAt(0,y,MT_MISC24));
 check(player->backpack&&player->maxammo[am_shell]==2*maxammo[am_shell],"standard backpack capacity");
 auto* killer=P_SpawnMobj(128*FRACUNIT,192*FRACUNIT,ONFLOORZ,MT_TROOP);
 kill(MT_SHOTGUY,0,y,killer);
 auto* gunner=P_SpawnMobj(64*FRACUNIT,y,ONFLOORZ,MT_CHAINGUY);
 P_DamageMobjAcid(gunner,playerBody,1000);
 P_SetMonsterWeaponDrops(MONSTER_DROPS_CLASSIC);
 G_SaveGame(5,(char*)"MONSTER AMMO");G_DoSaveGame();gameaction=ga_nothing;
 G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;
 playerBody=players[0].mo;player=playerBody->player;
 player->ammo[am_clip]=player->ammo[am_shell]=0;
 pickup(dropAt(0,y,MT_MISC22));pickup(dropAt(64*FRACUNIT,y,MT_CLIP));
 check(player->ammo[am_shell]==2&&player->ammo[am_clip]==5,"scarce infighting and acid drops preserve exact quantities through save/load");

 P_SetMonsterWeaponDrops(MONSTER_DROPS_AMMO);
 clearActors();
 auto skill=gameskill;gameskill=sk_baby;
 kill(MT_SHOTGUY,0,y);player->ammo[am_shell]=0;pickup(dropAt(0,y,MT_MISC22));
 check(player->ammo[am_shell]==8,"replacement ammo uses native easy skill bonus");
 gameskill=skill;
 clearActors();
 auto* corpse=P_SpawnMobj(0,y,ONFLOORZ,MT_SHOTGUY);
 P_DamageMobj(corpse,nullptr,playerBody,corpse->health);
 P_SetMobjState(corpse,S_SPOS_DIE5);corpse->tics=-1;
 pickup(dropAt(0,y,MT_MISC22));
 check(P_TeleportMove(playerBody,-200*FRACUNIT,-300*FRACUNIT),"move player clear of resurrection");
 auto* vile=P_SpawnMobj(0,y-48*FRACUNIT,ONFLOORZ,MT_VILE);
 vile->movedir=2;A_VileChase(vile);
 check(corpse->health>0,"native resurrection restores monster");
 check(!dropAt(0,y,MT_MISC22),"resurrection alone creates no drop");
 P_DamageMobj(corpse,nullptr,playerBody,1000);
 check(dropAt(0,y,MT_MISC22)!=nullptr,"second genuine death creates one drop");

 clearActors();
 P_SpawnMobj(0,y,ONFLOORZ,MT_SHOTGUY);
 check(P_TeleportMove(playerBody,0,y)&&dropAt(0,y,MT_MISC22),"telefrag uses the same drop policy");
 check(P_TeleportMove(playerBody,-200*FRACUNIT,-300*FRACUNIT),"player clear of blast fixture");
 clearActors();
 auto* barrel=P_SpawnMobj(0,y,ONFLOORZ,MT_BARREL);
 P_SpawnMobj(48*FRACUNIT,y,ONFLOORZ,MT_CHAINGUY);
 A_Explode(barrel);
 check(dropAt(48*FRACUNIT,y,MT_CLIP)!=nullptr,"barrel death uses the same drop policy");
 clearActors();
 auto* caster=P_SpawnMobj(0,y-104*FRACUNIT,ONFLOORZ,MT_VILE);
 caster->target=P_SpawnMobj(0,y,ONFLOORZ,MT_SHOTGUY);
 A_VileTarget(caster);A_VileAttack(caster);
 check(dropAt(0,y,MT_MISC22)!=nullptr,"Arch-vile fire attack uses the same drop policy");
 clearActors();
 auto* victim=P_SpawnMobj(0,y,ONFLOORZ,MT_SHOTGUY);
 P_WeakpointConfigure(1,0,0);
 P_WeakpointBeginShot(playerBody,wp_shotgun);
 wp_damage_event_t head{};
 head.target=victim;head.source=head.inflictor=playerBody;
 head.kind=WP_SHOTGUN_PELLET;head.baseDamage=100;head.impact.region=WP_HEAD;head.impact.confidence=100;
 P_WeakpointApply(&head);P_WeakpointEndShot();
 check(dropAt(0,y,MT_MISC22)!=nullptr,"weakpoint kill uses the same drop policy");
 P_WeakpointConfigure(0,0,0);

 clearActors();
 demorecording=true;kill(MT_SHOTGUY,0,y);demorecording=false;
 demoplayback=true;kill(MT_SHOTGUY,64*FRACUNIT,y);demoplayback=false;
 netgame=true;kill(MT_CHAINGUY,128*FRACUNIT,y);netgame=false;
 deathmatch=2;kill(MT_CHAINGUY,192*FRACUNIT,y);deathmatch=0;
 check(dropAt(0,y,MT_SHOTGUN)&&dropAt(64*FRACUNIT,y,MT_SHOTGUN)&&
       dropAt(128*FRACUNIT,y,MT_CHAINGUN)&&dropAt(192*FRACUNIT,y,MT_CHAINGUN),
       "demos, co-op and deathmatch retain Classic drops");
 clearActors();
 deathmatch=2;
 mapthing_t mapClip{0,128,0,2007,7};P_SpawnMapThing(&mapClip);
 player->ammo[am_clip]=0;pickup(dropAt(0,y,MT_CLIP));
 check(player->ammo[am_clip]==clipammo[am_clip],"map clip retains original quantity");
 leveltime+=30*TICRATE;
 for(int i=0;i<128&&!dropAt(0,y,MT_CLIP);++i)P_RespawnSpecials();
 check(dropAt(0,y,MT_CLIP)!=nullptr,"map clip still respawns in alternate deathmatch");
 deathmatch=0;
 G_InitNew(skill,gameepisode,gamemap);
 check(!dropAt(0,y,MT_CLIP),"a fresh level clears prior pickups");
 P_SetMonsterWeaponDrops(MONSTER_DROPS_CLASSIC);
}
static void medicalRecovery(){
 playerBody=players[0].mo;clearActors(); auto* player=playerBody->player;
 auto health=[&](int hp){player->health=playerBody->health=hp;};
 auto touch=[&](mobj_t* item){P_TouchSpecialThing(item,playerBody);};
 P_SetHealthRecovery(99);check(P_HealthRecoveryMode()==0,"invalid recovery setting defaults to Classic");
 P_SetHealthRecovery(HEALTH_RECOVERY_MEDIKITS);
 for(int hp=1;hp<=200;++hp){
  auto* item=P_SpawnMobj(playerBody->x,playerBody->y,playerBody->z,MT_MISC11);
  health(hp);touch(item);
  int restored=hp<100 ? (25<100-hp ? 25 : 100-hp) : 0;
  check(player->health==hp+restored,"medical pickup healing is capped");
  if(restored && restored<25){check(item->type==MT_MISC10 && item->monsterAmmoDrop==(0x4d00|25-restored),"exact overflow retained");P_RemoveMobj(item);}
  else if(!restored) P_RemoveMobj(item);
 }
 auto* item=P_SpawnMobj(playerBody->x,playerBody->y,playerBody->z,MT_MISC11);
 item->flags|=MF_COUNTITEM;int count=player->itemcount;health(90);touch(item);
 check(player->health==100&&item->monsterAmmoDrop==0x4d0f&&player->itemcount==count+1,"90 HP medikit yields 15 and one item credit");
 P_SetHealthRecovery(HEALTH_RECOVERY_CLASSIC);health(92);touch(item);
 check(player->health==100&&item->monsterAmmoDrop==0x4d07&&player->itemcount==count+1,"partial pack survives Classic and earns no second credit");
 G_SaveGame(5,(char*)"MEDICAL RECOVERY");G_DoSaveGame();gameaction=ga_nothing;
 G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();gameaction=ga_nothing;playerBody=players[0].mo;player=playerBody->player;
 item=dropAt(playerBody->x,playerBody->y,MT_MISC10);
 check(item&&item->monsterAmmoDrop==0x4d07,"remaining capacity survives native save/load");
 health(80);touch(item);check(player->health==87&&player->itemcount==count+1,"last seven points consumed exactly");
 P_SetHealthRecovery(HEALTH_RECOVERY_ALL);item=P_SpawnMobj(playerBody->x,playerBody->y,playerBody->z,MT_MISC10);
 health(97);touch(item);check(item->monsterAmmoDrop==0x4d07,"stimpack overflow retains seven points");
 netgame=true;health(80);touch(item);check(player->health==80&&item->monsterAmmoDrop==0x4d07,"restricted sessions reject existing packs");netgame=false;
 P_RemoveMobj(item);P_SetHealthRecovery(0);clearActors();
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
 monsterDrops();
 medicalRecovery();
 std::puts("PASS focused native survival update");std::exit(0);
}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 char app[]="survival-update",iwad[]="-iwad",file[]="-file",warp[]="-warp",one[]="1",smoke[]="-checkbigmap",sound[]="-nosound",music[]="-nomusic",config[]="-config",cfg[]="survival-update.cfg";
 char* args[]={app,iwad,argv[1],file,argv[2],warp,one,smoke,sound,music,config,cfg};myargc=12;myargv=args;R_BeforeTic=tests;D_DoomMain();return 0;
}
