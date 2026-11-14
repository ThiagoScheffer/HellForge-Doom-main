#include <chrono>
static void elemental2VisualFrame(){
    static int start=-1;static bool measured=false;auto *player=players[consoleplayer].mo;
    auto messages=[](){P_HudNotify(HUD_CRITICAL_BLEED,"CRITICAL BLEEDING - BARON OF HELL",0xFFFFD75A,1,88,1);P_HudNotify(HUD_FRENZY,"FRENZY TRIGGERED - MANCUBUS",0xFFB78AFF,2,88,2);P_HudNotify(HUD_EYE_RAGE,"EYE RAGE - SPIDER MASTERMIND",0xFFB78AFF,2,88,3);};
    if(start<0){
        settings.accelerated=settings.elementalDot=settings.elementalCombatFX=settings.combatMessages=settings.speedrunHUD=1;
        settings.voxelModels=settings.hitFlinch=settings.monsterBleeding=settings.advancedMonsterReactions=settings.headshotFrenzy=settings.crosshair=settings.mapLightCache=0;
        settings.speedrunAnchor=0;settings.hudStyle=0;applySettings();
        check(P_TeleportMove(player,0,fx(-192)),"dense elemental observer placement");player->angle=ANG90;player->player->cheats|=CF_GODMODE;
        auto *fat=P_SpawnMobj(fx(300),fx(450),ONFLOORZ,MT_FATSO);
        for(int i=0;i<36;++i){
            auto *m=P_SpawnMobj(fx(-160+(i%6)*64),fx(128+(i/6)*64),ONFLOORZ,i%2?MT_TROOP:MT_POSSESSED);m->health=5000;
            auto *missile=P_SpawnMobj(m->x,m->y,m->z+32*FRACUNIT,i%2?MT_PLASMA:MT_FATSHOT);missile->target=i%2?player:fat;
            if(i%2)P_WeakpointProjectileSpawn(missile,player,WP_PLASMA_DIRECT);
            for(int n=0;n<120&&!P_CombatDotKind(m);++n)P_DamageMobj(m,missile,missile->target,1);
            check(P_CombatDotKind(m)==(i%2?2:1),"dense scene has 36 active orange/blue burns");m->tics=-1;P_RemoveMobj(missile);
        }
        auto *caco=P_SpawnMobj(0,fx(64),ONFLOORZ,MT_HEAD);caco->health=1;
        auto *plasma=P_SpawnMobj(0,fx(64),fx(32),MT_PLASMA);plasma->target=player;P_WeakpointProjectileSpawn(plasma,player,WP_PLASMA_DIRECT);P_WeakpointProjectileHit(plasma,caco,5);
        check(P_CombatCharred(caco),"dense scene includes charred Cacodemon");P_RemoveMobj(plasma);P_RemoveMobj(fat);start=leveltime;messages();
    }
    int elapsed=leveltime-start;
    if(elapsed==40||elapsed==80)messages();
    if(elapsed==62&&!measured){
        std::vector<unsigned> pixels(SCREENWIDTH*SCREENHEIGHT);bool pending=worldPending;worldPending=true;
        std::vector<double> off,on;std::ofstream samples(output+"/hud-widget-cost.csv");samples<<"pair,enabled,microseconds_per_call\n";
        for(int pair=0;pair<9;++pair)for(int phase=0;phase<2;++phase){bool enabled=((pair+phase)&1)!=0;settings.speedrunHUD=settings.combatMessages=enabled;
            auto begin=std::chrono::steady_clock::now();for(int n=0;n<100;++n){combatWidgets(pixels);combatWidgets(pixels,true);}auto end=std::chrono::steady_clock::now();
            double cost=std::chrono::duration<double,std::micro>(end-begin).count()/100;samples<<pair<<","<<enabled<<","<<cost<<"\n";(enabled?on:off).push_back(cost);
        }
        std::sort(off.begin(),off.end());std::sort(on.begin(),on.end());
        std::printf("HUD_WIDGET_CPU_US disabled_median %.3f enabled_median %.3f disabled_p95 %.3f enabled_p95 %.3f samples 9 calls_per_sample 100\n",off[4],on[4],off[8],on[8]);
        settings.speedrunHUD=settings.combatMessages=1;worldPending=pending;measured=true;
    }
    if(elapsed==60||elapsed==80||elapsed==100){settings.hudStyle=(elapsed-60)/20;applySettings();}
    if(elapsed==64||elapsed==84||elapsed==104){int style=(elapsed-64)/20;visualCapture=output+"/elemental2-hud-"+std::to_string(style)+".png";I_Render3DScreenshotTo(visualCapture.c_str());}
    if(elapsed>=108){std::puts("PASS dense elemental 2 scene and three HUD layout captures");I_Quit();}
}
