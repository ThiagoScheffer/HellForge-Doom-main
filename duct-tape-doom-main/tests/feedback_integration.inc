// Actual engine damage, projectile collision and renderer event integration.
static void feedbackAndStatusIntegration() {
    auto *shooter=players[consoleplayer].mo;
    // Remove earlier save-fixture actors so retarget choices are unambiguous.
    for(auto *t=thinkercap.next;t!=&thinkercap;) {
        auto *next=t->next;
        if(t->function.acp1==(actionf_p1)P_MobjThinker&&((mobj_t*)t)!=shooter)P_RemoveMobj((mobj_t*)t);
        t=next;
    }
    P_WeakpointClear();settings.accelerated=1;settings.enhancedWeakPoints=1;
    settings.criticalParts=1;settings.weaponVulnerabilities=0;settings.localizedInjuries=0;
    settings.monsterMetalSparks=1;settings.doorSparks=0;settings.blood=0;settings.wounds=0;
    P_WeakpointConfigure(1,0,0);P_WeakpointConfigureExtras(0,0);
    P_WeakpointImpact=combatImpact;P_WeakpointBulletLocate=cosmeticBulletLocate;P_WeakpointLocate=weakpointLocate;
    P_WeakpointTargetAllowed=+[](const mobj_t*)->int{return 1;};
    shooter->angle=ANG90;P_TeleportMove(shooter,0,fx(-32));shooter->z=shooter->floorz;shooter->player->viewz=shooter->z+41*FRACUNIT;
    auto balance=p_wp_balance;
    Settings defaults;
    check(!defaults.monsterBleeding&&!defaults.headshotFrenzy&&defaults.monsterMetalSparks&&defaults.headshotFeedback==2,"new combat defaults match approved plan");
    readOption(defaults,"monster_bleeding",1);readOption(defaults,"headshot_frenzy",1);readOption(defaults,"monster_metal_sparks",0);readOption(defaults,"headshot_feedback",3);
    check(defaults.monsterBleeding&&defaults.headshotFrenzy&&!defaults.monsterMetalSparks&&defaults.headshotFeedback==3,"new F4 settings load independently");
    FILE *config=std::tmpfile();check(config!=nullptr,"new feedback config output");writeOptions(config,defaults);std::rewind(config);
    char line[256];int optionCount=0;while(std::fgets(line,sizeof(line),config))
        if(std::strstr(line,"monster_bleeding 1")||std::strstr(line,"headshot_frenzy 1")||std::strstr(line,"monster_metal_sparks 0")||std::strstr(line,"headshot_feedback 3"))++optionCount;
    std::fclose(config);check(optionCount==4,"new F4 choices persist through graphics serializer");
    // Every selector, including an invisible crosshair, on real human headshots.
    auto *human=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_POSSESSED);human->angle=ANG270;
    const auto &hs=sprites[human->sprite].spriteframes[human->frame&FF_FRAMEMASK];auto &hi=patch(firstspritelump+hs.lump[0]);
    doom_joints::Head h{};check(doom_joints::findHead(hi.pixels.data(),hi.width,hi.height,hi.left,hi.top,h),"HS feedback fixture head");
    int hx=(int)std::floor(h.middle+hi.left),hy=0;while(hy<hi.height&&!solid(hi,hx,hy))++hy;
    for(int mode=0;mode<4;++mode) {
        settings.headshotFeedback=mode;settings.crosshair=0;headshotUntil=0;debris.clear();human->health=10000;
        P_SetMobjState(human,(statenum_t)mobjinfo[MT_POSSESSED].spawnstate);
        woundView(human,shooter,hi,false,0,128,0,(float)hi.top,(float)(hi.top-hi.height),1,0);
        P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,hx+.5f-hi.left,128,hi.top-hy-.5f);P_WeakpointEndShot();
        check(human->health==9990,"feedback selector never changes headshot damage");
        check((headshotUntil>gameSeconds())==((mode&1)!=0),"HS selector controls HUD feedback");
        check(debris.empty(),"hidden blood suppresses physical HS spray in every selector mode");
        bool pending=worldPending;worldPending=true;
        check((!crosshairVertices(640,400).empty())==((mode&1)!=0),"HS HUD appears even with crosshair disabled");worldPending=pending;
    }
    settings.headshotFeedback=3;human->health=1;headshotUntil=0;
    P_SetMobjState(human,(statenum_t)mobjinfo[MT_POSSESSED].spawnstate);
    woundView(human,shooter,hi,false,0,128,0,(float)hi.top,(float)(hi.top-hi.height),1,0);
    P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,hx+.5f-hi.left,128,hi.top-hy-.5f);P_WeakpointEndShot();
    check(human->health<=0&&headshotUntil>gameSeconds(),"lethal headshots retain pre-death region feedback");P_RemoveMobj(human);
    // Mechanical bullets with damage rules OFF: cosmetic query cannot enable alpha collision or bonuses.
    settings.enhancedWeakPoints=settings.criticalParts=0;P_WeakpointConfigure(0,0,0);
    for(auto type:{MT_BABY,MT_SPIDER}) {
        auto *target=P_SpawnMobj(0,fx(128),ONFLOORZ,type);target->angle=ANG270;target->health=10000;
        const auto &sf=sprites[target->sprite].spriteframes[target->frame&FF_FRAMEMASK];auto &im=patch(firstspritelump+sf.lump[0]);
        for(int r=0;r<8;++r)patch(firstspritelump+sf.lump[r]);
        const auto &mask=weakpointRegionMask(type,im);int pixel=-1;
        float top=(float)std::max(im.top,im.height);
        for(size_t i=0;i<mask.size();++i)if(mask[i]==WP_LEGS&&std::abs((float)(i%im.width)+.5f-im.left)<units(target->radius)-2&&
            top-i/im.width-.5f>2&&top-i/im.width-.5f<units(target->height)-2){pixel=(int)i;break;}
        check(pixel>=0,"mechanical fixture has leg pixels inside legacy collision bounds");
        float x=pixel%im.width+.5f-im.left,z=top-pixel/im.width-.5f;
        forgetWounds(target);woundView(target,shooter,im,false,0,128,top-im.top,top,top-im.height,1,0);
        debris.clear();P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,x,128,z);P_WeakpointEndShot();
        check(target->health==9995,"cosmetic leg sparks preserve ordinary bullet damage");
        check(debris.size()==(size_t)p_wp_balance.sparkBullet,"confirmed metal-leg bullet reuses bounded door-track spark particles");
        settings.monsterMetalSparks=0;debris.clear();P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,x,128,z);P_WeakpointEndShot();
        check(debris.empty()&&target->health==9990,"metal sparks toggle independent of collision and damage");settings.monsterMetalSparks=1;
        // Locate a direct plasma impact, with vulnerabilities still off.
        auto *plasma=P_SpawnMobj(fx(x),0,fx(z-4),MT_PLASMA);plasma->target=shooter;
        P_WeakpointProjectileSpawn(plasma,shooter,WP_PLASMA_DIRECT);
        P_WeakpointProjectileStep(plasma,plasma->x,0,plasma->z,plasma->x,fx(96),plasma->z);
        wp_impact_t impact{};int localized=weakpointLocate(plasma,target,&impact);
        if(localized&&impact.region==WP_LEGS){
            debris.clear();P_WeakpointProjectileHit(plasma,target,5);
            check(target->health==9985&&debris.size()==(size_t)p_wp_balance.sparkBullet,"cosmetic-only direct plasma leg contact sparks without vulnerabilities");
        }
        P_RemoveMobj(plasma);P_RemoveMobj(target);
    }
    // Real ordinary bullet path records bleeding without enabling sprite critical collision.
    auto *imp=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_TROOP);imp->health=1000;imp->angle=ANG270;
    P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
    P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,0,128,24);P_WeakpointEndShot();
    check(imp->health==995&&P_WeakpointBleeding(imp),"real ordinary body hit causes independently enabled bleeding");
    imp->momx=fx(2);imp->momy=fx(3);auto *pain=imp->state;int start=leveltime;
    for(int i=1;i<=6;++i){leveltime=start+35*i;P_WeakpointTick(imp);}
    check(imp->health==983&&imp->state==pain&&imp->momx==fx(2)&&imp->momy==fx(3),"real bleed ticks preserve pain state and external momentum");
    check(P_WeakpointWalkSpeed(imp,10*FRACUNIT)==8*FRACUNIT,"real shared walk scaling");
    imp->health=1;leveltime=start+211;P_WeakpointConfigureExtras(0,0);P_WeakpointConfigureExtras(1,0);
    P_WeakpointBeginShot(shooter,wp_pistol);auto e=wp_damage_event_t{};e.target=imp;e.source=e.inflictor=shooter;e.kind=WP_BULLET;e.baseDamage=1;
    imp->health=3;P_WeakpointApply(&e);P_WeakpointEndShot();int kills=shooter->player->killcount;leveltime+=35;P_WeakpointTick(imp);
    check(imp->health<=0&&(imp->flags&MF_CORPSE)&&shooter->player->killcount==kills+1,"real bleeding death credits original player normally");P_RemoveMobj(imp);
    P_WeakpointConfigureExtras(0,1);P_WeakpointConfigure(1,0,0);settings.enhancedWeakPoints=1;p_wp_balance.frenzyChance=100;
    for(auto type:{MT_FATSO,MT_KNIGHT,MT_BRUISER}) {
        auto *actor=P_SpawnMobj(fx(-120),0,ONFLOORZ,type);actor->health=10000;
        auto *victim=P_SpawnMobj(fx(80),0,ONFLOORZ,type==MT_FATSO?MT_FATSO:type==MT_KNIGHT?MT_BRUISER:MT_KNIGHT);victim->health=10000;
        P_WeakpointBeginShot(shooter,wp_supershotgun);
        e={};e.target=actor;e.source=e.inflictor=shooter;e.kind=WP_SHOTGUN_PELLET;e.baseDamage=1;e.impact.region=WP_HEAD;e.impact.confidence=100;e.impact.distance=128;
        P_WeakpointApply(&e);P_WeakpointEndShot();
        check(P_WeakpointFrenzied(actor)&&actor->target==victim,"actual AI frenzy selects visible nearby monster");
        mobjtype_t missileType=type==MT_FATSO?MT_FATSHOT:MT_BRUISERSHOT;
        // Native spawn function attaches provenance before its initial collision check.
        auto *missile=P_SpawnMissile(actor,victim,missileType);
        check(P_WeakpointFriendlyMissile(missile)&&missile->target==actor,"native monster missile retains frenzy provenance and owner");
        leveltime+=350;P_WeakpointFrenzyTarget(actor);
        check(!P_WeakpointFrenzied(actor)&&P_WeakpointFriendlyMissile(missile),"native frenzy expiry preserves in-flight friendly fire");
        int before=victim->health;
        check(!P_TryMove(missile,victim->x-victim->radius,victim->y)&&victim->health<before,"real Knight/Baron cross-species immunity bypass on marked projectile");
        P_RemoveMobj(missile);P_RemoveMobj(actor);P_RemoveMobj(victim);
    }
    // Fire the real twenty-pellet native super-shotgun action into a stock head.
    {
        auto *player=shooter->player;auto held=player->readyweapon;auto wasFree=freeaim,wasClassic=classicaim;float pitch=tourpitch;
        auto *actor=P_SpawnMobj(0,fx(32),ONFLOORZ,MT_KNIGHT);actor->angle=ANG270;actor->health=10000;
        auto *victim=P_SpawnMobj(fx(180),fx(32),ONFLOORZ,MT_BRUISER);victim->health=10000;
        const auto &sf=sprites[actor->sprite].spriteframes[actor->frame&FF_FRAMEMASK];auto &im=patch(firstspritelump+sf.lump[0]);
        const auto &mask=weakpointRegionMask(actor->type,im);int best=-1,score=-1;
        for(int y=0;y<im.height;++y)for(int x=0;x<im.width;++x)if(mask[(size_t)y*im.width+x]==WP_HEAD) {
            int density=0;for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx)
                if(x+dx>=0&&x+dx<im.width&&y+dy>=0&&y+dy<im.height&&mask[(size_t)(y+dy)*im.width+x+dx]==WP_HEAD)++density;
            if(density>score){score=density;best=y*im.width+x;}
        }
        check(best>=0,"native frenzy fixture skull");
        float top=(float)std::max(im.top,im.height),x=best%im.width+.5f-im.left,z=top-best/im.width-.5f;
        freeaim=true;classicaim=false;shooter->angle=R_PointToAngle2(shooter->x,shooter->y,fx(x),actor->y);
        tourpitch=std::atan((z-units(player->viewz))/std::hypot(x,64.f));player->readyweapon=wp_supershotgun;player->ammo[am_shell]=100;
        woundView(actor,shooter,im,false,0,32,top-im.top,top,top-im.height,1,0);
        A_FireShotgun2(player,&player->psprites[ps_weapon]);
        check(P_WeakpointFrenzied(actor)&&actor->target==victim,"native twenty-pellet super-shotgun action triggers one surviving-head frenzy");
        P_RemoveMobj(actor);P_RemoveMobj(victim);player->readyweapon=held;freeaim=wasFree;classicaim=wasClassic;tourpitch=pitch;shooter->angle=ANG90;
    }
    // Native save/load remaps the new status references, not just their timers.
    {
        settings.monsterBleeding=settings.headshotFrenzy=1;P_WeakpointConfigureExtras(1,1);p_wp_balance.bleedChance=100;
        auto *actor=P_SpawnMobj(fx(-100),fx(96),ONFLOORZ,MT_KNIGHT);actor->health=10000;
        auto *victim=P_SpawnMobj(fx(100),fx(96),ONFLOORZ,MT_BRUISER);victim->health=10000;
        auto *bleeder=P_SpawnMobj(fx(200),fx(180),ONFLOORZ,MT_TROOP);bleeder->health=1000;
        P_WeakpointBeginShot(shooter,wp_supershotgun);
        e={};e.target=actor;e.source=e.inflictor=shooter;e.kind=WP_SHOTGUN_PELLET;e.baseDamage=1;e.impact.region=WP_HEAD;e.impact.confidence=100;e.impact.distance=64;
        P_WeakpointApply(&e);P_WeakpointEndShot();
        P_WeakpointBeginShot(shooter,wp_pistol);e={};e.target=bleeder;e.source=e.inflictor=shooter;e.kind=WP_BULLET;e.baseDamage=1;P_WeakpointApply(&e);P_WeakpointEndShot();
        auto *missile=P_SpawnMissile(actor,victim,MT_BRUISERSHOT);
        check(P_WeakpointBleeding(bleeder)&&P_WeakpointFrenzied(actor)&&P_WeakpointFriendlyMissile(missile),"native save fixture contains all new status/provenance records");
        G_SaveGame(9,(char*)"Feedback statuses");G_DoSaveGame();G_LoadGame((char*)"doomsav9.dsg");G_DoLoadGame();
        actor=victim=bleeder=missile=nullptr;shooter=players[consoleplayer].mo;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker) {
            auto *m=(mobj_t*)t;if(m->type==MT_KNIGHT&&m->health>0)actor=m;else if(m->type==MT_BRUISER&&m->health>0)victim=m;
            else if(m->type==MT_TROOP&&m->health>0)bleeder=m;else if(m->type==MT_BRUISERSHOT&&(m->flags&MF_MISSILE))missile=m;
        }
        check(actor&&victim&&bleeder&&missile,"native save remaps all four actors");
        check(P_WeakpointBleeding(bleeder)&&P_WeakpointFrenzied(actor)&&P_WeakpointFriendlyMissile(missile),"native save/load restores bleed/frenzy/projectile states");
        P_WeakpointFrenzyTarget(actor);check(actor->target==victim&&missile->target==actor,"native save/load preserves frenzy victim and projectile ownership");
        bleeder->health=2;int kills=shooter->player->killcount;leveltime+=35;P_WeakpointTick(bleeder);
        check(bleeder->health<=0&&shooter->player->killcount==kills+1,"restored bleeding preserves original player kill attribution");
        P_RemoveMobj(actor);P_RemoveMobj(victim);P_RemoveMobj(bleeder);P_RemoveMobj(missile);settings.monsterBleeding=settings.headshotFrenzy=0;
    }
    p_wp_balance=balance;P_WeakpointConfigureExtras(0,0);P_WeakpointImpact=nullptr;P_WeakpointBulletLocate=nullptr;
    settings.headshotFeedback=2;settings.crosshair=1;settings.monsterMetalSparks=1;
    std::puts("PASS real feedback selectors, lethal HS, metal-leg contacts, bleeding death and frenzy projectile infighting");
}

