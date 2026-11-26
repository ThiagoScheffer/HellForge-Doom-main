// Native and GPU fixture invoked by doom-critical-smoke --corpse-integration.
#include "../platform/corpse_decomposition.h"

static void corpseIntegrationFrame() {
    if(floors.empty()||levelSerial!=r_levelserial)return;
    static int phase=0,lastTic=-1;
    if(gametic==lastTic)return;
    lastTic=gametic;
    static mobj_t *actor=nullptr,*neighbor=nullptr;
    static CorpseLifetime first=0;
    static CorpseRestLocation rest;
    static std::size_t baseline=0;
    static int cardKey=0;
    static std::vector<byte> fresh;
    auto *player=players[consoleplayer].mo;
    auto match=[](const CorpseRestLocation &a,const CorpseRestLocation &b) {
        return a.x==b.x&&a.y==b.y&&a.floor==b.floor&&a.sector==b.sector;
    };
    auto screenshot=[](const char *name) {
        visualCapture=output+"/"+name+".png";
        I_Render3DScreenshotTo(visualCapture.c_str());
    };
    auto terminal=[](mobj_t *m) {
        P_SetMobjState(m,S_POSS_DIE5);
        m->flags|=MF_CORPSE;
        m->health=0;
        m->momx=m->momy=m->momz=0;
        m->z=m->floorz;
    };
    auto spawn=[] {return P_SpawnMobj(fx(40),fx(128),ONFLOORZ,MT_POSSESSED);};
    auto kill=[&] {
        P_DamageMobj(actor,player,player,actor->health+1);
        actor->momx=actor->momy=0;
    };
    auto settle=[&] {
        CorpseRestLocation location;
        bool groundedMoving=false;
        for(int n=0;n<175&&ragdollRestLocation(actor,location)==CorpseRestState::Moving;++n) {
            groundedMoving=groundedMoving||testRagdollLanded(actor);
            testUpdateCorpsePools();
            check(testCorpsePools(actor)==0,"moving or sliding corpse cannot have a pool");
            ++leveltime;
            watchBodies();
            const int advanced=testRagdollTics(actor);
            watchBodies();
            DeathView view{};
            ragdollView(*actor,view);
            ragdollView(*actor,view);
            check(testRagdollTics(actor)==advanced,"repeated watch and presentation are idempotent");
        }
        check(groundedMoving,"fixture includes a landed and moving body");
        check(ragdollRestLocation(actor,location)==CorpseRestState::Resting,"physics settles without a sprite pass");
        unsigned uploads=0;
        check(testRagdollDecay(actor,uploads)==0&&uploads==0,"off-screen advancement leaves textures untouched");
        return location;
    };
    if(phase==0) {
        settings.deathRagdoll=settings.deathBlast=settings.blood=settings.corpseDecomposition=1;
        settings.decorationAmbience=1;
        settings.decorationInsects=2;
        settings.corpseDecayDelay=2;
        R_GoreForget=corpseLifecycleEvent;
        testRagdollRestFixture();
        const auto &terminalState=states[S_POSS_DIE5];
        const int lump=firstspritelump+sprites[terminalState.sprite].spriteframes[terminalState.frame&FF_FRAMEMASK].lump[0];
        Image &source=lumpImage(lump,false);
        const auto beforeVariant=corpseDecayBytesUsed();
        int variantKey=-1-lump;
        check(&corpseDecayImage(lump,source,1,0,variantKey)!=&source&&
              corpseDecayBytesUsed()==beforeVariant+std::size_t(source.width)*source.height*6,
              "conventional variant consumes the shared budget");
        baseline=corpseDecayBytesUsed();
        const auto remaining=doom_decay::textureBudget-baseline;
        check(reserveCorpseDecayBytes(remaining),"exact budget reservation");
        check(corpseDecayBytesUsed()==doom_decay::textureBudget&&!reserveCorpseDecayBytes(1),"shared cap");
        releaseCorpseDecayBytes(remaining);
        check(corpseDecayBytesUsed()==baseline,"budget released");

        player->angle=ANG90;
        actor=spawn();
        actor->angle=ANG270;
        watchBodies();
        kill();
        testUpdateCorpsePools();
        check(testCorpsePools(actor)==0,"native death callback blocks a pre-registration pool");
        watchBodies();
        check(corpseDecayStage(actor)==0,"moving body is fresh");
        const int tics=testRagdollTics(actor);
        paused=true;
        ++leveltime;
        tickRagdoll(*actor);
        --leveltime;
        paused=false;
        check(tics==0&&testRagdollTics(actor)==tics,"paused simulation does not advance the ragdoll");
        rest=settle();
        check(std::hypot(rest.x-units(actor->x),rest.y-units(actor->y))>1,"body displaced from native actor");
        terminal(actor);
        neighbor=spawn();
        terminal(neighbor);
        corpseDecayReset();
        corpseDecayTick();
        testUpdateCorpsePools();
        testUpdateCorpsePools();
        first=corpseLifetime(actor);
        CorpseRestLocation pool;
        check(first&&testCorpsePools(actor,&pool)==1&&match(pool,rest),"one permanent pool at final supporting position");
        check(testCorpsePools(neighbor)==1,"neighbor at the same native position has its own pool");
        Uniforms camera{};
        camera.eye[0]=rest.x;camera.eye[1]=rest.y;
        decorationAmbienceReset();
        decorationAmbienceBegin(camera);
        CorpseRestLocation emitter;
        check(testCorpseEmitter(actor,emitter)&&match(emitter,rest),"scavenger emitter follows resting position and floor");
        screenshot("corpse-rest");
        phase=1;
        return;
    }
    unsigned uploads=0;
    const int stage=testRagdollDecay(actor,uploads);
    switch(phase++) {
    case 1: {
        DeathView death{};
        check(deathView(*actor,0,death)&&death.ragdoll,"fresh ragdoll still selected");
        RagdollCard card{};
        const float eye[]={units(player->x),units(player->y),units(players[consoleplayer].viewz)};
        check(ragdollCard(*death.ragdoll,0,0,eye,nullptr,card,0,0),"fresh final card");
        cardKey=card.key;
        fresh=images.at(cardKey).pixels;
        check(testSetCorpseAge(actor,120*TICRATE),"age record starts after settling");
        screenshot("corpse-stage1");
        break;
    }
    case 2:
        check(stage==1&&uploads==1&&testCorpsePools(actor)==1,"stage 1 uploaded once and retains its pool");
        check(images.at(cardKey).pixels!=fresh&&corpseDecayBytesUsed()==baseline+fresh.size(),"immutable snapshot charged");
        break;
    case 3:
        check(stage==1&&uploads==1,"unchanged appearance avoids another upload");
        check(testSetCorpseAge(actor,165*TICRATE),"set stage 2");
        screenshot("corpse-stage2");
        break;
    case 4:
        check(stage==2&&uploads==2&&testCorpsePools(actor)==0&&testCorpsePools(neighbor)==1,"stage 2 removes only its pool");
        check(testSetCorpseAge(actor,240*TICRATE),"set stage 3");
        screenshot("corpse-stage3");
        break;
    case 5:
        check(stage==3&&uploads==3&&testCorpsePools(actor)==0,"stage 3 stays dry");
        settings.corpseDecomposition=0;
        screenshot("corpse-restored");
        break;
    case 6: {
        check(stage==0&&uploads==4&&images.at(cardKey).pixels==fresh,"disabling decay restores exact pixels on same card");
        check(corpseDecayBytesUsed()==baseline&&testCorpsePools(actor)==0,"restoration releases the snapshot and preserves the marker");
        const auto oldNeighbor=corpseLifetime(neighbor);
        corpseLifecycleEvent(neighbor,0);
        testUpdateCorpsePools();
        check(corpseLifetime(neighbor)!=oldNeighbor&&testCorpsePools(neighbor)==1&&testCorpsePools(nullptr)==1,
              "retained historical deposit has no owner");
        dryCorpseBlood(neighbor,oldNeighbor);
        check(testCorpsePools(neighbor)==1,"old pool lifetime cannot delete a new pool");

        // Follow the native resurrection callback ordering, then kill before
        // the body watch can observe a living actor.
        P_SetMobjState(actor,static_cast<statenum_t>(actor->info->raisestate));
        actor->height=actor->info->height;
        actor->flags=actor->info->flags;
        R_GoreForget(actor,0);
        check(!corpseLifetime(actor)&&images.count(cardKey)==0,"resurrection immediately forgets the decor and card");
        actor->health=actor->info->spawnhealth;
        kill();
        watchBodies();
        settle();
        terminal(actor);
        testUpdateCorpsePools();
        check(corpseLifetime(actor)&&corpseLifetime(actor)!=first&&testCorpsePools(actor)==1,"rapid second death gets a new pool");
        settings.corpseDecomposition=1;
        corpseDecayReset();
        corpseDecayTick();
        check(testSetCorpseAge(actor,240*TICRATE),"off-screen second death can age before first bake");
        DeathView death{};
        RagdollCard card{};
        const float eye[]={units(player->x),units(player->y),units(players[consoleplayer].viewz)};
        check(deathView(*actor,0,death)&&death.ragdoll&&
              ragdollCard(*death.ragdoll,0,0,eye,nullptr,card,corpseDecayStage(actor),0),
              "first bake uses the current stage");
        check(testRagdollDecay(actor,uploads)==3&&uploads==1&&corpseDecayBytesUsed()>baseline,
              "first bake can jump directly to stage 3");
        P_HazardRecordDeath(actor,HAZARD_ACID);
        P_RemoveMobj(actor);
        check(!corpseLifetime(actor)&&testCorpsePools(nullptr)==1,"acid removal destroys the owned pool before detaching");
        check(corpseDecayBytesUsed()==baseline&&images.count(card.key)==0,"removal releases an aged card and its snapshot");
        P_RemoveMobj(neighbor);
        check(!corpseLifetime(neighbor)&&testCorpsePools(nullptr)==2,"ordinary removal keeps its historical deposit");

        bloodShutdown();
        settings.corpseDecomposition=0;
        auto *evicted=spawn();
        terminal(evicted);
        settings.blood=0;
        testUpdateCorpsePools();
        check(!corpseLifetime(evicted),"blood disabled cannot create a pool");
        settings.blood=1;
        testUpdateCorpsePools();
        check(testCorpsePools(evicted)==1,"blood re-enabled allows an eligible pool");
        std::vector<mobj_t *> replacements;
        for(int i=0;i<64;++i) {
            auto *corpse=spawn();
            terminal(corpse);
            replacements.push_back(corpse);
        }
        testUpdateCorpsePools();
        check(testCorpsePools(evicted)==0,"65th pool evicts the oldest");
        P_HazardRecordDeath(replacements.back(),HAZARD_ACID);
        P_RemoveMobj(replacements.back());
        replacements.pop_back();
        testUpdateCorpsePools();
        check(testCorpsePools(evicted)==0,"evicted pool is never recreated for the same death");
        for(auto *corpse:replacements)P_RemoveMobj(corpse);
        P_RemoveMobj(evicted);

        settings.corpseDecomposition=1;
        corpseDecayReset();
        auto *dry=spawn();
        terminal(dry);
        corpseDecayTick();
        check(testSetCorpseAge(dry,165*TICRATE),"tracked ordinary corpse before any pool");
        settings.blood=0;
        leveltime+=TICRATE;
        corpseDecayTick();
        settings.corpseDecomposition=0;
        settings.blood=1;
        testUpdateCorpsePools();
        check(testCorpsePools(dry)==0,"stage 2 with blood off prevents a future pool");
        P_RemoveMobj(dry);
        std::puts("PASS native rest, permanent pool ownership, scavenger placement and GPU decay restoration");
        I_Quit();
        break;
    }
    }
}
