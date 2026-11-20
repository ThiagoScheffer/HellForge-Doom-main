/* Runtime fixture for stock actors, the vanilla explosion and real saves. */
extern "C" void A_Explode(mobj_t *);
static std::vector<unsigned char> acidState() {
    std::vector<unsigned char> data(P_AcidSave(nullptr));
    if(!data.empty())check(P_AcidSave(data.data())==data.size(),"AC01 sizing matches bytes written");
    return data;
}

static void acidIntegration() {
    auto &player=players[consoleplayer];
    auto *source=player.mo;
    check(Settings{}.corrosiveBarrels==0&&settings.corrosiveBarrels==1,
          "acid defaults off and loads from graphics configuration");
    auto spawn=[](int x,int y,mobjtype_t type){return P_SpawnMobj(x*FRACUNIT,y*FRACUNIT,ONFLOORZ,type);};
    auto *barrel=spawn(190,128,MT_BARREL);
    auto *victim=spawn(190,210,MT_HEAD);
    P_AcidConfigure(0);
    P_DamageMobj(barrel,nullptr,source,1);
    check(P_AcidPoolCount()==0&&acidState().empty(),"disabled feature omits AC01");
    const int before=victim->health;
    barrel->target=source;
    A_Explode(barrel);
    const int classic=before-victim->health;
    check(classic>0&&P_AcidPoolCount()==0,"classic barrel blasts without acid");
    P_RemoveMobj(barrel);P_RemoveMobj(victim);
    G_SaveGame(5,(char*)"No corrosive state");G_DoSaveGame();
    P_AcidConfigure(1);
    G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
    check(P_AcidPoolCount()==0,"load save without AC01");
    source=player.mo;
    check(P_AcidEnabled()!=0,"acid remains configured after classic load");

    barrel=spawn(190,128,MT_BARREL);
    victim=spawn(190,210,MT_HEAD);
    P_DamageMobj(barrel,nullptr,source,2);
    p_acid_pool_view_t leak{};
    check(P_AcidPoolCount()==1&&P_AcidPoolGet(0,&leak)&&leak.hasOwner,"non-lethal damage starts one owned leak");
    const auto firstRadius=leak.radius;
    P_DamageMobj(barrel,nullptr,source,5);
    check(P_AcidPoolGet(0,&leak)&&leak.radius>firstRadius,"further damage increases leakage");
    leveltime+=35-leveltime%35;
    const auto oldRadius=leak.radius;
    P_AcidTick(barrel);
    check(P_AcidPoolGet(0,&leak)&&leak.radius>oldRadius&&leak.remaining==280,"live leak expands and refreshes");
    const auto leaking=acidState();
    G_SaveGame(5,(char*)"Active leak");G_DoSaveGame();
    auto badOwner=leaking;
    std::fill_n(badOwner.begin()+16+28,4,0xff);
    check(P_AcidLoad(badOwner.data(),badOwner.size())==badOwner.size()&&P_AcidPoolCount()==0,
          "invalid owner reference is discarded");
    G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
    check(acidState()==leaking&&P_AcidPoolGet(0,&leak)&&leak.hasOwner,"real save restores leak timing and owner");
    source=player.mo;
    barrel=nullptr;victim=nullptr;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
        if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        auto *m=(mobj_t*)t;
        if(m->type==MT_BARREL&&m->x==fx(190)&&m->y==fx(128))barrel=m;
        if(m->type==MT_HEAD&&m->x==fx(190)&&m->y==fx(210))victim=m;
    }
    check(barrel&&victim,"find actors restored in original thinker order");
    auto *flyer=spawn(220,128,MT_SKULL);
    flyer->z=flyer->floorz+fx(26);
    auto *ground=spawn(155,128,MT_TROOP);
    flyer->health=ground->health=240;
    P_DamageMobj(barrel,nullptr,source,100);
    check(barrel->health<=0&&barrel->target==source&&P_AcidPoolGet(0,&leak)&&!leak.hasOwner,
          "leak droplets stop on lethal damage and the stock death remembers the source");
    const int siege=victim->health;
    A_Explode(barrel);
    check(siege-victim->health==classic,"acid preserves stock explosion damage and radius");
    const int pools=P_AcidPoolCount();
    check(pools>=2&&pools<=4,"rupture creates central and reachable satellite pools");
    P_AcidBarrelExplode(barrel,source);
    check(P_AcidPoolCount()==pools,"duplicate rupture does not duplicate acid pools");
    const int flyingHealth=flyer->health,groundHealth=ground->health;
    check(flyingHealth>5&&groundHealth>5,"rupture fixture leaves live victims");
    const auto ruptured=acidState();
    G_SaveGame(5,(char*)"Immediately after rupture");G_DoSaveGame();
    leveltime+=16;
    P_AcidTick(flyer);P_AcidTick(ground);
    check(flyer->health==flyingHealth&&ground->health==groundHealth,"exposure waits for 17 tics");
    ++leveltime;
    P_AcidTick(flyer);P_AcidTick(ground);P_AcidTick(ground);
    check(flyer->health==flyingHealth-5&&ground->health==groundHealth-5,
          "rupture exposes airborne Lost Souls and overlapping pools apply one stream");
    P_RemoveMobj(barrel);
    check(P_AcidPoolCount()==pools,"removing barrel leaves its spill");
    const auto stable=acidState();
    auto corrupt=stable;corrupt[0]=0xff;
    check(P_AcidLoad(corrupt.data(),corrupt.size())==corrupt.size()&&P_AcidPoolCount()==0,
          "well-framed unknown AC01 version is skipped");
    check(P_AcidLoad(stable.data(),stable.size()-1)==0,"truncated AC01 is not read");
    G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
    check(acidState()==ruptured,"real save restores spill, exposures and timer bytes");
    auto *mo=player.mo;
    check(P_TeleportMove(mo,fx(190),fx(92)),"place player next to spill");
    mo->z=mo->floorz;
    mo->health=player.health=100;player.armorpoints=0;
    player.powers[pw_ironfeet]=100;
    P_AcidTick(mo);leveltime+=17;P_AcidTick(mo);
    check(mo->health==100,"radiation suit prevents pool exposure");
    player.powers[pw_ironfeet]=0;
    P_AcidTick(mo);leveltime+=17;P_AcidTick(mo);
    check(mo->health==95,"unprotected player takes 5 HP");
    check(P_TeleportMove(mo,fx(400),fx(-160)),"move player out of spill");
    mo->z=mo->floorz;
    leveltime+=17;P_AcidTick(mo);
    check(mo->health==90,"residual exposure follows a player out of the pool");
    leveltime+=351;P_AcidTick(mo);
    check(mo->health==90&&P_AcidPoolCount()==0,"exposure and pools expire");
    player.armorpoints=30;player.armortype=1;
    P_DamageMobjAcid(mo,nullptr,5);
    check(mo->health==86&&player.armorpoints==29,"acid follows normal green armor rules");
    player.cheats|=CF_GODMODE;
    P_DamageMobjAcid(mo,nullptr,5);
    check(mo->health==86,"god mode protects against acid");
    player.cheats&=~CF_GODMODE;
    player.powers[pw_invulnerability]=100;
    P_DamageMobjAcid(mo,nullptr,5);
    check(mo->health==86,"invulnerability protects against acid");
    player.powers[pw_invulnerability]=0;
    auto *doomed=spawn(320,190,MT_TROOP);
    doomed->health=5;
    P_DamageMobjAcid(doomed,mo,5);
    check(doomed->health<=0&&P_CombatVisualKind(doomed)==0,"acid death does not create fire corpse treatment");
    P_AcidConfigure(0);
    check(P_AcidLoad(ruptured.data(),ruptured.size())==ruptured.size()&&P_AcidPoolCount()==0,
          "disabled load consumes AC01 without activating it");
    P_AcidConfigure(1);
    P_AcidLoad(ruptured.data(),ruptured.size());
    check(P_AcidPoolCount()>0,"restore before actual level transition");
    G_InitNew(gameskill,gameepisode,gamemap);
    check(P_AcidPoolCount()==0,"level transition clears all pools");
    for(int i=0;i<15;++i)
        P_AcidBarrelExplode(spawn(170+i*14,180,MT_BARREL),nullptr);
    check(P_AcidPoolCount()==48,"multiple simultaneous ruptures share the 48-pool cap");
    netgame=true;check(P_AcidPoolCount()==0,"acid inactive in multiplayer");netgame=false;
    demoplayback=true;check(P_AcidPoolCount()==0,"acid inactive during demo playback");demoplayback=false;
    demorecording=true;check(P_AcidPoolCount()==0,"acid inactive during demo recording");demorecording=false;
    P_AcidConfigure(0);
    check(P_AcidPoolCount()==0&&acidState().empty(),"disabling during play clears pools and save state");
    std::printf("PASS %d corrosive barrel integration checks\n",checks);
}

