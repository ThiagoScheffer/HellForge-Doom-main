// Three bounded real-renderer scenes. Camera follows only for capture framing.
static void eyeVisualFrame() {
    static int stage=0,step=0,start=0,last=-1;static mobj_t *actor=nullptr,*victim=nullptr;
    if(last==leveltime)return;last=leveltime;auto *player=players[consoleplayer].mo;
    if(step==0){
        if(stage==3){std::puts("PASS three live eye-injury GPU captures");I_Quit();}
        settings.accelerated=settings.enhancedWeakPoints=settings.eyeInjuries=1;settings.weaponVulnerabilities=1;
        settings.monsterFeeding=settings.monsterBleeding=settings.headshotFrenzy=settings.hitFlinch=settings.voxelModels=0;
        settings.wounds=settings.glowingEyes=1;settings.crosshair=0;applySettings();P_WeakpointInitFeedingHooks();p_wp_balance.eyeChance=100;
        check(P_TeleportMove(player,0,fx(-192)),"eye GPU observer placement");player->angle=ANG90;player->z=player->floorz;player->player->viewz=player->z+41*FRACUNIT;player->player->cheats|=CF_GODMODE;
        actor=P_SpawnMobj(0,fx(128),ONFLOORZ,stage==0?MT_HEAD:stage==1?MT_PAIN:MT_SPIDER);actor->health=5000;actor->angle=ANG270;actor->tics=-1;
        if(stage==2){victim=P_SpawnMobj(0,fx(400),ONFLOORZ,MT_TROOP);victim->health=5000;victim->tics=-1;}
        start=leveltime;step=1;return;
    }
    if(step==1){
        actor->tics=-1;
        if(leveltime-start<4)return;
        const auto &sf=sprites[actor->sprite].spriteframes[actor->frame&FF_FRAMEMASK];const auto &im=lumpImage(firstspritelump+sf.lump[0],false);
        auto mask=doom_regions::build(actor->type,im.width,im.height,im.left,im.top,im.pixels.data());int eye=-1;
        for(size_t i=0;i<mask.eyes.size();++i)if(mask.eyes[i]){eye=(int)i;break;}
        check(eye>=0,"GPU stock eye landmark");P_WeakpointBeginShot(player,wp_pistol);
        shoot(player,units(actor->x)+eye%im.width+.5f-im.left,units(actor->y),units(actor->z)+std::max(im.top,im.height)-eye/im.width-.5f);P_WeakpointEndShot();
        check(stage==2?P_WeakpointEyeRage(actor):P_WeakpointBlind(actor),"GPU eye hit triggers status");
        start=leveltime;step=2;return;
    }
    player->angle=R_PointToAngle2(player->x,player->y,actor->x,actor->y);
    if(leveltime-start==82){
        check(stage==2?P_WeakpointEyeRage(actor):P_WeakpointBlind(actor),"live status remains active at capture");
        if(stage==2)check(actor->target==victim,"live Spider attacks another monster");
        visualCapture=output+(stage==0?"/caco-blind-fire.png":stage==1?"/pain-blind-fire.png":"/spider-eye-rage.png");I_Render3DScreenshotTo(visualCapture.c_str());
        std::printf("EYE_SCENE stage %d status confirmed at tic %d\n",stage,leveltime);
    }
    if(leveltime-start>=86){
        for(auto*t=thinkercap.next,*next=t->next;t!=&thinkercap;t=next,next=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*m=(mobj_t*)t;if(m!=player){forgetWounds(m);P_RemoveMobj(m);}}
        actor=victim=nullptr;++stage;step=0;
    }
}