// Bounded GPU fixture: real hits/status ticks, four representative captures.
static void feedbackVisualFrame() {
    static int stage=0,step=0,wait=0,last=-1;
    static mobj_t *actor=nullptr,*other=nullptr;
    static const char *names[]={"headshot-feedback","metal-leg-sparks","bleeding-trail","frenzy-infighting"};
    if(last==leveltime)return;last=leveltime;
    auto *shooter=players[consoleplayer].mo;
    if(R_BeforeTic)R_BeforeTic();
    if(step==0) {
        if(stage==4){std::puts("PASS GPU captures: crosshair-off HS, mechanical sparks, bleeding trail, frenzy infighting");I_Quit();}
        settings.enhancedWeakPoints=1;settings.weaponVulnerabilities=0;settings.localizedInjuries=0;
        settings.monsterBleeding=stage==2;settings.headshotFrenzy=stage==3;settings.headshotFeedback=stage==0?3:0;
        settings.monsterMetalSparks=1;settings.crosshair=0;settings.blood=settings.bloodSpray=1;settings.bloodLevel=2;
        settings.hitFlinch=0;settings.deathMotion=0;settings.wounds=1;
        p_wp_balance.bleedChance=p_wp_balance.frenzyChance=100;
        P_WeakpointConfigure(1,0,0);P_WeakpointConfigureExtras(stage==2,stage==3);
        P_TeleportMove(shooter,0,fx(-96));shooter->angle=ANG90;shooter->z=shooter->floorz;shooter->player->viewz=shooter->z+41*FRACUNIT;
        auto type=stage==1?MT_BABY:stage==3?MT_KNIGHT:MT_TROOP;
        actor=P_SpawnMobj(stage==2?fx(-48):0,fx(96),ONFLOORZ,type);actor->angle=ANG270;actor->health=10000;actor->tics=-1;
        if(stage==3){other=P_SpawnMobj(fx(136),fx(96),ONFLOORZ,MT_BRUISER);other->health=10000;other->angle=ANG180;other->tics=-1;}
        debris.clear();wait=leveltime;step=1;return;
    }
    actor->momx=actor->momy=actor->momz=0;actor->tics=-1;
    if(other){other->momx=other->momy=other->momz=0;other->tics=-1;}
    if(step==1&&leveltime-wait>=4) {
        const auto &sf=sprites[actor->sprite].spriteframes[actor->frame&FF_FRAMEMASK];
        const auto &im=lumpImage(firstspritelump+sf.lump[0],false);const auto &mask=weakpointRegionMask(actor->type,im);
        auto desired=stage==1?WP_LEGS:stage==2?WP_BODY:WP_HEAD;int pixel=-1;
        for(size_t i=0;i<mask.size();++i)if(mask[i]==desired){pixel=(int)i;break;}
        check(pixel>=0,"feedback GPU fixture region");
        float top=(float)std::max(im.top,im.height),x=units(actor->x)+pixel%im.width+.5f-im.left,z=units(actor->z)+top-pixel/im.width-.5f;
        P_WeakpointBeginShot(shooter,stage==3?wp_supershotgun:wp_pistol);
        // Frenzy trigger must be close, so move the player before that head ray.
        if(stage==3){P_TeleportMove(shooter,0,fx(0));shooter->player->viewz=shooter->z+41*FRACUNIT;}
        shoot(shooter,x,96,z);P_WeakpointEndShot();
        if(stage==2)check(P_WeakpointBleeding(actor),"GPU bullet starts bleeding");
        if(stage==3){check(P_WeakpointFrenzied(actor)&&actor->target==other,"GPU close head pellet starts infighting");}
        actor->momx=actor->momy=actor->momz=0;wait=leveltime;step=2;

        return;
    }
    if(step==2) {
        if(stage==2)P_TryMove(actor,actor->x+fx(1.5f),actor->y);
        if(stage==3&&leveltime-wait==3) {
            // An actual monster attack action, with ownership/provenance retained.
            actor->angle=R_PointToAngle2(actor->x,actor->y,other->x,other->y);
            P_SpawnMissile(actor,other,MT_BRUISERSHOT);
        }
        if(leveltime-wait>=(stage==2?45:stage==3?8:3)) {
            visualCapture=output+"/"+names[stage]+".png";I_Render3DScreenshotTo(visualCapture.c_str());step=3;
        }
        return;
    }
    if(step==3) {
        FILE *file=std::fopen(visualCapture.c_str(),"rb");check(file!=nullptr,"feedback GPU screenshot saved");std::fclose(file);
        P_RemoveMobj(actor);forgetWounds(actor);if(other){P_RemoveMobj(other);forgetWounds(other);other=nullptr;}
        ++stage;step=0;
    }
}