static void acidVisualFrame() {
    static int began=-1;
    static mobj_t *barrel=nullptr;
    static std::string leakPath,rupturePath;
    auto *player=players[consoleplayer].mo;
    if(began<0) {
        if(levelSerial!=r_levelserial||floors.empty())return;
        check(I_Render3DIsAccelerated()&&P_AcidEnabled()&&!settings.blood&&!settings.bloodSpray,
              "acid renderer enabled with all blood effects off");
        check(P_TeleportMove(player,fx(190),fx(-12)),"acid camera placement");
        player->player->cheats|=CF_GODMODE; // Keep the capture free of the damage palette flash.
        player->angle=ANG90;player->z=player->floorz;
        player->player->viewz=player->z+41*FRACUNIT;
        barrel=P_SpawnMobj(fx(190),fx(100),ONFLOORZ,MT_BARREL);
        P_DamageMobj(barrel,nullptr,player,4);
        began=leveltime;
    }
    const int age=leveltime-began;
    if(age>=8&&leakPath.empty()) {
        leakPath=output+"/acid-leak.png";
        I_Render3DScreenshotTo(leakPath.c_str());
    }
    if(age==14&&barrel) {
        barrel->target=player;A_Explode(barrel);P_RemoveMobj(barrel);barrel=nullptr;
    }
    if(age>=21&&!barrel&&rupturePath.empty()) {
        rupturePath=output+"/acid-rupture.png";
        I_Render3DScreenshotTo(rupturePath.c_str());
    }
    if(age>=25) {
        for(const auto &path:{leakPath,rupturePath}) {
            FILE *file=std::fopen(path.c_str(),"rb");
            check(file!=nullptr,"acid GPU capture saved");
            std::fclose(file);
        }
        std::puts("PASS Vulkan acid leak and airborne rupture with blood disabled");
        I_Quit();
    }
}
