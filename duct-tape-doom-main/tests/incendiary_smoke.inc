extern "C" int prndindex;

static std::vector<unsigned char> oilState() {
    std::vector<unsigned char> state(P_IncendiarySave(nullptr));
    if(!state.empty())check(P_IncendiarySave(state.data())==state.size(),"OI01 reports exact size");
    return state;
}

static mobj_t *burningBarrel(int x,int y) {
    return P_SpawnMobj(fx(x),fx(y),ONFLOORZ,MT_MISC77);
}

static void incendiaryIntegration() {
    auto &player=players[consoleplayer];
    auto *source=player.mo;
    check(Settings{}.incendiaryBarrels==0&&settings.incendiaryBarrels==1&&P_IncendiaryEnabled(),
          "incendiary defaults Off and loads an explicit On setting");
    P_CombatConfigure(0,0);
    P_CombatConfigureDot(0);
    player.cheats|=CF_GODMODE;
    P_IncendiaryConfigure(0);
    auto *original=burningBarrel(190,128);
    P_DamageMobj(original,nullptr,source,2000);
    check(original->health==1000&&original->flags==MF_SOLID&&original->state==&states[S_BBAR1]&&oilState().empty(),
          "Off preserves the stock FCAN decoration under otherwise lethal damage");
    G_SaveGame(5,(char*)"Old save without oil");G_DoSaveGame();
    P_IncendiaryConfigure(1);
    check(original->health==30&&(original->flags&MF_SHOOTABLE),"live enable arms an existing barrel");
    G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
    source=player.mo;
    player.cheats|=CF_GODMODE;
    mobj_t *restored=nullptr;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)
        if(t->function.acp1==(actionf_p1)P_MobjThinker&&((mobj_t*)t)->type==MT_MISC77)restored=(mobj_t*)t;
    check(restored&&restored->health==30&&P_IncendiaryPoolCount()==0,"old saves restore actors with the selected setting");
    P_RemoveMobj(restored);

    for(int switches=0;switches<4;++switches) {
        P_AcidConfigure(switches&1);P_IncendiaryConfigure(switches&2);
        const auto kills=player.killcount;
        auto *acid=P_SpawnMobj(fx(370),fx(170),ONFLOORZ,MT_BARREL);
        auto *oil=burningBarrel(190,128);
        acid->target=source;
        A_Explode(acid);
        P_DamageMobj(oil,nullptr,source,100);
        P_IncendiaryEndTic();
        check((P_AcidPoolCount()>0)==bool(switches&1),"acid selection is independent in each combination");
        check((P_IncendiaryPoolCount()>0)==bool(switches&2),"oil selection is independent in each combination");
        check(player.killcount==kills,"no decoration kill credit");
        if(switches==3) {
            const auto ac=acidState(),oi=oilState();
            G_SaveGame(5,(char*)"Coexisting acid and oil");G_DoSaveGame();
            G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
            check(acidState()==ac&&oilState()==oi,"AC01 and OI01 round-trip independently in the same save");
            P_IncendiaryEndTic();
            check(oilState()==oi,"restored spent decoration does not repeat the blast");
            source=player.mo;
        } else {
            P_RemoveMobj(acid);P_RemoveMobj(oil);
        }
        P_AcidClear();P_IncendiaryClear();
    }
    P_AcidConfigure(0);
    P_IncendiaryConfigure(0);P_IncendiaryConfigure(1);
    player.cheats|=CF_GODMODE;
    auto *oil=burningBarrel(195,100);
    P_DamageMobj(oil,nullptr,source,5);
    p_oil_pool_view_t leak{};
    check(oil->health==25&&P_IncendiaryPoolCount()==1&&P_IncendiaryPoolGet(0,&leak)&&leak.kind==OIL_LEAK,
          "nonlethal damage leaves one inert leak and the original barrel");
    const fixed_t firstLeakRadius=leak.radius;
    P_DamageMobj(oil,nullptr,source,5);
    check(P_IncendiaryPoolCount()==1,"repeated nonlethal checks do not stack leaks");
    leveltime+=5;
    P_DamageMobj(oil,source,source,1);
    check(P_IncendiaryPoolCount()==1&&P_IncendiaryPoolGet(0,&leak)&&leak.radius>firstLeakRadius&&leak.age==0,
          "a later weapon impact refreshes the spray and widens the same floor spill");
    const auto liveLeak=oilState();
    auto legacyLeak=liveLeak;
    for(int i=0;i<4;++i)legacyLeak[16+8+i]=(unsigned char)((11*FRACUNIT)>>(i*8));
    check(P_IncendiaryLoad(legacyLeak.data(),legacyLeak.size())==legacyLeak.size()&&
          P_IncendiaryPoolGet(0,&leak)&&leak.radius==11*FRACUNIT,"original OI01 leak radius still loads");
    P_IncendiaryLoad(liveLeak.data(),liveLeak.size());
    auto *rocket=P_SpawnMobj(oil->x,oil->y,oil->z+fx(8),MT_ROCKET);
    rocket->target=source;
    const int killCount=player.killcount;
    auto *nearby=P_SpawnMobj(fx(236),fx(100),ONFLOORZ,MT_TROOP);
    nearby->health=1000;
    P_RadiusAttack(rocket,source,40);
    check(oil->health==0&&oil->target==source&&!(oil->flags&MF_SHOOTABLE),
          "a real rocket radius queues a single attributed rupture");
    const int before=nearby->health;
    P_DamageMobj(oil,nullptr,source,999);
    P_IncendiaryEndTic();
    const int pools=P_IncendiaryPoolCount();
    check(pools>=2&&pools<=4&&nearby->health<before&&player.killcount==killCount,
          "one rupture damages nearby monsters and makes at most four pools");
    check(oil->state==&states[S_BEXP5]&&oil->frame==states[S_BEXP].frame,
          "rupture shows the first explosion sprite on the safe terminal state");
    oil->tics-=15;P_IncendiaryTick(oil);
    check(oil->frame==states[S_BEXP4].frame&&oil->state==&states[S_BEXP5],
          "explosion sprite advances without executing classic barrel actions");
    P_IncendiaryEndTic();
    check(P_IncendiaryPoolCount()==pools,"extra damage and flush cannot repeat an explosion");
    P_RemoveMobj(rocket);P_RemoveMobj(nearby);

    auto *rider=P_SpawnMobj(fx(195),fx(106),ONFLOORZ,MT_TROOP);
    auto *movingFloor=rider->subsector->sector;
    const auto savedFloor=movingFloor->floorheight;
    movingFloor->floorheight+=fx(12);
    P_ChangeSector(movingFloor,false);
    const int riderHealth=rider->health;
    P_IncendiaryTick(rider);leveltime+=20;P_IncendiaryTick(rider);
    check(rider->z==movingFloor->floorheight&&rider->health==riderHealth-3,"oil contact follows a moving floor");
    movingFloor->floorheight=savedFloor;
    P_ChangeSector(movingFloor,false);
    P_RemoveMobj(rider);

    check(P_TeleportMove(source,fx(195),fx(100)),"player can occupy the spent barrel footprint");
    source->z=source->floorz;source->health=player.health=100;
    player.cheats&=~CF_GODMODE;
    player.powers[pw_ironfeet]=100;
    player.armorpoints=0;player.armortype=0;
    auto *flyer=P_SpawnMobj(fx(201),fx(107),ONFLOORZ,MT_SKULL);
    flyer->z=flyer->floorz+fx(32);
    const int flightHealth=flyer->health;
    P_IncendiaryTick(source);P_IncendiaryTick(flyer);
    leveltime+=19;
    P_IncendiaryTick(source);
    check(source->health==100,"oil waits twenty tics before dealing damage");
    ++leveltime;
    P_IncendiaryTick(source);P_IncendiaryTick(source);P_IncendiaryTick(flyer);
    check(source->health==97&&flyer->health==flightHealth,"overlapping oil applies one hit, ignores high fliers and pierces radiation suits");
    player.armorpoints=20;player.armortype=1;
    leveltime+=20;P_IncendiaryTick(source);
    check(source->health==95&&player.armorpoints==19,"oil uses native armor handling");
    player.powers[pw_invulnerability]=100;
    leveltime+=20;P_IncendiaryTick(source);
    check(source->health==95,"oil obeys invulnerability");
    player.powers[pw_invulnerability]=0;player.cheats|=CF_GODMODE;
    leveltime+=20;P_IncendiaryTick(source);
    check(source->health==95,"oil obeys god mode");
    player.cheats&=~CF_GODMODE;
    check(P_TeleportMove(source,fx(390),fx(-155)),"player can escape oil");
    source->z=source->floorz;
    leveltime+=20;P_IncendiaryTick(source);
    check(source->health==93,"a residual tick follows the player out");
    const auto saved=oilState();
    G_SaveGame(5,(char*)"Active oil and exposure");G_DoSaveGame();
    auto corrupt=saved;corrupt[0]=0xff;
    check(P_IncendiaryLoad(corrupt.data(),corrupt.size())==corrupt.size()&&P_IncendiaryPoolCount()==0,
          "unknown well-framed oil version is ignored");
    check(P_IncendiaryLoad(saved.data(),saved.size()-1)==0,"a truncated oil trailer is ignored");
    G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
    source=player.mo;
    check(oilState()==saved,"real save restores active oil, exposure timers and sources");
    leveltime+=340;P_IncendiaryTick(source);
    check(P_IncendiaryPoolCount()==0&&source->health==93,"fire and residual exposure expire");

    player.cheats|=CF_GODMODE;
    auto *first=burningBarrel(130,85);
    auto *second=burningBarrel(192,85);
    auto *bfg=P_SpawnMobj(fx(132),fx(38),fx(8),MT_BFG);
    bfg->target=source;
    check(!P_TryMove(bfg,fx(130),fx(82))&&first->health==0,"physical BFG projectile queues a rupture");
    P_RemoveMobj(bfg);
    P_IncendiaryEndTic();
    check(second->health==0&&second->target==source,"chain preserves ultimate source and queues its next wave");
    G_SaveGame(5,(char*)"Queued chain");G_DoSaveGame();
    const auto firstWave=oilState();
    G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
    check(oilState()==firstWave,"save preserves completed wave");
    P_IncendiaryEndTic();
    check(P_IncendiaryPoolCount()>pools,"a queued barrel resumes its wave once after reload");
    source=player.mo;
    P_WeakpointTargetAllowed=nullptr;
    P_CombatConfigure(1,0);
    auto *victim=P_SpawnMobj(fx(190),fx(87),ONFLOORZ,MT_TROOP);
    victim->health=3;
    const int credit=player.killcount;
    P_IncendiaryTick(victim);leveltime+=20;P_IncendiaryTick(victim);
    check(victim->health<=0&&player.killcount==credit+1&&P_CombatCharred(victim),
          "saved oil retains attacker credit and fire-specific death visuals");
    auto *alreadyBurning=P_SpawnMobj(fx(183),fx(88),ONFLOORZ,MT_TROOP);
    P_CombatConfigureDot(1);
    for(int n=0;n<128&&!P_CombatDotKind(alreadyBurning);++n)
        P_CombatConfirmed(alreadyBurning,source,1,1,-1);
    check(P_CombatDotKind(alreadyBurning)==1,"fixture has an active native fire status");
    const int fireHealth=alreadyBurning->health;
    P_IncendiaryTick(alreadyBurning);leveltime+=20;P_IncendiaryTick(alreadyBurning);
    check(alreadyBurning->health==fireHealth,"oil yields to an active elemental fire DoT");
    P_CombatConfigureDot(0);P_CombatConfigure(0,0);

    P_IncendiaryConfigure(0);
    check(P_IncendiaryPoolCount()==0&&oilState().empty(),"disable clears fire and exposure immediately");
    auto *unlit=burningBarrel(200,300);
    check(!(unlit->flags&MF_SHOOTABLE),"new barrels stay inert after disable");
    P_IncendiaryConfigure(1);
    for(int gate=0;gate<3;++gate) {
        if(gate==0)netgame=true;
        if(gate==1)demoplayback=true;
        if(gate==2)demorecording=true;
        P_IncendiarySync();
        const int rng=prndindex;
        P_DamageMobj(unlit,nullptr,source,1000);
        P_IncendiaryEndTic();
        check(unlit->health==1000&&!(unlit->flags&MF_SHOOTABLE)&&prndindex==rng&&P_IncendiaryPoolCount()==0,
              "unsupported session restores stock flags and consumes no gameplay RNG");
        netgame=demoplayback=demorecording=false;
        P_IncendiarySync();
    }
    mobjinfo[MT_MISC77].spawnhealth=700;
    auto *modded=burningBarrel(240,300);
    check(modded->health==700&&modded->flags==MF_SOLID,"custom DeHackEd-style definition is excluded");
    mobjinfo[MT_MISC77].spawnhealth=1000;
    P_RemoveMobj(modded);

    const int split=R_PointInSubsector(fx(-24),fx(128))->sector-sectors;
    const auto ceiling=sectors[split].ceilingheight;
    auto *behind=P_SpawnMobj(fx(-12),fx(128),ONFLOORZ,MT_TROOP);
    behind->health=1000;
    sectors[split].ceilingheight=sectors[split].floorheight;
    auto *edge=burningBarrel(16,128);edge->angle=ANG180;
    P_DamageMobj(edge,nullptr,source,30);P_IncendiaryEndTic();
    for(int i=0;i<P_IncendiaryPoolCount();++i) {
        p_oil_pool_view_t pool{};
        check(P_IncendiaryPoolGet(i,&pool)&&pool.sector!=split,"oil does not spread across a closed partition");
    }
    P_IncendiaryTick(behind);leveltime+=20;P_IncendiaryTick(behind);
    check(behind->health==1000,"closed partition blocks both the rupture and oil contact");
    sectors[split].ceilingheight=ceiling;
    P_IncendiaryClear();
    for(int i=0;i<60;++i) {
        auto *b=burningBarrel(100+(i%15)*22,225+(i/15)*26);
        P_DamageMobj(b,nullptr,source,30);
    }
    for(int wave=0;wave<6;++wave)P_IncendiaryEndTic();
    check(P_IncendiaryPoolCount()==32,"simultaneous ruptures respect the active pool cap");
    G_InitNew(gameskill,gameepisode,gamemap);
    check(P_IncendiaryPoolCount()==0,"map transition clears oil and references");
    const auto demoPath=output+"/incendiary-map23.lmp";
    const unsigned char demo[]={223,2,1,23,0,0,0,1,0,1,0,0,0,128};
    {std::ofstream out(demoPath,std::ios::binary);out.write((const char*)demo,sizeof(demo));}
    director_demo=1;
    G_DeferedPlayDemo((char*)demoPath.c_str());
    G_DoPlayDemo();
    int stockBarrels=0;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
        if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        const auto *actor=(mobj_t*)t;
        if(actor->type!=MT_MISC77)continue;
        ++stockBarrels;
        check(actor->flags==MF_SOLID&&actor->health==1000,"native demo load creates unmodified FCAN actors");
    }
    check(demoplayback&&stockBarrels>0&&!P_IncendiaryEnabled(),"MAP23 demo starts with the oil preference inactive");
    std::printf("PASS %d incendiary barrel runtime checks\n",checks);
}

