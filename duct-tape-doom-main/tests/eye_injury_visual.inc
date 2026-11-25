#include <chrono>

static int eyeFixtureLump(const mobj_t *actor) {
    return firstspritelump + sprites[actor->sprite].spriteframes[actor->frame & FF_FRAMEMASK].lump[0];
}

static bool eyeBatch(int key, bool glowing) {
    const auto found = batches.find(key);
    return found != batches.end() && !found->second.empty() &&
        !!(found->second.front().mode & MODE_GLOWING_EYES) == glowing;
}

static void eyeStockVariants(mobjtype_t type, int sprite, int frontLump) {
    const auto &frames = sprites[sprite];
    int visible = 0, hidden = 0, mirrored = 0, attack = 0;
    std::set<int> visited, keys;
    for (int frame = 0; frame < frames.numframes; ++frame) {
        const auto &sf = frames.spriteframes[frame];
        if (!sf.rotate) continue; // death poses
        for (int view = 0; view < 8; ++view) {
            const int lump = firstspritelump + sf.lump[view];
            int key = -1 - lump;
            auto &original = lumpImage(lump, false);
            auto mask = doom_regions::build(type, original.width, original.height, original.left, original.top,
                                           original.pixels.data());
            const bool eye = mask.stock && std::any_of(mask.eyes.begin(), mask.eyes.end(), [](byte b){return b;});
            const auto saved = original.pixels;
            const auto &injured = monsterEyeInjuryImage(type, lump, original, original, key);
            if (!eye) {
                check(&original == &injured && key == -1 - lump, "hidden and unsupported views keep original texture");
                hidden += mask.stock;
                continue;
            }
            check(&original != &injured && injured.texture && injured.texture != original.texture,
                  "stock eye gets a distinct uploaded texture");
            check(injured.width == original.width && injured.height == original.height &&
                  injured.left == original.left && injured.top == original.top, "injury keeps dimensions and offsets");
            bool confined = true;
            for (size_t i = 0; i < mask.eyes.size(); ++i)
                confined &= injured.pixels[2*i+1] == saved[2*i+1] &&
                            (mask.eyes[i] || injured.pixels[2*i] == saved[2*i]);
            check(confined && injured.pixels != saved && original.pixels == saved, "only the verified eye changes");
            if (visited.insert(lump).second) check(keys.insert(key).second, "different stock lumps have distinct keys");
            mirrored += sf.flip[view] != 0;
            attack += frame == 2;
            ++visible;
        }
    }
    check(visible && hidden && mirrored && attack, "stock sweep exercises visible, hidden, mirrored and attack views");

    auto &front = lumpImage(frontLump, false);
    auto replacement = front;
    for (size_t i = 0; i < replacement.pixels.size(); i += 2) {
        if (!replacement.pixels[i+1]) continue;
        replacement.pixels[i] ^= 1;
        break;
    }
    int key = -1710000000 - int(type);
    check(&monsterEyeInjuryImage(type, frontLump, replacement, replacement, key) == &replacement &&
          key == -1710000000 - int(type), "replacement art leaves source and key intact");

    if (type == MT_HEAD) {
        std::set<int> tiers;
        for (int health : {70, 45, 20}) {
            int skinKey = -1 - frontLump;
            auto &skin = monsterSkinImage(type, frontLump, front, health, 100, skinKey);
            check(&skin != &front, "stock Cacodemon generates skin damage tier");
            int injuryKey = skinKey;
            auto &injured = monsterEyeInjuryImage(type, frontLump, front, skin, injuryKey);
            check(injuryKey != skinKey && tiers.insert(injuryKey).second && injured.pixels != skin.pixels,
                  "each skin damage tier composes with a distinct eye injury");
            bool preserved = true;
            for (size_t i = 0; i < injured.eyeDamageMask.size(); ++i)
                if (!injured.eyeDamageMask[i]) preserved &= injured.pixels[2*i] == skin.pixels[2*i];
            check(preserved, "eye variant retains existing damage outside the socket");
        }
    }
}

