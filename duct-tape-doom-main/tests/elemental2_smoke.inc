extern "C" {void P_Ticker(void);void G_DoPlayDemo(void);void G_DeferedPlayDemo(char*);extern int director_demo;}
static void elemental2Integration(){
    auto *player=players[consoleplayer].mo;settings.accelerated=settings.elementalDot=settings.elementalCombatFX=1;settings.voxelModels=settings.hitFlinch=0;
    P_CombatConfigureDot(1);P_CombatConfigure(1,0);P_WeakpointConfigure(0,0,0);P_WeakpointConfigureExtras(0,0);
    player->player->cheats|=CF_GODMODE;P_WeakpointTargetAllowed=nullptr;
    auto ignite=[&](mobj_t *victim,mobj_t *owner,mobjtype_t type){
        for(int i=0;i<120&&!P_CombatDotKind(victim);++i){
            auto *missile=P_SpawnMobj(victim->x,victim->y,victim->z+32*FRACUNIT,type);missile->target=owner;
            if(type==MT_PLASMA)P_WeakpointProjectileSpawn(missile,owner,WP_PLASMA_DIRECT);
            P_WeakpointProjectileHit(missile,victim,1);P_RemoveMobj(missile);
        }
        check(P_CombatDotKind(victim)!=0,"real confirmed projectile contact can activate damaging burn");
    };
    auto *imp=P_SpawnMobj(fx(-200),0,ONFLOORZ,MT_TROOP);auto *fat=P_SpawnMobj(fx(200),0,ONFLOORZ,MT_FATSO);
    struct Row{mobjtype_t victim,projectile;int ticks,damage;};
    for(const auto &row:std::vector<Row>{{MT_POSSESSED,MT_TROOPSHOT,6,3},{MT_HEAD,MT_FATSHOT,4,5},{MT_UNDEAD,MT_PLASMA,5,4}}){
        auto *m=P_SpawnMobj(0,fx(128),ONFLOORZ,row.victim);m->health=1000;m->tics=-1;
        auto *owner=row.projectile==MT_PLASMA?player:row.projectile==MT_FATSHOT?fat:imp;ignite(m,owner,row.projectile);
        int start=leveltime,hp=m->health;auto dealt=P_SessionStats()->dealt;P_WeakpointTick(m);check(m->health==hp,"native burn has no immediate application damage");
        for(int i=1;i<=row.ticks;++i){leveltime=start+i*35;P_WeakpointTick(m);check(m->health==hp-row.damage*i,"native authoritative DoT has exact per-second damage and final tick");}
        check(P_SessionStats()->dealt-dealt==(row.projectile==MT_PLASMA?20:0),"native session damage counts player-owned periodic health loss once");
        P_RemoveMobj(m);
    }
    auto *m=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_HEAD);m->health=500;ignite(m,player,MT_PLASMA);
    int hp=m->health;leveltime+=17;auto attempts=P_SessionStats()->attempts;auto history=P_SessionStats()->dealt;
    G_SaveGame(5,(char*)"Elemental v6");G_DoSaveGame();G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();m=nullptr;player=players[consoleplayer].mo;
    for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*a=(mobj_t*)t;if(a->type==MT_HEAD)m=a;}
    check(m&&P_CombatDotKind(m)==2&&m->health==hp,"actual save/load restores active Cacodemon blue burn");
    check(P_SessionStats()->attempts==attempts+1&&P_SessionStats()->dealt==history,"native checkpoint load starts one attempt without rolling back session damage");
    leveltime+=17;P_WeakpointTick(m);check(m->health==hp,"restored next tick phase waits the remaining interval");
    ++leveltime;P_WeakpointTick(m);check(m->health==hp-4&&P_SessionStats()->dealt==history+4,"restored plasma source remains player-attributed");
    P_CombatConfigureDot(0);hp=m->health;leveltime+=35;P_WeakpointTick(m);check(m->health==hp,"native feature disable stops further burn damage");
    P_CombatConfigureDot(1);ignite(m,player,MT_PLASMA);m->health=3;auto streak=P_SessionStats()->streak;leveltime+=35;P_WeakpointTick(m);
    check(m->health<=0&&P_CombatCharred(m)&&P_SessionStats()->streak==streak+1,"native DoT kill retains Cacodemon corpse and kill credit");
    P_WeakpointForget(m);m->health=m->info->spawnhealth;check(!P_CombatCharred(m)&&!P_CombatDotKind(m),"normal resurrection cleanup clears all Cacodemon elemental state");P_RemoveMobj(m);
    player->player->cheats=0;player->player->powers[pw_invulnerability]=0;player->health=player->player->health=100;
    player->player->armortype=2;player->player->armorpoints=100;auto taken=P_SessionStats()->taken;
    P_DamageMobj(player,nullptr,nullptr,10);check(player->health==95&&P_SessionStats()->taken==taken+5,"native damage taken accounts for armor once");
    player->player->cheats=CF_GODMODE;taken=P_SessionStats()->taken;P_DamageMobj(player,nullptr,nullptr,10);check(P_SessionStats()->taken==taken,"god protection adds no damage statistic");
    auto clock=P_SessionStats()->runtimeTics;paused=true;P_Ticker();paused=false;menuactive=true;players[consoleplayer].viewz=41*FRACUNIT;P_Ticker();menuactive=false;
    check(P_SessionStats()->runtimeTics==clock,"native pause and stopped menus do not advance session time");
    player->player->cheats=0;player->player->armorpoints=0;player->health=player->player->health=5;
    auto deaths=P_SessionStats()->deaths,attemptTime=P_SessionStats()->attemptTics;
    P_DamageMobj(player,nullptr,nullptr,20);P_DamageMobj(player,nullptr,nullptr,20);
    check(P_SessionStats()->deaths==deaths+1,"native player death counts once despite repeated damage calls");
    P_Ticker();check(P_SessionStats()->attemptTics==attemptTime,"native attempt timer freezes through dead-player world ticks");
    P_CombatConfigureDot(0);
    auto beforeDemo=*P_SessionStats();std::string demoPath=output+"/session-demo.lmp";
    const unsigned char demo[]={223,2,1,4,0,0,0,1,0,1,0,0,0,128};
    {std::ofstream f(demoPath,std::ios::binary);f.write((const char*)demo,sizeof(demo));}
    director_demo=1;G_DeferedPlayDemo((char*)demoPath.c_str());G_DoPlayDemo();
    check(demoplayback&&P_SessionStats()->attempts==beforeDemo.attempts&&P_SessionStats()->runtimeTics==beforeDemo.runtimeTics,"supported native demo initialization cannot register a playable attempt or runtime");
    std::printf("PASS %d focused elemental 2 / native session checks\n",checks);
}
