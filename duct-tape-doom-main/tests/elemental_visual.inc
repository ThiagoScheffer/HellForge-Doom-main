static void elementalVisualFrame(){
    static int started=-1;static mobj_t *vile=nullptr,*donor=nullptr;auto *player=players[consoleplayer].mo;
    if(started<0){
        settings.accelerated=settings.elementalCombatFX=settings.advancedMonsterReactions=settings.monsterBleeding=1;
        settings.hitFlinch=settings.voxelModels=settings.monsterFeeding=settings.headshotFrenzy=settings.crosshair=0;applySettings();P_WeakpointInitFeedingHooks();p_wp_balance.bleedChance=100;
        check(P_TeleportMove(player,0,fx(-192)),"elemental capture camera placement");player->angle=ANG90;player->player->cheats|=CF_GODMODE;
        auto *fat=P_SpawnMobj(fx(280),fx(220),ONFLOORZ,MT_FATSO);auto *fire=P_SpawnMobj(fx(256),fx(220),fx(32),MT_FATSHOT);fire->target=fat;
        auto *corpse=P_SpawnMobj(fx(-64),fx(64),ONFLOORZ,MT_POSSESSED);corpse->health=1;P_WeakpointProjectileHit(fire,corpse,20);check(P_CombatCharred(corpse),"GPU scene fire corpse status");
        auto *plasma=P_SpawnMobj(fx(256),fx(220),fx(32),MT_PLASMA);plasma->target=player;P_WeakpointProjectileSpawn(plasma,player,WP_PLASMA_DIRECT);
        corpse=P_SpawnMobj(fx(64),fx(64),ONFLOORZ,MT_TROOP);corpse->health=1;P_WeakpointProjectileHit(plasma,corpse,20);check(P_CombatCharred(corpse),"GPU scene plasma corpse status");
        donor=P_SpawnMobj(fx(-90),fx(200),ONFLOORZ,MT_TROOP);donor->health=5000;
        for(int i=0;i<100&&!P_CombatVisualKind(donor);++i)P_DamageMobj(donor,fire,fat,1);
        donor->tics=-1;check(P_CombatVisualKind(donor)==1,"GPU scene burning actor status");
        vile=P_SpawnMobj(fx(90),fx(200),ONFLOORZ,MT_VILE);vile->health=400;
        P_WeakpointBeginShot(player,wp_pistol);wp_damage_event_t e{};e.target=vile;e.source=e.inflictor=player;e.kind=WP_BULLET;e.baseDamage=1;P_WeakpointApply(&e);P_WeakpointEndShot();
        vile->state=&states[vile->info->spawnstate];vile->tics=-1;
        P_RemoveMobj(fire);P_RemoveMobj(plasma);P_RemoveMobj(fat);started=leveltime;
    }
    if(leveltime-started==56){check(vile->health>400,"GPU scene live siphoning heals without fire attack");visualCapture=output+"/elemental-combat.png";I_Render3DScreenshotTo(visualCapture.c_str());}
    if(leveltime-started>=60){std::puts("PASS one elemental renderer scene");I_Quit();}
}
