extern "C" int I_TestRevenantSouls();

static void revenantSoulIntegration() {
    auto *player=players[consoleplayer].mo;
    check(P_TeleportMove(player,0,fx(-120))!=0,"place player outside resurrection fixture");
    Settings defaults;
    check(!defaults.revenantSoulRelease,"Revenant Soul Release defaults to Off");
    readOption(defaults,"revenant_soul_release",1);
    FILE *profile=std::tmpfile();
    check(profile!=nullptr,"open temporary settings profile");
    writeOptions(profile,defaults);
    std::rewind(profile);
    Settings restored;
    char line[128],key[64];double value;
    while(std::fgets(line,sizeof(line),profile))
        if(std::sscanf(line,"%63s %lf",key,&value)==2)readOption(restored,key,value);
    std::fclose(profile);
    check(restored.revenantSoulRelease==1,"setting persists through config round-trip");
    settings.accelerated=1;
    R_RevenantDeath=revenantDeath;
    auto kill=[&](mobjtype_t type,float x,float y) {
        auto *thing=P_SpawnMobj(fx(x),fx(y),ONFLOORZ,type);
        P_DamageMobj(thing,nullptr,player,thing->health);
        return thing;
    };
    settings.revenantSoulRelease=0;
    auto *disabled=kill(MT_UNDEAD,-92,122);
    check(I_TestRevenantSouls()==0&&(disabled->flags&MF_CORPSE),"Off leaves the ordinary corpse");
    P_RemoveMobj(disabled);
    settings.revenantSoulRelease=1;
    auto *other=kill(MT_TROOP,-92,122);
    check(I_TestRevenantSouls()==0,"other monsters emit no soul");
    P_RemoveMobj(other);
    auto *revenant=kill(MT_UNDEAD,0,111);
    check(I_TestRevenantSouls()==1&&(revenant->flags&MF_CORPSE),"one soul and the original corpse");
    P_DamageMobj(revenant,nullptr,player,1000);
    check(I_TestRevenantSouls()==1,"repeat damage cannot emit a duplicate soul");
    auto state=revenant->info->deathstate;
    while(states[state].nextstate!=S_NULL)state=states[state].nextstate;
    P_SetMobjState(revenant,(statenum_t)state);revenant->tics=-1;
    auto *vile=P_SpawnMobj(0,fx(64),ONFLOORZ,MT_VILE);
    vile->movedir=2;
    A_VileChase(vile);
    check(revenant->health==revenant->info->spawnhealth,"native resurrection succeeds");
    check(I_TestRevenantSouls()==1,"resurrection does not release a soul");
    P_DamageMobj(revenant,nullptr,player,revenant->health);
    check(I_TestRevenantSouls()==2,"resurrected Revenant releases a second soul");
    for(int i=0;i<30;++i)kill(MT_UNDEAD,float(-200+i*12),220);
    check(I_TestRevenantSouls()==24,"simultaneous deaths are capped at 24 souls");
    const int tic=leveltime;
    paused=true;P_Ticker();paused=false;
    check(leveltime==tic&&I_TestRevenantSouls()==24,"paused simulation holds soul time");
    leveltime+=2*TICRATE;
    check(I_TestRevenantSouls()==0,"all souls expire");
    kill(MT_UNDEAD,-92,122);
    check(I_TestRevenantSouls()==1,"expired slots can be reused");
    G_InitNew(sk_medium,1,1);
    check(I_TestRevenantSouls()==0,"level change clears souls");
    std::puts("PASS Revenant Soul Release integration");
}

static void revenantSoulVisualFrame() {
    static mobj_t *actor=nullptr;
    static int born=-1;
    if(levelSerial!=r_levelserial||floors.empty())return;
    auto capture=[](const char *name) {
        visualCapture=output+"/"+name+".png";
        I_Render3DScreenshotTo(visualCapture.c_str());
    };
    auto place=[](float x,float y,angle_t angle) {
        auto *player=players[consoleplayer].mo;
        check(P_TeleportMove(player,fx(x),fx(y))!=0,"place soul camera");
        player->angle=angle;player->z=player->floorz;
        player->player->viewz=player->z+fx(41);
    };
    if(!actor) {
        place(96,-48,ANG90);
        actor=P_SpawnMobj(fx(114),fx(128),ONFLOORZ,MT_UNDEAD);
        actor->angle=ANG270;actor->tics=-1;
        born=leveltime;
        P_DamageMobj(actor,nullptr,players[consoleplayer].mo,actor->health);
        check(I_TestRevenantSouls()==1,"live configuration enables death hook");
    }
    switch(leveltime-born) {
    case 7:capture("emergence");break;
    case 12:capture("resist");break;
    case 17:capture("tug");break;
    case 34:capture("escape");break;
    case 38:place(-96,128,0);break;
    case 39:capture("wall-active");break;
    case 42:place(96,-48,ANG90);break;
    case 49:capture("dissolve");break;
    case 62:capture("expired");break;
    case 64:place(-96,128,0);break;
    case 65:capture("wall-expired");break;
    case 68:
        for(const char *name:{"emergence","resist","tug","escape","dissolve","expired","wall-active","wall-expired"}) {
            FILE *file=std::fopen((output+"/"+name+".png").c_str(),"rb");
            check(file!=nullptr,"Windows soul capture saved");std::fclose(file);
        }
        std::puts("PASS Windows Revenant Soul Release sequence");
        I_Quit();
    }
}
