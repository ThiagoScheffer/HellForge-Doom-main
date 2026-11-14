static int staggerCalls=0;
static mobj_t *testDonor=nullptr;
static void elementalRules(){
    auto balance=p_wp_balance;
    for(int type:{MT_KNIGHT,MT_BRUISER,MT_VILE}){
        reset(type);P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
        P_WeakpointBeginShot(&source,wp_pistol);auto e=event(WP_BULLET,WP_BODY,5);P_WeakpointApply(&e);P_WeakpointEndShot();
        check(P_WeakpointBleedTier(&target)==1,"Knight/Baron/Vile share progressive bleeding");
    }
    reset(MT_BRUISER);P_WeakpointConfigureExtras(1,0);P_CombatConfigure(0,1);p_wp_balance.bleedChance=100;
    auto hit=[](){P_WeakpointBeginShot(&source,wp_pistol);auto e=event(WP_BULLET,WP_BODY,5);P_WeakpointApply(&e);P_WeakpointEndShot();};hit();hit();
    check(P_CombatRage(&target)&&P_WeakpointWalkSpeed(&target,1000)==880,"Baron speed is 110 percent of the existing bleeding speed");
    hit();check(P_WeakpointWalkSpeed(&target,1000)==880,"Baron speed never stacks with reapplication");
    leveltime+=281;check(!P_CombatRage(&target),"Baron reaction expires after eight seconds");
    reset(MT_FATSO);P_WeakpointConfigureExtras(1,0);P_CombatConfigure(0,1);p_wp_balance.bleedChance=100;
    P_CombatCompatible=+[](mobj_t*)->int{return 1;};P_CombatStagger=+[](mobj_t*){++staggerCalls;};staggerCalls=0;hit();hit();
    check(staggerCalls==1,"Mancubus tier-two meaningful hit staggers");
    for(int i=0;i<20;++i)hit();check(staggerCalls==1,"repeated hits cannot bypass six-second stagger cooldown");
    leveltime+=210;hit();check(staggerCalls==2,"stagger can recur only after cooldown");
    P_CombatCompatible=+[](mobj_t*)->int{return 0;};leveltime+=210;hit();check(staggerCalls==2,"incompatible volley state cannot be interrupted by new stagger");
    reset(MT_TROOP);P_CombatConfigure(1,0);mobj_t fire{};fire.type=MT_FATSHOT;fire.flags=MF_MISSILE;
    check(P_CombatDamageKind(&fire,nullptr)==1&&P_CombatDamageKind(&source,&fire)==0,"fire provenance belongs to actual projectile, never attacker type");
    mobj_t plasma{};plasma.type=MT_PLASMA;P_WeakpointProjectileSpawn(&plasma,&source,WP_PLASMA_DIRECT);
    check(P_CombatDamageKind(&plasma,&source)==2,"plasma uses recorded player projectile provenance");
    plasma.type=MT_BFG;check(P_CombatDamageKind(&plasma,&source)==0,"BFG is not a plasma rifle effect");
    for(int i=0;i<64;++i)P_CombatConfirmed(&target,&source,1,5,-1);
    check(P_CombatVisualKind(&target)==1&&wp_test_rng_calls==0,"ignition uses no gameplay random calls");
    mobj_t arch{},flame{};P_WeakpointForget(&target);
    P_CombatFireBegin(&arch,&flame);check(P_CombatDamageKind(&arch,&arch)==1&&P_CombatDamageKind(&flame,&arch)==1,"Arch-vile direct/radius provenance shares one real attack context");
    P_CombatConfirmed(&target,&arch,1,5,-1);int initial=P_CombatVisualKind(&target);
    for(int i=0;i<100;++i)P_CombatConfirmed(&target,&arch,1,5,-1);
    check(P_CombatVisualKind(&target)==initial,"Arch-vile direct and radius damage cannot repeat ignition chance");P_CombatFireEnd();
    check(!P_CombatDamageKind(&flame,&arch),"visual-only Arch-vile fire cannot ignite");
    target.health=0;P_CombatConfirmed(&target,&source,0,5,1);check(P_CombatCharred(&target),"death while burning leaves cosmetic charred corpse");
    std::vector<unsigned char> data(P_WeakpointSave(nullptr));P_WeakpointSave(data.data());check(data[0]==5,"new expansion uses save trailer v5");
    P_WeakpointClear();P_WeakpointLoad(data.data(),data.size());check(P_CombatCharred(&target),"charred corpse survives indexed save load");
    std::vector<unsigned char> legacy(data.begin(),data.begin()+168);legacy[0]=4;uint32_t legacySize=176;
    for(int i=0;i<4;++i)legacy.push_back((unsigned char)(legacySize>>(i*8)));for(char c:{'W','P','0','1'})legacy.push_back(c);
    check(P_WeakpointLoad(legacy.data(),legacy.size())==176&&!P_CombatCharred(&target),"legacy v4 loads without inventing elemental state");
    P_WeakpointForget(&target);target.health=100;check(!P_CombatCharred(&target)&&!P_CombatVisualKind(&target),"resurrection forget clears charred and burning state");
    target.health=0;P_CombatConfirmed(&target,&source,2,100,0);check(P_CombatCharred(&target)&&P_CombatVisualKind(&target)==2,"plasma fatal effect is blue and charred");
    P_CombatConfigure(0,0);check(!P_CombatCharred(&target)&&!P_CombatVisualKind(&target),"disabling elemental FX clears statuses immediately");
    for(int type:{MT_KNIGHT,MT_BRUISER,MT_HEAD,MT_SPIDER,MT_WOLFSS}){
        reset(type);P_CombatConfigure(1,0);target.health=0;P_CombatConfirmed(&target,&source,1,100,0);check(!P_CombatCharred(&target),"noneligible monster cannot become charred");
    }
    reset(MT_TROOP);P_CombatConfigure(1,0);target.player=&player;target.health=0;P_CombatConfirmed(&target,&source,1,100,0);
    check(P_CombatVisualKind(&target)==1&&!P_CombatCharred(&target),"fatal player fire selects feedback without changing corpse art");
    reset(MT_VILE);target.health=800;P_WeakpointConfigureExtras(1,0);P_CombatConfigure(0,1);p_wp_balance.bleedChance=100;p_wp_balance.bleedDamage=0;hit();
    int start=target.health;mobj_t donor{};donor.type=MT_TROOP;donor.health=100;testDonor=&donor;
    P_CombatCompatible=+[](mobj_t*)->int{return 1;};P_CombatDonor=+[](mobj_t*,int corpse)->mobj_t*{return corpse?nullptr:testDonor;};P_WeakpointTick(&target);
    for(int i=1;i<=6;++i){leveltime=10+i*35;P_WeakpointTick(&target);}
    check(donor.health==28&&target.health==start+72,"siphon transfers six actual twelve-HP ticks, maximum 72");
    int after=target.health;leveltime+=35;P_WeakpointTick(&target);check(target.health==after&&donor.health==28,"siphon cooldown prevents additional transfer");
    reset(MT_VILE);P_WeakpointConfigureExtras(1,0);P_CombatConfigure(0,1);target.health=900;p_wp_balance.bleedDamage=0;hit();
    donor.health=3;testDonor=&donor;P_CombatCompatible=+[](mobj_t*)->int{return 1;};P_CombatDonor=+[](mobj_t*,int corpse)->mobj_t*{return !corpse&&testDonor&&testDonor->health>0?testDonor:nullptr;};P_WeakpointTick(&target);after=target.health;leveltime+=35;P_WeakpointTick(&target);
    check(target.health==after+3&&donor.health==0,"siphon cannot heal more health than available donor HP");
    testDonor=nullptr;leveltime+=35;after=target.health;P_WeakpointTick(&target);check(target.health==after,"missing donor produces no free healing");
    std::vector<unsigned char> save(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());check(P_WeakpointRuntimeCount()>0,"active reaction timer save loads safely");
    P_CombatConfigure(0,0);p_wp_balance=balance;P_CombatCompatible=nullptr;P_CombatStagger=nullptr;P_CombatDonor=nullptr;
}