static int eyeCacheBudget(int type, int lump) {
    auto &original = lumpImage(lump, false);
    const auto start = std::chrono::steady_clock::now();
    int firstKey = -1750000000;
    auto &first = monsterEyeInjuryImage(type, lump, original, original, firstKey);
    const auto generated = std::chrono::steady_clock::now();
    check(&first != &original && first.texture, "first lookup allocates a texture");
    const auto imageCount = images.size();
    for (int i = 0; i < 20000; ++i) {
        int key = -1750000000;
        const auto &cached = monsterEyeInjuryImage(type, lump, original, original, key);
        if (key != firstKey || cached.texture != first.texture)
            check(false, "warm requests must reuse exactly the same texture");
    }
    const auto warm = std::chrono::steady_clock::now();
    check(images.size() == imageCount, "no additional image uploads on warm requests");
    int exhausted = 0;
    int allocated = 1;
    for (int i = 1; i < 1024; ++i) {
        const int sourceKey = -1750000000 - i;
        int key = sourceKey;
        if (&monsterEyeInjuryImage(type, lump, original, original, key) != &original) {
            check(key != firstKey && key != sourceKey, "distinct source keys produce separate variants");
            ++allocated;
            continue;
        }
        const auto before = images.size();
        check(key == sourceKey &&
              &monsterEyeInjuryImage(type, lump, original, original, key) == &original && images.size() == before,
              "budget fallback is stable and keeps the source key");
        exhausted = sourceKey;
        break;
    }
    check(exhausted, "test filled the eye texture budget");
    int key = -1750000000;
    check(&monsterEyeInjuryImage(type, lump, original, original, key) == &first,
          "existing variants survive cache exhaustion");
    const auto micros = [](auto duration){ return std::chrono::duration<double, std::micro>(duration).count(); };
    std::printf("EYE_CACHE first %.1f us, mean warm %.3f us, %d variants added (%zu estimated bytes)\n",
                micros(generated-start), micros(warm-generated)/20000., allocated,
                size_t(allocated)*original.width*original.height*7u);
    return exhausted;
}

static void eyeFixtureBlind(mobj_t *actor,mobj_t *player) {
    wp_damage_event_t hit{};
    hit.target=actor;hit.source=hit.inflictor=player;hit.kind=WP_BULLET;hit.baseDamage=1;
    hit.impact.eyeContact=1;hit.impact.confidence=100;
    P_WeakpointBeginShot(player,wp_pistol);P_WeakpointApply(&hit);P_WeakpointEndShot();
    check(P_WeakpointBlind(actor),"deterministic fixture hit blinds floater");
    actor->frame=0;actor->tics=-1;
}

