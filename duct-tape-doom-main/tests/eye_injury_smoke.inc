// Focused native gameplay paths; uses the generated arena and real stock art.
extern "C" {void A_HeadAttack(mobj_t*);void A_PainAttack(mobj_t*);}
static void eyeIntegration() {
    auto *shooter=players[consoleplayer].mo;
    check(P_TeleportMove(shooter,0,fx(-32)),"eye fixture player placement");shooter->angle=ANG90;shooter->z=shooter->floorz;shooter->player->viewz=shooter->z+41*FRACUNIT;
    settings.accelerated=1;settings.enhancedWeakPoints=settings.eyeInjuries=1;settings.wounds=settings.glowingEyes=settings.hitFlinch=settings.voxelModels=settings.blood=0;
    P_WeakpointConfigure(1,1,0);P_WeakpointConfigureEyes(1);P_WeakpointConfigureExtras(0,0);P_WeakpointConfigureFeeding(0);P_WeakpointInitFeedingHooks();
    P_CriticalBegin=begin;P_CriticalThing=criticalThing;prepareCriticalSurface=criticalSurface;criticalVoxelModel=nullptr;
    p_wp_balance.eyeChance=100;
    auto countType=[](mobjtype_t type){int n=0;for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker&&((mobj_t*)t)->type==type)++n;return n;};
    for(auto type:{MT_HEAD,MT_PAIN,MT_SPIDER}) {
        auto *actor=P_SpawnMobj(0,fx(128),ONFLOORZ,type);actor->angle=ANG270;actor->tics=-1;actor->health=10000;
        auto &sf=sprites[actor->sprite].spriteframes[actor->frame&FF_FRAMEMASK];auto &im=patch(firstspritelump+sf.lump[0]);
        auto mask=doom_regions::build(type,im.width,im.height,im.left,im.top,im.pixels.data());
        auto at=std::find_if(mask.eyes.begin(),mask.eyes.end(),[](byte v){return v!=0;});check(at!=mask.eyes.end(),"stock front artwork exposes a verified eye overlay");
        int index=(int)(at-mask.eyes.begin()),x=index%im.width,y=index/im.width;
        auto changed=im.pixels;changed[index*2]^=1;auto replacement=doom_regions::build(type,im.width,im.height,im.left,im.top,changed.data());
        check(std::none_of(replacement.eyes.begin(),replacement.eyes.end(),[](byte v){return v!=0;}),"replacement artwork cannot reuse stock eye landmarks");
        // Actual art + eye overlay, independent of the glow/wound switches.
        const byte *pal=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);std::vector<byte> rgba((size_t)im.width*2*im.height*4,0);
        for(int row=0;row<im.height;++row)for(int col=0;col<im.width;++col)for(int side=0;side<2;++side){size_t p=(size_t)row*im.width+col,q=((size_t)row*im.width*2+col+side*im.width)*4;
            if(im.pixels[p*2+1]){for(int c=0;c<3;++c)rgba[q+c]=side&&mask.eyes[p]?(c==0?255:40):pal[im.pixels[p*2]*3+c];rgba[q+3]=255;}}
        auto *surface=SDL_CreateSurfaceFrom(im.width*2,im.height,SDL_PIXELFORMAT_RGBA32,rgba.data(),im.width*8);check(surface!=nullptr,"eye overlay surface");
        std::string path=output+"/eye-overlay-"+std::to_string((int)type)+".bmp";check(SDL_SaveBMP(surface,path.c_str()),"eye overlay capture saved");SDL_DestroySurface(surface);
        for(bool flip:{false,true}) {
            P_WeakpointForget(actor);actor->health=10000;actor->target=nullptr;actor->momx=actor->momy=actor->momz=0;actor->x=0;actor->y=fx(128);actor->z=actor->floorz;
            forgetWounds(actor);woundView(actor,shooter,im,flip,0,128,units(actor->z),im.top,im.top-im.height,1,0);
            float across=flip?im.width-im.left-x-.5f:x+.5f-im.left;
            P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,across,128,units(actor->z)+im.top-y-.5f);
            check(!P_WeakpointBlind(actor)&&!P_WeakpointEyeRage(actor),"injury waits for complete discharge");P_WeakpointEndShot();
            check(type==MT_SPIDER?P_WeakpointEyeRage(actor):P_WeakpointBlind(actor),"real mirrored/unmirrored eye ray creates the correct status with glow/wounds off");
        }
        if(type!=MT_SPIDER) {
            actor->target=nullptr;int before=countType(type==MT_HEAD?MT_HEADSHOT:MT_SKULL);
            if(type==MT_HEAD)A_HeadAttack(actor);else A_PainAttack(actor);
            check(countType(type==MT_HEAD?MT_HEADSHOT:MT_SKULL)==before+1,"blind attack uses real projectile/offspring spawn");
            for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto *m=(mobj_t*)t;
                if(type==MT_HEAD&&m->type==MT_HEADSHOT)check(m->target==actor,"blind Cacodemon projectile retains its shooter owner");
                if(type==MT_PAIN&&m->type==MT_SKULL&&m->health>0)check(!m->target&&(m->flags&MF_SKULLFLY),"blind Lost Soul charge inherits no hidden-player target");}
            auto *corpse=P_SpawnMobj(actor->x+fx(24),actor->y,ONFLOORZ,MT_TROOP);P_DamageMobj(corpse,shooter,shooter,10000);corpse->tics=-1;corpse->z=corpse->floorz;
            actor->z=actor->floorz;int skulls=countType(MT_SKULL);P_WeakpointFeedingTick(actor);check(P_WeakpointFeeding(actor),"native blinded floater seeks corpse at high health with Feeding off");
            P_WeakpointFeedingChase(actor);P_WeakpointFeedingAttack(actor);P_WeakpointFeedingChase(actor);
            check(!P_WeakpointBlind(actor)&&corpse->thinker.function.acv==(actionf_v)(-1),"native completed corpse bite cures blindness");
            check(countType(MT_SKULL)==skulls,"corpse feeding never spawns Lost Souls");
        } else {
            int hp=actor->health;wp_damage_event_t e{};e.target=actor;e.source=e.inflictor=shooter;e.kind=WP_PLASMA_DIRECT;e.baseDamage=10;P_WeakpointApply(&e);
            check(hp-actor->health==30,"native damage boundary caps rage plasma at three times");
            hp=actor->health;P_DamageMobj(actor,shooter,shooter,10);check(hp-actor->health==20,"native legacy damage doubles once");
            int cheats=shooter->player->cheats;shooter->player->cheats=0;shooter->health=shooter->player->health=500;shooter->player->armorpoints=0;P_DamageMobj(shooter,actor,actor,10);
            check(shooter->health==480,"native outgoing rage player damage doubles before protection");shooter->player->cheats=cheats;
        }
        P_RemoveMobj(actor);forgetWounds(actor);
        for(auto*t=thinkercap.next,*next=t->next;t!=&thinkercap;t=next,next=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*m=(mobj_t*)t;if(m!=shooter&&(m->type==MT_HEADSHOT||m->type==MT_SKULL))P_RemoveMobj(m);}
    }
    auto trigger=[&](mobj_t *actor){P_WeakpointBeginShot(shooter,wp_pistol);wp_damage_event_t e{};e.target=actor;e.source=e.inflictor=shooter;e.kind=WP_BULLET;e.baseDamage=1;e.impact.eyeContact=1;e.impact.confidence=100;P_WeakpointApply(&e);P_WeakpointEndShot();};
    auto *caco=P_SpawnMobj(fx(-200),0,ONFLOORZ,MT_HEAD);auto *pain=P_SpawnMobj(fx(200),0,ONFLOORZ,MT_PAIN);auto *spider=P_SpawnMobj(0,fx(-256),ONFLOORZ,MT_SPIDER);
    trigger(caco);trigger(pain);trigger(spider);int savedTime=leveltime;
    G_SaveGame(5,(char*)"Eye injuries v4");G_DoSaveGame();G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();caco=pain=spider=nullptr;
    for(auto*t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*m=(mobj_t*)t;if(m->type==MT_HEAD)caco=m;if(m->type==MT_PAIN)pain=m;if(m->type==MT_SPIDER)spider=m;}
    check(caco&&pain&&spider&&P_WeakpointBlind(caco)&&P_WeakpointBlind(pain)&&P_WeakpointEyeRage(spider),"real save/load restores all three actor-index eye statuses");
    leveltime=savedTime+350;P_WeakpointTick(spider);check(!P_WeakpointEyeRage(spider)&&P_WeakpointBlind(caco),"rage expires in ten seconds while blindness has no timeout");
    P_WeakpointConfigureEyes(0);check(!P_WeakpointBlind(caco)&&!P_WeakpointBlind(pain),"feature cancellation immediately restores normal perception");
    std::printf("PASS %d focused native eye-injury checks and stock overlays\n",checks);
}
