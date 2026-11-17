extern "C" {
#include "../platform/audio/impact_synth.h"
int I_TestCombatBloodWalls(int*,int);
extern int prndindex;
}
static void survivalIntegration() {
    auto *shooter=players[consoleplayer].mo;
    auto clearActors=[&](){
        for(auto *t=thinkercap.next;t!=&thinkercap;){auto *next=t->next;
            if(t->function.acp1==(actionf_p1)P_MobjThinker&&(mobj_t*)t!=shooter)P_RemoveMobj((mobj_t*)t);t=next;}
        P_WeakpointClear();P_WeakpointInitFeedingHooks();
    };
    clearActors();auto balance=p_wp_balance;
    settings.accelerated=1;settings.enhancedWeakPoints=1;settings.weaponVulnerabilities=0;settings.localizedInjuries=0;
    P_WeakpointConfigure(1,0,0);P_WeakpointConfigureExtras(0,0);P_WeakpointImpact=combatImpact;
    P_WeakpointTargetAllowed=+[](const mobj_t*)->int{return 1;};settings.monsterMetalSparks=1;settings.headshotFeedback=3;
    auto *mech=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_BABY);mech->health=10000;
    auto e=wp_damage_event_t{};e.target=mech;e.source=e.inflictor=shooter;e.baseDamage=5;e.kind=WP_SHOTGUN_PELLET;
    e.impact.region=WP_LEGS;e.impact.confidence=100;e.impact.x=0;e.impact.y=128;e.impact.z=8;
    ++leveltime;debris.clear();combatFeedbackStats={};int rng=prndindex,parts=combatFeedbackStats.metalParticles;P_WeakpointBeginShot(shooter,wp_supershotgun);
    for(int i=0;i<20;++i){auto pellet=e;P_WeakpointResolve(&pellet);P_WeakpointNotify(&pellet);}
    P_WeakpointEndShot();check(debris.size()==32&&combatFeedbackStats.metalParticles-parts==32,"strong metal spark burst capped at thirty-two per target/discharge");
    check(prndindex==rng&&mech->health==10000,"cosmetic bursts cannot alter damage or game RNG");
    for(const auto &p:debris)check(p.life>=.15f&&p.life<=.35f&&p.size>=.7f*1.75f,"strong sparks preserve bounded lifetime and configured visibility scale");
    for(auto region:{WP_UNKNOWN,WP_BRAIN,WP_CHASSIS}){
        parts=combatFeedbackStats.metalParticles;auto hit=e;hit.impact.region=region;P_WeakpointResolve(&hit);P_WeakpointNotify(&hit);
        check(parts==combatFeedbackStats.metalParticles,"unknown/chassis/brain do not emit metal sparks");
    }
    parts=combatFeedbackStats.metalParticles;auto splash=e;splash.kind=WP_ROCKET_SPLASH;P_WeakpointResolve(&splash);P_WeakpointNotify(&splash);
    check(parts==combatFeedbackStats.metalParticles,"rocket splash cannot emit anatomical sparks");
    P_RemoveMobj(mech);
    // Sound bank synthesis is tested offline; callbacks below exercise aggregation with -nosound.
    int frames=Impact_Length(IMPACT_HEADSHOT,11025);std::vector<float> sound(frames),again(frames);
    Impact_Render(IMPACT_HEADSHOT,1234,11025,sound.data(),frames);Impact_Render(IMPACT_HEADSHOT,1234,11025,again.data(),frames);
    double energy=0;for(float v:sound){check(std::isfinite(v)&&std::abs(v)<1,"headshot confirmation sample stays finite and unclipped");energy+=v*v;}
    check(sound==again&&energy>.01&&frames<2205,"original headshot synthesis is deterministic, audible-energy and under two hundred ms");
    // Existing renderer wall marks, valid geometry, and no-blood suppression.
    auto floorCache=std::move(floors);floors.resize(1);
    int bloodSprite=Game_Sprite(GAME_SPRITE_BLOOD);
    for(int frame=0;frame<sprites[bloodSprite].numframes;++frame)patch(firstspritelump+sprites[bloodSprite].spriteframes[frame].lump[0]);
    settings.blood=1;settings.bloodSpray=0;
    auto *head=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_KNIGHT);head->health=10000;
    wp_damage_event_t h{};h.target=head;h.source=h.inflictor=shooter;h.kind=WP_SHOTGUN_PELLET;h.baseDamage=5;
    h.impact.region=WP_HEAD;h.impact.confidence=100;h.impact.x=0;h.impact.y=144;h.impact.z=48;
    int calls=combatFeedbackStats.headSounds,bursts=combatFeedbackStats.headBursts;P_WeakpointBeginShot(shooter,wp_supershotgun);
    for(int i=0;i<20;++i){auto pellet=h;P_WeakpointResolve(&pellet);P_WeakpointNotify(&pellet);}P_WeakpointEndShot();
    check(combatFeedbackStats.headSounds-calls==1,"one headshot confirmation sound per complete shotgun discharge");
    check(combatFeedbackStats.headBursts-bursts==1,"shotgun world blood burst aggregated by target/discharge");
    int wallIds[3]={};int count=I_TestCombatBloodWalls(wallIds,3);check(count>0,"head impact near a real wall deposits blood");
    WallHit first{};check(firstSolidWall(0,144,48,0,1,0,64,first),"nearby first solid wall fixture");
    for(int i=0;i<std::min(count,3);++i)check(lines[wallIds[i]].v1->y==lines[first.line].v1->y&&lines[wallIds[i]].v2->y==lines[first.line].v2->y,"blood fan lands on the first wall plane across its adjacent linedefs");
    int before=I_TestCombatBloodWalls(nullptr,0);combatHeadBlood(h);int after=I_TestCombatBloodWalls(nullptr,0);
    check(after-before<=3,"no more than three new wall blood marks per impact");
    std::vector<short> textures;for(int i=0;i<numsides;++i){textures.push_back(sides[i].toptexture);textures.push_back(sides[i].bottomtexture);textures.push_back(sides[i].midtexture);sides[i].toptexture=sides[i].bottomtexture=sides[i].midtexture=0;}
    before=I_TestCombatBloodWalls(nullptr,0);combatHeadBlood(h);after=I_TestCombatBloodWalls(nullptr,0);
    check(before==after,"invalid surfaces cannot receive blood");
    for(int i=0;i<numsides;++i)sides[i].toptexture=sides[i].bottomtexture=sides[i].midtexture=(short)skytexture;
    before=I_TestCombatBloodWalls(nullptr,0);combatHeadBlood(h);after=I_TestCombatBloodWalls(nullptr,0);check(before==after,"sky surfaces cannot receive blood");
    for(int i=0;i<numsides;++i){sides[i].toptexture=textures[i*3];sides[i].bottomtexture=textures[i*3+1];sides[i].midtexture=textures[i*3+2];}
    settings.headshotFeedback=0;calls=combatFeedbackStats.headSounds;bursts=combatFeedbackStats.headBursts;
    P_WeakpointBeginShot(shooter,wp_pistol);auto off=h;off.kind=WP_BULLET;P_WeakpointResolve(&off);P_WeakpointNotify(&off);P_WeakpointEndShot();
    check(calls==combatFeedbackStats.headSounds&&bursts==combatFeedbackStats.headBursts,"feedback Off suppresses sound and all head visuals");
    settings.headshotFeedback=2;settings.headshotSound=0;calls=combatFeedbackStats.headSounds;
    P_WeakpointBeginShot(shooter,wp_pistol);off=h;off.kind=WP_BULLET;P_WeakpointResolve(&off);P_WeakpointNotify(&off);P_WeakpointEndShot();
    check(calls==combatFeedbackStats.headSounds,"independent headshot sound toggle");settings.headshotSound=1;
    settings.blood=0;bursts=combatFeedbackStats.headBursts;combatHeadBlood(h);
    check(bursts==combatFeedbackStats.headBursts,"visual blood disabled suppresses physical HS spray");
    floors=std::move(floorCache);P_RemoveMobj(head);
    // Real native state frames, path collision and Cacodemon descent.
    check(P_TeleportMove(shooter,fx(-240),fx(-32))!=0,"player moved outside feeding threat radius");
    P_WeakpointImpact=nullptr;settings.monsterFeeding=1;p_wp_balance.feedThreat=128;P_WeakpointConfigureFeeding(1);
    for(auto type:{MT_SERGEANT,MT_HEAD}) {
        clearActors();auto *actor=P_SpawnMobj(0,0,ONFLOORZ,type);actor->health=actor->info->spawnhealth*30/100;
        auto *prey=P_SpawnMobj(0,fx(96),ONFLOORZ,MT_TROOP);prey->health=100;prey->tics=-1;
        if(type==MT_HEAD)actor->z=actor->floorz+fx(80);
        int hp=actor->health,preyHp=prey->health;
        P_WeakpointTick(actor);check(P_WeakpointFeeding(actor)&&actor->target==prey,"native path selects valid live prey at thirty percent health");
        for(int i=0;i<100&&actor->health==hp;++i){++leveltime;P_MobjThinker(actor);}
        check(actor->health>hp&&prey->health==preyHp-16,"actual native feeding animation deals one sixteen-HP bite before healing");
        check(actor->health-hp<=std::min(16,actor->info->spawnhealth*5/100),"native recovery bound");
        if(type==MT_HEAD)check(actor->z-actor->floorz<=24*FRACUNIT,"Cacodemon descends instead of eating while hovering high");
        int recovered=actor->health;P_WeakpointConfigureFeeding(0);check(!P_WeakpointFeeding(actor)&&actor->health==recovered,"disable removes feeding state but retains earned recovery");P_WeakpointConfigureFeeding(1);
    }
    clearActors();
    auto *actor=P_SpawnMobj(0,0,ONFLOORZ,MT_SERGEANT);actor->health=45;
    auto *body=P_SpawnMobj(0,fx(48),ONFLOORZ,MT_TROOP);P_DamageMobj(body,shooter,shooter,1000);
    statenum_t lastState=(statenum_t)body->info->deathstate;
    for(int i=0;i<32&&states[lastState].nextstate!=S_NULL;++i)lastState=states[lastState].nextstate;
    P_SetMobjState(body,lastState);body->tics=-1;body->z=body->floorz;
    check(P_WeakpointFoodEligible(body,1),"native final biological corpse is edible");
    P_WeakpointTick(actor);int hp=actor->health;
    for(int i=0;i<80&&actor->health==hp;++i){++leveltime;P_MobjThinker(actor);}
    check(actor->health==60&&body->thinker.function.acv==(actionf_v)(-1),"completed native eating animation removes corpse exactly once and heals fifteen HP");
    // A real wall forbids selection even when the food is inside search range.
    clearActors();actor=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_SERGEANT);actor->health=45;
    auto *blocked=P_SpawnMobj(0,fx(192),ONFLOORZ,MT_TROOP);
    P_WeakpointTick(actor);check(P_WeakpointFeeding(actor)&&actor->target!=blocked,"survival search cannot select food behind a real blocking wall");P_RemoveMobj(blocked);
    // Shared native save boundaries with active new state and severity.
    clearActors();actor=P_SpawnMobj(0,0,ONFLOORZ,MT_SERGEANT);actor->health=45;
    auto *prey=P_SpawnMobj(0,fx(96),ONFLOORZ,MT_TROOP);prey->health=100;prey->tics=-1;P_WeakpointTick(actor);
    settings.monsterBleeding=1;P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
    auto *bleeder=P_SpawnMobj(fx(192),0,ONFLOORZ,MT_FATSO);bleeder->health=1000;
    for(int shot=0;shot<3;++shot){P_WeakpointBeginShot(shooter,wp_pistol);wp_damage_event_t hit{};hit.target=bleeder;hit.source=hit.inflictor=shooter;hit.kind=WP_BULLET;hit.baseDamage=1;P_WeakpointApply(&hit);P_WeakpointEndShot();}
    check(P_WeakpointBleedTier(bleeder)==3&&P_WeakpointFeeding(actor),"native v3 fixture contains active hemorrhage and feeding pursuit");
    G_SaveGame(7,(char*)"Survival v3");G_DoSaveGame();G_LoadGame((char*)"doomsav7.dsg");G_DoLoadGame();shooter=players[consoleplayer].mo;
    actor=bleeder=prey=nullptr;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto *m=(mobj_t*)t;
        if(m->type==MT_SERGEANT&&m->health>0)actor=m;else if(m->type==MT_FATSO&&m->health>0)bleeder=m;else if(m->type==MT_TROOP&&m->health>0)prey=m;}
    check(actor&&bleeder&&prey&&P_WeakpointFeeding(actor)&&actor->target==prey&&P_WeakpointBleedTier(bleeder)==3,"actual native save/load restores v3 tiers and feeding target references");
    clearActors();P_WeakpointConfigureFeeding(0);P_WeakpointConfigureExtras(0,0);settings.monsterFeeding=settings.monsterBleeding=0;p_wp_balance=balance;
    std::puts("PASS survival engine paths: spark budgets, original sound, blood walls, native bites/corpses/descent and v3 saves");
}