// Three bounded real-renderer scenes. Camera follows only for capture framing.
static void eyeVisualFrame() {
    static int stage=0,step=0,start=0,last=-1,frontLump=0,injuredKey=0,coatedKeys[2]={};
    static mobj_t *actor=nullptr,*victim=nullptr;
    static mobj_t *group[3]={};
    static int groupKey=0,exhaustedKey=0;
    static double cpu[2]={},gpu[2]={};
    static int samples[2]={};
    if(last==leveltime)return;last=leveltime;auto *player=players[consoleplayer].mo;
    if(stage==3) {
        if(step==0) {
            actor=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_HEAD);actor->angle=ANG270;
            player->angle=ANG90;eyeFixtureBlind(actor,player);
            frontLump=eyeFixtureLump(actor);
            exhaustedKey=eyeCacheBudget(MT_HEAD,frontLump);
            G_SaveGame(5,(char*)"Visible blindness");G_DoSaveGame();
            G_LoadGame((char*)"doomsav5.dsg");G_DoLoadGame();
            start=leveltime;step=1;return;
        }
        if(leveltime-start<3)return;
        actor=nullptr;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)
            if(t->function.acp1==(actionf_p1)P_MobjThinker&&((mobj_t*)t)->type==MT_HEAD)actor=(mobj_t*)t;
        check(actor&&P_WeakpointBlind(actor),"save/load restores gameplay blindness on the rendered Cacodemon");
        auto &original=lumpImage(frontLump,false);
        int key=-1-frontLump;
        check(&monsterEyeInjuryImage(MT_HEAD,frontLump,original,original,key)!=&original&&eyeBatch(key,false),
              "loaded blinded actor draws a regenerated injured sprite");
        key=exhaustedKey;
        check(&monsterEyeInjuryImage(MT_HEAD,frontLump,original,original,key)!=&original,
              "level reload clears rejected identities and texture budget");
        std::printf("PASS %d stock and live eye-injury renderer checks\n",checks);I_Quit();
    }
    if(step==0){
        settings.accelerated=settings.enhancedWeakPoints=settings.eyeInjuries=1;settings.weaponVulnerabilities=1;
        settings.monsterFeeding=settings.monsterBleeding=settings.headshotFrenzy=settings.hitFlinch=settings.voxelModels=0;
        settings.wounds=settings.glowingEyes=settings.visibleEyeInjuries=settings.corrosiveBarrels=1;
        settings.blood=settings.bloodSpray=settings.crosshair=0;
        applySettings();P_WeakpointInitFeedingHooks();p_wp_balance.eyeChance=100;
        check(P_TeleportMove(player,0,fx(-192)),"eye GPU observer placement");player->angle=ANG90;player->z=player->floorz;player->player->viewz=player->z+41*FRACUNIT;player->player->cheats|=CF_GODMODE;
        actor=P_SpawnMobj(0,fx(128),ONFLOORZ,stage==0?MT_HEAD:stage==1?MT_PAIN:MT_SPIDER);actor->health=5000;actor->angle=ANG270;actor->tics=-1;
        if(stage==2){victim=P_SpawnMobj(0,fx(400),ONFLOORZ,MT_TROOP);victim->health=5000;victim->tics=-1;}
        frontLump=eyeFixtureLump(actor);
        if(stage!=2)eyeStockVariants(actor->type,actor->sprite,frontLump);
        for (int n=0;n<2;++n) {cpu[n]=gpu[n]=0;samples[n]=0;}
        start=leveltime;step=1;return;
    }
    if(step==1){
        actor->tics=-1;
        if(leveltime-start==4 && stage!=2) {
            check(eyeBatch(-1-frontLump,true),"unblinded monster uses ordinary glowing texture");
            visualCapture=output+(stage==0?"/caco-normal.png":"/pain-normal.png");
            I_Render3DScreenshotTo(visualCapture.c_str());
        }
        if(leveltime-start<7)return;
        const auto &sf=sprites[actor->sprite].spriteframes[actor->frame&FF_FRAMEMASK];const auto &im=lumpImage(firstspritelump+sf.lump[0],false);
        auto mask=doom_regions::build(actor->type,im.width,im.height,im.left,im.top,im.pixels.data());int eye=-1;
        for(size_t i=0;i<mask.eyes.size();++i)if(mask.eyes[i]){eye=(int)i;break;}
        check(eye>=0,"GPU stock eye landmark");
        auto hit=[&] {
            P_WeakpointBeginShot(player,wp_pistol);
            shoot(player,units(actor->x)+eye%im.width+.5f-im.left,units(actor->y),
                  units(actor->z)+std::max(im.top,im.height)-eye/im.width-.5f);
            P_WeakpointEndShot();
        };
        if(stage!=2&&leveltime-start<10) {
            if(leveltime-start==7) {
                p_wp_balance.eyeChance=0;hit();p_wp_balance.eyeChance=100;
                check(!P_WeakpointBlind(actor),"eye hit with unsuccessful roll is not blindness");
                actor->frame=0;actor->tics=-1;
            }
            return;
        }
        if(stage!=2)check(eyeBatch(-1-frontLump,true),"unsuccessful eye hit keeps ordinary eye art and glow");
        hit();
        check(stage==2?P_WeakpointEyeRage(actor):P_WeakpointBlind(actor),"GPU eye hit triggers status");
        if(stage!=2) {
            actor->health=actor->info->spawnhealth;
            actor->frame=0;actor->tics=-1;actor->angle=ANG270;
            injuredKey=-1-frontLump;
            auto &original=lumpImage(frontLump,false);
            auto &injured=monsterEyeInjuryImage(actor->type,frontLump,original,original,injuredKey);
            coatAcidActor(actor,player->x+fx(32),player->y);
            coatedKeys[0]=-1-frontLump;coatedKeys[1]=injuredKey;
            auto &ordinaryAcid=acidCoatedImage(actor,original,coatedKeys[0],units(player->x),units(player->y),false,nullptr);
            auto &injuredAcid=acidCoatedImage(actor,injured,coatedKeys[1],units(player->x),units(player->y),false,&original);
            check(coatedKeys[0]!=-1-frontLump&&coatedKeys[1]!=injuredKey&&coatedKeys[0]!=coatedKeys[1]&&
                  ordinaryAcid.pixels!=injuredAcid.pixels&&injuredAcid.pixels!=injured.pixels,
                  "healthy and injured acid composites retain distinct artwork");
            for(size_t i=0;i<injured.eyeDamageMask.size();++i)
                if(injured.eyeDamageMask[i])check(injuredAcid.pixels[2*i]==injured.pixels[2*i],"acid preserves closed eyelid");
            forgetAcidActor(actor);
        }
        start=leveltime;step=2;return;
    }
    player->angle=R_PointToAngle2(player->x,player->y,actor->x,actor->y);
    if(stage!=2&&step==2) {
        const int age=leveltime-start;
        if(age==2) {
            check(P_WeakpointBlind(actor)&&actor->health==actor->info->spawnhealth&&eyeBatch(injuredKey,false),
                  "blinded full-health monster draws injury without glow");
            visualCapture=output+(stage==0?"/caco-injured.png":"/pain-injured.png");
            I_Render3DScreenshotTo(visualCapture.c_str());
        }
        if(age==5)settings.visibleEyeInjuries=0;
        if(age==8) {
            check(P_WeakpointBlind(actor)&&eyeBatch(-1-frontLump,true),"cosmetic switch restores normal art and glow only");
            settings.visibleEyeInjuries=1;
        }
        if(age==11) {
            check(eyeBatch(injuredKey,false),"reenabling cosmetics reuses injured texture");
            for(int n=0;n<3;++n) {
                group[n]=P_SpawnMobj(fx(float(n-1)*96),fx(n==1?248.f:128.f),ONFLOORZ,actor->type);
                group[n]->angle=R_PointToAngle2(group[n]->x,group[n]->y,player->x,player->y);
                eyeFixtureBlind(group[n],player);
                group[n]->health=group[n]->info->spawnhealth/5;
            }
            settings.monsterSkinDamage=1;
            auto &original=lumpImage(frontLump,false);
            groupKey=-1-frontLump;
            auto &skin=monsterSkinImage(actor->type,frontLump,original,group[0]->health,group[0]->info->spawnhealth,groupKey);
            monsterEyeInjuryImage(actor->type,frontLump,original,skin,groupKey);
            coatAcidActor(actor,player->x+fx(32),player->y);
            settings.visibleEyeInjuries=0;
            I_Render3DProfile(1,0);
        }
        if(age>11&&age<49)
            for(auto *member:group) {
                member->frame=0;member->tics=-1;
                member->angle=R_PointToAngle2(member->x,member->y,player->x,player->y);
            }
        if(age==30)settings.visibleEyeInjuries=1;
        const int mode=age>=15&&age<30?0:age>=34&&age<49?1:-1;
        if(mode>=0) {
            const auto *frame=I_Render3DLastFrame();
            cpu[mode]+=frame->cpuMs;gpu[mode]+=frame->gpuMs;++samples[mode];
        }
        if(age==49) {
            check(eyeBatch(groupKey,false)&&batches.at(groupKey).size()>=18,
                  "three blinded monsters share the same injured sprite batch");
            I_Render3DProfile(0,0);
            for(int n=0;n<2;++n)
                std::printf("EYE_FRAME stage %d visible %d samples %d mean CPU %.3f ms GPU fence %.3f ms\n",
                            stage,n,samples[n],cpu[n]/samples[n],gpu[n]/samples[n]);
            for(auto *member:group)P_RemoveMobj(member);
            forgetAcidActor(actor);
            actor->tics=1;
        }
    }
    if(step==2&&leveltime-start==82){
        check(stage==2?P_WeakpointEyeRage(actor):P_WeakpointBlind(actor),"live status remains active at capture");
        if(stage==2)check(actor->target==victim,"live Spider attacks another monster");
        visualCapture=output+(stage==0?"/caco-blind-fire.png":stage==1?"/pain-blind-fire.png":"/spider-eye-rage.png");I_Render3DScreenshotTo(visualCapture.c_str());
        std::printf("EYE_SCENE stage %d status confirmed at tic %d\n",stage,leveltime);
    }
    if(stage!=2&&step==2&&leveltime-start>=86) {
        check(P_TeleportMove(actor,0,fx(128)),"return floater to the recovery fixture");
        actor->frame=0;actor->tics=-1;actor->angle=ANG270;actor->momx=actor->momy=actor->momz=0;
        auto *corpse=P_SpawnMobj(actor->x+fx(24),actor->y,ONFLOORZ,MT_TROOP);
        P_DamageMobj(corpse,player,player,10000);corpse->tics=-1;corpse->z=corpse->floorz;
        actor->z=actor->floorz;
        coatAcidActor(actor,player->x+fx(32),player->y);
        start=leveltime;step=3;return;
    }
    if(step==3) {
        // The real feeding tick may be waiting for the next scheduled search.
        actor->tics=-1;
        P_WeakpointFeedingTick(actor);
        P_WeakpointFeedingChase(actor);P_WeakpointFeedingAttack(actor);P_WeakpointFeedingChase(actor);
        if(P_WeakpointBlind(actor)) {
            check(leveltime-start<70,"GPU scene recovers through the real corpse-feeding path");
            return;
        }
        actor->frame=0;actor->tics=-1;actor->angle=ANG270;
        player->angle=R_PointToAngle2(player->x,player->y,actor->x,actor->y);
        start=leveltime;step=4;return;
    }
    if(step==4&&leveltime-start==2) {
        auto &original=lumpImage(frontLump,false);
        int key=-1-frontLump;
        auto &skin=monsterSkinImage(actor->type,frontLump,original,actor->health,actor->info->spawnhealth,key);
        auto &coated=acidCoatedImage(actor,skin,key,units(player->x),units(player->y),false,nullptr);
        check(&coated!=&skin&&eyeBatch(key,true),"recovered actor keeps the ordinary acid composite");
        forgetAcidActor(actor);
    }
    if(step==4&&leveltime-start==4) {
        check(eyeBatch(-1-frontLump,true),"recovered monster draws its ordinary glowing texture");
        visualCapture=output+(stage==0?"/caco-recovered.png":"/pain-recovered.png");
        I_Render3DScreenshotTo(visualCapture.c_str());
    }
    if(step==4&&leveltime-start==6) {
        eyeFixtureBlind(actor,player);
        actor->health=actor->info->spawnhealth;
    }
    if(step==4&&leveltime-start==8) {
        check(eyeBatch(injuredKey,false),"a later blindness event reuses the injured sprite");
        P_DamageMobj(actor,player,player,10000);
        check(!P_WeakpointBlind(actor),"death clears living blindness eligibility");
    }
    if((stage==2&&leveltime-start>=86)||(step==4&&leveltime-start>=10)){
        for(auto*t=thinkercap.next,*next=t->next;t!=&thinkercap;t=next,next=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker){auto*m=(mobj_t*)t;if(m!=player){forgetWounds(m);P_RemoveMobj(m);}}
        actor=victim=nullptr;++stage;step=0;
    }
}
