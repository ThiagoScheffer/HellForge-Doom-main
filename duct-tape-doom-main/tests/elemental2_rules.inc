static void dotFixture(int type=MT_POSSESSED){reset(type);target.flags|=MF_COUNTKILL;info.flags|=MF_COUNTKILL;P_CombatConfigureDot(1);source.player=&players[0];players[0].mo=&source;}
static void elemental2Rules(){
    struct Burn{int kind,damage,ticks,low,high;};const Burn burns[]={{3,3,6,38,39},{1,5,4,25,26},{2,4,5,30,31}};
    for(const auto &b:burns){
        for(int byte:{b.low,b.high}){
            dotFixture();wp_test_rng_value=byte;P_CombatConfirmed(&target,&source,b.kind,1,-1);
            check((P_CombatDotKind(&target)!=0)==(byte==b.low),"15/10/12 percent byte RNG boundaries");check(wp_test_rng_calls==1,"one gameplay ignition roll for eligible hit");
        }
        dotFixture();P_CombatConfirmed(&target,&source,b.kind,1,-1);int hp=target.health;P_WeakpointTick(&target);
        check(target.health==hp,"no damage tick on application");leveltime+=34;P_WeakpointTick(&target);check(target.health==hp,"no early partial-second tick");
        for(int i=1;i<=b.ticks;++i){leveltime=10+i*35;P_WeakpointTick(&target);check(target.health==hp-i*b.damage,"one exact periodic damage tick including configured final tic");P_WeakpointTick(&target);check(target.health==hp-i*b.damage,"same-tic repeated status update cannot duplicate damage");}
        check(wp_test_last_source==&source&&wp_test_rng_calls==1,"periodic damage retains owner and cannot recursively ignite");
        leveltime+=35;P_WeakpointTick(&target);check(target.health==hp-b.ticks*b.damage&&!P_CombatDotKind(&target),"one uninterrupted burn has bounded total damage");
    }
    for(int type:{MT_POSSESSED,MT_SHOTGUY,MT_CHAINGUY,MT_TROOP,MT_WOLFSS,MT_HEAD}){
        dotFixture(type);P_CombatConfirmed(&target,&source,3,1,-1);bool valid=type==MT_POSSESSED||type==MT_SHOTGUY||type==MT_CHAINGUY;
        check((P_CombatDotKind(&target)!=0)==valid,"Imp ignition whitelist excludes other monsters");
    }
    dotFixture();mobj_t imp{};imp.type=MT_TROOP;check(!P_CombatDamageKind(&imp,&imp),"Imp melee never has fire provenance");
    mobj_t missile{};missile.type=MT_TROOPSHOT;missile.flags=MF_MISSILE;check(P_CombatDamageKind(&missile,&imp)==3,"actual Imp projectile has distinct provenance");
    dotFixture(MT_HEAD);P_CombatConfirmed(&target,&source,1,1,-1);check(P_CombatDotKind(&target)==1,"Mancubus can ignite Cacodemon");
    dotFixture(MT_CYBORG);P_CombatConfirmed(&target,&source,2,1,-1);check(P_CombatDotKind(&target)==2,"player plasma supports ordinary shootable sprite bosses");
    dotFixture();P_CombatConfirmed(&target,&source,1,0,-1);check(!P_CombatDotKind(&target)&&!wp_test_rng_calls,"zero damage cannot ignite or roll");
    P_CombatConfirmed(&target,&source,1,1,-1);leveltime=30;P_CombatConfirmed(&target,&source,1,1,-1);int hp=target.health;
    leveltime=45;P_WeakpointTick(&target);check(target.health==hp-5,"same-element refresh retains phase and only one stream");
    P_CombatConfirmed(&target,&source,2,1,-1);hp=target.health;P_WeakpointTick(&target);check(target.health==hp,"replacement has no immediate tick");
    leveltime=80;P_WeakpointTick(&target);check(target.health==hp-4&&P_CombatDotKind(&target)==2,"element replacement resets one next-tick phase");
    std::vector<unsigned char> saved(P_WeakpointSave(nullptr));P_WeakpointSave(saved.data());P_WeakpointClear();P_WeakpointLoad(saved.data(),saved.size());hp=target.health;leveltime+=35;P_WeakpointTick(&target);
    check(target.health==hp-4&&wp_test_last_source==&source,"v6 restores periodic phase and indexed ownership");
    P_CombatConfigureDot(0);hp=target.health;leveltime+=35;P_WeakpointTick(&target);check(target.health==hp&&!P_CombatDotKind(&target),"disable immediately cancels damaging status");
    dotFixture();P_CombatConfirmed(&target,&source,3,1,-1);
    for(int i=1;i<=5;++i){leveltime=10+i*35;P_WeakpointTick(&target);}leveltime=220;
    std::vector<unsigned char> boundary(P_WeakpointSave(nullptr));P_WeakpointSave(boundary.data());P_WeakpointClear();P_WeakpointLoad(boundary.data(),boundary.size());hp=target.health;P_WeakpointTick(&target);
    check(target.health==hp-3,"save on exact expiry restores the pending final tick");
    std::vector<unsigned char> oldFive(boundary.begin(),boundary.begin()+204);oldFive[0]=5;uint32_t oldSize=212;
    for(int i=0;i<4;++i)oldFive.push_back((unsigned char)(oldSize>>(i*8)));for(char c:{'W','P','0','1'})oldFive.push_back(c);
    check(P_WeakpointLoad(oldFive.data(),oldFive.size())==212&&!P_CombatDotKind(&target),"v5 prefix loads with no invented damaging status");
    dotFixture();P_CombatConfirmed(&target,&source,2,1,-1);static int minimalKind=0;minimalKind=0;P_CombatVisual=+[](mobj_t*,int kind){minimalKind=kind;};P_WeakpointTick(&target);check(minimalKind==5,"FX-off damaging plasma still requests bounded minimal blue feedback");P_CombatVisual=nullptr;
    dotFixture(MT_HEAD);P_CombatConfigure(1,0);target.health=3;P_CombatConfirmed(&target,&source,2,1,-1);leveltime+=35;P_WeakpointTick(&target);
    check(P_CombatCharred(&target)&&P_SessionStats()->streak==1&&P_SessionStats()->dealt==3,"plasma DoT kill chars Cacodemon and credits only actual HP");
    P_WeakpointForget(&target);target.health=100;check(!P_CombatCharred(&target),"Cacodemon resurrection forget clears charring");
    dotFixture();P_CombatConfirmed(&target,&source,1,1,-1);P_WeakpointForget(&source);leveltime+=35;P_WeakpointTick(&target);check(wp_test_last_source==&source,"removed player owner safely falls back to its player slot");
    dotFixture();P_CombatConfirmed(&target,&imp,1,1,-1);P_WeakpointForget(&imp);leveltime+=35;P_WeakpointTick(&target);check(!wp_test_last_source,"removed monster owner leaves no stale pointer");
    dotFixture();imp.health=10;P_CombatConfirmed(&target,&imp,1,1,-1);imp.health=0;P_WeakpointDied(&imp);leveltime+=35;P_WeakpointTick(&target);check(wp_test_last_source==&imp,"dead but retained monster owner remains valid for attribution until removal");
    for(int mode=0;mode<3;++mode){dotFixture();if(mode==0)netgame=true;else if(mode==1)demoplayback=true;else demorecording=true;P_CombatConfirmed(&target,&source,2,1,-1);check(!P_CombatDotKind(&target)&&!wp_test_rng_calls,"unsupported modes bypass ignition and RNG");}
    reset();P_HudConfigure(1);combat_notice_t messages[3];
    for(int i=0;i<20;++i)P_HudNotify(HUD_CUSTOM,"ONE EVENT",0xFFFFD75A,1,88,5);
    check(P_HudNotices(messages)==1&&messages[0].color==0xFFFFD75A,"same event coalesces with typed color");
    P_HudNotify(HUD_CUSTOM,"TWO",0xFFB78AFF,2,88,6);P_HudNotify(HUD_CUSTOM,"THREE",0xFFB78AFF,2,88,7);P_HudNotify(HUD_CUSTOM,"LOW",0,0,88,8);
    check(P_HudNotices(messages)==3,"queue bounded at three and rejects lower priority overflow");
    for(int i=0;i<88;++i)P_SessionTick(1);check(!P_HudNotices(messages),"messages expire by simulation time");
    P_HudConfigure(0);P_HudNotify(HUD_CUSTOM,"OFF",0,3,88,9);check(!P_HudNotices(messages),"disabled messages neither queue nor affect combat");
    reset(MT_BRUISER);P_HudConfigure(1);P_WeakpointConfigureExtras(1,0);P_WeakpointBeginShot(&source,wp_pistol);auto e=event(WP_BULLET,WP_HEAD,5);P_WeakpointApply(&e);P_WeakpointEndShot();
    check(P_HudNotices(messages)==1&&messages[0].kind==HUD_CRITICAL_BLEED&&messages[0].color==0xFFFFD75A,"confirmed critical tier entry emits yellow message");
    for(int i=0;i<10;++i){P_WeakpointBeginShot(&source,wp_pistol);e=event(WP_BULLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();}check(P_HudNotices(messages)<=3,"ongoing bleeding cannot flood notification queue");
    reset(MT_HEAD);P_HudConfigure(1);P_WeakpointConfigureEyes(1);int oldEye=p_wp_balance.eyeChance;p_wp_balance.eyeChance=100;P_WeakpointBeginShot(&source,wp_pistol);e=event(WP_BULLET,WP_EYE,1);e.impact.eyeContact=1;P_WeakpointApply(&e);P_WeakpointEndShot();
    check(P_HudNotices(messages)==1&&messages[0].kind==HUD_EYE&&messages[0].color==0xFFB78AFF,"successful eye activation publishes purple event");p_wp_balance.eyeChance=oldEye;
    reset(MT_BRUISER);P_HudConfigure(1);P_WeakpointConfigureExtras(0,1);int oldFrenzy=p_wp_balance.frenzyChance;p_wp_balance.frenzyChance=100;
    mobj_t victim{};victim.type=MT_TROOP;victim.health=100;victim.flags=MF_COUNTKILL|MF_SHOOTABLE;link(&victim);
    P_WeakpointBeginShot(&source,wp_supershotgun);e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();
    check(P_HudNotices(messages)==1&&messages[0].kind==HUD_FRENZY,"actual infighting activation publishes frenzy event");
    P_WeakpointBeginShot(&source,wp_supershotgun);e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();check(P_HudNotices(messages)==1,"refreshing active frenzy cannot spam activation messages");p_wp_balance.frenzyChance=oldFrenzy;
    reset();P_SessionMap(1,2);check(P_SessionStats()->attempts==1,"first map entry is attempt one");
    for(int i=0;i<35;++i)P_SessionTick(1);check(P_SessionStats()->attemptTics==35&&P_SessionStats()->runtimeTics==35,"session/attempt clocks use simulation tics");
    target.flags|=MF_COUNTKILL;target.health=-50;P_SessionDamage(&target,&source,7,100);check(P_SessionStats()->dealt==7&&P_SessionStats()->streak==1,"overkill excluded from session damage and killstreak");
    target.player=&player;target.health=-1;P_SessionDamage(&target,&source,3,20);check(P_SessionStats()->deaths==1&&!P_SessionStats()->streak&&P_SessionStats()->taken==3,"actual death increments once and clears streak");
    P_SessionDamage(&target,&source,0,20);check(P_SessionStats()->deaths==1,"already-dead target cannot increment death again");
    P_SessionTick(0);check(P_SessionStats()->attemptTics==35&&P_SessionStats()->runtimeTics==36,"attempt freezes after death while active world runtime continues");
    mobj_t late{};late.flags=MF_COUNTKILL;late.health=-1;P_SessionDamage(&late,&source,3,20);check(!P_SessionStats()->streak&&P_SessionStats()->dealt==10,"posthumous player damage counts without reviving a dead attempt killstreak");
    P_SessionMap(1,2);check(P_SessionStats()->attempts==2&&P_SessionStats()->deaths==1&&!P_SessionStats()->attemptTics&&P_SessionStats()->dealt==10,"checkpoint reload keeps history and starts one attempt");
    P_SessionMap(1,3);check(P_SessionStats()->attempts==1&&!P_SessionStats()->deaths&&P_SessionStats()->taken==3,"new map visit resets map history, not session totals");
    P_SessionNewVisit();P_SessionMap(1,3);check(P_SessionStats()->attempts==1,"new game starts fresh map visit");
    P_CombatConfigureDot(0);P_HudConfigure(0);
}