static void incendiaryVisualFrame() {
    static int start=-1;
    static int lastAge=-1;
    static mobj_t *barrel=nullptr;
    static std::vector<std::string> captures;
    auto *player=players[consoleplayer].mo;
    if(start<0) {
        if(levelSerial!=r_levelserial||floors.empty())return;
        check(I_Render3DIsAccelerated()&&P_IncendiaryEnabled()&&!settings.blood&&!settings.elementalCombatFX&&!settings.elementalDot,
              "oil effects are available with blood and optional elemental effects off");
        check(P_TeleportMove(player,fx(192),fx(21)),"oil camera placement");
        player->player->cheats|=CF_GODMODE;
        player->angle=ANG90;player->z=player->floorz;
        player->player->viewz=player->z+41*FRACUNIT;
        barrel=burningBarrel(192,123);
        start=leveltime;
    }
    auto capture=[&](const char *name) {
        captures.push_back(output+"/"+name+".png");
        I_Render3DScreenshotTo(captures.back().c_str());
    };
    const int age=leveltime-start;
    if(age==lastAge)return;
    lastAge=age;
    switch(age) {
    case 3:capture("oil-intact");break;
    case 7:P_DamageMobj(barrel,nullptr,player,7);break;
    case 10:capture("oil-leak");break;
    case 16:P_DamageMobj(barrel,nullptr,player,60);P_IncendiaryEndTic();break;
    case 20:capture("oil-rupture");break;
    case 26:capture("oil-rupture-later");break;
    case 32:capture("oil-fireball");break;
    case 61:capture("oil-burning");break;
    case 302:capture("oil-fading");break;
    case 344:
        check(P_IncendiaryPoolCount()==0,"all oil extinguished by the final capture");
        capture("oil-expired");break;
    case 350:
        for(const auto &path:captures) {
            FILE *f=std::fopen(path.c_str(),"rb");
            check(f!=nullptr,"oil GPU screenshot saved");std::fclose(f);
        }
        check(captures.size()==8,"captured all oil lifecycle stages");
        std::puts("PASS Vulkan oil lifecycle with optional combat effects disabled");
        I_Quit();
    }
}

static void incendiaryLargeMap() {
    auto *player=players[consoleplayer].mo;
    player->player->cheats|=CF_GODMODE;
    std::vector<mobj_t*> actors;
    int barrels=0;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
        if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        auto *actor=(mobj_t*)t;
        actors.push_back(actor);
        if(actor->type==MT_MISC77&&(actor->flags&MF_SHOOTABLE)) {
            P_DamageMobj(actor,nullptr,player,30);
            ++barrels;
        }
    }
    P_IncendiaryEndTic();
    const int pools=P_IncendiaryPoolCount();
    check(actors.size()>2000&&barrels>0&&pools>0&&pools<=32,"large WAD has living thinkers and active oil");
    const auto start=SDL_GetTicksNS();
    for(int tic=0;tic<40;++tic) {
        ++leveltime;
        for(auto *actor:actors)
            if(actor->thinker.function.acp1==(actionf_p1)P_MobjThinker)P_IncendiaryTick(actor);
    }
    std::printf("PASS %zu active thinkers, %d barrels, %d pools, 40 oil updates in %.3f ms\n",
                actors.size(),barrels,pools,(SDL_GetTicksNS()-start)/1e6);
}
