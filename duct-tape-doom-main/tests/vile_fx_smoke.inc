extern "C" {
#include "m_random.h"
void A_VileAttack(mobj_t *);
void A_VileChase(mobj_t *);
int I_TestVileHurtBursts();
}

static int vileHurtCalls,vileImpactCalls;
static auto vileAttackTrace(bool withEffects) {
    auto *vile=P_SpawnMobj(fx(-96),fx(128),ONFLOORZ,MT_VILE);
    auto *victim=P_SpawnMobj(fx(48),fx(128),ONFLOORZ,MT_TROOP);
    victim->health=1000;
    vile->target=victim;
    leveltime=100;
    M_ClearRandom();
    R_VileImpact=withEffects?+[](fixed_t x,fixed_t y,fixed_t z) {
        ++vileImpactCalls;vileImpact(x,y,z);
    }:nullptr;
    vileImpactCalls=0;
    std::vector<std::array<int,8>> trace;
    P_SetMobjState(vile,S_VILE_ATK1);
    for(int tic=1;tic<=66;++tic) {
        ++leveltime;
        if(tic==25)check(P_TeleportMove(victim,fx(80),fx(156))!=0,"move live flame victim");
        P_MobjThinker(vile);
        const auto *fire=vile->tracer;
        if(fire&&tic>10&&tic<66)P_MobjThinker(vile->tracer);
        trace.push_back({int(vile->state-states),vile->tics,victim->health,victim->momz,
                         fire?fire->x:0,fire?fire->y:0,fire?fire->z:0,fire?fire->type:-1});
    }
    check(trace[8][7]==-1&&trace[9][7]==MT_FIRE,"fire spawns at the original windup phase");
    check(trace[35][4]!=trace[12][4]&&trace[35][5]!=trace[12][5],"fire tracks moving victim in both axes");
    check(trace[64][2]==1000&&trace[65][2]<980&&trace[65][3]>0,
          "direct and splash damage and lift begin on the original impact tic");
    check(vileImpactCalls==(withEffects?1:0),"exactly one render impact per completed attack");
    std::printf("Arch-vile impact at tic 66, %d HP loss, vertical momentum %d\n",
                1000-victim->health,victim->momz);
    P_RemoveMobj(vile->tracer);vile->tracer=nullptr;
    P_RemoveMobj(victim);P_RemoveMobj(vile);
    return trace;
}

static void vileIntegration() {
    auto *shooter=players[consoleplayer].mo;
    check(P_TeleportMove(shooter,0,fx(-32))!=0,"place shotgun fixture");
    shooter->angle=ANG90;
    shooter->z=shooter->floorz;
    shooter->player->viewz=shooter->z+fx(41);
    settings.accelerated=1;settings.fluidMonsters=1;
    P_WeakpointConfigure(0,0,0);
    const auto baseline=vileAttackTrace(false);
    const auto enabled=vileAttackTrace(true);
    check(baseline==enabled,"effects preserve each attack frame, fire position, damage and lift");
    auto *vile=P_SpawnMobj(0,fx(64),ONFLOORZ,MT_VILE);
    vileHurtCalls=0;vileImpactCalls=0;
    R_VileHurt=+[](const mobj_t *target) { ++vileHurtCalls;vileHurt(target); };
    check(vile->state==&states[vile->info->spawnstate],"Arch-vile starts in its idle state");
    A_VileAttack(vile);
    check(vileImpactCalls==0,"targetless cast emits no impact");
    P_DamageMobj(vile,nullptr,nullptr,0);
    check(vileHurtCalls==0,"zero damage emits no hurt flare");
    auto *player=shooter->player;
    player->readyweapon=wp_shotgun;player->ammo[am_shell]=10;
    shooter->angle=ANG270;
    A_FireShotgun(player,&player->psprites[ps_weapon]);
    check(vileHurtCalls==0,"missed shotgun discharge emits no hurt flare");
    shooter->angle=ANG90;
    for(int shot=0;shot<3;++shot) {
        const int health=vile->health,calls=vileHurtCalls;
        A_FireShotgun(player,&player->psprites[ps_weapon]);
        check(vile->health<health&&vile->health>0&&vileHurtCalls>calls,
              "native shotgun pellets reach confirmed surviving health loss");
    }
    check(I_TestVileHurtBursts()==1,"repeated shotgun hits share one bounded reaction");
    leveltime+=14;
    check(I_TestVileHurtBursts()==0,"hurt flare expires after 0.4 seconds");
    P_DamageMobj(vile,nullptr,shooter,vile->health-10);
    check(vile->health==10,"living low-health caster");
    auto *corpse=P_SpawnMobj(0,fx(111),ONFLOORZ,MT_TROOP);
    P_DamageMobj(corpse,nullptr,shooter,corpse->health);
    corpse->tics=-1;vile->movedir=2; // DI_NORTH.
    A_VileChase(vile);
    check(corpse->health==corpse->info->spawnhealth&&vile->state==&states[S_VILE_HEAL1],
          "native resurrection remains available at low health");
    const int calls=vileHurtCalls;
    P_DamageMobj(vile,nullptr,shooter,11);
    check(vile->health<=0&&vileHurtCalls==calls,"lethal damage emits no living hurt flare");
    P_RemoveMobj(vile);P_RemoveMobj(corpse);
    R_VileHurt=nullptr;R_VileImpact=nullptr;
    check(I_TestVileHurtBursts()==1,"removal leaves only the short world-space sparks");
    G_InitNew(sk_medium,1,1);
    check(I_TestVileHurtBursts()==0,"level load drops all old hurt effects");
    std::printf("PASS %d Arch-vile gameplay and event checks\n",checks);
}

