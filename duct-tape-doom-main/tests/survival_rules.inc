static int foodPath=1,foodAnimations=0,foodConsumed=0,foodMoves=0,painVoices=0;
static void feedingFixture(int type=MT_SERGEANT) {
    reset(type);target.flags|=MF_COUNTKILL;info.flags=MF_COUNTKILL;info.spawnhealth=type==MT_HEAD?400:150;
    target.health=info.spawnhealth*30/100;target.radius=20*FRACUNIT;target.height=56*FRACUNIT;
    foodPath=1;foodAnimations=foodConsumed=foodMoves=painVoices=0;
    P_WeakpointFoodPath=+[](mobj_t*,mobj_t*)->int{return foodPath;};
    P_WeakpointFoodMove=+[](mobj_t*)->int{++foodMoves;return 1;};
    P_WeakpointFoodAnimate=+[](mobj_t*){++foodAnimations;};
    P_WeakpointFoodConsume=+[](mobj_t *m){++foodConsumed;m->flags=0;m->thinker.prev->next=m->thinker.next;m->thinker.next->prev=m->thinker.prev;P_WeakpointForget(m);};
    P_WeakpointPainVoice=+[](mobj_t*){++painVoices;};
    P_WeakpointConfigureFeeding(1);
}
static void survivalRules() {
    const auto balance=p_wp_balance;
    reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
    for(int discharge=1;discharge<=4;++discharge){
        P_WeakpointBeginShot(&source,wp_supershotgun);
        for(int pellet=0;pellet<20;++pellet){auto e=event(WP_SHOTGUN_PELLET,WP_BODY,1);P_WeakpointApply(&e);}
        P_WeakpointEndShot();check(P_WeakpointBleedTier(&target)==std::min(3,discharge),"at most one tier per complete twenty-pellet discharge");
    }
    check(P_WeakpointBleedDamage(&target)==4&&P_WeakpointWalkSpeed(&target,100*FRACUNIT)==80*FRACUNIT,"hemorrhage has four HP ticks but same twenty-percent slow");
    int health=target.health;
    for(int i=1;i<=10;++i){leveltime=10+35*i;P_WeakpointTick(&target);check(target.health==health-4*i,"ten hemorrhage damage ticks");}
    ++leveltime;P_WeakpointTick(&target);check(P_WeakpointBleedTier(&target)==0,"hemorrhage expires completely");
    for(int tier=2;tier<=3;++tier){
        reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
        for(int i=0;i<tier;++i){P_WeakpointBeginShot(&source,wp_pistol);auto e=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&e);P_WeakpointEndShot();}
        target.target=&source;int opportunities=0;for(int i=0;i<100;++i)opportunities+=P_WeakpointBleedOpportunity(&target);
        check(opportunities==(tier==2?15:25),"bounded deterministic attack opportunities without animation/damage changes");
        int hp=target.health;leveltime+=35;P_WeakpointTick(&target);check(target.health==hp-(tier==2?3:4),"tier-specific damage");
        std::vector<unsigned char> save(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());
        check(P_WeakpointBleedTier(&target)==tier,"severity persists in v3 actor-index save");
        // Downgrade this one-record fixture to the prior v2 wire layout.
        std::vector<unsigned char> v2(save.begin(),save.begin()+92);v2[0]=2;
        for(int i=0;i<4;++i)v2.push_back((unsigned char)(100u>>(i*8)));for(char c:{'W','P','0','1'})v2.push_back(c);
        P_WeakpointLoad(v2.data(),v2.size());check(P_WeakpointBleedTier(&target)==1,"prior v2 flat bleeds load as tier one");
        P_WeakpointConfigureExtras(0,0);check(!P_WeakpointBleedTier(&target)&&!P_WeakpointBleedOpportunity(&target),"disable removes all tier and aggression modifiers");
    }
    for(auto species:{MT_SERGEANT,MT_HEAD})for(int scenario=0;scenario<8;++scenario){
        feedingFixture(species);mobjinfo_t preyInfo{};preyInfo.flags=MF_COUNTKILL;
        mobj_t prey{};prey.info=&preyInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.x=16*FRACUNIT;prey.height=56*FRACUNIT;link(&prey);
        if(scenario==1)++target.health;if(scenario==2)foodPath=0;if(scenario==3)wp_test_sight=0;
        if(scenario==4)prey.x=385*FRACUNIT;if(scenario==5)prey.type=MT_BRUISER;
        if(scenario==6){players[0].mo=&source;source.health=100;}if(scenario==7)P_WeakpointConfigureFeeding(0);
        P_WeakpointTick(&target);check((bool)P_WeakpointFeeding(&target)==(scenario==0),"feeding threshold, path, sight, radius, species, player threat and toggle gates");
        if(scenario==0){
            check(P_WeakpointFeedingChase(&target)&&foodAnimations==1,"feeding enters an existing attack animation");
            int hp=target.health;P_WeakpointFeedingAttack(&target);
            int heal=species==MT_HEAD?16:7;check(prey.health==84&&target.health==hp+heal,"feeding bite uses sixteen damage and actual-damage-bounded five-percent healing");
            P_WeakpointFeedingAttack(&target);check(prey.health==84,"one bite per animation action");
            P_WeakpointFeedingHurt(&target);check(!P_WeakpointFeeding(&target),"incoming damage cancels feeding action");
            P_WeakpointTick(&target);check(!P_WeakpointFeeding(&target),"feeding cooldown prevents immediate restart");
        }
    }
    for(auto type:{MT_TROOP,MT_POSSESSED,MT_SHOTGUY,MT_CHAINGUY,MT_WOLFSS,MT_SHADOWS,MT_HEAD,MT_SERGEANT,MT_BRUISER,MT_CYBORG,MT_BABY,MT_SPIDER}){
        feedingFixture();mobjinfo_t bodyInfo{};bodyInfo.flags=MF_COUNTKILL;mobj_t body{};body.info=&bodyInfo;body.type=type;
        body.health=100;body.flags=MF_SHOOTABLE;
        bool live=type==MT_TROOP||type==MT_POSSESSED||type==MT_SHOTGUY||type==MT_CHAINGUY;
        check((bool)P_WeakpointFoodEligible(&body,0)==live,"only the four named living prey types");
        body.health=0;body.flags=MF_CORPSE;body.tics=-1;
        bool corpse=type!=MT_BRUISER&&type!=MT_CYBORG&&type!=MT_BABY&&type!=MT_SPIDER;
        check((bool)P_WeakpointFoodEligible(&body,1)==corpse,"biological corpse whitelist excludes bosses/mechanical props");
        body.z=9*FRACUNIT;check(!P_WeakpointFoodEligible(&body,1),"airborne remains cannot be consumed");
    }
    feedingFixture();mobjinfo_t corpseInfo{};corpseInfo.flags=MF_COUNTKILL;
    mobj_t corpse{};corpse.info=&corpseInfo;corpse.type=MT_TROOP;corpse.health=0;corpse.flags=MF_CORPSE;corpse.tics=-1;corpse.x=16*FRACUNIT;link(&corpse);
    mobj_t prey{};prey.info=&corpseInfo;prey.type=MT_POSSESSED;prey.health=100;prey.flags=MF_SHOOTABLE;prey.x=8*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);check(target.target==&corpse,"accessible corpse preferred over closer live prey");
    P_WeakpointFeedingChase(&target);int hp=target.health;P_WeakpointFeedingAttack(&target);
    check(target.health==hp&&foodConsumed==0,"corpse recovery waits for the eating animation to complete");
    std::vector<unsigned char> save(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());
    check(P_WeakpointFeeding(&target)&&target.target==&corpse,"pending corpse animation restores through the shared save trailer");
    P_WeakpointFeedingChase(&target);check(target.health==hp+15&&foodConsumed==1,"completed corpse action heals ten percent and consumes once");
    check(!P_WeakpointFoodEligible(&corpse,1),"consumed corpse cannot resurrect or feed again");
    // A lethal living bite earns only its actual damage, and its corpse is ineligible.
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=2;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=16*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);P_WeakpointFeedingChase(&target);hp=target.health;P_WeakpointFeedingAttack(&target);
    check(target.health==hp+2&&prey.health<=0,"overkill never generates extra healing");
    prey.flags=MF_CORPSE;prey.tics=-1;check(!P_WeakpointFoodEligible(&prey,1),"rewarded lethal bite prevents immediate corpse double recovery");
    save.resize(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());
    check(!P_WeakpointFoodEligible(&prey,1),"corpse no-recovery marker survives save/load");P_WeakpointForget(&prey);
    check(P_WeakpointFoodEligible(&prey,1),"resurrection/removal lifecycle clears old corpse metadata");
    feedingFixture(MT_HEAD);corpse={};corpse.info=&corpseInfo;corpse.type=MT_TROOP;corpse.flags=MF_CORPSE;corpse.tics=-1;corpse.x=16*FRACUNIT;link(&corpse);target.z=80*FRACUNIT;
    P_WeakpointTick(&target);hp=target.health;P_WeakpointFeedingChase(&target);P_WeakpointFeedingAttack(&target);
    check(target.health==hp&&foodAnimations==0,"floating Cacodemon cannot eat far above a ground corpse");
    target.z=0;P_WeakpointFeedingChase(&target);P_WeakpointFeedingAttack(&target);P_WeakpointFeedingChase(&target);
    check(target.health==hp+40&&foodConsumed==1,"Cacodemon eats only after descending to feeding height");
    // Timing/cancellation and deterministic disabled behavior.
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=100*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);check(P_WeakpointFeeding(&target),"pursuit starts for reachable nearby prey");leveltime+=105;P_WeakpointTick(&target);
    check(!P_WeakpointFeeding(&target),"three-second pursuit/animation timeout");
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=16*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);P_WeakpointConfigureFeeding(0);check(!P_WeakpointFeeding(&target)&&target.target==nullptr,"disabling feeding immediately releases target and overrides");
    check(wp_test_rng_calls==0,"feeding decisions use no additional random values");
    feedingFixture(MT_SHADOWS);P_WeakpointTick(&target);check(!P_WeakpointFeeding(&target),"Spectres never become feeding predators");
    // A single episode may earn several bites, but never exceed the sixty-percent recovery ceiling.
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=16*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);
    for(int i=0;i<12&&P_WeakpointFeeding(&target);++i){P_WeakpointFeedingChase(&target);P_WeakpointFeedingAttack(&target);P_WeakpointFeedingChase(&target);}
    check(target.health==90&&!P_WeakpointFeeding(&target),"repeated earned bites stop exactly at sixty percent, never maximum-health regeneration");
    // Search periods, damage interruption and nearby player threats.
    feedingFixture();P_WeakpointTick(&target);prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=16*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);check(!P_WeakpointFeeding(&target),"food search is periodic instead of rescanning every tic");leveltime+=35;P_WeakpointTick(&target);check(P_WeakpointFeeding(&target),"next search interval can discover new prey");
    players[0].mo=&source;source.health=100;P_WeakpointTick(&target);check(!P_WeakpointFeeding(&target),"an immediate visible player threat interrupts recovery");
    for(int mode=0;mode<4;++mode){
        feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.x=16*FRACUNIT;link(&prey);
        if(mode==0)netgame=true;else if(mode==1)demoplayback=true;else if(mode==2)demorecording=true;else P_WeakpointTargetAllowed=+[](const mobj_t*)->int{return 0;};
        P_WeakpointTick(&target);check(!P_WeakpointFeeding(&target)&&wp_test_rng_calls==0,"feeding obeys network/demo/voxel exclusions without RNG");
    }
    // Simulate an overlapping scripted profile to prove frenzy's target remains authoritative.
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE|MF_COUNTKILL;prey.height=56*FRACUNIT;prey.x=16*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);target.type=MT_KNIGHT;P_WeakpointConfigureExtras(0,1);p_wp_balance.frenzyChance=100;
    P_WeakpointBeginShot(&source,wp_supershotgun);auto head=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&head);P_WeakpointEndShot();target.type=MT_SERGEANT;
    P_WeakpointFeedingTick(&target);check(P_WeakpointFrenzied(&target)&&!P_WeakpointFeeding(&target)&&target.target==&prey,"feeding cancellation cannot override an authoritative frenzy target");
    reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;painVoices=0;P_WeakpointPainVoice=+[](mobj_t*){++painVoices;};state_t unchanged{};target.state=&unchanged;
    for(int i=0;i<3;++i){P_WeakpointBeginShot(&source,wp_pistol);auto hit=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&hit);P_WeakpointEndShot();}
    leveltime+=105;P_WeakpointTick(&target);check(painVoices==1&&target.state==&unchanged,"heavy bleed pain vocals never restart animation");P_WeakpointTick(&target);check(painVoices==1,"pain vocals are cooldown limited");
    P_WeakpointConfigureExtras(0,0);leveltime+=105;P_WeakpointTick(&target);check(painVoices==1,"disabled bleeding cancels pain vocals too");
    reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;source.player=&players[0];players[0].mo=&source;
    P_WeakpointBeginShot(&source,wp_pistol);auto hit=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&hit);P_WeakpointEndShot();
    mobj_t replacement{};replacement.player=&players[0];replacement.health=100;link(&replacement);P_WeakpointForget(&source);players[0].mo=&replacement;
    save.resize(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());
    leveltime+=35;P_WeakpointTick(&target);
    check(wp_test_last_source==&replacement,"original player attribution survives source-body removal, replacement and save/load");
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=100*FRACUNIT;link(&prey);
    P_WeakpointTick(&target);P_WeakpointForget(&prey);check(target.target==nullptr,"food removal clears native actor target before vertical movement can dereference it");
    feedingFixture();prey={};prey.info=&corpseInfo;prey.type=MT_TROOP;prey.health=100;prey.flags=MF_SHOOTABLE;prey.height=56*FRACUNIT;prey.x=100*FRACUNIT;link(&prey);
    P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;P_WeakpointBeginShot(&source,wp_pistol);auto bodyHit=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&bodyHit);P_WeakpointEndShot();P_WeakpointTick(&target);
    save.resize(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointConfigureFeeding(0);P_WeakpointConfigureExtras(0,0);P_WeakpointLoad(save.data(),save.size());
    P_WeakpointConfigureFeeding(1);P_WeakpointConfigureExtras(1,0);check(!P_WeakpointBleedTier(&target)&&!P_WeakpointFeeding(&target),"loading with features off cannot re-arm saved bleeding/feeding when toggled back on");
    p_wp_balance=balance;P_WeakpointConfigureFeeding(0);P_WeakpointConfigureExtras(0,0);
}
