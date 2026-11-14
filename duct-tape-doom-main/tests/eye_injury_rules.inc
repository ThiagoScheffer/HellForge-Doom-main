// Eye-injury rules against deterministic damage/RNG boundaries.
static void eyeInjuryRules() {
    const auto balance=p_wp_balance;
    auto eyeHit=[&](weapontype_t weapon=wp_pistol,int count=1,int confidence=100){
        P_WeakpointBeginShot(&source,weapon);
        for(int i=0;i<count;++i){auto e=event(count>1?WP_SHOTGUN_PELLET:WP_BULLET,WP_BODY,1,confidence);e.impact.eyeContact=1;P_WeakpointApply(&e);}
        P_WeakpointEndShot();
    };
    for(int type:{MT_HEAD,MT_PAIN,MT_SPIDER}){
        reset(type);P_WeakpointConfigureEyes(1);p_wp_balance.eyeChance=15;wp_test_rng_value=39;
        eyeHit();check(!P_WeakpointBlind(&target)&&!P_WeakpointEyeRage(&target)&&wp_test_rng_calls==1,"15% eye chance excludes first nonqualifying RNG bucket");
        wp_test_rng_value=38;eyeHit();check(P_WeakpointBlind(&target)||P_WeakpointEyeRage(&target),"15% eye chance includes final qualifying RNG bucket");
        check(wp_test_rng_calls==2,"one chance roll per pistol discharge");
    }
    reset(MT_HEAD);P_WeakpointConfigureEyes(1);wp_test_rng_value=255;eyeHit(wp_supershotgun,20);
    check(wp_test_rng_calls==1&&!P_WeakpointBlind(&target),"twenty eye pellets give exactly one 15% roll");
    reset(MT_HEAD);P_WeakpointConfigureEyes(1);target.health=1;eyeHit();check(wp_test_rng_calls==0&&!P_WeakpointBlind(&target),"lethal discharge has no eye roll");
    for(int exclusion=0;exclusion<6;++exclusion){
        reset(MT_HEAD);P_WeakpointConfigureEyes(exclusion!=0);
        if(exclusion==1)P_WeakpointConfigure(0,1,1);
        if(exclusion==2)netgame=true;if(exclusion==3)demoplayback=true;if(exclusion==4)demorecording=true;
        eyeHit(wp_pistol,1,exclusion==5?0:100);
        check(!P_WeakpointBlind(&target)&&wp_test_rng_calls==0,"disabled/compatibility/unconfident eye injuries do not consume RNG");
    }
    reset(MT_HEAD);P_WeakpointConfigureEyes(1);eyeHit(wp_bfg);check(!P_WeakpointBlind(&target)&&!wp_test_rng_calls,"noneligible weapon cannot trigger eye status");
    reset(MT_HEAD);P_WeakpointConfigureEyes(1);p_wp_balance.eyeChance=100;
    auto projectile=event(WP_PLASMA_DIRECT,WP_EYE);projectile.impact.eyeContact=1;P_WeakpointApply(&projectile);check(!P_WeakpointBlind(&target),"projectile eye contact does not cause blindness");
    eyeHit();int calls=wp_test_rng_calls;eyeHit();check(P_WeakpointBlind(&target)&&wp_test_rng_calls==calls,"already-blind target cannot reroll");
    check(!P_WeakpointBlindFireDue(&target),"blind fire waits its interval");leveltime+=70;
    check(P_WeakpointBlindFireDue(&target)&&!P_WeakpointBlindFireDue(&target),"one blind attack opportunity per 70 tics");
    angle_t yaw;fixed_t slope;P_WeakpointBlindAim(&target,&yaw,&slope);check(std::abs((double)slope/FRACUNIT)<=.269,"random blind pitch bounded to fifteen degrees");
    std::vector<unsigned char> save(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());check(save[0]==6,"eye state uses backward-compatible save extension v6");
    P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());check(P_WeakpointBlind(&target),"blindness and next fire time survive actor-index load");
    P_WeakpointConfigureEyes(0);P_WeakpointLoad(save.data(),save.size());check(!P_WeakpointBlind(&target),"disabled eye feature cannot restore pending blindness");
    // v3 prefix remains loadable and cannot invent an eye status.
    std::vector<unsigned char> old(save.begin(),save.begin()+144);old[0]=3;uint32_t bytes=152;
    for(int i=0;i<4;++i)old.push_back((unsigned char)(bytes>>(8*i)));for(char c:{'W','P','0','1'})old.push_back(c);
    P_WeakpointConfigureEyes(1);check(P_WeakpointLoad(old.data(),old.size())==152&&!P_WeakpointBlind(&target),"legacy v3 loads without eye state");
    reset(MT_SPIDER);P_WeakpointConfigureEyes(1);p_wp_balance.eyeChance=100;eyeHit();
    check(P_WeakpointEyeRage(&target)&&P_WeakpointFrenzied(&target),"eye rage is independent of headshot frenzy switch");
    int hp=target.health;auto plasma=event(WP_PLASMA_DIRECT,WP_BRAIN,10);P_WeakpointApply(&plasma);
    check(hp-target.health==30,"rage plasma remains three times, not six");
    hp=target.health;auto bullet=event(WP_BULLET,WP_BODY,10);P_WeakpointApply(&bullet);check(hp-target.health==20,"rage ordinary event doubles once");
    hp=target.health;P_DamageMobj(&target,nullptr,nullptr,10);check(hp-target.health==20,"legacy incoming damage doubles through central boundary");
    mobj_t victim{};victim.player=&player;victim.health=100;
    P_DamageMobj(&victim,&target,&target,10);check(victim.health==80,"raging Spider doubles damage to player");
    mobj_t missile{};P_WeakpointMonsterMissile(&missile,&target);check(P_WeakpointFriendlyMissile(&missile),"eye rage marks projectiles independently of headshot frenzy");
    mobj_t other{};other.type=MT_SPIDER;other.flags=MF_SHOOTABLE|MF_COUNTKILL;other.health=100;other.x=20*FRACUNIT;link(&other);
    P_WeakpointFrenzyTarget(&target);check(target.target==&other,"rage selects same-species non-player target");other.health=0;
    P_WeakpointFrenzyTarget(&target);check(P_WeakpointEyeRage(&target)&&!target.target,"rage persists without a valid victim");
    leveltime+=350;P_WeakpointTick(&target);check(!P_WeakpointEyeRage(&target)&&P_WeakpointFriendlyMissile(&missile),"rage expires but launched projectile provenance survives");
    P_WeakpointClear();
    for(int type:{MT_HEAD,MT_PAIN}){
        feedingFixture(type);P_WeakpointConfigureFeeding(0);P_WeakpointConfigureEyes(1);p_wp_balance.eyeChance=100;target.health=info.spawnhealth;
        mobjinfo_t corpseInfo{};corpseInfo.flags=MF_COUNTKILL;mobj_t corpse{};corpse.type=MT_TROOP;corpse.flags=MF_CORPSE;corpse.info=&corpseInfo;corpse.tics=-1;corpse.x=24*FRACUNIT;corpse.radius=16*FRACUNIT;link(&corpse);
        eyeHit();P_WeakpointFeedingTick(&target);check(P_WeakpointFeeding(&target),"high-health blinded floater feeds with normal feeding disabled");
        P_WeakpointFeedingChase(&target);check(P_WeakpointBlind(&target)&&foodConsumed==0,"approach/animation alone cannot cure blindness");
        P_WeakpointFeedingAttack(&target);P_WeakpointFeedingChase(&target);
        check(!P_WeakpointBlind(&target)&&foodConsumed==1&&!P_WeakpointFeeding(&target),"one consumed corpse bite cures either floater");
        P_WeakpointClear();
    }
    p_wp_balance=balance;P_WeakpointConfigureEyes(0);P_WeakpointClear();
}