static void vileVisualFrame() {
    static mobj_t *caster=nullptr,*bait=nullptr;
    static int start=-1,last=-1;
    if(levelSerial!=r_levelserial||floors.empty()||last==leveltime)return;
    last=leveltime;
    auto *player=players[consoleplayer].mo;
    auto capture=[](const char *name) {
        visualCapture=output+"/"+name+".png";
        I_Render3DScreenshotTo(visualCapture.c_str());
    };
    if(start<0) {
        check(P_TeleportMove(player,0,fx(-120))!=0,"place visual camera");
        player->angle=ANG90;player->z=player->floorz;
        player->player->viewz=player->z+fx(41);
        caster=P_SpawnMobj(fx(-40),fx(67),ONFLOORZ,MT_VILE);
        bait=P_SpawnMobj(fx(31),fx(118),ONFLOORZ,MT_TROOP);
        caster->angle=ANG270;caster->tics=-1;
        bait->tics=-1;bait->health=250;
        start=leveltime;
        capture("vile-idle");
    }
    const int tic=leveltime-start;
    if(tic==8) {
        P_DamageMobj(caster,nullptr,nullptr,500);
        caster->target=bait;
        P_SetMobjState(caster,S_VILE_PAIN);
        caster->tics=-1;
    }
    if(tic==10)capture("vile-hurt");
    if(tic==24)capture("vile-low-health");
    if(tic==32)P_SetMobjState(caster,S_VILE_ATK1);
    if(tic==37)capture("vile-windup");
    if(tic==53)capture("vile-target-fire");
    if(tic==54)check(P_TeleportMove(bait,fx(72),fx(130))!=0,"move the visible fire target");
    if(tic==61)capture("vile-tracking");
    if(tic==65)caster->target=nullptr;
    if(tic==67)capture("vile-no-target");
    if(tic==71)caster->target=bait;
    if(tic==94)capture("vile-late-fire");
    if(tic==99)capture("vile-impact");
    if(tic==110)P_SetMobjState(caster,S_VILE_ATK1);
    if(tic==126)P_DamageMobj(bait,nullptr,player,bait->health+1);
    if(tic==128)capture("vile-target-death");
    if(tic==132) {
        check(caster->tracer&&caster->tracer->type==MT_FIRE,"second fire is still active after target death");
        P_RemoveMobj(caster->tracer);caster->tracer=nullptr;
        caster->target=nullptr;P_RemoveMobj(bait);
        P_DamageMobj(caster,nullptr,player,caster->health+1);
    }
    if(tic==140)capture("vile-death");
    if(tic==146) {
        P_RemoveMobj(caster);
        capture("vile-removed");
    }
    if(tic==153) {
        for(const auto *name:{"vile-idle","vile-hurt","vile-low-health","vile-windup",
                              "vile-target-fire","vile-tracking","vile-late-fire","vile-impact",
                              "vile-no-target","vile-target-death","vile-death","vile-removed"}) {
            FILE *file=std::fopen((output+"/"+name+".png").c_str(),"rb");
            check(file!=nullptr,"Windows Arch-vile screenshot saved");std::fclose(file);
        }
        std::puts("PASS Windows Arch-vile visual sequence");
        I_Quit();
    }
}
