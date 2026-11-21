// Bounded actual GPU scenes, native feeding actors and cosmetic captures.
static void survivalVisualFrame() {
    static const char *names[]={"metal-sparks","head-blood-wall","hemorrhage-moving","hemorrhage-pool","pinky-feeding","caco-feeding"};
    static int stage=0,step=0,wait=0,last=-1,startHP=0;static bool actionCaptured=false;
    static mobj_t *actor=nullptr,*food=nullptr;
    if(last==leveltime)return;last=leveltime;if(R_BeforeTic)R_BeforeTic();
    auto *shooter=players[consoleplayer].mo;
    if(step==0){
        if(stage==6){std::puts("PASS survival GPU captures: strong metal sparks, green head blood/wall, moving/stationary hemorrhage, Pinky eating and Cacodemon biting");I_Quit();}
        settings.enhancedWeakPoints=1;settings.weaponVulnerabilities=0;settings.localizedInjuries=0;settings.monsterMetalSparks=1;
        settings.headshotFeedback=stage==1?3:0;settings.crosshair=0;settings.headshotSound=1;settings.blood=settings.bloodSpray=1;settings.bloodLevel=2;
        settings.hitFlinch=0;settings.deathMotion=0;settings.wounds=1;settings.monsterBleeding=stage==2||stage==3;settings.monsterFeeding=stage>=4;
        P_WeakpointConfigure(1,0,0);P_WeakpointConfigureExtras(settings.monsterBleeding,0);P_WeakpointConfigureFeeding(settings.monsterFeeding);
        p_wp_balance.bleedChance=100;P_WeakpointInitFeedingHooks();
        check(P_TeleportMove(shooter,0,fx(-128))!=0,"survival GPU observer placement");shooter->angle=ANG90;shooter->z=shooter->floorz;shooter->player->viewz=shooter->z+41*FRACUNIT;
        auto type=stage==0?MT_BABY:stage==1?MT_KNIGHT:stage==4?MT_SERGEANT:stage==5?MT_HEAD:MT_TROOP;
        actor=P_SpawnMobj(stage==2?fx(-48):0,stage>=4?fx(32):stage==1?fx(118):fx(96),ONFLOORZ,type);actor->angle=ANG270;
        actor->health=stage>=4?actor->info->spawnhealth*30/100:1000;startHP=actor->health;
        if(stage==5)actor->z=actor->floorz+fx(80);
        if(stage<4)actor->tics=-1;
        if(stage>=4){food=P_SpawnMobj(0,fx(104),ONFLOORZ,MT_TROOP);food->angle=ANG270;food->tics=-1;
            if(stage==4){statenum_t state=(statenum_t)food->info->deathstate;for(int i=0;i<32&&states[state].nextstate!=S_NULL;++i)state=states[state].nextstate;
                P_SetMobjState(food,state);food->health=0;food->height=food->info->height/4;food->flags=MF_CORPSE|MF_COUNTKILL;food->tics=-1;}
            else food->health=100;
        }
        debris.clear();combatFeedbackStats={};wait=leveltime;actionCaptured=false;step=1;return;
    }
    if(stage<4){actor->tics=-1;actor->momx=actor->momy=actor->momz=0;}
    if(food&&food->thinker.function.acv!=(actionf_v)(-1)){food->tics=-1;food->momx=food->momy=food->momz=0;}
    check(leveltime-wait<260,"survival GPU scene timeout");
    if(step==1&&stage<4&&leveltime-wait>=4){
        const auto &sf=sprites[actor->sprite].spriteframes[actor->frame&FF_FRAMEMASK];const auto &im=lumpImage(firstspritelump+sf.lump[0],false);
        const auto &mask=weakpointRegionMask(actor->type,im);auto desired=stage==0?WP_LEGS:stage==1?WP_HEAD:WP_BODY;int pixel=-1;
        int best=INT_MAX;
        for(size_t i=0;i<mask.size();++i)if(mask[i]==desired){
            int dx=(int)(i%im.width)-im.left,dy=im.top-(int)(i/im.width)-24,score=stage>=2?dx*dx+dy*dy:(int)i;
            if(score<best){best=score;pixel=(int)i;}
        }
        check(pixel>=0,"survival GPU stock region");float top=(float)std::max(im.top,im.height),x=units(actor->x)+pixel%im.width+.5f-im.left,z=top-pixel/im.width-.5f;
        for(int shot=0;shot<(stage>=2?3:1);++shot){
            if(stage>=2){
                actor->state=&states[actor->info->spawnstate];actor->sprite=actor->state->sprite;actor->frame=actor->state->frame;actor->tics=-1;
                forgetWounds(actor);
            }
            int dropsBefore=combatFeedbackStats.bleedDrops;
            P_WeakpointBeginShot(shooter,wp_pistol);shoot(shooter,x,units(actor->y),z);P_WeakpointEndShot();
            if(stage>=2) {
                check(P_WeakpointBleedTier(actor)==shot+1,"actual GPU bullets progress through all three bleeding tiers");
                if(shot==0)check(combatFeedbackStats.bleedDrops>dropsBefore,"confirmed bleeding emits on application");
            }
        }
        if(stage>=2){if(P_WeakpointBleedTier(actor)!=3)std::fprintf(stderr,"GPU hemorrhage tier=%d health=%d position=%.2f,%.2f\n",P_WeakpointBleedTier(actor),actor->health,units(actor->x),units(actor->y));check(P_WeakpointBleedTier(actor)==3,"actual GPU bullets build bounded hemorrhage");}
        actor->momx=actor->momy=actor->momz=0;actor->tics=-1;wait=leveltime;step=2;return;
    }
    if(step==1&&stage>=4){
        if(!actionCaptured&&(actor->state==&states[stage==4?S_SARG_ATK2:S_HEAD_ATK2])){
            visualCapture=output+"/survival-"+(stage==4?std::string("pinky-eating"):std::string("caco-biting"))+".png";I_Render3DScreenshotTo(visualCapture.c_str());actionCaptured=true;return;
        }
        if(actor->health>startHP){check(actionCaptured,"native feeding animation was observed before recovery");
            if(stage==4)check(food->thinker.function.acv==(actionf_v)(-1),"GPU eating consumed the actual corpse");
            else check(food->health==84&&actor->z-actor->floorz<=24*FRACUNIT,"GPU Cacodemon descended and delivered one actual bite");
            visualCapture=output+"/survival-"+names[stage]+".png";I_Render3DScreenshotTo(visualCapture.c_str());step=3;return;
        }
    }
    if(step==2){
        if(stage==2)P_TryMove(actor,actor->x+fx(1.5f),actor->y);
        if(leveltime-wait>=(stage<2?3:50)){
            if(stage==0)check(combatFeedbackStats.metalParticles>0,"GPU metal sparks emitted");
            if(stage==1)check(combatFeedbackStats.headBursts>0&&combatFeedbackStats.wallSplats>0,"GPU head blood and nearby wall marks emitted");
            if(stage==3)check(combatFeedbackStats.bleedPools>0,"stationary hemorrhage pools emitted");
            visualCapture=output+"/survival-"+names[stage]+".png";I_Render3DScreenshotTo(visualCapture.c_str());step=3;return;
        }
    }
    if(step==3){
        FILE *f=std::fopen(visualCapture.c_str(),"rb");check(f!=nullptr,"survival GPU capture saved");std::fclose(f);
        P_RemoveMobj(actor);forgetWounds(actor);if(food&&food->thinker.function.acv!=(actionf_v)(-1))P_RemoveMobj(food);food=nullptr;
        ++stage;step=0;
    }
}
