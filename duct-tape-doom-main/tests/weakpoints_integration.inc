// Included by critical_smoke.cpp: stock-art coverage and real engine paths.
extern "C" {
#include "g_game.h"
void G_DoSaveGame(void);
void G_DoLoadGame(void);
void P_XYMovement(mobj_t*);
boolean P_Move(mobj_t*);
void A_CyberAttack(mobj_t*);
void A_BFGSpray(mobj_t*);
void A_FireShotgun(player_t*,pspdef_t*);
void A_FireShotgun2(player_t*,pspdef_t*);
}
static void dumpRegionOverlay(int type,const Image &im,const std::string &name) {
    const auto &mask=weakpointRegionMask(type,im);
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::string path=output+"/"+name+".ppm";FILE *file=std::fopen(path.c_str(),"wb");check(file!=nullptr,"region overlay output");
    std::fprintf(file,"P6\n%d %d\n255\n",im.width,im.height);
    const byte colors[][3]={{20,25,30},{0,0,0},{255,40,40},{255,160,20},{0,255,150},{255,0,255},{0,150,255},{0,0,0}};
    for(size_t i=0;i<mask.size();++i) {
        byte rgb[3]={20,25,30};if(im.pixels[i*2+1])for(int c=0;c<3;++c) {
            rgb[c]=palette[im.pixels[i*2]*3+c];
            if(mask[i]>=WP_HEAD&&mask[i]<=WP_LEGS)rgb[c]=(byte)((rgb[c]+2*colors[mask[i]][c])/3);
        }
        std::fwrite(rgb,1,3,file);
    }
    std::fclose(file);
}
static void enhancedCoverage() {
    auto *shooter=players[consoleplayer].mo;
    settings.enhancedWeakPoints=1;settings.weaponVulnerabilities=0;settings.localizedInjuries=0;
    settings.wounds=0;P_WeakpointConfigure(1,0,0);
    std::string path=output+"/region-coverage.csv";FILE *report=std::fopen(path.c_str(),"w");check(report!=nullptr,"coverage output");
    std::fputs("monster,stock_views,head_views,face_views,eye_views,brain_views,leg_views,normal_only_views\n",report);
    for(auto type:{MT_TROOP,MT_UNDEAD,MT_VILE,MT_KNIGHT,MT_BRUISER,MT_FATSO,MT_CYBORG,MT_SERGEANT,MT_SHADOWS,MT_HEAD,MT_PAIN,MT_BABY,MT_SPIDER}) {
        auto *monster=P_SpawnMobj(0,fx(128),ONFLOORZ,type);monster->angle=ANG270;
        int sprite=monster->sprite,views=0,coverage[8]={},overlayCount=0;
        std::set<int> liveFrames;
        for(int seed:{mobjinfo[type].spawnstate,mobjinfo[type].seestate,mobjinfo[type].meleestate,mobjinfo[type].missilestate,mobjinfo[type].painstate}) {
            std::set<int> visited;
            for(int at=seed;at&&at<NUMSTATES&&visited.insert(at).second;at=states[at].nextstate)
                if(states[at].sprite==sprite)liveFrames.insert(states[at].frame&FF_FRAMEMASK);
        }
        if(type==MT_VILE)for(int state:{S_VILE_HEAL1,S_VILE_HEAL2,S_VILE_HEAL3})liveFrames.insert(states[state].frame&FF_FRAMEMASK);
        // Stock fingerprints identify live artwork; do not test corpse art
        // as a living weak point merely because it shares a sprite family.
        for(int frame=0;frame<sprites[sprite].numframes;++frame)for(int rotation=0;rotation<8;++rotation) {
            if(!liveFrames.count(frame))continue;
            const auto &sf=sprites[sprite].spriteframes[frame];
            int lump=firstspritelump+sf.lump[rotation];Image &im=patch(lump);
            const auto &mask=weakpointRegionMask(type,im);
            unsigned hash=2166136261u;for(byte value:im.pixels)hash=(hash^value)*16777619u;
            bool stock=false;for(const auto &entry:doom_stock_eye_table)if(entry.width==im.width&&entry.height==im.height&&entry.hash==hash){stock=true;break;}
            for(const auto &entry:doom_region_stock_table)if(entry.width==im.width&&entry.height==im.height&&entry.hash==hash){stock=true;break;}
            check(stock,"live stock frame has fingerprint metadata, including fullbright attacks and heals");++views;
            if(frame==0&&rotation==0) {
                std::vector<byte> replacement=im.pixels;
                for(size_t i=0;i<replacement.size();i+=2)if(replacement[i+1]){replacement[i]^=1;break;}
                auto changed=doom_regions::build(type,im.width,im.height,im.left,im.top,replacement.data());
                check(!changed.stock,"replacement artwork cannot inherit stock metadata after its pixels change");
                if(type==MT_HEAD||type==MT_PAIN||type==MT_SERGEANT||type==MT_SHADOWS||type==MT_BABY||type==MT_SPIDER)
                    check(std::none_of(changed.pixels.begin(),changed.pixels.end(),[](byte p){return p>=WP_HEAD&&p<=WP_LEGS;}),"unrecognized non-humanoid art has no guessed critical region");
            }
            bool parts[8]={};for(byte part:mask)parts[part]=true;
            bool anatomical=false;for(int p=WP_HEAD;p<=WP_LEGS;++p){if(parts[p])++coverage[p];anatomical|=parts[p];}
            if(!anatomical)++coverage[WP_BODY];
            int attackFrame=states[mobjinfo[type].missilestate].frame&FF_FRAMEMASK,painFrame=states[mobjinfo[type].painstate].frame&FF_FRAMEMASK;
            if((rotation==0&&(frame==0||frame==attackFrame||frame==painFrame))||(frame==0&&(rotation==1||rotation==2||rotation==4))) {
                dumpRegionOverlay(type,im,"regions-"+std::to_string(type)+"-"+std::to_string(frame)+"-"+std::to_string(rotation));++overlayCount;
            }
            for(int region=WP_BODY;region<=WP_CHASSIS;++region) {
                if(!parts[region])continue;
                size_t pixel=std::find(mask.begin(),mask.end(),(byte)region)-mask.begin();
                int x=(int)(pixel%im.width),y=(int)(pixel/im.width);
                monster->sprite=(spritenum_t)sprite;monster->frame=frame;monster->health=10000;monster->flags|=MF_SHOOTABLE;
                forgetWounds(monster);
                float fittedTop=(float)std::max(im.top,im.height);
                woundView(monster,shooter,im,sf.flip[rotation],0,128,fittedTop-im.top,fittedTop,fittedTop-im.height,1,0,nullptr,rotation);
                float across=sf.flip[rotation]?im.width-im.left-x-0.5f:x+0.5f-im.left;
                P_WeakpointBeginShot(shooter,wp_shotgun);
                shoot(shooter,across,128,fittedTop-y-0.5f);
                P_WeakpointEndShot();
                int expected=P_WeakpointScale(5,P_WeakpointRegionPercent(type,WP_SHOTGUN_PELLET,(wp_region_t)region,100));
                if(monster->health!=10000-expected)std::fprintf(stderr,"Region mismatch type=%d frame=%d rotation=%d region=%d pixel=%d,%d damage=%d expected=%d\n",type,frame,rotation,region,x,y,10000-monster->health,expected);
                check(monster->health==10000-expected,"stock anatomy must share exact sprite collision and damage classification");
            }
        }
        check(views>0,"every new monster has verified live artwork coverage");
        int required=type==MT_HEAD||type==MT_PAIN?WP_EYE:type==MT_SERGEANT||type==MT_SHADOWS?WP_FACE:type==MT_BABY||type==MT_SPIDER?WP_BRAIN:WP_HEAD;
        check(coverage[required]>0,"every new monster has detectable critical anatomy");
        if(type==MT_BABY||type==MT_SPIDER)check(coverage[WP_LEGS]>0,"mechanical legs have a distinct verified mask");
        std::fprintf(report,"%d,%d,%d,%d,%d,%d,%d,%d\n",type,views,coverage[WP_HEAD],coverage[WP_FACE],coverage[WP_EYE],coverage[WP_BRAIN],coverage[WP_LEGS],coverage[WP_BODY]);
        P_RemoveMobj(monster);forgetWounds(monster);
    }
    std::fclose(report);std::puts("PASS full stock live-frame/rotation anatomy coverage; overlays and CSV saved");
}
static void featureSwitchConfig() {
    Settings configured;
    check(!configured.enhancedWeakPoints&&!configured.weaponVulnerabilities&&!configured.localizedInjuries,"all three new feature switches default off");
    readOption(configured,"critical_parts",1);
    readOption(configured,"enhanced_weak_points",1);readOption(configured,"monster_weapon_vulnerabilities",1);readOption(configured,"localized_injury_effects",1);
    check(configured.criticalParts&&configured.enhancedWeakPoints&&configured.weaponVulnerabilities&&configured.localizedInjuries,"F4 feature switches load independently through the existing config table");
    readOption(configured,"enhanced_weak_points",0);readOption(configured,"monster_weapon_vulnerabilities",0);readOption(configured,"localized_injury_effects",0);
    check(configured.criticalParts,"disabling new features does not disable original human headshots");
    FILE *file=std::tmpfile();check(file!=nullptr,"feature config output");writeOptions(file,configured);std::rewind(file);
    char line[256];int found=0;while(std::fgets(line,sizeof(line),file))if(std::strstr(line,"enhanced_weak_points 0")||std::strstr(line,"monster_weapon_vulnerabilities 0")||std::strstr(line,"localized_injury_effects 0"))++found;
    std::fclose(file);check(found==3,"all three feature switches persist through the existing graphics serializer");
}
static void realProjectiles() {
    auto *shooter=players[consoleplayer].mo;
    settings.enhancedWeakPoints=1;settings.weaponVulnerabilities=1;settings.localizedInjuries=0;
    P_WeakpointConfigure(1,1,0);P_WeakpointLocate=weakpointLocate;
    // This headless fixture has no GPU; ensure every human/demon projectile
    // target frame needed by the fallback placement is decoded into the CPU cache.
    for(auto type:{MT_HEAD,MT_PAIN,MT_BABY,MT_SPIDER,MT_FATSO,MT_CYBORG,MT_SKULL}) {
        auto *target=P_SpawnMobj(0,fx(128),ONFLOORZ,type);target->health=10000;target->angle=ANG270;
        for(int f=0;f<sprites[target->sprite].numframes;++f)for(int r=0;r<8;++r)patch(firstspritelump+sprites[target->sprite].spriteframes[f].lump[r]);
        auto *plasma=P_SpawnMobj(0,fx(90),fx(24),MT_PLASMA);plasma->target=shooter;plasma->momx=0;plasma->momy=fx(24);
        P_WeakpointProjectileSpawn(plasma,shooter,WP_PLASMA_DIRECT);
        player_t *player=shooter->player;auto previous=player->readyweapon;player->readyweapon=wp_pistol;
        int before=target->health;
        bool moved=P_TryMove(plasma,0,fx(124))!=0;
        check(!moved,"plasma physically collides with the target actor");
        int damage=before-target->health;
        if(type==MT_SKULL)check(target->health<=0,"physical plasma impact kills Lost Soul");
        else {
            int percent=type==MT_HEAD||type==MT_PAIN?200:type==MT_BABY||type==MT_SPIDER?300:type==MT_FATSO?175:150;
            bool valid=false;for(int roll=1;roll<=8;++roll)valid|=damage==P_WeakpointScale(roll*mobjinfo[MT_PLASMA].damage,percent);
            check(valid,"real projectile collision applies the original plasma provenance multiplier");
        }
        player->readyweapon=previous;P_RemoveMobj(plasma);P_RemoveMobj(target);forgetWounds(target);
    }
    // Real rocket splash immunity must remain in vanilla radius processing.
    auto *cyber=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_CYBORG);
    auto *spider=P_SpawnMobj(fx(40),fx(128),ONFLOORZ,MT_SPIDER);
    auto *rocket=P_SpawnMobj(0,fx(100),fx(24),MT_ROCKET);rocket->target=shooter;
    int ch=cyber->health,sh=spider->health;P_RadiusAttack(rocket,shooter,128);
    check(cyber->health==ch&&spider->health==sh,"Cyberdemon and Spider Mastermind keep vanilla splash immunity");
    P_RemoveMobj(rocket);P_RemoveMobj(cyber);P_RemoveMobj(spider);
    std::puts("PASS physical projectile provenance and splash-immunity integration");
}
static void realInjuriesAndSave() {
    auto *shooter=players[consoleplayer].mo;
    settings.localizedInjuries=1;P_WeakpointConfigure(1,1,1);
    int oldLeg=p_wp_balance.legChance,oldAim=p_wp_balance.aimChance;
    p_wp_balance.legChance=p_wp_balance.aimChance=100;
    auto *baby=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_BABY);
    wp_damage_event_t e{};e.target=baby;e.source=e.inflictor=shooter;e.kind=WP_ROCKET_DIRECT;
    e.impact.region=WP_LEGS;e.impact.confidence=100;e.baseDamage=4;P_WeakpointApply(&e);
    check(P_WeakpointImmobilized(baby)!=0,"real mechanical injury activates");
    fixed_t oldX=baby->x;baby->movedir=0;
    check(P_Move(baby)&&baby->x==oldX,"immobilization blocks AI walking without replacing states");
    baby->momx=FRACUNIT;P_XYMovement(baby);check(baby->x==oldX&&baby->momx==0,"immobilization blocks horizontal drift");
    baby->z=fx(16);baby->momz=-FRACUNIT;baby->tics=-1;P_MobjThinker(baby);
    check(baby->z<fx(16),"immobilization preserves vertical gravity");baby->z=baby->floorz;
    auto *cyber=P_SpawnMobj(fx(100),fx(100),ONFLOORZ,MT_CYBORG);cyber->target=shooter;cyber->health=399;
    e={};e.target=cyber;e.source=e.inflictor=shooter;e.kind=WP_BULLET;e.impact.region=WP_HEAD;e.impact.confidence=100;e.baseDamage=5;
    P_WeakpointApply(&e);angle_t expected=R_PointToAngle2(cyber->x,cyber->y,shooter->x,shooter->y);
    A_CyberAttack(cyber);mobj_t *enemyRocket=nullptr;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker) {
        auto *m=(mobj_t*)t;if(m->type==MT_ROCKET&&m->target==cyber)enemyRocket=m;
    }
    check(enemyRocket!=nullptr,"injured heavy still performs its normal ranged attack");
    int32_t deviation=(int32_t)(enemyRocket->angle-expected);
    check(deviation!=0&&std::abs((int64_t)deviation)<=4294967296LL*8/360,"real heavy projectile uses a bounded aim deviation");
    check(enemyRocket->target==cyber&&cyber->target==shooter,"injury preserves projectile ownership and attack target");
    P_RemoveMobj(enemyRocket);P_DamageMobj(cyber,shooter,shooter,10000);
    check(cyber->health<=0&&(cyber->flags&MF_CORPSE),"injury never prevents normal boss death processing");P_RemoveMobj(cyber);
    // Save actual native records, not a synthetic subset of the serializer.
    auto *plasma=P_SpawnMobj(fx(120),fx(0),fx(24),MT_PLASMA);plasma->target=shooter;
    P_WeakpointProjectileSpawn(plasma,shooter,WP_PLASMA_DIRECT);
    P_WeakpointProjectileStep(plasma,fx(120),0,fx(24),fx(120),fx(20),fx(24));
    G_SaveGame(8,(char*)"Weakpoint integration");G_DoSaveGame();
    G_LoadGame((char*)"doomsav8.dsg");G_DoLoadGame();
    baby=plasma=nullptr;shooter=players[consoleplayer].mo;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker) {
        auto *m=(mobj_t*)t;if(m->type==MT_BABY&&m->health>0)baby=m;
        if(m->type==MT_PLASMA&&(m->flags&MF_MISSILE))plasma=m;
    }
    check(baby&&P_WeakpointImmobilized(baby),"actual native save/load restores remaining injury state");
    check(plasma&&plasma->target==shooter,"actual native save/load restores in-flight owner identity");
    float start[3],end[3];check(P_WeakpointTrajectory(plasma,start,end)&&end[1]==20,"actual native save/load restores projectile provenance/trajectory");
    leveltime+=35;check(!P_WeakpointImmobilized(baby),"restored injury expires instead of permanently freezing a monster");
    // A native save without our trailer represents the previous mod format.
    FILE *file=std::fopen("doomsav8.dsg","rb");check(file!=nullptr,"native save output");
    std::fseek(file,0,SEEK_END);long size=std::ftell(file);std::rewind(file);std::vector<byte> bytes((size_t)size);
    check(std::fread(bytes.data(),1,bytes.size(),file)==bytes.size(),"native save readback");std::fclose(file);
    size_t tail=P_WeakpointLoad(bytes.data(),bytes.size());check(tail>0&&tail<bytes.size(),"new save contains a bounded optional trailer");
    file=std::fopen("weakpoint-legacy.dsg","wb");check(file!=nullptr,"legacy compatibility fixture");
    std::fwrite(bytes.data(),1,bytes.size()-tail,file);std::fclose(file);
    G_LoadGame((char*)"weakpoint-legacy.dsg");G_DoLoadGame();check(P_WeakpointRuntimeCount()==0,"previous-format native saves load without stale injury records");
    p_wp_balance.legChance=oldLeg;p_wp_balance.aimChance=oldAim;
    std::puts("PASS real AI injury, native save/load and previous-format save compatibility");
}
static void localizedRockets() {
    auto *shooter=players[consoleplayer].mo;
    settings.weaponVulnerabilities=settings.localizedInjuries=1;P_WeakpointConfigure(1,1,1);
    int oldEye=p_wp_balance.eyeExecutionChance,oldLeg=p_wp_balance.legChance;
    p_wp_balance.eyeExecutionChance=p_wp_balance.legChance=100;
    for(auto type:{MT_HEAD,MT_PAIN,MT_BABY,MT_SPIDER}) {
        auto *target=P_SpawnMobj(0,fx(128),ONFLOORZ,type);target->angle=ANG270;
        auto &sf=sprites[target->sprite].spriteframes[target->frame&FF_FRAMEMASK];auto &im=patch(firstspritelump+sf.lump[0]);
        const auto &mask=weakpointRegionMask(type,im);int desired=type==MT_HEAD||type==MT_PAIN?WP_EYE:WP_LEGS;
        int best=-1,score=-1;float top=(float)std::max(im.top,im.height);
        for(int y=0;y<im.height;++y)for(int x=0;x<im.width;++x) {
            if(mask[(size_t)y*im.width+x]!=desired)continue;
            float across=x+0.5f-im.left,z=top-y-0.5f;
            if(std::abs(across)>units(target->radius)+10||z<4.5f||z>units(target->height)+3)continue;
            int density=0;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
                if(x+dx>=0&&x+dx<im.width&&y+dy>=0&&y+dy<im.height&&mask[(size_t)(y+dy)*im.width+x+dx]==desired)++density;
            if(density>score){score=density;best=y*im.width+x;}
        }
        check(best>=0,"stock critical region has a physically reachable rocket contact");
        float across=sf.flip[0]?im.width-im.left-best%im.width-0.5f:best%im.width+0.5f-im.left;
        float z=top-best/im.width-0.5f;
        auto *rocket=P_SpawnMobj(fx(across),0,fx(z-4),MT_ROCKET);rocket->target=shooter;
        P_WeakpointProjectileSpawn(rocket,shooter,WP_ROCKET_DIRECT);
        P_WeakpointProjectileStep(rocket,rocket->x,0,rocket->z,rocket->x,fx(96),rocket->z);
        forgetWounds(target);woundView(target,shooter,im,sf.flip[0],0,128,top-im.top,top,top-im.height,1,0,nullptr,0);
        wp_impact_t impact{};
        if(!weakpointLocate(rocket,target,&impact)||impact.region!=desired)
            std::fprintf(stderr,"Rocket localization type=%d requested=%d got=%d confidence=%d pixel=%d,%d\n",type,desired,impact.region,impact.confidence,best%im.width,best/im.width);
        check(impact.region==desired&&impact.confidence==100,"swept contact footprint localizes the actual stock eye/leg region");
        bool moved=P_TryMove(rocket,rocket->x,fx(96))!=0;check(!moved,"localized rocket uses the ordinary physical actor collision");
        if(desired==WP_EYE)check(target->health<=0,"physical rocket eye hit executes with configured 100 percent chance");
        else check(P_WeakpointImmobilized(target),"physical rocket leg hit applies the configured bounded injury");
        P_RemoveMobj(rocket);P_RemoveMobj(target);forgetWounds(target);
    }
    p_wp_balance.eyeExecutionChance=oldEye;p_wp_balance.legChance=oldLeg;
    std::puts("PASS real swept rocket eye/leg localization and conditional outcomes");
}
static void realBFGAndMovement() {
    auto *shooter=players[consoleplayer].mo;P_WeakpointConfigure(1,1,0);
    auto *skull=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_SKULL);skull->health=10000;
    auto *bfg=P_SpawnMobj(0,fx(80),fx(24),MT_BFG);bfg->target=shooter;
    P_WeakpointProjectileSpawn(bfg,shooter,WP_BFG_DIRECT);
    check(!P_TryMove(bfg,0,fx(124))&&skull->health<=0,"real BFG main projectile kills only its physical Lost Soul target");
    P_RemoveMobj(bfg);P_RemoveMobj(skull);
    skull=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_SKULL);skull->health=10000;
    auto *hidden=P_SpawnMobj(0,fx(220),ONFLOORZ,MT_SKULL);hidden->health=10000;
    auto *outside=P_SpawnMobj(fx(280),fx(-32),ONFLOORZ,MT_SKULL);outside->health=10000;
    bfg=P_SpawnMobj(0,fx(80),fx(24),MT_BFG);bfg->target=shooter;bfg->angle=ANG90;
    P_WeakpointProjectileSpawn(bfg,shooter,WP_BFG_DIRECT);A_BFGSpray(bfg);
    check(skull->health<=0,"actual BFG spray tracer kills a valid Lost Soul target");
    check(hidden->health==10000&&outside->health==10000,"BFG discharge cannot execute Lost Souls behind walls or outside valid tracer rays");
    P_RemoveMobj(bfg);P_RemoveMobj(skull);P_RemoveMobj(hidden);P_RemoveMobj(outside);
    // Exercise the normal movement entry point, including trajectory capture
    // before a collision. Change the held weapon after the projectile exists.
    auto *target=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_HEAD);target->health=10000;
    auto *plasma=P_SpawnMobj(0,fx(70),fx(24),MT_PLASMA);plasma->target=shooter;plasma->momy=fx(25);
    P_WeakpointProjectileSpawn(plasma,shooter,WP_PLASMA_DIRECT);
    P_XYMovement(plasma);P_XYMovement(plasma);
    check(target->health<10000,"projectile movement captures provenance before an actor contact");
    float start[3],end[3];check(P_WeakpointTrajectory(plasma,start,end)&&end[1]>start[1],"movement between tics records a finite swept trajectory");
    P_RemoveMobj(plasma);P_RemoveMobj(target);
    skull=P_SpawnMobj(0,fx(-8),ONFLOORZ,MT_SKULL);skull->health=10000;
    boolean wasFree=freeaim,wasClassic=classicaim;float wasPitch=tourpitch;
    freeaim=true;classicaim=false;tourpitch=0;shooter->angle=ANG90;
    P_SpawnPlayerMissile(shooter,MT_PLASMA);
    check(skull->health<=0,"provenance is recorded before an immediate player-projectile spawn collision");
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker) {
        auto *m=(mobj_t*)t;if(m->type==MT_PLASMA&&m->target==shooter)P_RemoveMobj(m);
    }
    P_RemoveMobj(skull);freeaim=wasFree;classicaim=wasClassic;tourpitch=wasPitch;
    std::puts("PASS real BFG direct/tracer eligibility and moving projectile contact");
}
static void realShotgunDischarges() {
    auto *shooter=players[consoleplayer].mo;auto *player=shooter->player;
    auto held=player->readyweapon;boolean oldFree=freeaim,oldClassic=classicaim;float oldPitch=tourpitch;
    freeaim=true;classicaim=false;P_WeakpointConfigure(1,1,0);
    for(auto weapon:{wp_shotgun,wp_supershotgun}) {
        auto *target=P_SpawnMobj(0,fx(32),ONFLOORZ,MT_SERGEANT);target->angle=ANG270;target->health=10000;
        auto &sf=sprites[target->sprite].spriteframes[target->frame&FF_FRAMEMASK];auto &im=patch(firstspritelump+sf.lump[0]);
        const auto &mask=weakpointRegionMask(MT_SERGEANT,im);
        int best=-1,score=-1;
        for(int y=0;y<im.height;++y)for(int x=0;x<im.width;++x)if(mask[(size_t)y*im.width+x]==WP_FACE) {
            int density=0;for(int dy=-5;dy<=5;++dy)for(int dx=-5;dx<=5;++dx)
                if(x+dx>=0&&x+dx<im.width&&y+dy>=0&&y+dy<im.height&&mask[(size_t)(y+dy)*im.width+x+dx]==WP_FACE)++density;
            if(density>score){score=density;best=y*im.width+x;}
        }
        check(best>=0,"shotgun fixture has a reliable stock face");
        float top=(float)std::max(im.top,im.height),across=best%im.width+0.5f-im.left,z=top-best/im.width-0.5f;
        shooter->angle=R_PointToAngle2(shooter->x,shooter->y,fx(across),target->y);
        float distance=std::hypot(across,64.f);tourpitch=std::atan((z-units(player->viewz))/distance);
        player->readyweapon=weapon;player->ammo[am_shell]=100;
        woundView(target,shooter,im,false,0,32,top-im.top,top,top-im.height,1,0,nullptr,0);
        if(weapon==wp_shotgun)A_FireShotgun(player,&player->psprites[ps_weapon]);
        else A_FireShotgun2(player,&player->psprites[ps_weapon]);
        check(target->health<=0,"complete close-range native shotgun discharge can execute a confirmed face target");
        P_RemoveMobj(target);forgetWounds(target);
    }
    player->readyweapon=held;freeaim=oldFree;classicaim=oldClassic;tourpitch=oldPitch;shooter->angle=ANG90;
    std::puts("PASS native standard/super shotgun discharge tracking and close face executions");
}
static void cachedHitBenchmark() {
    auto *shooter=players[consoleplayer].mo;
    auto *target=P_SpawnMobj(0,fx(128),ONFLOORZ,MT_TROOP);
    const auto &sf=sprites[target->sprite].spriteframes[target->frame&FF_FRAMEMASK];const auto &im=patch(firstspritelump+sf.lump[0]);
    double times[2]={};volatile int checksum=0;
    for(int mode=0;mode<2;++mode) {
        target->type=mode?MT_TROOP:MT_POSSESSED;settings.enhancedWeakPoints=mode;P_WeakpointConfigure(mode,0,0);
        woundView(target,shooter,im,false,0,128,0,(float)im.top,(float)(im.top-im.height),1,0);
        uint64_t start=0;
        for(int i=-100;i<10000;++i) {
            if(i==0)start=SDL_GetTicksNS();
            critical_hit_t hit{};criticalBegin(shooter,0,fx(-32),fx(41),0,fx(2048),0,fx(2048));
            if(!criticalThing(target,&hit)||hit.fraction<0)I_Error((char*)"Cached hit benchmark missed the fixture sprite");
            checksum+=hit.percent;
        }
        times[mode]=(SDL_GetTicksNS()-start)/1000.0/10000;
    }
    check(times[1]<times[0]*4+5,"cached anatomical lookup has no major recurring query-time regression");
    FILE *file=std::fopen((output+"/collision-benchmark.csv").c_str(),"w");check(file!=nullptr,"collision benchmark output");
    std::fprintf(file,"path,queries,mean_microseconds\noriginal_human,10000,%.6f\nexpanded_anatomy,10000,%.6f\n",times[0],times[1]);std::fclose(file);
    std::printf("Cached same-art query benchmark: human %.3f us; expanded %.3f us (checksum %d)\n",times[0],times[1],(int)checksum);
    target->type=MT_TROOP;P_RemoveMobj(target);forgetWounds(target);settings.enhancedWeakPoints=1;P_WeakpointConfigure(1,0,0);
}
static void balanceHitCounts() {
    // A fixed valid roll isolates balance from aim, AI, splash and damage
    // variance. With unchanged fire cadence, hit counts also give an ideal
    // TTK ratio after the first impact; this is not a full combat benchmark.
    struct Scenario {mobjtype_t type;wp_damage_kind_t kind;wp_region_t region;int damage;};
    const Scenario rows[]={
        {MT_TROOP,WP_BULLET,WP_HEAD,10},{MT_UNDEAD,WP_BULLET,WP_HEAD,10},{MT_VILE,WP_BULLET,WP_HEAD,10},
        {MT_KNIGHT,WP_BULLET,WP_HEAD,10},{MT_BRUISER,WP_BULLET,WP_HEAD,10},
        {MT_FATSO,WP_PLASMA_DIRECT,WP_BODY,20},{MT_CYBORG,WP_PLASMA_DIRECT,WP_BODY,20},
        {MT_SERGEANT,WP_SHOTGUN_PELLET,WP_FACE,10},{MT_SHADOWS,WP_SHOTGUN_PELLET,WP_FACE,10},
        {MT_HEAD,WP_PLASMA_DIRECT,WP_BODY,20},{MT_PAIN,WP_PLASMA_DIRECT,WP_BODY,20},{MT_SKULL,WP_PLASMA_DIRECT,WP_BODY,20},
        {MT_BABY,WP_PLASMA_DIRECT,WP_BODY,20},{MT_SPIDER,WP_PLASMA_DIRECT,WP_BODY,20}};
    FILE *file=std::fopen((output+"/balance-hit-counts.csv").c_str(),"w");check(file!=nullptr,"balance comparison output");
    std::fputs("monster,health,kind,fixed_roll,old_damage,new_damage,old_hits,new_hits,relative_ttk_after_first_impact\n",file);
    for(const auto &row:rows) {
        auto *target=P_SpawnMobj(0,fx(128),ONFLOORZ,row.type);int health=target->health,damage[2];
        for(int mode=0;mode<2;++mode) {
            P_WeakpointConfigure(mode,mode,0);wp_damage_event_t e{};e.target=target;e.source=e.inflictor=players[consoleplayer].mo;
            e.kind=row.kind;e.impact.region=row.region;e.impact.confidence=100;e.baseDamage=row.damage;P_WeakpointResolve(&e);damage[mode]=e.finalDamage;
        }
        int oldHits=(health+damage[0]-1)/damage[0],newHits=(health+damage[1]-1)/damage[1];
        check(damage[0]==row.damage&&newHits<=oldHits,"isolated balance comparison preserves baseline damage and applies the proposed weakness");
        double ratio=oldHits<=1?1.0:(double)(newHits-1)/(oldHits-1);
        std::fprintf(file,"%d,%d,%d,%d,%d,%d,%d,%d,%.4f\n",row.type,health,row.kind,row.damage,damage[0],damage[1],oldHits,newHits,ratio);
        P_RemoveMobj(target);forgetWounds(target);
    }
    std::fclose(file);P_WeakpointConfigure(1,0,0);std::puts("PASS isolated fixed-roll damage/hit-count and ideal TTK comparisons; balance defaults unchanged");
}
static void enhancedVisualFrame() {
    static const mobjtype_t types[]={MT_TROOP,MT_UNDEAD,MT_VILE,MT_KNIGHT,MT_BRUISER,MT_FATSO,MT_CYBORG,MT_SERGEANT,MT_SHADOWS,MT_HEAD,MT_PAIN,MT_BABY,MT_SPIDER};
    static int index=0,step=0,last=-1,wait=0;
    static mobj_t *victim=nullptr;
    static bool benchmarkDone=false;
    static int benchmarkFrames=0;
    static unsigned profileSerial=0;
    static double cpu[2]={},gpu[2]={};
    if(last==leveltime)return;last=leveltime;if(R_BeforeTic)R_BeforeTic();
    auto *shooter=players[consoleplayer].mo;
    if(step==0) {
        check(I_Render3DIsAccelerated()!=0,"enhanced visual renderer");
        if(index==(int)(sizeof(types)/sizeof(types[0]))){std::puts("PASS enhanced GPU captures for all thirteen new monster profiles");I_Quit();}
        if(!victim) {
            P_TeleportMove(shooter,0,fx(-32));shooter->angle=ANG90;shooter->z=shooter->floorz;
            players[consoleplayer].viewz=shooter->z+41*FRACUNIT;
            victim=P_SpawnMobj(0,fx(128),ONFLOORZ,types[index]);victim->angle=ANG270;victim->health=1000;victim->tics=-1;
        }
        wait=leveltime;step=1;return;
    }
    victim->tics=-1;victim->momx=victim->momy=victim->momz=0;
    if(step==1&&index==0&&!benchmarkDone&&leveltime-wait>=4) {
        settings.enhancedWeakPoints=settings.weaponVulnerabilities=settings.localizedInjuries=0;
        P_WeakpointConfigure(0,0,0);I_Render3DProfile(1,0);benchmarkFrames=0;step=10;return;
    }
    if(step==10) {
        const auto *profile=I_Render3DLastFrame();
        if(!profile||!profile->world||profile->serial==profileSerial)return;
        profileSerial=profile->serial;int phase=benchmarkFrames/12,within=benchmarkFrames%12;
        if(within>=4){cpu[phase]+=profile->cpuMs;gpu[phase]+=profile->gpuMs;}
        ++benchmarkFrames;
        if(benchmarkFrames==12) {
            settings.enhancedWeakPoints=settings.weaponVulnerabilities=settings.localizedInjuries=1;P_WeakpointConfigure(1,1,1);
        }
        if(benchmarkFrames==24) {
            FILE *file=std::fopen((output+"/frame-benchmark.csv").c_str(),"w");check(file!=nullptr,"bounded GPU frame benchmark output");
            std::fprintf(file,"rules,frames,mean_cpu_ms,mean_gpu_ms\noriginal,8,%.6f,%.6f\nenhanced,8,%.6f,%.6f\n",cpu[0]/8,gpu[0]/8,cpu[1]/8,gpu[1]/8);std::fclose(file);
            std::printf("Bounded same-scene frame benchmark: original CPU/GPU %.3f/%.3f ms; enhanced %.3f/%.3f ms\n",cpu[0]/8,gpu[0]/8,cpu[1]/8,gpu[1]/8);
            I_Render3DProfile(0,0);benchmarkDone=true;step=1;wait=leveltime;
        }
        return;
    }
    if(step==1&&leveltime-wait>=4) {
        visualCapture=output+"/enhanced-"+std::to_string(types[index])+"-before.png";
        I_Render3DScreenshotTo(visualCapture.c_str());step=2;return;
    }
    if(step==2) {
        const auto &sf=sprites[victim->sprite].spriteframes[victim->frame&FF_FRAMEMASK];
        const Image &im=lumpImage(firstspritelump+sf.lump[0],false);const auto &mask=weakpointRegionMask(victim->type,im);
        wp_region_t desired=victim->type==MT_SERGEANT||victim->type==MT_SHADOWS?WP_FACE:
            victim->type==MT_HEAD||victim->type==MT_PAIN?WP_EYE:victim->type==MT_BABY||victim->type==MT_SPIDER?WP_BRAIN:WP_HEAD;
        int best=-1,score=-1;
        for(int y=0;y<im.height;++y)for(int x=0;x<im.width;++x)if(mask[(size_t)y*im.width+x]==desired) {
            int density=0;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
                if(x+dx>=0&&x+dx<im.width&&y+dy>=0&&y+dy<im.height&&mask[(size_t)(y+dy)*im.width+x+dx]==desired)++density;
            if(density>score){score=density;best=y*im.width+x;}
        }
        check(best>=0,"enhanced visual profile has a visible stock critical region");
        float across=sf.flip[0]?im.width-im.left-best%im.width-0.5f:best%im.width+0.5f-im.left;
        float top=(float)std::max(im.top,im.height),z=top-best/im.width-0.5f;
        int before=victim->health;P_WeakpointBeginShot(shooter,wp_shotgun);
        shoot(shooter,across,128,z);P_WeakpointEndShot();
        int expected=P_WeakpointScale(5,P_WeakpointRegionPercent(victim->type,WP_SHOTGUN_PELLET,desired,100));
        check(before-victim->health==expected,"enhanced GPU critical damage matches region profile");
        WoundSpot wound[1];check(woundSpots(victim,wound,1)==1,"enhanced GPU critical hit leaves an attached wound independently of the damage rule");
        victim->momx=victim->momy=victim->momz=0;victim->tics=-1;
        visualCapture=output+"/enhanced-"+std::to_string(types[index])+"-after.png";
        I_Render3DScreenshotTo(visualCapture.c_str());step=3;return;
    }
    if(step==3) {
        FILE *file=std::fopen(visualCapture.c_str(),"rb");check(file!=nullptr,"enhanced GPU screenshot saved");std::fclose(file);
        P_RemoveMobj(victim);forgetWounds(victim);victim=nullptr;++index;step=0;
    }
}
