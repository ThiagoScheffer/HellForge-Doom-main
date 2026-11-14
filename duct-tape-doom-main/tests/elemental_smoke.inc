static void elementalIntegration(){
    auto *player=players[consoleplayer].mo;
    check(P_TeleportMove(player,0,fx(-32)),"elemental observer placement");player->angle=ANG90;player->z=player->floorz;player->player->viewz=player->z+41*FRACUNIT;
    settings.accelerated=settings.criticalParts=settings.enhancedWeakPoints=settings.elementalCombatFX=1;settings.voxelModels=settings.hitFlinch=settings.wounds=settings.blood=0;
    P_WeakpointConfigure(1,1,0);P_WeakpointConfigureExtras(0,0);P_CombatConfigure(1,0);P_WeakpointInitFeedingHooks();
    P_CriticalBegin=begin;P_CriticalThing=criticalThing;prepareCriticalSurface=criticalSurface;criticalVoxelModel=nullptr;
    // Exercise the two requested human types through actual frame art and vanilla damage.
    int heads=0,fallbacks=0;
    for(auto type:{MT_CHAINGUY,MT_WOLFSS}){
        auto *m=P_SpawnMobj(0,fx(128),ONFLOORZ,type);m->angle=ANG270;
        auto &info=mobjinfo[type];
        for(auto state:{info.spawnstate,info.seestate,info.missilestate,info.painstate})for(int rot=0;rot<8;++rot){
            P_SetMobjState(m,(statenum_t)state);m->health=10000;
            auto &sf=sprites[m->sprite].spriteframes[m->frame&FF_FRAMEMASK];for(int n=0;n<8;++n)patch(firstspritelump+sf.lump[n]);
            auto &im=patch(firstspritelump+sf.lump[rot]);bool flip=sf.flip[rot];doom_joints::Head head{};
            int hx=0,hy=-1;if(doom_joints::findHead(im.pixels.data(),im.width,im.height,im.left,im.top,head)){
                hx=(int)std::floor(head.middle+im.left);for(int y=0;y<im.height;++y)if(im.top-y-.5f>=head.neck&&solid(im,hx,y)){hy=y;break;}
            }
            if(hy<0){++fallbacks;continue;}
            for(auto weapon:{wp_pistol,wp_shotgun}){
                forgetWounds(m);woundView(m,player,im,flip,0,128,0,(float)im.top,(float)(im.top-im.height),1,0);
                int hp=m->health;P_WeakpointBeginShot(player,weapon);shoot(player,flip?im.width-im.left-hx-.5f:hx+.5f-im.left,128,im.top-hy-.5f);P_WeakpointEndShot();
                check(hp-m->health==10,"SS/chaingunner bullet and pellet head damage is exactly two times");++heads;
            }
        }
        P_RemoveMobj(m);forgetWounds(m);
    }
    check(heads>100,"requested human frames/rotations/mirroring remain covered");
    // Shared sparks already support both mechanical types: verify region gates unchanged.
    settings.monsterMetalSparks=1;P_WeakpointImpact=combatImpact;
    for(auto type:{MT_BABY,MT_SPIDER}){
        auto *m=P_SpawnMobj(0,fx(128),ONFLOORZ,type);m->health=10000;
        wp_damage_event_t e{};e.target=m;e.source=player;e.kind=WP_BULLET;e.impact.confidence=100;e.baseDamage=e.finalDamage=5;e.eventId=1000+type;e.impact.region=WP_LEGS;
        int before=combatFeedbackStats.metalParticles;combatImpact(&e);check(combatFeedbackStats.metalParticles>before,"both mechanics retain confirmed leg sparks");
        e.impact.region=WP_BRAIN;before=combatFeedbackStats.metalParticles;combatImpact(&e);check(combatFeedbackStats.metalParticles==before,"brain contact cannot emit leg sparks");P_RemoveMobj(m);
    }
    P_WeakpointImpact=nullptr;
    auto *m=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_TROOP);auto *fat=P_SpawnMobj(fx(256),0,ONFLOORZ,MT_FATSO);
    auto *fire=P_SpawnMobj(0,fx(112),fx(32),MT_FATSHOT);fire->target=fat;
    m->health=5;P_WeakpointProjectileHit(fire,m,20);check(P_CombatCharred(m)&&P_CombatVisualKind(m)==1,"real Mancubus projectile fatal contact creates fire corpse");
    P_RemoveMobj(fire);P_RemoveMobj(fat);
    // Actual save body + trailer, preserving corpse identity rather than replacing it.
    G_SaveGame(5,(char*)"Elemental v5");G_DoSaveGame();G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();m=nullptr;
    for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*a=(mobj_t*)t;if(a->type==MT_TROOP&&a->health<=0)m=a;}
    check(m&&P_CombatCharred(m),"actual native save/load preserves charred corpse");
    P_WeakpointForget(m);m->health=60;check(!P_CombatCharred(m),"native resurrection cleanup clears charred state");P_RemoveMobj(m);
    player=players[consoleplayer].mo;
    auto *plasma=P_SpawnMobj(0,fx(112),fx(32),MT_PLASMA);plasma->target=player;P_WeakpointProjectileSpawn(plasma,player,WP_PLASMA_DIRECT);
    m=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_POSSESSED);m->health=1;P_WeakpointProjectileHit(plasma,m,5);
    check(P_CombatCharred(m)&&P_CombatVisualKind(m)==2,"real player plasma fatal contact keeps distinct blue provenance");P_RemoveMobj(plasma);P_RemoveMobj(m);
    // Actual local donor query and transfer; target attack/resurrection state guards.
    settings.advancedMonsterReactions=settings.monsterBleeding=1;P_CombatConfigure(1,1);P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
    auto *vile=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_VILE);vile->health=400;vile->tics=-1;
    auto *donor=P_SpawnMobj(fx(128),fx(128),ONFLOORZ,MT_TROOP);donor->tics=-1;
    P_WeakpointBeginShot(player,wp_pistol);wp_damage_event_t e{};e.target=vile;e.source=e.inflictor=player;e.kind=WP_BULLET;e.baseDamage=1;P_WeakpointApply(&e);P_WeakpointEndShot();
    check(P_CombatDonor(vile,0)==donor,"native bounded BLOCKMAP query selects local living donor");
    P_SetMobjState(vile,(statenum_t)vile->info->spawnstate);P_WeakpointTick(vile);
    for(int i=0;i<100&&!P_CombatVisualKind(donor);++i)P_CombatConfirmed(donor,player,1,1,-1);
    check(P_CombatVisualKind(donor)==1,"native save fixture contains active burning");
    G_SaveGame(5,(char*)"Active elemental");G_DoSaveGame();G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();vile=donor=nullptr;player=players[consoleplayer].mo;
    for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*a=(mobj_t*)t;if(a->type==MT_VILE)vile=a;if(a->type==MT_TROOP&&a->health>0)donor=a;}
    check(vile&&donor&&P_CombatVisualKind(donor)==1,"native load restores active burning alongside channel timers");
    int hp=vile->health,dh=donor->health;leveltime+=35;P_WeakpointTick(vile);
    check(dh-donor->health==12&&vile->health==hp+10,"native siphon transfers 12 HP and retains ordinary two-HP bleed tick");
    P_SetMobjState(vile,S_VILE_HEAL1);check(!P_CombatCompatible(vile),"resurrection state excludes siphoning");
    P_WeakpointTick(vile);leveltime+=35;hp=vile->health;dh=donor->health;P_WeakpointTick(vile);check(donor->health==dh&&vile->health<=hp,"resurrection interruption cancels transfer");
    // Corpse fallback is finite, excludes protected death actions, and reserves resurrection reach.
    P_SetMobjState(vile,(statenum_t)vile->info->spawnstate);P_WeakpointForget(vile);vile->health=400;
    donor->health=1;P_DamageMobj(donor,player,player,10);
    statenum_t final=(statenum_t)donor->info->deathstate;
    for(int i=0;i<32&&states[final].tics!=-1;++i)final=states[final].nextstate;
    P_SetMobjState(donor,final);donor->z=donor->floorz;
    check(P_CombatDonor(vile,1)==donor,"completed distant ordinary corpse is a valid finite fallback");
    P_WeakpointBeginShot(player,wp_pistol);e={};e.target=vile;e.source=e.inflictor=player;e.kind=WP_BULLET;e.baseDamage=1;P_WeakpointApply(&e);P_WeakpointEndShot();P_SetMobjState(vile,(statenum_t)vile->info->spawnstate);
    vile->state=&states[vile->info->spawnstate];vile->tics=-1;P_WeakpointTick(vile);hp=vile->health;leveltime+=35;P_WeakpointTick(vile);
    check(donor->thinker.function.acv==(actionf_v)(-1)&&vile->health==hp+10,"corpse removal heals once through supported lifecycle, retaining bleed damage");
    P_RemoveMobj(vile);

    // God protection precedes confirmed fire and fatal feedback.
    fire=P_SpawnMobj(0,fx(-16),fx(32),MT_FATSHOT);player->player->cheats|=CF_GODMODE;
    hp=player->health;P_DamageMobj(player,fire,fire,10);check(player->health==hp&&!P_CombatVisualKind(player),"ignored player fire damage produces no ignition");
    player->player->cheats=0;player->player->armorpoints=0;player->health=player->player->health=5;P_DamageMobj(player,fire,fire,20);
    check(player->health<=0&&P_CombatVisualKind(player)==1,"actual fatal fire selects player death feedback");
    std::printf("PASS %d focused elemental native checks; %d human head rays; %d conservative frame fallbacks\n",checks,heads,fallbacks);
}
